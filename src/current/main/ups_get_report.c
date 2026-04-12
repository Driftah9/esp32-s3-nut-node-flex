/*============================================================================
 MODULE: ups_get_report

 RESPONSIBILITY
 - Poll USB Feature reports (GET_REPORT control transfers) for devices
   that expose values only via Feature reports, not interrupt-IN.
 - APC Back-UPS: battery voltage, input voltage, output voltage
 - Results fed into ups_state via ups_state_apply_update()

 APC Back-UPS Feature Report IDs (empirically determined + NUT source):
   0x01  Battery voltage — uint16 LE, unit = 10mV
         e.g. raw=1200 → 12000mV = 12.0V
   0x02  Input voltage  — uint16 LE, unit = 1V (whole volts)
         e.g. raw=120 → 120V (US), raw=230 → 230V (EU)
   0x03  Output voltage — same format as input

 DESIGN — SINGLE USB CLIENT OWNER PATTERN
 =========================================
 The ESP-IDF USB host client handle is NOT safe to use from multiple tasks.
 Only usb_client_task (in ups_usb_hid.c) owns the client handle and may
 call usb_host_transfer_submit_control() and usb_host_client_handle_events().

 ups_get_report works via a FreeRTOS queue:
   1. A timer task (ups_get_report_timer_task) wakes every N seconds and
      posts report IDs to s_request_queue.
   2. usb_client_task calls ups_get_report_service_queue() on each event
      loop iteration. This drains the queue, issues the control transfer,
      pumps events until completion, and decodes the result.
      Because this runs inside usb_client_task, it is the only caller of
      usb_host_transfer_submit_control — no concurrency issue.

 This is the correct single-owner pattern for ESP-IDF USB host.

 VERSION HISTORY
 R0  v15.8  Initial - APC Back-UPS Feature report polling.
 R1  v15.8  Rewrite - single-owner queue pattern to fix USB concurrency.
 R2  v0.14  Fix XCHK probe buffer: cap raised 16->64, buf[64] to match large
            declared Feature report sizes (e.g. rid=0x28 declares 63 bytes).
            wLength=16 on a 63-byte Feature report triggers IDF v5.5.4 DWC
            assert (hcd_dwc.c:2388 rem_len check). Now requests declared size
            up to 64 bytes, preventing crash-loop on PowerWalker VI 3000 RLE.
 R8  v0.31  DECODE_STANDARD Feature report support:
            - service_probe_queue(): route XCHK probe responses through
              ups_hid_parser_decode_report() for DECODE_STANDARD devices.
              Previously only DECODE_EATON_MGE probes were decoded.
            - Recurring poll: add DECODE_STANDARD decode branch using
              ups_hid_parser_decode_report() + ups_state_apply_update().
            - Recurring poll buffer increased from 16 to 64 bytes; wLength
              set from ups_hid_parser_max_input_bytes() for DECODE_STANDARD.
            - Timer task: DECODE_STANDARD now polls all Input RIDs from
              parsed descriptor via ups_hid_parser_get_input_rids(), instead
              of falling through to hardcoded Tripp Lite RIDs.
            Fixes PowerWalker VI 3000 SCL (0665:5161) OB not detected and
            battery.runtime missing: GET_REPORT on rid=0x30 now decoded
            through the standard field cache (Charging/Discharging/ACPresent).
 R7  v0.29   Add rid=0x06 to s_eaton_rids[] periodic polling list.
            Eaton 3S sends rid=0x06 as interrupt-IN only on mains events,
            not periodically. After initial boot burst data goes stale.
            Periodic GET_REPORT on rid=0x06 (every 30s) provides fresh
            charge/runtime via decode_eaton_feature case 0x06. Fixes
            Eaton 3S stale data regression reported in submission 713d7c.
 R6  v0.26   Eaton rid=0x06 Feature: demote flags-based OL assertion. flags=0x0000
            in all submissions - not reliable for OL. Only non-zero flags trigger
            OB. OL now from standard field cache (vendor page 0xFFFF) or default.
 R5  v0.26   Add rid=0x85 to Eaton GET_REPORT probe list and bootstrap queue.
            0x85 is a speculative OB status probe: in MGE HID the 0x8x range
            maps to alarm/event rids in interrupt-IN. GET_REPORT on 0x85 may
            return a snapshot of current alarm flags including AC status.
            decode_eaton_feature() case 0x85 logs raw bytes at WARN for
            discharge-event correlation. Decode follows once OB byte confirmed.
 R4  vFIX2  Add rid=0x06 to decode_eaton_feature(): if Eaton firmware supports
            Feature GET_REPORT on rid=0x06, the bootstrap probe queued at
            enumeration (ups_usb_hid Step 7b) now actually applies the result
            to state instead of logging and discarding it.
            Feed service_probe_queue() responses through decode_eaton_feature()
            for DECODE_EATON_MGE devices -- previously all probe responses were
            logged only, making the Eaton bootstrap probes completely inert.
 R3  v0.20  GET_REPORT transfer allocation padded by 64 bytes beyond declared
            size. CyberPower 3000R (0764:0601) returns MORE data than its
            descriptor declares for rid=0x28 (63 bytes declared, device sends
            more). DWC OTG assert fires at hcd_dwc.c:2341:
              rem_len <= (transfer->num_bytes - sizeof(usb_setup_packet_t))
            With alloc = 8 + 63 = 71, assert fires when rem_len > 63.
            Fix: alloc = 8 + buf_sz + 64 so transfer->num_bytes allows up to
            buf_sz + 64 bytes of response. wLength in setup packet unchanged
            (device is still told to send buf_sz bytes). ctrl_cb already
            clips payload to CTRL_PAYLOAD_MAX=64 so callers are unaffected.

============================================================================*/

#include "ups_get_report.h"
#include "ups_state.h"
#include "ups_hid_map.h"
#include "ups_hid_parser.h"

#include <string.h>
#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_log.h"
#include "esp_err.h"
#include "esp_timer.h"

#include "usb/usb_host.h"

static const char *TAG = "ups_get_report";

/* ---- Request queue --------------------------------------------------- */
/* Timer task posts report IDs here; usb_client_task drains and services */

typedef struct {
    uint8_t rid;
} get_report_req_t;

