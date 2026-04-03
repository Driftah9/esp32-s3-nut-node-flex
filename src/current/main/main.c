/*============================================================================
 ESP32-S3 UPS NUT Node
 CORE ORCHESTRATOR

 VERSION: v15.14
 DATE: 2026-03-16
 ESP-IDF: v5.3.1

 REVERT HISTORY
 R0  v14.7 confirmed-stable single-file baseline
 R1  v14.7 modular baseline (real WiFi/HTTP/NUT modules)
 R2  v14.7 modular + ups_state + USB skeleton
 R3  v14.8 modular + USB scan/identify module
 R4  v14.24 remove ups_state_set_demo_defaults() call at boot.
 R5  v14.25 SoftAP lifecycle: disable on STA connect, re-enable after
            60s STA dropout. Portal redesign + Basic Auth password
            protection via cfg->portal_pass (empty = open, first boot).
 R6  v15.3  Version bump to match HID parser/descriptor versions.
            CyberPower direct-decode bypass now fully operational.
 R7  v15.6  APC runtime from rid=0C bytes 1-2. BR1000G confirmed.
            cache scan: uid=0x73 RunTimeToEmpty, uid=0x67 RelativeSOC.
 R8  v15.7  Remove input/output voltage everywhere.
            Dynamic portal dashboard: static Mfr/Model/Driver/Status rows,
            optional rows (Charge/Runtime/Batt V/Load) only when valid.
            AJAX addOrUpdate() inserts new rows dynamically on discovery.
 R9  v15.9  /compat page hierarchy expand/collapse, runtime display Xm Ys.
 R10 v15.10 DB-driven identity: mfr/model overrides from ups_device_db.
            derive_status debug log. CPS→CyberPower, garbled product
            string fallback to model_hint.
 R11 v0.12-flex diag_capture_check_and_arm() called after cfg load, before
                wifi/USB init. Arms ring buffer + vprintf hook if NVS flag set.

============================================================================*/

#include "esp_log.h"
#include "nvs_flash.h"

#include "cfg_store.h"
#include "wifi_mgr.h"
#include "http_portal.h"
#include "nut_server.h"
#include "nut_client.h"
#include "nut_bridge.h"
#include "ups_state.h"
#include "ups_usb_hid.h"
#include "diag_capture.h"

static const char *TAG = "UPS_USB_M15";

static app_cfg_t g_cfg;
static ups_state_t g_ups;

void app_main(void) {
    ESP_LOGI(TAG, "=======================================");
    ESP_LOGI(TAG, "ESP32 UPS NUT Node - flex v0.3");
    ESP_LOGI(TAG, "ESP-IDF v5.3.1 target");
    ESP_LOGI(TAG, "=======================================");

    ESP_ERROR_CHECK(nvs_flash_init());

    cfg_store_load_or_defaults(&g_cfg);
    cfg_store_ensure_ap_ssid(&g_cfg);
    cfg_store_commit(&g_cfg);

    /* Arm diagnostic capture if requested - must run before wifi/USB init
     * so the full boot sequence is captured in the ring buffer */
    diag_capture_check_and_arm();
    ESP_LOGI(TAG, "op_mode=%u upstream=%s:%u", (unsigned)g_cfg.op_mode,
             g_cfg.upstream_host, (unsigned)g_cfg.upstream_port);

    /* UPS state — starts fully zeroed. Populated by HID reports after
     * USB enumeration (~1s). No demo defaults. */
    ups_state_init(&g_ups);

    wifi_mgr_start_apsta(&g_cfg);
    ups_usb_hid_start(&g_cfg);
    http_portal_start(&g_cfg);

    /* Mode dispatch */
    switch (g_cfg.op_mode) {
        case OP_MODE_NUT_CLIENT:
            ESP_LOGI(TAG, "Mode 2: NUT CLIENT - pushing to %s:%u",
                     g_cfg.upstream_host, (unsigned)g_cfg.upstream_port);
            nut_client_start(&g_cfg);
            break;

        case OP_MODE_BRIDGE:
            ESP_LOGI(TAG, "Mode 3: BRIDGE - forwarding raw HID to %s:%u",
                     g_cfg.upstream_host, (unsigned)g_cfg.upstream_port);
            nut_bridge_start(&g_cfg);
            break;

        case OP_MODE_STANDALONE:
        default:
            ESP_LOGI(TAG, "Mode 1: STANDALONE - serving NUT on tcp/3493");
            nut_server_start(&g_cfg);
            break;
    }

    ESP_LOGI(TAG, "Ready. Portal: http://%s/config (SoftAP) or http://<STA-IP>/config",
             WIFI_MGR_SOFTAP_IP_STR);
}
