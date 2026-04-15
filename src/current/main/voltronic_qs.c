/*============================================================================
 MODULE: voltronic_qs

 Voltronic-QS serial protocol over USB HID Interface 0.
 See voltronic_qs.h for protocol documentation.

 DESIGN - SINGLE USB CLIENT OWNER PATTERN
 Uses the same queue pattern as ups_get_report.c:
   1. Timer task posts QS_CMD requests to a queue every 2 seconds.
   2. usb_client_task calls voltronic_qs_service() each event loop iteration.
   3. service() sends SET_REPORT command, reads EP 0x81 response, parses,
      and applies to ups_state.

 VERSION HISTORY
 R0  v0.34  Initial - QS protocol implementation.

============================================================================*/

#include "voltronic_qs.h"
#include "ups_state.h"
#include "cfg_store.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "usb/usb_host.h"

static const char *TAG = "voltronic_qs";

#define QS_IFACE        0       /* Vendor serial interface */
#define QS_EP_IN        0x81    /* Interrupt IN endpoint on IF0 */
#define QS_POLL_MS      2000    /* Poll interval */
#define QS_RESP_MAX     128     /* Max response length */

/* Command types */
typedef enum {
    QS_CMD_QUERY   = 0,   /* QS\r - live data */
    QS_CMD_RATINGS = 1,   /* F\r  - nominal ratings (one-shot) */
} qs_cmd_t;

/* ---- State ---- */
static bool                     s_active    = false;
static usb_host_client_handle_t s_client    = NULL;
static usb_device_handle_t      s_dev       = NULL;
static QueueHandle_t            s_cmd_queue = NULL;
static TaskHandle_t             s_timer_task = NULL;

/* Ratings (from F command, read once).
 * s_nom_batt_volt is used for battery charge estimation in parse_qs_response.
 * If the F command never succeeds (device not ready at startup, truncated
 * response, etc.) s_nom_batt_volt stays 0 - blocking ALL charge estimation.
 * Fix: default 12.0V covers virtually all small/medium UPS units using a
 * single 12V lead-acid cell. Overwritten by a real F command response. */
static float s_nom_voltage   = 0;
static float s_nom_batt_volt = 12.0f;  /* safe default; overwritten by F cmd */
static bool  s_ratings_valid = false;  /* true once real F response applied   */

/* ---- Control transfer callback ---- */
static volatile bool s_ctrl_done = false;
static volatile int  s_ctrl_status = -1;

static void ctrl_cb(usb_transfer_t *xfer)
{
    s_ctrl_status = xfer->status;
    s_ctrl_done   = true;
}