static QueueHandle_t             s_request_queue = NULL;
static usb_host_client_handle_t  s_client        = NULL;
static usb_device_handle_t       s_dev           = NULL;
static int                       s_intf_num      = -1;
static const ups_device_entry_t *s_entry         = NULL;
static TaskHandle_t              s_timer_task    = NULL;
static volatile bool             s_active        = false;

/* ---- XCHK one-shot probe state --------------------------------------- */
/* Queued by ups_hid_parser_run_xchk() via callback; serviced here in     */
/* usb_client_task. Independent of recurring QUIRK_NEEDS_GET_REPORT path. */
typedef struct {
    uint8_t  rid;
    uint16_t size;
} probe_req_t;

static QueueHandle_t            s_probe_queue  = NULL;
static usb_host_client_handle_t s_probe_client = NULL;
static usb_device_handle_t      s_probe_dev    = NULL;
static int                      s_probe_intf   = -1;

/* ---- Forward declarations -------------------------------------------- */
static void decode_apc_smartups_feature(uint8_t rid, const uint8_t *data, size_t len);

/* ---- Control transfer state ------------------------------------------ */
/*
 * ctrl_cb owns the transfer lifetime from submit until free.
 * The callback copies payload into s_ctrl_payload, frees the transfer,
 * then signals s_ctrl_done. Caller reads s_ctrl_payload after done.
 *
 * s_inflight tracks whether a transfer is registered with the USB host.
 * If non-NULL when stop() is called, the next DEV_GONE will cancel it
 * and ctrl_cb will free it safely. stop() must not free it directly.
 */
#define CTRL_PAYLOAD_MAX 64u
static volatile usb_transfer_t *s_inflight      = NULL;
static volatile bool            s_ctrl_done     = false;
static volatile esp_err_t       s_ctrl_status   = ESP_FAIL;
static volatile uint8_t         s_ctrl_payload[CTRL_PAYLOAD_MAX];
static volatile size_t          s_ctrl_pay_len  = 0;

static void ctrl_cb(usb_transfer_t *t)
{
    if (!t) return;

    /* Log raw transfer outcome for every Eaton callback — critical for diagnosis */
    ESP_LOGI(TAG, "[CTRL_CB] status=%d actual_bytes=%d",
             (int)t->status, (int)t->actual_num_bytes);

    /* Map transfer status to human-readable label */
    const char *status_str;
    switch (t->status) {
        case USB_TRANSFER_STATUS_COMPLETED: status_str = "COMPLETED"; break;
        case USB_TRANSFER_STATUS_ERROR:     status_str = "ERROR";     break;
        case USB_TRANSFER_STATUS_TIMED_OUT: status_str = "TIMED_OUT"; break;
        case USB_TRANSFER_STATUS_CANCELED:  status_str = "CANCELLED"; break;
        case USB_TRANSFER_STATUS_STALL:     status_str = "STALL";     break;
        case USB_TRANSFER_STATUS_OVERFLOW:  status_str = "OVERFLOW";  break;
        case USB_TRANSFER_STATUS_SKIPPED:   status_str = "SKIPPED";   break;
        default:                            status_str = "UNKNOWN";   break;
    }
    ESP_LOGI(TAG, "[CTRL_CB] transfer status: %s (%d)", status_str, (int)t->status);

    s_ctrl_status  = (t->status == USB_TRANSFER_STATUS_COMPLETED) ? ESP_OK : ESP_FAIL;
    s_ctrl_pay_len = 0;
    if (s_ctrl_status == ESP_OK && t->actual_num_bytes > 8u) {
        size_t plen = (size_t)(t->actual_num_bytes - 8u);
        if (plen > CTRL_PAYLOAD_MAX) plen = CTRL_PAYLOAD_MAX;
        memcpy((void *)s_ctrl_payload, t->data_buffer + 8u, plen);
        s_ctrl_pay_len = plen;
        /* Log raw payload bytes */
        char hexbuf[64] = {0};
        int  pos = 0;
        for (size_t i = 0; i < plen && i < 16u; i++) {
            pos += snprintf(hexbuf + pos, sizeof(hexbuf) - (size_t)pos,
                            "%02X%s", t->data_buffer[8u + i], (i == plen-1u) ? "" : " ");
        }
        ESP_LOGI(TAG, "[CTRL_CB] payload (%u bytes): %s", (unsigned)plen, hexbuf);
    } else if (s_ctrl_status == ESP_OK) {
        ESP_LOGW(TAG, "[CTRL_CB] COMPLETED but actual_num_bytes=%d (<=8, no payload)",
                 (int)t->actual_num_bytes);
    }
    usb_host_transfer_free(t);
    s_inflight  = NULL;
    s_ctrl_done = true;   /* signal last */
}

/* ---- Issue one GET_REPORT — called from usb_client_task only --------- */
/*
 * USB HID GET_REPORT control transfer:
 *   bmRequestType = 0xA1   D-to-H, Class, Interface
 *   bRequest      = 0x01   GET_REPORT
 *   wValue        = (3 << 8) | rid   type=Feature(3), report_id=rid
 *   wIndex        = interface number
 *   wLength       = report size
 *
 * Takes explicit client/dev/intf so both recurring polling (s_client/s_dev)
 * and one-shot XCHK probes (s_probe_client/s_probe_dev) can share the logic.
 *
 * MUST be called from the same task that owns client (usb_client_task).
 * Pumps usb_host_client_handle_events() in-place while waiting.
 */
