/*============================================================================
 MODULE: diag_capture

 RESPONSIBILITY
 - NVS flag read: if diag_dur key is set (90 or 120), arm the capture
 - Allocate ring buffer from PSRAM (128KB) or heap fallback (32KB)
 - Install vprintf hook that mirrors every log line to the ring buffer
 - Timer task fires at duration end, removes hook, marks log ready
 - Scrub function removes password values from buffer before display

 NVS KEY
 - Namespace: "cfg"  Key: "diag_dur"  Type: uint8
 - Value 0 = idle (normal boot)
 - Value 90 or 120 = capture that many seconds
 - Key is cleared immediately on arm (crash during capture = clean next boot)

 REVERT HISTORY
 R0  v0.12-flex  Initial implementation

============================================================================*/

#include "diag_capture.h"

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "nvs.h"

static const char *TAG = "diag_cap";

#define DIAG_BUF_PSRAM   (128 * 1024)
#define DIAG_BUF_HEAP    (32  * 1024)
#define DIAG_NVS_NS      "cfg"
#define DIAG_NVS_KEY     "diag_dur"

/* --------------------------------------------------------------------------
 * Module state
 * ---------------------------------------------------------------------- */

static char            *s_buf        = NULL;
static size_t           s_cap        = 0;
static volatile size_t  s_pos        = 0;
static volatile bool    s_armed      = false;
static volatile bool    s_ready      = false;
static bool             s_scrubbed   = false;
static uint8_t          s_duration   = 0;
static int64_t          s_arm_us     = 0;   /* esp_timer_get_time() at arm */
static vprintf_like_t   s_orig_vp    = NULL;

/* --------------------------------------------------------------------------
 * vprintf hook - mirrors log to ring buffer, passes through to UART
 * ---------------------------------------------------------------------- */

static int diag_vprintf(const char *fmt, va_list args)
{
    if (s_buf && s_pos < s_cap) {
        va_list args2;
        va_copy(args2, args);
        size_t cur    = s_pos;
        size_t remain = s_cap - cur;
        if (remain > 1) {
            int n = vsnprintf(s_buf + cur, remain, fmt, args2);
            if (n > 0) {
                s_pos = cur + ((size_t)n < remain ? (size_t)n : remain - 1);
            }
        }
        va_end(args2);
    }
    /* Also output to UART via original handler */
    return s_orig_vp ? s_orig_vp(fmt, args) : vprintf(fmt, args);
}

/* --------------------------------------------------------------------------
 * Timer task - fires after requested duration, closes capture
 * ---------------------------------------------------------------------- */

static void diag_timer_task(void *arg)
{
    vTaskDelay(pdMS_TO_TICKS((uint32_t)s_duration * 1000UL));

    /* Remove hook first so the completion log goes to UART only */
    if (s_orig_vp) {
        esp_log_set_vprintf(s_orig_vp);
        s_orig_vp = NULL;
    }

    /* Append a completion marker to the buffer */
    if (s_buf && s_pos < s_cap) {
        const char *marker = "\n[diag] capture complete\n";
        size_t mlen = strlen(marker);
        if (s_pos + mlen < s_cap) {
            memcpy(s_buf + s_pos, marker, mlen + 1);
            s_pos += mlen;
        }
    }

    s_armed = false;
    s_ready = true;

    ESP_LOGI(TAG, "capture done - %u bytes in buffer", (unsigned)s_pos);
    vTaskDelete(NULL);
}

/* --------------------------------------------------------------------------
 * Public API
 * ---------------------------------------------------------------------- */

void diag_capture_check_and_arm(void)
{
    nvs_handle_t h;
    if (nvs_open(DIAG_NVS_NS, NVS_READWRITE, &h) != ESP_OK) return;

    uint8_t dur = 0;
    nvs_get_u8(h, DIAG_NVS_KEY, &dur);

    if (dur != 90 && dur != 120) {
        nvs_close(h);
        return; /* normal boot */
    }

    /* Clear NVS flag immediately - crash during capture = clean next boot */
    nvs_set_u8(h, DIAG_NVS_KEY, 0);
    nvs_commit(h);
    nvs_close(h);

    /* Allocate ring buffer - prefer PSRAM */
    s_cap = DIAG_BUF_PSRAM;
    s_buf = (char *)heap_caps_malloc(s_cap, MALLOC_CAP_SPIRAM);
    if (!s_buf) {
        s_cap = DIAG_BUF_HEAP;
        s_buf = (char *)malloc(s_cap);
    }
    if (!s_buf) {
        ESP_LOGE(TAG, "ring buffer alloc failed - capture aborted");
        return;
    }
    memset(s_buf, 0, s_cap);

    s_pos      = 0;
    s_duration = dur;
    s_armed    = true;
    s_ready    = false;
    s_scrubbed = false;
    s_arm_us   = esp_timer_get_time();

    /* Install vprintf hook */
    s_orig_vp = esp_log_set_vprintf(diag_vprintf);

    ESP_LOGI(TAG, "armed - %us capture, buffer %uKB (%s)",
             (unsigned)dur,
             (unsigned)(s_cap / 1024),
             s_cap == DIAG_BUF_PSRAM ? "PSRAM" : "heap");

    /* Start countdown task */
    xTaskCreate(diag_timer_task, "diag_tmr", 2048, NULL, 3, NULL);
}

bool diag_capture_is_armed(void)    { return s_armed; }
bool diag_capture_is_ready(void)    { return s_ready; }
uint8_t diag_capture_get_duration(void) { return s_duration; }

uint32_t diag_capture_get_elapsed_s(void)
{
    if (!s_armed && !s_ready) return 0;
    return (uint32_t)((esp_timer_get_time() - s_arm_us) / 1000000LL);
}

const char *diag_capture_get_log(size_t *len_out)
{
    if (!s_ready || !s_buf) {
        if (len_out) *len_out = 0;
        return NULL;
    }
    if (len_out) *len_out = s_pos;
    return s_buf;
}

/* --------------------------------------------------------------------------
 * Scrub - replace password values with asterisks in-place
 * ---------------------------------------------------------------------- */

static void scrub_field(const char *secret)
{
    if (!s_buf || !secret || !secret[0]) return;
    size_t slen = strlen(secret);
    char *p = s_buf;
    while ((p = (char *)memchr(p, secret[0], s_pos - (size_t)(p - s_buf))) != NULL) {
        if ((size_t)(p - s_buf) + slen > s_pos) break;
        if (memcmp(p, secret, slen) == 0) {
            memset(p, '*', slen);
            p += slen;
        } else {
            p++;
        }
    }
}

void diag_capture_scrub(const app_cfg_t *cfg)
{
    if (!s_buf || !s_ready || s_scrubbed) return;
    if (!cfg) return;

    scrub_field(cfg->sta_pass);
    scrub_field(cfg->ap_pass);
    scrub_field(cfg->nut_pass);
    scrub_field(cfg->portal_pass);

    s_scrubbed = true;
    ESP_LOGI(TAG, "scrub complete");
}