/* ---- Send command via SET_REPORT and read response ---- */
static bool qs_send_command(const char *cmd, char *resp_out, size_t resp_sz)
{
    if (!s_client || !s_dev) return false;

    /* Prepare SET_REPORT payload: command padded to 8 bytes */
    uint8_t payload[8] = {0};
    size_t  cmd_len = strlen(cmd);
    if (cmd_len > 8) cmd_len = 8;
    memcpy(payload, cmd, cmd_len);

    /* Allocate control transfer */
    usb_transfer_t *xfer = NULL;
    esp_err_t err = usb_host_transfer_alloc(8 + sizeof(payload) + 64, 0, &xfer);
    if (err != ESP_OK || !xfer) {
        ESP_LOGE(TAG, "transfer_alloc failed: %s", esp_err_to_name(err));
        return false;
    }

    /* SET_REPORT (Output type=0x02, report ID=0x00) to Interface 0 */
    xfer->device_handle = s_dev;
    xfer->callback      = ctrl_cb;
    xfer->bEndpointAddress = 0x00;
    xfer->num_bytes     = 8 + sizeof(payload);
    xfer->timeout_ms    = 500;

    usb_setup_packet_t *setup = (usb_setup_packet_t *)xfer->data_buffer;
    setup->bmRequestType = 0x21;  /* Host-to-Device, Class, Interface */
    setup->bRequest      = 0x09;  /* SET_REPORT */
    setup->wValue        = 0x0200; /* Output report, ID 0 */
    setup->wIndex        = QS_IFACE;
    setup->wLength       = sizeof(payload);
    memcpy(xfer->data_buffer + 8, payload, sizeof(payload));

    s_ctrl_done = false;
    err = usb_host_transfer_submit_control(s_client, xfer);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "SET_REPORT submit failed: %s", esp_err_to_name(err));
        usb_host_transfer_free(xfer);
        return false;
    }

    /* Pump events until callback fires */
    for (int i = 0; i < 100 && !s_ctrl_done; i++) {
        usb_host_client_handle_events(s_client, 5);
    }
    usb_host_transfer_free(xfer);

    if (!s_ctrl_done || s_ctrl_status != 0) {
        ESP_LOGW(TAG, "SET_REPORT failed (done=%d status=%d)",
                 (int)s_ctrl_done, s_ctrl_status);
        return false;
    }

    /* Small delay for device to process command */
    vTaskDelay(pdMS_TO_TICKS(100));

    /* Read response from EP 0x81 in chunks (max 8 bytes per interrupt read) */
    char full_resp[QS_RESP_MAX] = {0};
    size_t total = 0;

    for (int chunk = 0; chunk < 8 && total < QS_RESP_MAX - 1; chunk++) {
        usb_transfer_t *rxfer = NULL;
        err = usb_host_transfer_alloc(64, 0, &rxfer);
        if (err != ESP_OK || !rxfer) break;

        rxfer->device_handle    = s_dev;
        rxfer->callback         = ctrl_cb;
        rxfer->bEndpointAddress = QS_EP_IN;
        rxfer->num_bytes        = 8;
        rxfer->timeout_ms       = 300;

        s_ctrl_done = false;
        err = usb_host_transfer_submit(rxfer);
        if (err != ESP_OK) {
            usb_host_transfer_free(rxfer);
            break;
        }

        for (int i = 0; i < 60 && !s_ctrl_done; i++) {
            usb_host_client_handle_events(s_client, 5);
        }

        if (s_ctrl_done && s_ctrl_status == 0 && rxfer->actual_num_bytes > 0) {
            size_t n = rxfer->actual_num_bytes;
            if (total + n >= QS_RESP_MAX) n = QS_RESP_MAX - 1 - total;
            memcpy(full_resp + total, rxfer->data_buffer, n);
            total += n;
        }
        usb_host_transfer_free(rxfer);

        /* Check if response is complete (contains \r) */
        if (memchr(full_resp, '\r', total) || memchr(full_resp, '\n', total)) {
            break;
        }

        if (!s_ctrl_done) break;  /* timeout - no more data */
    }

    if (total > 0 && resp_out) {
        if (total >= resp_sz) total = resp_sz - 1;
        memcpy(resp_out, full_resp, total);
        resp_out[total] = '\0';
    }

    return total > 0;
}