static esp_err_t do_get_feature_report(usb_host_client_handle_t client,
                                        usb_device_handle_t      dev,
                                        int                      intf_num,
                                        uint8_t rid, uint8_t *buf, size_t buf_sz,
                                        size_t *out_len)
{
    if (!dev || !client || intf_num < 0) return ESP_ERR_INVALID_STATE;

    /* Add 64-byte overflow padding beyond the declared report size.
     * Non-compliant devices (e.g. CyberPower 3000R rid=0x28) return more
     * bytes than their descriptor declares. Without padding the DWC OTG
     * assertion fires: rem_len > (transfer->num_bytes - setup_packet_size).
     * wLength in the setup packet stays at buf_sz (device told to send
     * buf_sz bytes). Extra alloc only prevents the HCI buffer overflow. */
    size_t alloc = 8u + buf_sz + 64u;
    usb_transfer_t *t = NULL;
    esp_err_t err = usb_host_transfer_alloc(alloc, 0, &t);
    if (err != ESP_OK || !t) {
        ESP_LOGE(TAG, "transfer_alloc rid=0x%02X: %s", rid, esp_err_to_name(err));
        return err;
    }

    uint8_t *s = t->data_buffer;
    s[0] = 0xA1u;
    s[1] = 0x01u;
    s[2] = rid;
    s[3] = 0x03u;   /* Feature report type */
    s[4] = (uint8_t)(intf_num);
    s[5] = 0x00u;
    s[6] = (uint8_t)(buf_sz & 0xFFu);
    s[7] = (uint8_t)(buf_sz >> 8u);

    t->device_handle    = dev;
    t->bEndpointAddress = 0x00u;
    t->callback         = ctrl_cb;
    t->context          = NULL;
    t->num_bytes        = alloc;

    /* Log exact setup packet bytes so we can verify bmRequestType, wValue, wIndex, wLength */
    ESP_LOGI(TAG, "[SETUP] rid=0x%02X setup: %02X %02X %02X %02X %02X %02X %02X %02X intf=%d wlen=%u",
             rid, s[0], s[1], s[2], s[3], s[4], s[5], s[6], s[7],
             intf_num, (unsigned)buf_sz);

    s_ctrl_done    = false;
    s_ctrl_status  = ESP_FAIL;
    s_ctrl_pay_len = 0;
    s_inflight     = t;   /* track for stop() to detect pending transfer */

    err = usb_host_transfer_submit_control(client, t);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "[SETUP] submit_control rid=0x%02X FAILED: %s (0x%x)",
                 rid, esp_err_to_name(err), (unsigned)err);
        s_inflight = NULL;
        usb_host_transfer_free(t);
        return err;
    }
    ESP_LOGI(TAG, "[SETUP] submit_control rid=0x%02X OK - polling for callback", rid);

    /* Pump USB event loop until callback fires.
     * ctrl_cb always frees the transfer - caller must NOT free t after this point.
     * On DEV_GONE the host cancels all transfers and fires ctrl_cb with error status.
     * Max wait: 3000ms (600 x 5ms) - increased from 1500ms to handle slow APC responses. */
    const TickType_t slice = pdMS_TO_TICKS(5);
    const int        max   = 600;
    for (int i = 0; i < max && !s_ctrl_done; i++) {
        usb_host_client_handle_events(client, 0);
        vTaskDelay(slice);
        /* Log progress at 500ms intervals so we can see if callback ever fires */
        if ((i > 0) && (i % 100 == 0)) {
            ESP_LOGW(TAG, "[SETUP] rid=0x%02X still waiting... (%dms elapsed, inflight=%s)",
                     rid, i * 5, s_inflight ? "yes" : "no");
        }
    }

    if (!s_ctrl_done) {
        /* Genuine timeout: callback never fired. Transfer is still registered
         * with the USB host. s_inflight remains set; on next DEV_GONE the
         * host will cancel the transfer and ctrl_cb will free it safely. */
        ESP_LOGW(TAG, "GET_REPORT rid=0x%02X: timed out after 3000ms - inflight=%s",
                 rid, s_inflight ? "yes (pending DEV_GONE cancel)" : "no (cb fired but s_ctrl_done not set?)");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "[SETUP] rid=0x%02X callback fired OK", rid);

    /* ctrl_cb has fired and freed the transfer. t is no longer valid.
     * Payload was copied into s_ctrl_payload before the free. */
    if (s_ctrl_status != ESP_OK) {
        ESP_LOGW(TAG, "GET_REPORT rid=0x%02X: transfer completed with error status", rid);
        return ESP_FAIL;
    }

    size_t plen = s_ctrl_pay_len;
    if (plen > buf_sz) plen = buf_sz;
    if (plen == 0) {
        ESP_LOGW(TAG, "GET_REPORT rid=0x%02X: empty payload", rid);
        return ESP_FAIL;
    }
    memcpy(buf, (const void *)s_ctrl_payload, plen);
    if (out_len) *out_len = plen;
    return ESP_OK;
}

/* ---- Decode APC Feature reports -------------------------------------- */
static void decode_apc_feature(uint8_t rid, const uint8_t *data, size_t len)
{
    /* HID GET_REPORT responses include the report ID as byte 0 of the payload.
     * So for a descriptor field declared as N bytes, the response is N+1 bytes:
     *   data[0] = report ID (echo)
     *   data[1..N] = actual field data
     *
     * rid=0x07 descriptor (after rid byte):
     *   bytes 1-2: uint16 LE  uid=0073 page=85 → BatteryVoltage
     *   bytes 3-4: uint16 LE  uid=004B page=85 → ConfigVoltage (nominal)
     *   bytes 5-6: uint16 LE  uid=0065 page=84 → APC vendor (likely input V)
     *   bytes 7-8: uint16 LE  uid=00DB page=85 → ACVoltage (output V)
     * Total response: 9 bytes [rid, b1, b2, b3, b4, b5, b6, b7, b8]
     */

    /* Log full raw bytes regardless of rid */
    {
        char hexbuf[64] = {0};
        int  pos = 0;
        size_t n = (len > 16u) ? 16u : len;
        for (size_t i = 0; i < n; i++) {
            pos += snprintf(hexbuf + pos, sizeof(hexbuf) - (size_t)pos,
                            "%02X%s", data[i], (i == n-1u) ? "" : " ");
        }
        ESP_LOGI(TAG, "[APC Feature] rid=0x%02X len=%u bytes: %s", rid, (unsigned)len, hexbuf);
    }

    switch (rid) {
    case 0x17: {
        /* rid=0x17: AC line voltage
         * descriptor: page=85 uid=002A size=16 (ConfigVoltage/LineVoltage)
         * response: [0x17, lo, hi]  — uint16 LE, unit = 1V (whole volts)
         * confirmed: 0x0078 = 120 → 120V US mains
         * On-line (OL): input.voltage = output.voltage = this value
         * On-battery (OB): input.voltage = 0 or absent; output.voltage = this value
         * Sanity: 80..300V
         */
        if (len < 3u) {
            ESP_LOGW(TAG, "[APC Feature] rid=0x17: only %u bytes (need 3)", (unsigned)len);
            break;
        }
        uint16_t volts = (uint16_t)(data[1] | ((uint16_t)data[2] << 8u));
        ESP_LOGI(TAG, "[APC Feature] rid=0x17 line_voltage=%uV (raw=%u)", volts, volts);

        if (volts >= 80u && volts <= 300u) {
            uint32_t mv = (uint32_t)volts * 1000u;
            ups_state_update_t upd;
            memset(&upd, 0, sizeof(upd));
            upd.valid                = true;
            upd.input_voltage_valid  = true;
            upd.input_voltage_mv     = mv;
            upd.output_voltage_valid = true;
            upd.output_voltage_mv    = mv;
            ups_state_apply_update(&upd);
            ESP_LOGI(TAG, "[APC Feature] input.voltage = output.voltage = %uV", volts);
        } else if (volts == 0u) {
            /* On-battery: clear input voltage; output still driven by inverter.
             * We don't have separate output-only measurement so leave both clear. */
            ups_state_update_t upd;
            memset(&upd, 0, sizeof(upd));
            upd.valid                = true;
            upd.input_voltage_valid  = false;
            upd.output_voltage_valid = false;
            ups_state_apply_update(&upd);
            ESP_LOGI(TAG, "[APC Feature] line_voltage=0 — on battery, clearing input/output voltage");
        } else {
            ESP_LOGW(TAG, "[APC Feature] rid=0x17 volts=%u — outside 80-300V, ignoring", volts);
        }
        break;
    }
    default:
        /* Unexpected rid from probe list — logged above, nothing to decode */
        break;
    }
}

