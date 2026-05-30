/*============================================================================
 MODULE: ups_db_apc

 APC / Schneider Electric device table entries (VID 0x051D).

 Known PIDs:
   0x0002  Back-UPS (XS 1500M, BR1000G confirmed)
   0x0003  Smart-UPS C / Smart-UPS (C 1500 confirmed)
   0x0000  VID-only wildcard fallback

 VERSION HISTORY
 R0  v15.17  Extracted from ups_device_db.c.
 R1  v0.44   Add Feature report polling tables. Decode functions sourced
             from NUT apc-hid.c hid2nut[] mappings.
============================================================================*/
#include "ups_db_apc.h"
#include "ups_state.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "ups_db_apc";

/* ---- APC Back-UPS (PID 0x0002) Feature report decoders --------------- */

static void decode_apc_backups_rid_0x17(uint8_t rid, const uint8_t *data, size_t len)
{
    if (len < 3u) {
        ESP_LOGW(TAG, "rid=0x%02X: short read %u bytes (need 3)", rid, (unsigned)len);
        return;
    }
    uint16_t volts = (uint16_t)(data[1] | ((uint16_t)data[2] << 8u));
    ESP_LOGI(TAG, "[APC Back-UPS Feature] rid=0x%02X line_voltage=%uV", rid, volts);

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
    } else if (volts == 0u) {
        ups_state_update_t upd;
        memset(&upd, 0, sizeof(upd));
        upd.valid                = true;
        upd.input_voltage_valid  = false;
        upd.output_voltage_valid = false;
        ups_state_apply_update(&upd);
    } else {
        ESP_LOGW(TAG, "rid=0x%02X volts=%u — outside valid range, ignoring", rid, volts);
    }
}

static void decode_apc_backups_rid_0x50(uint8_t rid, const uint8_t *data, size_t len)
{
    if (len < 2u) {
        ESP_LOGW(TAG, "rid=0x%02X: short read %u bytes (need 2)", rid, (unsigned)len);
        return;
    }
    uint8_t load = data[1];
    ESP_LOGI(TAG, "[APC Back-UPS Feature] rid=0x%02X ups.load=%u%%", rid, load);

    if (load <= 100u) {
        ups_state_update_t upd;
        memset(&upd, 0, sizeof(upd));
        upd.valid          = true;
        upd.ups_load_valid = true;
        upd.ups_load_pct   = load;
        ups_state_apply_update(&upd);
    } else {
        ESP_LOGW(TAG, "rid=0x%02X load=%u — outside 0-100%%, ignoring", rid, load);
    }
}

/* APC Back-UPS (PID 0x0002) Feature report polling list */
static const hid_get_report_info_t s_apc_backups_get_report_table[] = {
    { .rid = 0x17, .nut_var = "input.voltage", .flags = 0, .decode_fn = decode_apc_backups_rid_0x17 },
    { .rid = 0x50, .nut_var = "ups.load",      .flags = 0, .decode_fn = decode_apc_backups_rid_0x50 },
    { .rid = 0,    .nut_var = NULL,             .flags = 0, .decode_fn = NULL },  /* terminator */
};

/* ---- APC Smart-UPS (PID 0x0003) Feature report decoders --------------- */

static void decode_apc_smartups_rid_0x06(uint8_t rid, const uint8_t *data, size_t len)
{
    if (len < 3u) {
        ESP_LOGW(TAG, "rid=0x%02X: short read %u bytes (need 3)", rid, (unsigned)len);
        return;
    }
    uint8_t charging    = data[1];
    uint8_t discharging = data[2];
    ESP_LOGI(TAG, "[APC Smart-UPS Feature] rid=0x%02X charging=%u discharging=%u",
             rid, charging, discharging);

    /* TODO: update ups_state with charging/discharging flags once ups_state_update_t
     * supports per-source charging/discharging fields. For now, log only. */
}