/* ---- Parse QS response ---- */
static void parse_qs_response(const char *resp)
{
    /* Format: (inV inVfault outV load% Hz battV temp flags\r
     * Example: (230.0 230.0 230.0 019 50.0 13.5 25.0 00001001
     *
     * Complications from real device (PowerWalker VI 3000 SCL):
     * - Temperature can be "--.-" (not a valid float)
     * - Interrupt reads sometimes lose the leading '(' and first field
     * - Response may arrive across multiple 8-byte USB chunks
     *
     * Strategy: tokenize by space, parse positionally, skip bad fields. */
    const char *s = resp;
    while (*s == '(' || *s == ' ') s++;  /* skip leading ( and spaces */

    /* Tokenize into fields */
    char buf[QS_RESP_MAX];
    strlcpy(buf, s, sizeof(buf));

    /* Strip trailing \r \n \0 */
    size_t blen = strlen(buf);
    while (blen > 0 && (buf[blen-1] == '\r' || buf[blen-1] == '\n' || buf[blen-1] == '\0')) {
        buf[--blen] = '\0';
    }

    char *fields[10] = {0};
    int nfields = 0;
    char *saveptr = NULL;
    for (char *tok = strtok_r(buf, " ", &saveptr); tok && nfields < 10; tok = strtok_r(NULL, " ", &saveptr)) {
        fields[nfields++] = tok;
    }

    /* Need at least 8 fields: inV inVfault outV load Hz battV temp flags */
    if (nfields < 8) {
        ESP_LOGW(TAG, "QS parse: only %d fields (need 8): %s", nfields, resp);
        return;
    }

    float inV   = strtof(fields[0], NULL);
    /* fields[1] = inVfault (not used) */
    float outV  = strtof(fields[2], NULL);
    int   load  = atoi(fields[3]);
    float freq  = strtof(fields[4], NULL);
    float battV = strtof(fields[5], NULL);

    /* Temperature: may be "--.-" which means sensor unavailable */
    float temp  = 0;
    bool  temp_valid = false;
    if (fields[6][0] != '-' || fields[6][1] != '-') {
        temp = strtof(fields[6], NULL);
        temp_valid = (temp > -40 && temp < 80);
    }

    const char *flags = fields[7];

    /* Decode status flags FIRST - needed for the inV sanity check below.
     * 8 chars, leftmost = bit7. */
    bool util_fail   = (strlen(flags) >= 1 && flags[0] == '1');
    bool low_battery = (strlen(flags) >= 2 && flags[1] == '1');

    /* Sanity: input voltage should be > 50V for mains. If not, response
     * was likely truncated/shifted and we got battery voltage in field[0].
     * EXCEPTION: when on battery (util_fail=true), inV=0 is correct -
     * the UPS legitimately reports 0V input when mains is disconnected. */
    if (inV < 50.0f && !util_fail) {
        ESP_LOGW(TAG, "QS parse: inV=%.1f too low and not on battery - skipping", inV);
        return;
    }

    ups_state_update_t upd;
    memset(&upd, 0, sizeof(upd));
    upd.valid = true;

    /* Input voltage */
    if (inV > 0 && inV < 300) {
        upd.input_voltage_valid = true;
        upd.input_voltage_mv    = (uint32_t)(inV * 1000.0f);
    }

    /* Output voltage */
    if (outV > 0 && outV < 300) {
        upd.output_voltage_valid = true;
        upd.output_voltage_mv    = (uint32_t)(outV * 1000.0f);
    }

    /* Load */
    if (load >= 0 && load <= 100) {
        upd.ups_load_valid = true;
        upd.ups_load_pct   = (uint8_t)load;
    }

    /* Battery voltage */
    if (battV > 0 && battV < 100) {
        upd.battery_voltage_valid = true;
        upd.battery_voltage_mv    = (uint32_t)(battV * 1000.0f);
    }

    /* Battery charge % - estimated from battery voltage vs nominal.
     * Voltage range per cell for sealed lead-acid:
     *   Float charge:  ~2.27V/cell  -> x12 cells = 27.2V for 24V pack
     *   Full (resting):~2.12V/cell  -> 25.4V
     *   Empty/cutoff:  ~1.75V/cell  -> 21.0V
     *
     * On mains: battery is being float-charged, voltage > full resting.
     *   Report 100% when battV >= float threshold.
     * On battery: linear interpolation between cutoff and full resting. */
    if (battV > 0.0f && s_nom_batt_volt > 0.0f) {
        float cutoff  = s_nom_batt_volt * 0.875f;   /* 21.0V for 24V */
        float full    = s_nom_batt_volt * 1.058f;   /* 25.4V for 24V, open-circuit full */
        float float_v = s_nom_batt_volt * 1.133f;   /* 27.2V for 24V, float charge */
        int   pct;

        if (!util_fail) {
            /* On mains - always report 100% (float charging) */
            pct = 100;
        } else if (battV >= float_v) {
            /* Just disconnected, still at float voltage */
            pct = 100;
        } else if (battV <= cutoff) {
            pct = 0;
        } else {
            /* Linear between cutoff and float voltage for on-battery */
            pct = (int)((battV - cutoff) / (float_v - cutoff) * 100.0f);
            if (pct < 0)   pct = 0;
            if (pct > 100) pct = 100;
        }

        upd.battery_charge_valid = true;
        upd.battery_charge       = (uint8_t)pct;
        ESP_LOGD(TAG, "[QS] battV=%.2f nomV=%.1f -> charge=%d%%",
                 battV, s_nom_batt_volt, pct);
    }

    /* Battery runtime - voltage-based estimate, only meaningful on battery.
     * On mains the float voltage does not reflect capacity. */
    if (util_fail && battV > 0.0f && s_nom_batt_volt > 0.0f &&
        upd.battery_charge_valid && upd.battery_charge > 0) {
        int effective_load = (load > 1) ? load : 10;
        /* Base: 15 min at 100% load for a typical small UPS.
         * Scale linearly with charge%, inversely with load. */
        uint32_t est_s = (uint32_t)(900.0f
                          * (100.0f / (float)effective_load)
                          * ((float)upd.battery_charge / 100.0f));
        if (est_s > 0u && est_s < 36000u) {
            upd.battery_runtime_valid = true;
            upd.battery_runtime_s     = est_s;
        }
    }

    /* AC status */
    upd.input_utility_present_valid = true;
    upd.input_utility_present       = !util_fail;

    /* Flags */
    uint32_t uflags = 0;
    if (low_battery) uflags |= 0x04u;
    if (!util_fail)  uflags |= 0x01u;  /* charging when on-line (simplified) */
    upd.ups_flags_valid = true;
    upd.ups_flags       = uflags;

    /* Derive and apply */
    /* Build status string directly */
    char status[16] = {0};
    if (util_fail) {
        if (low_battery) strlcpy(status, "OB DISCHRG LB", sizeof(status));
        else             strlcpy(status, "OB DISCHRG",    sizeof(status));
    } else {
        strlcpy(status, "OL", sizeof(status));
    }
    strlcpy(upd.ups_status, status, sizeof(upd.ups_status));

    ups_state_apply_update(&upd);

    ESP_LOGI(TAG, "[QS] inV=%.1f outV=%.1f load=%d%% battV=%.1f temp=%s flags=%s -> %s",
             inV, outV, load, battV,
             temp_valid ? "valid" : "n/a",
             flags, status);
}