/* ---- Report ID lists ------------------------------------------------- */
/* APC Back-UPS: rid=0x07 is the voltage Feature report (8 bytes, 4×uint16 LE)
 * Fields per HID descriptor (after report-ID byte at offset 0):
 *   bytes 1-2: uid=0073 page=85 → Voltage (battery voltage)
 *   bytes 3-4: uid=004B page=85 → ConfigVoltage
 *   bytes 5-6: uid=0065 page=84 → APC vendor (input or output voltage)
 *   bytes 7-8: uid=00DB page=85 → Voltage (AC output voltage)
 *
 * Note: HID GET_REPORT response prepends the report ID as byte 0,
 * so the 8-byte payload we receive is: [rid, v0_lo, v0_hi, v1_lo, v1_hi,
 *                                        v2_lo, v2_hi, v3_lo, v3_hi]
 *
 * rids 0x01/0x02/0x03 exist in the descriptor but return only the report
 * ID echo + 1 data byte of indeterminate meaning (not voltages). Skip them.
 */
/* Probe a wide range of APC Feature report IDs to find voltage data.
 * Results logged at INFO level for raw analysis — decode will follow.
 * rids in descriptor: 0x01-0x07, 0x17, 0x36, 0x40-0x42, 0x50, 0x52
 * rids seen in interrupt-IN but not in descriptor:
 *   0x20, 0x21, 0x22, 0x23, 0x25, 0x28, 0x29, 0x82, 0x85-0x88
 * We probe a representative set here. */
/* Production: rid=0x17 only (confirmed = AC line voltage, 120V US / 230V EU)
 * Battery voltage is not available via GET_REPORT on APC Back-UPS (0002)
 * firmware — all high rids (0x82-0x88) STALL on this device. */
static const uint8_t s_apc_rids[]          = { 0x17 };
static const size_t  s_apc_rids_n          = sizeof(s_apc_rids) / sizeof(s_apc_rids[0]);
/* APC Smart-UPS (PID 0003) Feature rids:
 *   rid=0x06  charging flag (byte[1]) + discharging flag (byte[2])
 *   rid=0x0E  battery.voltage (byte[1], raw value)
 * Runtime arrives on interrupt-IN (rid=0x0D) so no GET_REPORT needed. */
static const uint8_t s_apc_smartups_rids[] = { 0x06, 0x0E };
static const size_t  s_apc_smartups_rids_n = sizeof(s_apc_smartups_rids) / sizeof(s_apc_smartups_rids[0]);
/* Eaton/MGE (PID FFFF) Feature rids:
 *   rid=0x20  battery.charge - byte[1] = charge% (0-100)
 *             Confirmed from two 3S 700 submissions (2026-03-30).
 *             Response: [0x20, charge_pct]  e.g. [0x20, 0x02] = 2%
 *   rid=0xFD  unknown - returns 2 bytes [0xFD, 0x29], short read
 *             Not decoded yet - logged for future analysis.
 *   rid=0x85  speculative BatterySystem status probe.
 *             In MGE/Eaton HID 0x8x maps to alarm/event rids in interrupt-IN.
 *             Probed here to see if GET_REPORT returns readable status bytes.
 *             Raw bytes logged in decode_eaton_feature() for OB analysis.
 */
static const uint8_t s_eaton_rids[]        = { 0x06, 0x20, 0xFD, 0x85 };
static const size_t  s_eaton_rids_n        = sizeof(s_eaton_rids) / sizeof(s_eaton_rids[0]);
static const uint8_t s_tripplite_rids[]    = { 0x01, 0x0C };
static const size_t  s_tripplite_rids_n    = sizeof(s_tripplite_rids) / sizeof(s_tripplite_rids[0]);

/* Voltronic/PowerWalker Feature reports for periodic polling.
 * 0x22 = PresentStatus (ac_present, charging, discharging, low_batt)
 * 0x21 = Percent load
 * 0x18 = Input voltage AC
 * 0x1B = Output voltage AC
 * 0x36 = Battery voltage
 * 0x34 = Battery charge (same as interrupt-IN rid, but Feature type)
 */
static const uint8_t s_voltronic_rids[]    = { 0x22, 0x21, 0x18, 0x1B, 0x36, 0x34 };
static const size_t  s_voltronic_rids_n    = sizeof(s_voltronic_rids) / sizeof(s_voltronic_rids[0]);

