/*============================================================================
 MODULE: ups_db_cyberpower

 CyberPower Systems device table entries (VID 0x0764).

 Known PIDs:
   0x0501  CP consumer series (AVR/SX/ST/CP - CP550HG, SX550G confirmed)
   0x0601  OR/PR rackmount series
   0x0005  Older CP/BC models
   0x0000  VID-only wildcard fallback

 PID 0x0501 (consumer): all runtime data on vendor RIDs 0x20-0x88.
 PID 0x0601 (rackmount): uses standard HID RIDs (0x08, 0x0B) - NOT vendor RIDs.
   DECODE_CYBERPOWER falls through to standard field-cache path for 0x0601.

 VERSION HISTORY
 R0  v15.17  Extracted from ups_device_db.c.
 R1  v0.16   Corrected 0x0601 entry: not same decode path as 0x0501. Uses
             standard HID RIDs. known_good=false pending re-submission.
             Source: CyberPower 3000R submission 2026-04-04.
============================================================================*/
#include "ups_db_cyberpower.h"

static const ups_device_entry_t s_cyberpower_entries[] = {

    /* CyberPower AVR/SX/ST/CP consumer series - confirmed CP550HG, SX550G
     * Decode: direct-decode only (descriptor useless)
     * rids: 0x20=charge, 0x21=runtime, 0x29=status, 0x80=ac_present,
     *       0x85=flags, 0x88=battery_voltage
     * NUT DDL: battery.voltage.nominal=12V, runtime.low=300s
     * US-targeted: input.voltage.nominal=120V */
    {
        .vid         = 0x0764,
        .pid         = 0x0501,
        .vendor_name = "CyberPower",
        .model_hint  = "ST/CP/SX Series (PID 0501)",
        .decode_mode = DECODE_CYBERPOWER,
        .quirks      = QUIRK_DIRECT_DECODE |
                       QUIRK_VOLTAGE_LOGMAX_FIX |
                       QUIRK_BATT_VOLT_SCALE |
                       QUIRK_FREQ_SCALE_0_1,
        .known_good  = true,
        .battery_voltage_nominal_mv = 12000,
        .battery_runtime_low_s      = 300,
        .battery_charge_low         = 20,
        .battery_charge_warning     = 50,
        .input_voltage_nominal_v    = 120,
        .ups_type                   = "line-interactive",
    },

    /* CyberPower OR/PR/RT/UT rackmount (PID 0601).
     * NOTE: PID 0601 does NOT use vendor RIDs 0x20-0x88 like PID 0501.
     * Interrupt-IN data arrives on standard HID RIDs: rid=0x08 (battery.charge)
     * and rid=0x0B (status byte, meaning TBD). The DECODE_CYBERPOWER direct path
     * will not recognize these RIDs and falls through to the standard field-cache
     * path. QUIRK_DIRECT_DECODE is retained but QUIRK_NEEDS_GET_REPORT is NOT
     * set - device sends unsolicited interrupt-IN every 2s.
     * Source: CyberPower 3000R submission 2026-04-04 (0764:0601).
     * known_good=false until fix is verified with re-submission. */
    {
        .vid         = 0x0764,
        .pid         = 0x0601,
        .vendor_name = "CyberPower",
        .model_hint  = "OR/PR/RT/UT Series (PID 0601)",
        .decode_mode = DECODE_CYBERPOWER,
        .quirks      = QUIRK_DIRECT_DECODE |
                       QUIRK_VOLTAGE_LOGMAX_FIX |
                       QUIRK_BATT_VOLT_SCALE |
                       QUIRK_FREQ_SCALE_0_1 |
                       QUIRK_ACTIVE_PWR_LOGMAX_FIX,
        .known_good  = false,
        .battery_voltage_nominal_mv = 24000,
        .battery_runtime_low_s      = 300,
        .battery_charge_low         = 20,
        .battery_charge_warning     = 50,
        .input_voltage_nominal_v    = 120,
        .ups_type                   = "line-interactive",
    },

    /* Older CyberPower model - standard HID path */
    {
        .vid         = 0x0764,
        .pid         = 0x0005,
        .vendor_name = "CyberPower",
        .model_hint  = "900AVR/BC900D (PID 0005)",
        .decode_mode = DECODE_STANDARD,
        .quirks      = QUIRK_VOLTAGE_LOGMAX_FIX,
        .known_good  = false,
        .battery_voltage_nominal_mv = 12000,
        .battery_runtime_low_s      = 300,
        .battery_charge_low         = 20,
        .battery_charge_warning     = 50,
        .input_voltage_nominal_v    = 120,
        .ups_type                   = "line-interactive",
    },

    /* Cyber Energy / ST Micro OEM */
    {
        .vid         = 0x0483,
        .pid         = 0xa430,
        .vendor_name = "CyberEnergy",
        .model_hint  = "USB Series (ST Micro OEM)",
        .decode_mode = DECODE_STANDARD,
        .quirks      = 0,
        .known_good  = false,
        .battery_voltage_nominal_mv = 12000,
        .battery_runtime_low_s      = 300,
        .battery_charge_low         = 20,
        .battery_charge_warning     = 50,
        .input_voltage_nominal_v    = 120,
        .ups_type                   = "line-interactive",
    },

    /* VID-only wildcard: any other CyberPower PID */
    {
        .vid         = 0x0764,
        .pid         = 0,
        .vendor_name = "CyberPower",
        .model_hint  = NULL,
        .decode_mode = DECODE_STANDARD,
        .quirks      = QUIRK_VOLTAGE_LOGMAX_FIX | QUIRK_FREQ_SCALE_0_1,
        .known_good  = false,
        .battery_voltage_nominal_mv = 12000,
        .battery_runtime_low_s      = 300,
        .battery_charge_low         = 20,
        .battery_charge_warning     = 50,
        .input_voltage_nominal_v    = 120,
        .ups_type                   = "line-interactive",
    },
};

const ups_device_entry_t *ups_db_cyberpower_get_entries(size_t *out_count)
{
    if (out_count) *out_count = sizeof(s_cyberpower_entries) / sizeof(s_cyberpower_entries[0]);
    return s_cyberpower_entries;
}
