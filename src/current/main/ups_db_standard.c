/*============================================================================
 MODULE: ups_db_standard

 Standard HID path device entries. These vendors follow HID PDC spec
 closely - only minor quirk flags required, no custom decode logic.
 Grouped in one file (mirrors NUT belkin-hid.c pattern).

 VERSION HISTORY
 R0  v15.17  Extracted from ups_device_db.c.
============================================================================*/
#include "ups_db_standard.h"

static const ups_device_entry_t s_standard_entries[] = {

    /* ---- Tripp Lite (VID 0x09AE) ------------------------------------ */
    /* Some models expose values only via Feature reports.
     * NUT DDL: battery.voltage.nominal=24V, runtime.low=120s
     * US-targeted: input.voltage.nominal=120V */
    {
        .vid         = 0x09AE,
        .pid         = 0,
        .vendor_name = "Tripp Lite",
        .model_hint  = "OMNI/SMART/INTERNETOFFICE",
        .decode_mode = DECODE_STANDARD,
        .quirks      = QUIRK_NEEDS_GET_REPORT,
        .known_good  = false,
        .battery_voltage_nominal_mv = 24000,
        .battery_runtime_low_s      = 120,
        .battery_charge_low         = 10,
        .battery_charge_warning     = 50,
        .input_voltage_nominal_v    = 120,
        .ups_type                   = "line-interactive",
    },

    /* ---- Belkin (VID 0x050D) ---------------------------------------- */
    {
        .vid         = 0x050D,
        .pid         = 0,
        .vendor_name = "Belkin",
        .model_hint  = "F6H/F6C Series",
        .decode_mode = DECODE_STANDARD,
        .quirks      = 0,
        .known_good  = false,
        .battery_voltage_nominal_mv = 12000,
        .battery_runtime_low_s      = 120,
        .battery_charge_low         = 10,
        .battery_charge_warning     = 50,
        .input_voltage_nominal_v    = 120,
        .ups_type                   = "standby",
    },

    /* ---- Liebert / Vertiv (VID 0x10AF) ------------------------------ */
    /* GXT4/PSI5 double-conversion online UPS.
     * Note: at least two Liebert firmware types share VID:PID 10AF:0001.
     * Newer firmware uses standard HID PDC paths; older firmware has
     * incorrect exponents. Standard path handles both reasonably.
     * NUT DDL: battery.voltage.nominal=24V (conservative default) */
    {
        .vid         = 0x10AF,
        .pid         = 0,
        .vendor_name = "Liebert",
        .model_hint  = "GXT4/PSI5",
        .decode_mode = DECODE_STANDARD,
        .quirks      = 0,
        .known_good  = false,
        .battery_voltage_nominal_mv = 24000,
        .battery_runtime_low_s      = 120,
        .battery_charge_low         = 10,
        .battery_charge_warning     = 50,
        .input_voltage_nominal_v    = 120,
        .ups_type                   = "online",
    },

    /* ---- HP (VID 0x03F0) -------------------------------------------- */
    /* T-series G2/G3 line-interactive.
     * NUT DDL values from T750/T1000 G3 confirmed entries.
     * US-targeted: input.voltage.nominal=120V */
    {
        .vid         = 0x03F0,
        .pid         = 0,
        .vendor_name = "HP",
        .model_hint  = "T750/T1000/T1500/T3000 G2/G3",
        .decode_mode = DECODE_STANDARD,
        .quirks      = 0,
        .known_good  = false,
        .battery_voltage_nominal_mv = 24000,
        .battery_runtime_low_s      = 120,
        .battery_charge_low         = 10,
        .battery_charge_warning     = 50,
        .input_voltage_nominal_v    = 120,
        .ups_type                   = "line-interactive",
    },

    /* ---- PowerWalker / BlueWalker (VID 0x0665) ----------------------- */
    /* VID 0665 = WayTech/INNO TECH USB controller OEM used by BlueWalker.
     * NOT Powercom (Powercom = VID 0x0d9f). Confirmed from USB ID databases.
     * PowerWalker VI 3000 SCL (PID 0x5161). Standard HID fields on pages
     * 0x84/0x85. Charging (0x44) and Discharging (0x45) on page 0x85.
     * ACPresent (0x00D0) declared on page 0x85 (non-standard - normally 0x84).
     * Large rid=0x30 Input report (~24 bytes) - fixed by INT-IN buffer fix v0.30.
     * 230V European model. 2x12V 9Ah battery cells in series = 24V nominal.
     *
     * QUIRK_NEEDS_GET_REPORT: rid=0x30 never arrives on interrupt-IN (0 seen
     * in XCHK). ACPresent/Charging/Discharging flags are at byte offsets
     * 16-21 of rid=0x30 (24B report). Only accessible via GET_REPORT Feature.
     * Without this quirk, ups.status is stuck on OL and never transitions
     * to OB on mains loss. (Submission b4c432, 2026-04-11.) */
    {
        .vid         = 0x0665,
        .pid         = 0x5161,
        .vendor_name = "PowerWalker",
        .model_hint  = "VI 3000 SCL",
        .decode_mode = DECODE_STANDARD,
        .quirks      = QUIRK_NEEDS_GET_REPORT,
        .known_good  = false,
        .battery_voltage_nominal_mv = 24000,
        .battery_runtime_low_s      = 120,
        .battery_charge_low         = 10,
        .battery_charge_warning     = 50,
        .input_voltage_nominal_v    = 230,
        .ups_type                   = "line-interactive",
    },

    /* ---- Dell (VID 0x047C) ------------------------------------------ */
    {
        .vid         = 0x047C,
        .pid         = 0,
        .vendor_name = "Dell",
        .model_hint  = "H750E/H950E/H1000E/H1750E",
        .decode_mode = DECODE_STANDARD,
        .quirks      = 0,
        .known_good  = false,
        .battery_voltage_nominal_mv = 24000,
        .battery_runtime_low_s      = 120,
        .battery_charge_low         = 10,
        .battery_charge_warning     = 50,
        .input_voltage_nominal_v    = 120,
        .ups_type                   = "line-interactive",
    },
};

const ups_device_entry_t *ups_db_standard_get_entries(size_t *out_count)
{
    if (out_count) *out_count = sizeof(s_standard_entries) / sizeof(s_standard_entries[0]);
    return s_standard_entries;
}