/* ---- Decode APC Smart-UPS Feature reports ----------------------------- */
static void decode_apc_smartups_feature(uint8_t rid, const uint8_t *data, size_t len)
{
    char hexbuf[48] = {0};
    int  pos = 0;
    size_t n = (len > 8u) ? 8u : len;
    for (size_t i = 0; i < n; i++) {
        pos += snprintf(hexbuf + pos, sizeof(hexbuf) - (size_t)pos,
                        "%02X%s", data[i], (i == n-1u) ? "" : " ");
    }
    ESP_LOGI(TAG, "[SMRT Feature] rid=0x%02X len=%u: %s", rid, (unsigned)len, hexbuf);

    switch (rid) {
    case 0x06: {
        if (len < 3u) {
            ESP_LOGW(TAG, "[SMRT Feature] rid=0x06: short read %u bytes", (unsigned)len);
            break;
        }
        bool charging    = (data[1] != 0u);
        bool discharging = (data[2] != 0u);
        ESP_LOGI(TAG, "[SMRT Feature] rid=0x06 charging=%u discharging=%u",
                 (unsigned)charging, (unsigned)discharging);
        ups_state_update_t upd;
        memset(&upd, 0, sizeof(upd));
        upd.valid           = true;
        upd.ups_flags_valid = true;
        if (charging)    upd.ups_flags |= 0x01u;
        if (discharging) upd.ups_flags |= 0x02u;
        upd.input_utility_present_valid = true;
        upd.input_utility_present       = !discharging;
        ups_state_apply_update(&upd);
        break;
    }
    case 0x0E: {
        if (len < 2u) {
            ESP_LOGW(TAG, "[SMRT Feature] rid=0x0E: short read %u bytes", (unsigned)len);
            break;
        }
        uint8_t raw = data[1];
        /* raw = whole volts (tentative). Sanity: 8..60V for 12/24/48V systems. */
        if (raw >= 8u && raw <= 60u) {
            ups_state_update_t upd;
            memset(&upd, 0, sizeof(upd));
            upd.valid                 = true;
            upd.battery_voltage_valid = true;
            upd.battery_voltage_mv    = (uint32_t)raw * 1000u;
            ups_state_apply_update(&upd);
            ESP_LOGI(TAG, "[SMRT Feature] battery.voltage=%uV", (unsigned)raw);
        } else {
            ESP_LOGW(TAG, "[SMRT Feature] rid=0x0E raw=%u outside 8-60V - ignoring", (unsigned)raw);
        }
        break;
    }
    default:
        break;
    }
}

/* ---- Decode Voltronic/PowerWalker Feature reports -------------------- */
/*
 * Confirmed Feature report IDs from Linux testing (ups7.py, working.py):
 *   0x22 = PresentStatus (bit0=AC, bit1=Chrg, bit2=Dischrg, bit3=LB, bit4=RB)
 *   0x21 = Percent load (0-100)
 *   0x18 = Input voltage AC
 *   0x1B = Output voltage AC
 *   0x36 = Battery voltage
 *   0x34 = Battery charge
 * For 0x18, 0x1B, 0x36, 0x34: route through standard descriptor decode.
 * For 0x22, 0x21: custom decode (not in descriptor's field cache correctly).
 */
static void decode_voltronic_feature(uint8_t rid, const uint8_t *buf, size_t len)
{
    if (len < 2) return;
    const uint8_t *p    = buf + 1;
    size_t         plen = len - 1;

    ups_state_update_t upd;
    memset(&upd, 0, sizeof(upd));
    upd.valid = true;

    switch (rid) {
    case 0x22:
        /* PresentStatus: bit0=ACPresent, bit1=Charging, bit2=Discharging,
         * bit3=LowBatt, bit4=NeedReplace.
         *
         * NOTE: PowerWalker VI 3000 SCL returns 0xFF for this report,
         * meaning ALL flags set (including contradictory charging+discharging).
         * 0xFF is treated as invalid - skip status extraction. The interrupt-IN
         * rid=0x32 (byte[3] bit4) is the reliable ACPresent source. */
        if (plen >= 1) {
            uint8_t flags = p[0];

            if (flags == 0xFFu) {
                ESP_LOGW(TAG, "[VOLT] Feature 0x22 status=0xFF (invalid, all bits set) - ignoring");
                return;
            }

            upd.input_utility_present_valid = true;
            upd.input_utility_present       = (flags & 0x01u) != 0u;

            uint32_t uflags = 0;
            if (flags & 0x02u) uflags |= 0x01u;  /* charging */
            if (flags & 0x04u) uflags |= 0x02u;  /* discharging */
            if (flags & 0x08u) uflags |= 0x04u;  /* low battery */
            if (flags & 0x10u) uflags |= 0x10u;  /* need replacement */
            upd.ups_flags_valid = true;
            upd.ups_flags       = uflags;

            ESP_LOGI(TAG, "[VOLT] Feature 0x22 status=0x%02X ac=%u chrg=%u dischrg=%u lb=%u",
                     flags,
                     (unsigned)(flags & 0x01u),
                     (unsigned)((flags >> 1) & 1u),
                     (unsigned)((flags >> 2) & 1u),
                     (unsigned)((flags >> 3) & 1u));
            ups_state_apply_update(&upd);
        }
        return;

    case 0x21:
        /* Percent load */
        if (plen >= 1 && p[0] <= 100u) {
            upd.ups_load_valid = true;
            upd.ups_load_pct   = p[0];
            ESP_LOGI(TAG, "[VOLT] Feature 0x21 ups.load=%u%%", p[0]);
            ups_state_apply_update(&upd);
        }
        return;

    case 0x36:
        /* Battery voltage: device returns raw=0x64 (100) which does not map
         * to a real voltage (QS shows ~26.9V for this device). The standard
         * descriptor decode produces 100000 mV (100V) which fails the
         * mv < 100000 check, then falls back to raw=100 -> 0.1V. Both wrong.
         * Skip this report - battery.voltage comes from QS mode instead. */
        ESP_LOGD(TAG, "[VOLT] Feature 0x36 skipped (unreliable on this device)");
        return;

    case 0x18:
    case 0x1B:
        /* Input/output voltage: standard decode may work for these.
         * But on PowerWalker, 0x18 returns 18 FF 18 and 0x1B returns 1B FF 1B
         * which are likely rid echo patterns, not real data. Skip if payload
         * looks like a rid echo (byte[1] == 0xFF and byte[2] == rid). */
        if (plen >= 2 && p[0] == 0xFFu && p[1] == rid) {
            ESP_LOGD(TAG, "[VOLT] Feature 0x%02X skipped (rid echo pattern)", rid);
            return;
        }
        /* Fall through to standard decode if it looks real */
        if (ups_hid_parser_decode_report(buf, len, &upd)) {
            ups_state_apply_update(&upd);
            ESP_LOGI(TAG, "[VOLT] Feature 0x%02X: standard decode applied", rid);
        }
        return;

    default:
        /* 0x34 and others: route through standard descriptor decode */
        if (ups_hid_parser_decode_report(buf, len, &upd)) {
            ups_state_apply_update(&upd);
            ESP_LOGI(TAG, "[VOLT] Feature 0x%02X: standard decode applied", rid);
        }
        return;
    }
}