/* ---- Parse F (ratings) response ---- */
static void parse_ratings(const char *resp)
{
    /* Format: #nomV nomA nomBattV nomHz\r
     * Example: #230.0 13.0 24.0 50.0
     * USB chunk reads may lose the leading '#' and first field.
     * Sanity check: nomV should be > 50 (mains voltage). If not,
     * fields are shifted and we need to adjust. */
    const char *s = resp;
    while (*s == '#' || *s == ' ') s++;

    float nomV = 0, nomA = 0, nomBattV = 0, nomHz = 0;
    int parsed = sscanf(s, "%f %f %f %f", &nomV, &nomA, &nomBattV, &nomHz);

    /* If nomV < 50, the first field was lost in USB chunking.
     * What we got is: nomA nomBattV nomHz (missing nomV).
     * nomA is typically 1-20A, nomBattV is 12/24/36/48V, nomHz is 50/60. */
    if (parsed >= 3 && nomV < 50.0f) {
        ESP_LOGW(TAG, "[F] Ratings response truncated (nomV=%.1f too low). "
                 "Shifting: nomA=%.1f->nomV, nomBattV=%.1f->nomA, nomHz=%.1f->nomBattV",
                 nomV, nomA, nomBattV, nomHz);
        /* What we parsed as nomV/nomA/nomBattV is actually nomA/nomBattV/nomHz */
        float actual_nomA     = nomV;
        float actual_nomBattV = nomA;
        float actual_nomHz    = nomBattV;
        nomV     = 0;  /* unknown - use DB nominal */
        nomA     = actual_nomA;
        nomBattV = actual_nomBattV;
        nomHz    = actual_nomHz;
    }

    if (parsed >= 3) {
        s_nom_voltage   = nomV;
        s_nom_batt_volt = nomBattV;
        s_ratings_valid = true;
        ESP_LOGI(TAG, "[F] Ratings: nomV=%.1f nomA=%.1f nomBattV=%.1f nomHz=%.1f",
                 nomV, nomA, nomBattV, nomHz);
    } else {
        ESP_LOGW(TAG, "[F] Ratings parse failed: %s", resp);
    }
}