static void decode_apc_smartups_rid_0x0e(uint8_t rid, const uint8_t *data, size_t len)
{
    if (len < 2u) {
        ESP_LOGW(TAG, "rid=0x%02X: short read %u bytes (need 2)", rid, (unsigned)len);
        return;
    }
    uint16_t batt_v_raw = data[1];
    uint32_t batt_v_mv = batt_v_raw * 10u;
    ESP_LOGI(TAG, "[APC Smart-UPS Feature] rid=0x%02X battery.voltage=%u mV", rid, (unsigned)batt_v_mv);

    ups_state_update_t upd;
    memset(&upd, 0, sizeof(upd));
    upd.valid = true;
    upd.battery_voltage_valid = true;
    upd.battery_voltage_mv = batt_v_mv;
    ups_state_apply_update(&upd);
}

/* APC Smart-UPS (PID 0x0003) Feature report polling list */
static const hid_get_report_info_t s_apc_smartups_get_report_table[] = {
    { .rid = 0x06, .nut_var = "ups.status",      .flags = 0, .decode_fn = decode_apc_smartups_rid_0x06 },
    { .rid = 0x0E, .nut_var = "battery.voltage", .flags = 0, .decode_fn = decode_apc_smartups_rid_0x0e },
    { .rid = 0,    .nut_var = NULL,              .flags = 0, .decode_fn = NULL },  /* terminator */
};

static const ups_device_entry_t s_apc_entries[] = {

    /* APC Back-UPS (PID 0x0002) - confirmed XS 1500M and BR1000G
     * Interrupt-IN decode: direct rid=0x0C (charge + runtime) + standard (charging flags)
     * Feature report polling: rid=0x17 (AC line voltage), rid=0x50 (ups.load)
     * NUT DDL: battery.voltage.nominal=24V, runtime.low=120s */
    {
        .vid         = 0x051D,
        .pid         = 0x0002,
        .vendor_name = "APC",
        .model_hint  = "Back-UPS (PID 0002)",
        .decode_mode = DECODE_APC_BACKUPS,
        .quirks      = QUIRK_VENDOR_PAGE_REMAP | QUIRK_NEEDS_GET_REPORT,
        .known_good  = true,
        .battery_voltage_nominal_mv = 24000,
        .battery_runtime_low_s      = 120,
        .battery_charge_low         = 10,
        .battery_charge_warning     = 50,
        .input_voltage_nominal_v    = 120,
        .ups_type                   = "line-interactive",
        .get_report_table           = s_apc_backups_get_report_table,
    },

    /* APC Smart-UPS C / Smart-UPS (PID 0x0003) - confirmed Smart-UPS C 1500
     * Interrupt-IN decode: direct rid=0x07 (status) + rid=0x0D (runtime) + standard (charge)
     * Feature report polling: rid=0x06 (charging flags), rid=0x0E (battery voltage)
     * NUT DDL: battery.voltage.nominal=24V, runtime.low=120s */
    {
        .vid         = 0x051D,
        .pid         = 0x0003,
        .vendor_name = "APC",
        .model_hint  = "Smart-UPS C / Smart-UPS (PID 0003)",
        .decode_mode = DECODE_APC_SMARTUPS,
        .quirks      = QUIRK_VENDOR_PAGE_REMAP | QUIRK_NEEDS_GET_REPORT,
        .known_good  = true,
        .battery_voltage_nominal_mv = 24000,
        .battery_runtime_low_s      = 120,
        .battery_charge_low         = 10,
        .battery_charge_warning     = 50,
        .input_voltage_nominal_v    = 120,
        .ups_type                   = "line-interactive",
        .get_report_table           = s_apc_smartups_get_report_table,
    },

    /* VID-only wildcard: other APC PIDs - standard HID path */
    {
        .vid         = 0x051D,
        .pid         = 0,
        .vendor_name = "APC",
        .model_hint  = "Smart-UPS / other",
        .decode_mode = DECODE_STANDARD,
        .quirks      = QUIRK_VENDOR_PAGE_REMAP,
        .known_good  = false,
        .battery_voltage_nominal_mv = 24000,
        .battery_runtime_low_s      = 120,
        .battery_charge_low         = 10,
        .battery_charge_warning     = 50,
        .input_voltage_nominal_v    = 120,
        .ups_type                   = "line-interactive",
        .get_report_table           = NULL,  /* uses standard path only */
    },
};

const ups_device_entry_t *ups_db_apc_get_entries(size_t *out_count)
{
    if (out_count) *out_count = sizeof(s_apc_entries) / sizeof(s_apc_entries[0]);
    return s_apc_entries;
}