/* ---- Decode Eaton/MGE Feature reports -------------------------------- */
static void decode_eaton_feature(uint8_t rid, const uint8_t *data, size_t len)
{
    char hexbuf[32] = {0};
    int  pos = 0;
    size_t n = (len > 8u) ? 8u : len;
    for (size_t i = 0; i < n; i++) {
        pos += snprintf(hexbuf + pos, sizeof(hexbuf) - (size_t)pos,
                        "%02X%s", data[i], (i == n-1u) ? "" : " ");
    }
    ESP_LOGI(TAG, "[MGE Feature] rid=0x%02X len=%u: %s", rid, (unsigned)len, hexbuf);

    switch (rid) {
    case 0x06: {
        /*
         * rid=0x06 via GET_REPORT: some Eaton firmware versions respond to a
         * Feature GET_REPORT on this rid with current UPS state, even though it
         * normally only arrives as interrupt-IN on mains events.
         * Format is identical to the interrupt-IN version (confirmed in hid_parser):
         *   data[0] = rid echo
         *   data[1] = battery.charge (0-100%)
         *   data[2:3] = battery.runtime_s uint16 LE
         *   data[4:5] = flags (always 0x0000 observed; non-zero = OB)
         * Applied to state if values pass sanity checks.
         */
        if (len < 6u) {
            ESP_LOGW(TAG, "[MGE Feature] rid=0x06: short read %u bytes (need 6)", (unsigned)len);
            break;
        }
        uint8_t  charge    = data[1];
        uint16_t runtime_s = (uint16_t)(data[2] | ((uint16_t)data[3] << 8));
        uint16_t flags     = (uint16_t)(data[4] | ((uint16_t)data[5] << 8));

        if (charge <= 100u) {
            ups_state_update_t upd;
            memset(&upd, 0, sizeof(upd));
            upd.valid                 = true;
            upd.battery_charge_valid  = true;
            upd.battery_charge        = charge;
            if (runtime_s > 0u) {
                upd.battery_runtime_valid = true;
                upd.battery_runtime_s     = runtime_s;
            }
            /* flags=0x0000 in all submissions - not reliable for OL.
             * Only trust non-zero flags for OB. OL from field cache. */
            if (flags != 0x0000u) {
                upd.input_utility_present_valid = true;
                upd.input_utility_present       = false;  /* OB */
            }
            ups_state_apply_update(&upd);
            ESP_LOGI(TAG, "[MGE Feature] rid=0x06 charge=%u%% runtime=%us flags=0x%04X -> applied",
                     (unsigned)charge, (unsigned)runtime_s, (unsigned)flags);
        } else {
            ESP_LOGW(TAG, "[MGE Feature] rid=0x06 charge=0x%02X out of range - "
                     "Feature GET_REPORT not supported by this firmware", (unsigned)charge);
        }
        break;
    }
    case 0x20: {
        /*
         * rid=0x20: NOT live battery.charge despite initial assumption.
         * Three Eaton 3S 700 submissions (2026-03-30, 2026-04-02) all returned
         * 0x02 (2%) on batteries confirmed by the submitters to be fully charged.
         * This register likely reflects a discharge threshold or configuration
         * value, not the live state of charge.
         *
         * Real battery.charge and runtime come from rid=0x06 interrupt-IN,
         * which fires on power events (mains loss, state change). Decoded in
         * ups_hid_parser.c DECODE_EATON_MGE path.
         *
         * Logged here for diagnostics only. NOT applied to state.
         */
        if (len < 2u) {
            ESP_LOGW(TAG, "[MGE Feature] rid=0x20: short read %u bytes", (unsigned)len);
            break;
        }
        uint8_t charge_raw = data[1];
        ESP_LOGI(TAG, "[MGE Feature] rid=0x20 raw=0x%02X (%u) - diagnostic only, not applied",
                 (unsigned)charge_raw, (unsigned)charge_raw);
        break;
    }
    case 0x85: {
        /* rid=0x85: speculative BatterySystem/alarm status probe.
         * 0x8x rids arrive as interrupt-IN ALARM/EVENT reports during steady state.
         * GET_REPORT on 0x85 may return a snapshot of current alarm flags.
         * Byte layout unknown - log all bytes for discharge-event correlation.
         * If a mains-loss log shows this rid with a consistent non-zero byte,
         * that byte position is the OB status source. Decode follows in next version. */
        ESP_LOGW(TAG, "[MGE Feature] rid=0x85 raw (%u bytes): %s - OB status probe",
                 (unsigned)len, hexbuf);
        break;
    }
    case 0xFD:
        /*
         * rid=0xFD: returns 2 bytes [0xFD, 0x29] on Eaton 3S 700.
         * Meaning unknown. Logged above for analysis. No decode yet.
         * Candidates: firmware version, battery status flags, runtime low threshold.
         */
        if (len < 2u) {
            ESP_LOGW(TAG, "[MGE Feature] rid=0xFD: short read %u bytes", (unsigned)len);
        }
        /* No decode - raw log above is sufficient for now */
        break;
    default:
        break;
    }
}