/* ---- Timer task: posts commands to queue ---- */
static void qs_timer_task(void *arg)
{
    (void)arg;

    /* Initial delay */
    vTaskDelay(pdMS_TO_TICKS(1000));

    /* Query ratings once */
    qs_cmd_t cmd = QS_CMD_RATINGS;
    xQueueSend(s_cmd_queue, &cmd, pdMS_TO_TICKS(500));

    /* Wait for ratings to be processed */
    vTaskDelay(pdMS_TO_TICKS(1000));

    /* Periodic QS polling */
    while (s_active) {
        cmd = QS_CMD_QUERY;
        xQueueSend(s_cmd_queue, &cmd, pdMS_TO_TICKS(500));

        /* Retry F command every ~60s if ratings never confirmed */
        static uint32_t s_qs_count = 0;
        s_qs_count++;
        if (!s_ratings_valid && (s_qs_count % 30) == 0) {
            cmd = QS_CMD_RATINGS;
            xQueueSend(s_cmd_queue, &cmd, pdMS_TO_TICKS(500));
            ESP_LOGI(TAG, "Retrying F command (ratings not yet confirmed)");
        }

        for (int ms = 0; ms < QS_POLL_MS && s_active; ms += 100) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

    ESP_LOGI(TAG, "Timer task exiting");
    s_timer_task = NULL;
    vTaskDelete(NULL);
}

/* ---- Public API ---- */

esp_err_t voltronic_qs_start(usb_host_client_handle_t client,
                              usb_device_handle_t dev)
{
    if (s_active) {
        ESP_LOGW(TAG, "Already active");
        return ESP_ERR_INVALID_STATE;
    }

    s_client = client;
    s_dev    = dev;

    /* Claim Interface 0 */
    esp_err_t err = usb_host_interface_claim(client, dev, QS_IFACE, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to claim Interface %d: %s", QS_IFACE, esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "Claimed Interface %d for QS serial", QS_IFACE);

    /* Create command queue */
    if (!s_cmd_queue) {
        s_cmd_queue = xQueueCreate(4, sizeof(qs_cmd_t));
    } else {
        xQueueReset(s_cmd_queue);
    }

    s_active = true;

    /* Start timer task */
    xTaskCreate(qs_timer_task, "qs_timer", 3072, NULL, 4, &s_timer_task);

    ESP_LOGI(TAG, "QS serial polling started (interval=%dms)", QS_POLL_MS);
    return ESP_OK;
}

void voltronic_qs_stop(void)
{
    s_active = false;

    if (s_timer_task) {
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    if (s_dev && s_client) {
        usb_host_interface_release(s_client, s_dev, QS_IFACE);
        ESP_LOGI(TAG, "Released Interface %d", QS_IFACE);
    }

    s_client = NULL;
    s_dev    = NULL;
    ESP_LOGI(TAG, "QS serial polling stopped");
}

void voltronic_qs_service(void)
{
    if (!s_active || !s_cmd_queue) return;

    qs_cmd_t cmd;
    if (xQueueReceive(s_cmd_queue, &cmd, 0) != pdTRUE) return;

    char resp[QS_RESP_MAX] = {0};

    switch (cmd) {
    case QS_CMD_RATINGS:
        ESP_LOGI(TAG, "Sending F command (ratings)...");
        if (qs_send_command("F\r", resp, sizeof(resp))) {
            parse_ratings(resp);
        } else {
            ESP_LOGW(TAG, "F command failed - no response");
        }
        break;

    case QS_CMD_QUERY:
        if (qs_send_command("QS\r", resp, sizeof(resp))) {
            parse_qs_response(resp);
        } else {
            ESP_LOGW(TAG, "QS command failed - no response");
        }
        break;
    }
}

bool voltronic_qs_active(void)
{
    return s_active;
}
