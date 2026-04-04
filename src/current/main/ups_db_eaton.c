/*============================================================================
 MODULE: ups_db_eaton

 Eaton / MGE / Powerware device table entries (VID 0x0463).

 Known PIDs:
   0xFFFF  3S / 5E / 5P / Ellipse / Evolution family (3S 550/700/850 confirmed)
   0x0000  VID-only wildcard fallback (covers remaining Eaton models)

 Eaton 3S HID behaviour (confirmed from three community submissions, 2026-03-30/04-02):
   - 926-byte HID descriptor, 111 fields, 10 declared report IDs
   - Descriptor uses vendor page 0xFFFF extensively - all skipped by parser
   - All standard field cache entries (charge, runtime, voltage, status) MISSING
     via the standard HID path - descriptor is not useful for live data
   - 12 undeclared report IDs seen in interrupt-IN stream during first 30s:
       0x21, 0x22, 0x23, 0x25, 0x28, 0x29 (0x2x range - UPS state data)
       0x80, 0x82, 0x85, 0x86, 0x87, 0x88 (0x8x range - alarm/event)
   - rid=0x06 interrupt-IN: primary source of battery.charge and runtime.
       Fires on power events (mains loss, state change) - NOT during steady state.
       Format: [0x06][charge_pct][runtime_lo][runtime_hi][flags_lo][flags_hi]
       Confirmed sample: 06 63 B4 10 00 00
         charge=0x63=99%, runtime=0x10B4=4276s, flags=0x0000 (OL)
       NOT seen during 30s XCHK window - event-driven only.
   - GET_REPORT rid=0x20: returns 0x02 (2%) consistently on fully charged
       batteries across all three submissions. Does NOT reflect live charge.
       Likely a configuration/threshold register. Logged for diagnostics only.
   - GET_REPORT rid=0xFD: returns 0x29 (=41) on all submissions. Meaning TBD.
       Possible candidates: runtime-minutes, charge threshold, extended status.
       Need a second data point at different charge state to decode.
   - NUT subdriver: mge-hid.c (Eaton/MGE HID 1.x)
   - EU-targeted: input.voltage.nominal=230V, battery.voltage.nominal=12V (3S series)

 Open items for future versions:
   - Capture rid=0x06 on-battery (OB) event to decode flag byte OB bit positions
   - Decode rid=0xFD: need second submission at different charge state
   - Decode 0x2x interrupt-IN range for supplemental status/runtime
   - Map 0x8x interrupt rids to alarm flags

 VERSION HISTORY
 R0  v15.17  Extracted + added confirmed 3S entry with DECODE_EATON_MGE.
 R1  v0.15   Corrected rid=0x20 comment - confirmed NOT live charge (wrong on
             full batteries). rid=0x06 added as confirmed primary source.
             rid=0xFD meaning TBD noted.
============================================================================*/
#include "ups_db_eaton.h"

static const ups_device_entry_t s_eaton_entries[] = {

    /* Eaton 3S / 5E / 5P / Ellipse / Evolution (PID 0xFFFF)
     * Confirmed: 3S 700 (three submissions, 2026-03-30 and 2026-04-02)
     * battery.charge + runtime: rid=0x06 interrupt-IN (event-driven, fires on mains events)
     * GET_REPORT rid=0x20 returns 2% on full batteries - not live charge, not used
     * GET_REPORT rid=0xFD: value=0x29=41, meaning TBD
     * OL status: confirmed from rid=0x06 flags=0x0000. OB flags TBD.
     * NUT mge-hid: battery.voltage.nominal=12V for 3S series
     * EU-targeted: input.voltage.nominal=230V */
    {
        .vid         = 0x0463,
        .pid         = 0xFFFF,
        .vendor_name = "Eaton/MGE",
        .model_hint  = "3S/5E/5P/Ellipse/Evolution (PID FFFF)",
        .decode_mode = DECODE_EATON_MGE,
        .quirks      = QUIRK_NEEDS_GET_REPORT,
        .known_good  = true,
        .battery_voltage_nominal_mv = 12000,
        .battery_runtime_low_s      = 120,
        .battery_charge_low         = 10,
        .battery_charge_warning     = 50,
        .input_voltage_nominal_v    = 230,
        .ups_type                   = "line-interactive",
    },

    /* VID-only wildcard: other Eaton/MGE/Powerware PIDs
     * Standard HID path - larger Eaton models (9PX, 5PX, Pulsar) use
     * compliant HID descriptors and do not need a custom decode path.
     * NUT DDL: battery.voltage.nominal=24V (mid-range default)
     * EU-targeted: input.voltage.nominal=230V */
    {
        .vid         = 0x0463,
        .pid         = 0,
        .vendor_name = "Eaton/MGE",
        .model_hint  = "3S/5E/5P/Ellipse/Evolution/other",
        .decode_mode = DECODE_STANDARD,
        .quirks      = 0,
        .known_good  = false,
        .battery_voltage_nominal_mv = 24000,
        .battery_runtime_low_s      = 120,
        .battery_charge_low         = 10,
        .battery_charge_warning     = 50,
        .input_voltage_nominal_v    = 230,
        .ups_type                   = "line-interactive",
    },
};

const ups_device_entry_t *ups_db_eaton_get_entries(size_t *out_count)
{
    if (out_count) *out_count = sizeof(s_eaton_entries) / sizeof(s_eaton_entries[0]);
    return s_eaton_entries;
}