/* ---- Timer task - only posts to queue, does NO USB work -------------- */
static void get_report_timer_task(void *arg)
{
    (void)arg;

    const uint8_t *rids   = NULL;
    size_t         rids_n = 0;

    /* DECODE_STANDARD: dynamically poll all Input RIDs from the parsed
     * HID descriptor. This covers PowerWalker (rid=0x30, 24 bytes) and
     * any other standard-decode device without hardcoded RID lists. */
    uint8_t dyn_rids[16];
    uint8_t dyn_count = 0;

    if (s_entry && s_entry->decode_mode == DECODE_APC_BACKUPS) {
        rids   = s_apc_rids;
        rids_n = s_apc_rids_n;
    } else if (s_entry && s_entry->decode_mode == DECODE_APC_SMARTUPS) {
        rids   = s_apc_smartups_rids;
        rids_n = s_apc_smartups_rids_n;
    } else if (s_entry && s_entry->decode_mode == DECODE_EATON_MGE) {
        rids   = s_eaton_rids;
        rids_n = s_eaton_rids_n;
    } else if (s_entry && s_entry->decode_mode == DECODE_VOLTRONIC) {
        rids   = s_voltronic_rids;
        rids_n = s_voltronic_rids_n;
    } else if (s_entry && s_entry->decode_mode == DECODE_STANDARD) {
        dyn_count = ups_hid_parser_get_input_rids(dyn_rids, sizeof(dyn_rids));
        rids   = dyn_rids;
        rids_n = dyn_count;
    } else {
        rids   = s_tripplite_rids;
        rids_n = s_tripplite_rids_n;
    }

    ESP_LOGI(TAG, "Timer task started (%u RIDs, interval=%dms)",
             (unsigned)rids_n, UPS_GET_REPORT_INTERVAL_MS);

    /* Initial delay — let interrupt-IN reader stabilise */
    vTaskDelay(pdMS_TO_TICKS(2000));

    while (s_active) {
        /* Post each RID to the queue — usb_client_task will service them */
        for (size_t i = 0; i < rids_n && s_active; i++) {
            get_report_req_t req = { .rid = rids[i] };
            if (xQueueSend(s_request_queue, &req, pdMS_TO_TICKS(500)) != pdTRUE) {
                ESP_LOGW(TAG, "request queue full, dropping rid=0x%02X", rids[i]);
            }
        }
        /* Sleep until next cycle */
        for (int ms = 0; ms < UPS_GET_REPORT_INTERVAL_MS && s_active; ms += 100) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

    ESP_LOGI(TAG, "Timer task exiting");
    s_timer_task = NULL;
    vTaskDelete(NULL);
}

/* ---- XCHK probe queue service ---------------------------------------- */
/*
 * Drains one probe request, issues GET_REPORT (Feature type), logs raw bytes.
 * Called from ups_get_report_service_queue() on every usb_client_task loop.
 * Safe when probe was never initialised — guard checks handle validity.
 */
static void service_probe_queue(void)
{
    if (!s_probe_queue || !s_probe_dev || !s_probe_client || s_probe_intf < 0) return;

    probe_req_t req;
    if (xQueueReceive(s_probe_queue, &req, 0) != pdTRUE) return;

    uint16_t sz = req.size;
    if (sz == 0u || sz > 64u) sz = 8u;

    ESP_LOGI(TAG, "[XCHK Probe] rid=0x%02X wlen=%u - issuing GET_REPORT (Feature type=3)",
             (unsigned)req.rid, (unsigned)sz);

    uint8_t   buf[64];
    size_t    got = 0;
    esp_err_t err = do_get_feature_report(s_probe_client, s_probe_dev, s_probe_intf,
                                           req.rid, buf, (size_t)sz, &got);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "[XCHK Probe] rid=0x%02X: STALL or error - RID does not support Feature type",
                 (unsigned)req.rid);
        return;
    }

    /* Log raw response bytes */
    {
        char   hexbuf[64] = {0};
        int    pos        = 0;
        size_t n          = (got > 16u) ? 16u : got;
        for (size_t i = 0; i < n; i++) {
            pos += snprintf(hexbuf + pos, sizeof(hexbuf) - (size_t)pos,
                            "%02X%s", buf[i], (i == n - 1u) ? "" : " ");
        }
        ESP_LOGI(TAG, "[XCHK Probe] rid=0x%02X response (%u bytes): %s",
                 (unsigned)req.rid, (unsigned)got, hexbuf);
    }

    /* Route probe responses through the appropriate decoder so bootstrap
     * GET_REPORT probes (queued at enumeration in ups_usb_hid Step 7b)
     * actually update state instead of just logging.
     *
     * Eaton/MGE: vendor-specific decode_eaton_feature().
     * DECODE_STANDARD: generic parser - Feature response has same format
     *   as Input report (buf[0]=rid, buf[1..]=payload). The field cache
     *   built from the HID descriptor will extract matching fields.
     * APC: handled by their own decode paths (not probed here). */
    if (err == ESP_OK && got >= 2u && s_entry) {
        if (s_entry->decode_mode == DECODE_EATON_MGE) {
            decode_eaton_feature(req.rid, buf, got);
        } else if (s_entry->decode_mode == DECODE_STANDARD) {
            ups_state_update_t upd;
            if (ups_hid_parser_decode_report(buf, got, &upd)) {
                ups_state_apply_update(&upd);
                ESP_LOGI(TAG, "[XCHK Probe] rid=0x%02X: standard decode applied to state",
                         (unsigned)req.rid);
            }
        }
    }

    /* Annotate each field in the descriptor for this RID with its NUT var name.
     * GET_REPORT response: buf[0] = rid echo, buf[1..] = payload.
     * Pass payload only (skip rid byte) to annotate_report. */
    if (got >= 2u) {
        const hid_desc_t *desc = ups_hid_parser_get_desc();
        if (desc) {
            ups_hid_map_annotate_report(desc, req.rid,
                                        buf + 1u, got - 1u,
                                        TAG);
        }
    }
}

/* ---- Public probe API ------------------------------------------------- */

void ups_get_report_probe_init(usb_host_client_handle_t client,
                               usb_device_handle_t      dev,
                               int                      intf_num)
{
    s_probe_client = client;
    s_probe_dev    = dev;
    s_probe_intf   = intf_num;

    if (!s_probe_queue) {
        s_probe_queue = xQueueCreate(8, sizeof(probe_req_t));
        if (!s_probe_queue) {
            ESP_LOGE(TAG, "probe_init: failed to create probe queue");
            return;
        }
    } else {
        xQueueReset(s_probe_queue);
    }
    ESP_LOGI(TAG, "[XCHK Probe] probe queue ready - will fire after XCHK settle");
}

void ups_get_report_probe_rid(uint8_t rid, uint16_t probe_size)
{
    if (!s_probe_queue) {
        ESP_LOGW(TAG, "[XCHK Probe] probe_rid called before probe_init - ignoring rid=0x%02X",
                 (unsigned)rid);
        return;
    }
    probe_req_t req = { .rid = rid, .size = probe_size };
    if (xQueueSend(s_probe_queue, &req, 0) != pdTRUE) {
        ESP_LOGW(TAG, "[XCHK Probe] probe queue full - dropping rid=0x%02X", (unsigned)rid);
    } else {
        ESP_LOGI(TAG, "[XCHK Probe] queued rid=0x%02X size=%u", (unsigned)rid, (unsigned)probe_size);
    }
}

void ups_get_report_probe_clear(void)
{
    s_probe_client = NULL;
    s_probe_dev    = NULL;
    s_probe_intf   = -1;
    if (s_probe_queue) {
        xQueueReset(s_probe_queue);
    }
}

/* ---- Public API ------------------------------------------------------- */

/*
 * ups_get_report_service_queue — called by usb_client_task on every iteration.
 *
 * Drains up to one pending request per call (avoids blocking the USB event
 * loop for too long — the control transfer pump is ~5ms per slice × 300 max
 * = 1.5s worst case, which is acceptable since usb_client_task uses
 * portMAX_DELAY on usb_host_client_handle_events only when idle).
 */
void ups_get_report_service_queue(void)
{
    /* Recurring Feature report polling (QUIRK_NEEDS_GET_REPORT devices only) */
    if (s_active && s_request_queue && s_dev) {
        get_report_req_t req;
        if (xQueueReceive(s_request_queue, &req, 0) == pdTRUE) {
            /* Request size must match what the device expects in wLength.
             * Requesting too many bytes causes some devices to STALL.
             *
             * Eaton/MGE: rid=0x20 is 1 byte payload + 1 rid echo = 2 total.
             * Requesting 16 causes STALL/timeout on Eaton devices.
             * DECODE_STANDARD: use largest Input report size from descriptor
             *   (e.g. PowerWalker rid=0x30 = 24 bytes).
             * APC/others: 16 bytes is safe. */
            uint8_t buf[64];
            size_t  buf_sz;
            if (s_entry && s_entry->decode_mode == DECODE_EATON_MGE) {
                buf_sz = 2u;
            } else if (s_entry && s_entry->decode_mode == DECODE_VOLTRONIC) {
                buf_sz = 16u;  /* Voltronic Feature reports are small (1-4 bytes) */
            } else if (s_entry && s_entry->decode_mode == DECODE_STANDARD) {
                uint16_t max_in = ups_hid_parser_max_input_bytes();
                buf_sz = (max_in > 0 && max_in <= 64u) ? (size_t)max_in : 16u;
            } else {
                buf_sz = 16u;
            }
            ESP_LOGI(TAG, "[GR] rid=0x%02X mode=%d intf=%d wlen=%u",
                     req.rid,
                     s_entry ? (int)s_entry->decode_mode : -1,
                     s_intf_num,
                     (unsigned)buf_sz);
            size_t    got = 0;
            esp_err_t err = do_get_feature_report(s_client, s_dev, s_intf_num,
                                                   req.rid, buf, buf_sz, &got);
            if (err == ESP_OK && got > 0) {
                if (s_entry && s_entry->decode_mode == DECODE_APC_BACKUPS) {
                    decode_apc_feature(req.rid, buf, got);
                } else if (s_entry && s_entry->decode_mode == DECODE_APC_SMARTUPS) {
                    decode_apc_smartups_feature(req.rid, buf, got);
                } else if (s_entry && s_entry->decode_mode == DECODE_EATON_MGE) {
                    decode_eaton_feature(req.rid, buf, got);
                } else if (s_entry && s_entry->decode_mode == DECODE_VOLTRONIC) {
                    decode_voltronic_feature(req.rid, buf, got);
                } else if (s_entry && s_entry->decode_mode == DECODE_STANDARD) {
                    ups_state_update_t upd;
                    if (ups_hid_parser_decode_report(buf, got, &upd)) {
                        ups_state_apply_update(&upd);
                        ESP_LOGI(TAG, "[GR] rid=0x%02X: standard decode applied", (unsigned)req.rid);
                    }
                }
            }
        }
    }

    /* XCHK one-shot probes - independent of recurring polling state */
    service_probe_queue();
}

void ups_get_report_start(usb_host_client_handle_t client,
                          usb_device_handle_t      dev,
                          int                      intf_num,
                          const ups_device_entry_t *entry)
{
    if (s_active) {
        ESP_LOGW(TAG, "already active — stop first");
        return;
    }
    if (!client || !dev || intf_num < 0 || !entry) {
        ESP_LOGW(TAG, "start: invalid args");
        return;
    }

    s_client   = client;
    s_dev      = dev;
    s_intf_num = intf_num;
    s_entry    = entry;
    s_active   = true;

    if (!s_request_queue) {
        s_request_queue = xQueueCreate(8, sizeof(get_report_req_t));
        if (!s_request_queue) {
            ESP_LOGE(TAG, "failed to create request queue");
            s_active = false;
            return;
        }
    } else {
        xQueueReset(s_request_queue);
    }

    ESP_LOGI(TAG, "Starting for %s (quirks=0x%04X, mode=%d)",
             entry->vendor_name ? entry->vendor_name : "?",
             (unsigned)entry->quirks,
             (int)entry->decode_mode);

    BaseType_t ret = xTaskCreatePinnedToCore(get_report_timer_task, "gr_timer",
                                              4096, NULL, 3, &s_timer_task, 0);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "failed to create timer task");
        s_active      = false;
        s_timer_task  = NULL;
    }
}

void ups_get_report_stop(void)
{
    if (!s_active) return;
    ESP_LOGI(TAG, "Stopping...");
    s_active   = false;
    s_dev      = NULL;
    s_client   = NULL;
    s_intf_num = -1;
    s_entry    = NULL;
    ups_get_report_probe_clear();
    /* Timer task will see s_active=false and self-delete.
     * If a control transfer is in-flight (s_inflight != NULL), ctrl_cb will
     * free it when the USB host cancels it during its own DEV_GONE teardown.
     * Do NOT wait here - this function is called from cleanup_device() which
     * runs in usb_client_task. Waiting for ctrl_cb here would deadlock because
     * ctrl_cb fires via usb_host_client_handle_events() which only runs in
     * the same usb_client_task that is blocked waiting here. */
    if (s_inflight != NULL) {
        ESP_LOGI(TAG, "Stop: transfer in-flight, will be freed by ctrl_cb on USB teardown");
    }
}

bool ups_get_report_running(void)
{
    return s_active;
}
