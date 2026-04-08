/*============================================================================
 MODULE: ups_hid_parser

 RESPONSIBILITY
 Decode raw USB HID interrupt-IN reports into structured UPS state updates.

 VERSION HISTORY
 v15.0  Complete rewrite — usage-based (vendor-agnostic).
 v15.3  CyberPower direct-decode bypass (page-filter workaround).
 v15.4  Replace all hardcoded VID:PID logic with ups_device_db lookup.
        Fix derive_status UNKNOWN overwrite bug:
          Previously, each report decoded in isolation — a report that had
          no utility/charge info would derive "UNKNOWN" and overwrite a
          valid "OL" already in g_state.  Fix: derive_status only writes
          "UNKNOWN" into upd->ups_status when it genuinely cannot form
          an opinion; ups_state_apply_update already skips writing status
          if upd->ups_status[0] == 0 — so we clear ups_status on "no
          opinion" instead of writing "UNKNOWN".
 v15.5  APC Back-UPS direct-decode: rid=0C byte0=charge% confirmed.
 v15.6  APC runtime from rid=0C bytes 1-2 (uint16_le, seconds).
        APC BR1000G tested and confirmed — same VID:PID, same decode path.
        cache scan: add uid=0x73 (APC non-standard RunTimeToEmpty)
          and uid=0x67 (RelativeSOC fallback) to battery_runtime and
          battery_charge respectively.
        rid=0C comment updated with confirmed field map from two devices.
 v15.7  Remove input_voltage and output_voltage from cache, standard
        decode path, and CyberPower direct-decode.
 v15.14 Add DECODE_APC_SMARTUPS for APC Smart-UPS C / Smart-UPS (PID 0003).
        decode_apc_smartups_direct(): rid=0x0D runtime (uint16-LE seconds),
        rid=0x07 status flags (confirmed: bit2=AC present, bit1=discharging),
        rid=0x0C charge via standard descriptor path.
        Confirmed from issue #1 Smart-UPS C 1500 on-battery discharge log.
        Neither appears via interrupt IN on any tested device —
        APC voltages are Feature-only (GET_REPORT, M-series future task),
        CyberPower rids 0x23 were direct-decoded but data is not needed
        for NUT/HA integration at this time.

 DESIGN
  1. At enumeration: ups_usb_hid calls ups_hid_parser_set_descriptor().
     Device is looked up in ups_device_db.  Quirk flags and decode mode
     are noted.  Field cache is built from parsed descriptor.

  2. On each interrupt-IN report: ups_hid_parser_decode_report() is called.
     - DECODE_CYBERPOWER: direct byte-position decode for known-broken
       CyberPower descriptors.
     - DECODE_STANDARD: generic HID field-cache decode for all other devices.

  3. derive_status builds the NUT compound status string from flags.
     "No opinion" → empty string (caller skips writing to g_state).
     "UNKNOWN" is only written when the parser has data but cannot
     determine on/off-battery state.

 STATUS FLAG RESOLUTION (priority order)
  a) input_utility_present_valid  (most reliable)
  b) discharging_flag / charging_flag
  c) battery_charge_valid (fallback — assume OL if charge known)

 REVERT HISTORY
 R0  v15.0  Initial usage-based rewrite
 R1  v15.3  CyberPower direct-decode bypass
 R2  v15.4  DB-driven device detection; derive_status bug fix
 R3  v15.6  APC runtime from rid=0C; uid=0x73 cache scan
 R4  v15.7  Remove input/output voltage decode and cache entries
 R5  v0.6-flex  Phase 4 dynamic XCHK: s_seen_rids bitmask accumulated
                from interrupt-IN traffic; 30s settle timer fires
                ups_hid_parser_run_xchk() comparing live seen vs
                descriptor-declared Input RIDs. Replaces static
                expected_rids[] list in ups_hid_desc.c.
 R6  v0.7-flex  Phase 4 probe: ups_xchk_probe_fn_t callback registered
                by ups_usb_hid. run_xchk Part 2 queues GET_REPORT probes
                for declared-but-silent Input RIDs via the callback.
                Probe fires in usb_client_task via ups_get_report.
                Probe size: feature_bytes from descriptor, fallback to
                input_bytes, clamped to 16. Raw response logged in hex.
 R7  v0.14      Fix XCHK probe size cap: 16->64 in run_xchk Part 2 and in
                ups_get_report service_probe_queue(). Previous cap caused
                wLength=16 for rid=0x28 (63 bytes declared) on
                CyberPower/PowerWalker devices. IDF v5.5.4 DWC assert fires
                when wLength < declared size (hcd_dwc.c:2388). Now passes
                declared size up to 64 bytes end-to-end.
 R8  v0.15      Eaton/MGE rid=0x06 flags decode: flags=0x0000 -> OL
                (input_utility_present=true). Confirmed from Eaton 3S 700
                submission 2026-04-02 (sample: 06 63 B4 10 00 00).
                OB bit positions remain TBD - need discharge event log.
 R9  v0.16      Fix DECODE_CYBERPOWER goto: changed "if (rid != 0x20) goto
                finalize" to "if (changed) goto finalize". Old logic silently
                discarded all RIDs except 0x20 without running the standard
                path. CyberPower 3000R (0764:0601) sends battery.charge on
                rid=0x08 which IS in the field cache - but was silently
                dropped. Fix allows standard path to run as fallback for any
                RID the CP direct decode does not recognise.
                Add rid=0x0B diagnostic log (3000R sends 1 byte here, value
                0x13=19 observed - meaning TBD, need discharge event).
                Source: CyberPower 3000R submission 2026-04-04.
 R16 v0.29   Make goto finalize unconditional for rid=0x06/0x21 Eaton blocks.
                v0.28 used `if (changed) goto finalize` which would fail to
                skip the standard path if charge>100 or runtime=0 left changed
                false. Now unconditionally skips standard path for all Eaton
                rids. Primary stale-data fix is in ups_get_report.c R7 (add
                rid=0x06 to periodic polling).
                Source: Eaton 3S submissions 30b6f9 + 713d7c (MyDisplayName).
 R15 v0.28   Fix Eaton stale-data regression from v0.27 (R14).
                Root cause: v0.27 added 0xFFFF fields to the field cache, which
                made the standard descriptor path find cache hits on Eaton rids.
                The standard path then extracted bits from vendor-format payloads
                using descriptor-declared bit offsets - corrupting the correctly
                decoded charge/runtime values from the direct-decode path.
                Fix: add goto finalize after Eaton rid=0x06/0x21 decode blocks
                (skip standard path when direct decode succeeded) and add goto
                finalize for all other Eaton rids (standard path cannot decode
                vendor-format payloads). Also add !battery_runtime_valid guard
                to standard path runtime extraction to prevent overwrite of
                direct-decoded values.
                Source: Eaton 3S submission 2026-04-07 (MyDisplayName).
 R14 v0.26   Eaton vendor page 0xFFFF support for OL/OB detection.
                Root cause: Eaton 3S descriptor has 111 fields ALL on vendor
                page 0xFFFF. Parser filtered them out (only kept 0x84/0x85).
                Standard field cache (ac_present, charging, discharging) was
                always empty for Eaton - no standard-path OL/OB source.
                Fix: include page 0xFFFF in descriptor interesting filter
                (ups_hid_desc.c) and add page 0xFF to field cache scan.
                MGE HID protocol uses same usage IDs as standard pages.
                Demote flags-based OL from rid=0x06/0x21: flags=0x0000 was
                observed in ALL submissions including mains-loss events.
                Only non-zero flags trigger OB. OL now comes from standard
                field cache (ACPresent/Charging/Discharging on declared rids)
                or conservative charge-data default in derive_status().
 R13 v0.26   Eaton 0x8x alarm rids: raise log cap from 8 to 16 bytes, elevate
                0x8x range to WARN level. These rids are the most likely source
                of OL->OB state change notification on mains loss. Full byte
                capture at WARN ensures visibility in discharge-event logs even
                at reduced verbosity. OB decode follows once a discharge log is
                captured confirming which byte(s) change.
 R12 vFIX2  Decode rid=0x21 in DECODE_EATON_MGE path -- same byte layout as
                rid=0x06, treated as steady-state heartbeat. rid=0x06 fires on
                mains events only; rid=0x21 arrives during normal steady-state
                operation within the first 30s (confirmed from Eaton 3S 700
                submissions). Sanity guards protect against wrong-format reads.
                Log all other unrecognised Eaton interrupt-IN rids (0x2x, 0x8x)
                at INFO level for field analysis.
 R11 vFIX   Shorten XCHK settle timer from 30s to 5s for DECODE_EATON_MGE
                devices. rid=0x06 is event-driven and may never arrive on
                stable AC; the 30s wait delayed even the Input-RID cross-check.
                Feature-report bootstrap (rid=0x20) is handled in ups_usb_hid
                Step 7b immediately at enumeration, not via XCHK.
 R10 v0.18      Per-RID arrival interval tracker. Three parallel arrays
                (s_rid_last_ms, s_rid_ema_ms, s_rid_samples) track the EMA
                inter-report interval for each RID seen in traffic. After 3+
                samples the learned interval feeds the status debounce in
                ups_state_apply_update(). source_rid and status_debounce_ms
                are filled in the finalize block and passed through
                ups_state_update_t. Exposed via
                ups_hid_parser_get_rid_interval().

============================================================================*/

#include "ups_hid_parser.h"
#include "ups_hid_desc.h"
#include "ups_state.h"
#include "ups_device_db.h"

#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include <stdbool.h>

#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "ups_hid_parser";

/* ---- Active device entry from DB ------------------------------------- */
static const ups_device_entry_t *s_device = NULL;

/* ---- Dynamic RID tracking (Phase 4) ---------------------------------- */
/* 256-bit bitmask: bit (rid) set when RID rid arrives in interrupt-IN.   */
/* Cleared on reset. XCHK runs 30s after enumeration via one-shot timer.  */
static uint8_t            s_seen_rids[32];
static esp_timer_handle_t s_xchk_timer = NULL;

/* ---- Per-RID arrival interval tracker -------------------------------- */
/* Tracks the EMA inter-report interval for each RID seen in traffic.     */
/* Used to feed status debounce: require status stable for 1.5x interval  */
/* before committing to g_state. Prevents false OL<->OB flicker caused    */
/* by a single anomalous report during power events.                       */
/*                                                                         */
/* EMA formula: ema = (ema * 7 + new_delta) / 8                           */
/* - Stabilises in ~5 reports (~10s for a 2s-interval device)             */
/* - Weights recent samples; tracks slow drift over time                   */
/* Samples capped at 31 - prevents overflow and keeps EMA responsive.     */
static uint32_t s_rid_last_ms[256];   /* last arrival timestamp per RID   */
static uint32_t s_rid_ema_ms[256];    /* EMA interval in ms (0 = no data) */
static uint8_t  s_rid_samples[256];   /* sample count (capped at 31)      */

/* XCHK probe callback - set by ups_usb_hid, routes to ups_get_report.    */
static ups_xchk_probe_fn_t s_xchk_probe_cb = NULL;

/* Forward declaration - defined after ups_hid_parser_set_descriptor */
static void xchk_timer_cb(void *arg);

/* ---- Field cache ----------------------------------------------------- */
typedef struct {
    const hid_field_t *battery_charge;
    const hid_field_t *battery_runtime;
    const hid_field_t *battery_voltage;
    const hid_field_t *charging_flag;
    const hid_field_t *discharging_flag;
    const hid_field_t *low_battery_flag;
    const hid_field_t *fully_charged_flag;
    const hid_field_t *fully_discharged_flag;
    const hid_field_t *need_replacement;
    const hid_field_t *input_frequency;
    const hid_field_t *output_frequency;
    const hid_field_t *ups_load;
    const hid_field_t *ups_temperature;
    const hid_field_t *ac_present;
    bool valid;
} hid_field_cache_t;

static hid_field_cache_t s_cache;
static hid_desc_t        s_desc;

/* ----------------------------------------------------------------------- */

void ups_hid_parser_reset(void)
{
    memset(&s_cache, 0, sizeof(s_cache));
    memset(&s_desc,  0, sizeof(s_desc));
    s_device = NULL;
    memset(s_seen_rids,    0, sizeof(s_seen_rids));
    memset(s_rid_last_ms,  0, sizeof(s_rid_last_ms));
    memset(s_rid_ema_ms,   0, sizeof(s_rid_ema_ms));
    memset(s_rid_samples,  0, sizeof(s_rid_samples));
    if (s_xchk_timer) {
        esp_timer_stop(s_xchk_timer);
        esp_timer_delete(s_xchk_timer);
        s_xchk_timer = NULL;
    }
    /* Note: s_xchk_probe_cb is NOT cleared on reset.
     * The callback is registered once at enumeration and stays valid
     * until ups_hid_parser_set_xchk_probe_cb(NULL) is called on disconnect. */
}

/* ----------------------------------------------------------------------- */

void ups_hid_parser_set_xchk_probe_cb(ups_xchk_probe_fn_t fn)
{
    s_xchk_probe_cb = fn;
}

const hid_desc_t *ups_hid_parser_get_desc(void)
{
    return s_desc.valid ? &s_desc : NULL;
}

/* ----------------------------------------------------------------------- */

void ups_hid_parser_set_descriptor(const hid_desc_t *desc)
{
    if (!desc || !desc->valid) {
        ESP_LOGW(TAG, "set_descriptor: invalid descriptor");
        return;
    }

    ups_hid_parser_reset();
    s_desc = *desc;

    /* ---- Look up device in DB ---- */
    {
        uint16_t vid = 0, pid = 0;
        ups_state_get_vid_pid(&vid, &pid);
        s_device = ups_device_db_lookup(vid, pid);
        ups_device_db_log(s_device, vid, pid);
    }

    /* ---- Apply QUIRK_VENDOR_PAGE_REMAP before building cache ----
       (field scan below uses normalised page numbers)
       Note: remap is applied to the copy in s_desc, not the original.
       usage_page is uint8_t — APC vendor pages 0xFF84/0xFF85 are stored
       as 0x84/0x85 after the descriptor parser already masks the low byte.
       This block is a no-op for current hardware but kept for clarity. */
    if (s_device && (s_device->quirks & QUIRK_VENDOR_PAGE_REMAP)) {
        ESP_LOGI(TAG, "QUIRK_VENDOR_PAGE_REMAP active (APC vendor pages already normalised by descriptor parser)");
    }

    /* ---- Determine if direct-decode is needed ----
       Either the DB says DECODE_CYBERPOWER, OR heuristic: very few Input
       fields on power/battery pages → descriptor is broken, fall back. */
    bool use_direct = (s_device && s_device->decode_mode == DECODE_CYBERPOWER);

    if (!use_direct) {
        uint8_t pd_bs_inputs = 0, other_inputs = 0, rid20 = 0;
        for (uint16_t i = 0; i < s_desc.field_count; i++) {
            const hid_field_t *f = &s_desc.fields[i];
            if (f->report_type != 0) continue;
            if (f->usage_page == HID_PAGE_POWER_DEVICE ||
                f->usage_page == HID_PAGE_BATTERY_SYSTEM) {
                pd_bs_inputs++;
            } else {
                other_inputs++;
            }
            if (f->report_id == 0x20) rid20++;
        }
        if (pd_bs_inputs <= 2 && other_inputs == 0 && rid20 >= 1) {
            use_direct = true;
            ESP_LOGW(TAG, "Heuristic: only %u power/battery Input fields found "
                     "— enabling CyberPower direct-decode as fallback", pd_bs_inputs);
        }
    }

    if (use_direct) {
        ESP_LOGI(TAG, "Decode mode: DIRECT (CyberPower bypass active)");
    } else if (s_device && s_device->decode_mode == DECODE_APC_BACKUPS) {
        ESP_LOGI(TAG, "Decode mode: APC Back-UPS (direct + standard combined)");
    } else if (s_device && s_device->decode_mode == DECODE_APC_SMARTUPS) {
        ESP_LOGI(TAG, "Decode mode: APC Smart-UPS (direct INT-IN + GET_REPORT)");
    } else if (s_device && s_device->decode_mode == DECODE_EATON_MGE) {
        ESP_LOGI(TAG, "Decode mode: EATON/MGE (direct INT-IN undocumented rids)");
    } else {
        ESP_LOGI(TAG, "Decode mode: STANDARD (generic HID descriptor path)");
    }

    /* Store mode back so decode_report can check it */
    /* We abuse s_device — if heuristic forced direct but DB says standard,
       we need a local flag. Use a module-level bool. */
    /* (s_device->decode_mode is const — store separately) */
    /* Simple: just check use_direct via a static flag below */

    /* ---- Build field cache ---- */
    for (uint16_t i = 0; i < s_desc.field_count; i++) {
        const hid_field_t *f = &s_desc.fields[i];
        if (f->report_type != 0) continue;

        uint8_t  pg  = f->usage_page;
        uint16_t uid = f->usage_id;

        /* Battery System usages (page 0x85 or Eaton/MGE vendor page 0xFF).
         * MGE HID protocol uses same usage IDs as standard on vendor page. */
        if (pg == HID_PAGE_BATTERY_SYSTEM || pg == 0xFFu) {
            switch (uid) {
            case HID_USAGE_BS_ABSOLUTESOC:      /* 0x66 RemainingCapacity */
            case HID_USAGE_BS_RELATIVESOC:      /* 0x65 AbsoluteSOC */
            case 0x0064u:                        /* 0x64 RelativeSOC */
            case 0x0067u:                        /* 0x67 RelativeSOC (APC variant) */
                if (!s_cache.battery_charge) s_cache.battery_charge = f;
                break;
            case HID_USAGE_BS_RUNTIMETOEMPTY:   /* 0x68 */
            case 0x0073u:                        /* 0x73 APC non-standard RunTimeToEmpty */
            case 0x0085u:                        /* 0x85 APC Smart-UPS RunTimeToEmpty */
                if (!s_cache.battery_runtime) s_cache.battery_runtime = f;
                break;
            case 0x0083u:
                if (!s_cache.battery_voltage) s_cache.battery_voltage = f;
                break;
            case HID_USAGE_BS_CHARGING:         /* 0x44 */
            case 0x008Bu:                        /* 0x8B APC Smart-UPS charging flag */
                if (!s_cache.charging_flag) s_cache.charging_flag = f;
                break;
            case HID_USAGE_BS_DISCHARGING:      /* 0x45 */
            case 0x002Cu:                        /* 0x2C APC Smart-UPS discharging flag */
                if (!s_cache.discharging_flag) s_cache.discharging_flag = f;
                break;
            case HID_USAGE_BS_BELOWREMCAPLIMIT: /* 0x42 */
                if (!s_cache.low_battery_flag) s_cache.low_battery_flag = f;
                break;
            case HID_USAGE_BS_FULLYCHARGED:     /* 0x46 */
                if (!s_cache.fully_charged_flag) s_cache.fully_charged_flag = f;
                break;
            case HID_USAGE_BS_FULLYDISCHARGED:  /* 0x47 */
                if (!s_cache.fully_discharged_flag) s_cache.fully_discharged_flag = f;
                break;
            case HID_USAGE_BS_NEEDREPLACEMENT:  /* 0x4B */
                if (!s_cache.need_replacement) s_cache.need_replacement = f;
                break;
            case HID_USAGE_BS_ACPRESENT:        /* 0xD0 */
                if (!s_cache.ac_present) s_cache.ac_present = f;
                break;
            default: break;
            }
        }
        /* Power Device usages (page 0x84 or Eaton/MGE vendor page 0xFF).
         * Separate 'if' (not 'else if') so 0xFF fields check both pages.
         * Only overlap is uid=0xD0 (ACPresent) - first-match wins via guards. */
        if (pg == HID_PAGE_POWER_DEVICE || pg == 0xFFu) {
            switch (uid) {
            case HID_USAGE_PD_FREQUENCY:        /* 0x32 */
                if (!s_cache.input_frequency)       s_cache.input_frequency  = f;
                else if (!s_cache.output_frequency) s_cache.output_frequency = f;
                break;
            case HID_USAGE_PD_PERCENTLOAD:      /* 0x35 */
                if (!s_cache.ups_load) s_cache.ups_load = f;
                break;
            case HID_USAGE_PD_TEMPERATURE:      /* 0x36 */
                if (!s_cache.ups_temperature) s_cache.ups_temperature = f;
                break;
            case HID_USAGE_PD_ACPRESENT:        /* 0xD0 */
                if (!s_cache.ac_present) s_cache.ac_present = f;
                break;
            default: break;
            }
        }
    }

    s_cache.valid = true;

    /* Log field cache */
    ESP_LOGI(TAG, "Field cache:");
    ESP_LOGI(TAG, "  battery.charge  : %s (rid=%02X)",
             s_cache.battery_charge  ? "found" : "MISSING",
             s_cache.battery_charge  ? s_cache.battery_charge->report_id  : 0xFF);
    ESP_LOGI(TAG, "  battery.runtime : %s (rid=%02X)",
             s_cache.battery_runtime ? "found" : "MISSING",
             s_cache.battery_runtime ? s_cache.battery_runtime->report_id : 0xFF);
    ESP_LOGI(TAG, "  battery.voltage : %s (rid=%02X)",
             s_cache.battery_voltage ? "found" : "MISSING",
             s_cache.battery_voltage ? s_cache.battery_voltage->report_id : 0xFF);
    ESP_LOGI(TAG, "  ups.load        : %s", s_cache.ups_load        ? "found" : "MISSING");
    ESP_LOGI(TAG, "  ac_present      : %s", s_cache.ac_present      ? "found" : "MISSING");
    ESP_LOGI(TAG, "  charging_flag   : %s", s_cache.charging_flag   ? "found" : "MISSING");
    ESP_LOGI(TAG, "  discharging_flag: %s", s_cache.discharging_flag ? "found" : "MISSING");
    ESP_LOGI(TAG, "  low_battery_flag: %s", s_cache.low_battery_flag ? "found" : "MISSING");
    if (use_direct) ESP_LOGI(TAG, "  [direct-decode ACTIVE]");

    /* ---- Schedule dynamic XCHK after settle window ----
     * Default: 30s for most devices (enough for interrupt-IN traffic to
     * accumulate so cross-check is meaningful).
     * Exception: DECODE_EATON_MGE uses a 5s window.  rid=0x06 is
     * event-driven and may never arrive naturally on stable AC; a shorter
     * settle keeps XCHK useful for Input RIDs that ARE declared, while the
     * bootstrap GET_REPORT probes (queued in ups_usb_hid Step 7b) handle
     * the Feature-report data path immediately on enumeration. */
    uint64_t xchk_delay_us = 30ULL * 1000000ULL;
    if (s_device && s_device->decode_mode == DECODE_EATON_MGE) {
        xchk_delay_us = 5ULL * 1000000ULL;
        ESP_LOGI(TAG, "[XCHK] Eaton/MGE detected — settle timer reduced to 5s");
    }
    esp_timer_create_args_t xchk_ta = {
        .callback        = xchk_timer_cb,
        .arg             = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name            = "hid_xchk"
    };
    if (esp_timer_create(&xchk_ta, &s_xchk_timer) == ESP_OK) {
        esp_timer_start_once(s_xchk_timer, xchk_delay_us);
        ESP_LOGI(TAG, "[XCHK] dynamic RID cross-check scheduled in %"PRIu64"s",
                 xchk_delay_us / 1000000ULL);
    } else {
        ESP_LOGW(TAG, "[XCHK] failed to create settle timer");
    }
}

/* ----------------------------------------------------------------------- */

void ups_hid_parser_run_xchk(void)
{
    if (!s_desc.valid) {
        ESP_LOGW(TAG, "[XCHK] no valid descriptor - skipping");
        return;
    }

    uint16_t seen_count        = 0;
    uint16_t undeclared_count  = 0;
    uint16_t unseen_input_count = 0;

    ESP_LOGI(TAG, "[XCHK] ---- Dynamic RID cross-check (30s settle) ----");

    /* Part 1: every RID seen in interrupt-IN traffic */
    for (uint16_t rid = 0; rid <= 255; rid++) {
        if (!(s_seen_rids[rid >> 3] & (1u << (rid & 7u)))) continue;
        seen_count++;

        /* Is this RID declared as an Input report in the descriptor? */
        bool declared_input = false;
        for (uint8_t di = 0; di < s_desc.report_count; di++) {
            if (s_desc.reports[di].report_id == (uint8_t)rid &&
                s_desc.reports[di].input_bytes > 0) {
                declared_input = true;
                break;
            }
        }
        if (!declared_input) {
            ESP_LOGW(TAG, "[XCHK] rid=0x%02X seen in traffic but NOT declared as Input in descriptor"
                     " (vendor extension or undocumented)", (unsigned)rid);
            undeclared_count++;
        } else {
            ESP_LOGI(TAG, "[XCHK] rid=0x%02X seen in traffic, declared in descriptor - OK", (unsigned)rid);
        }
    }

    /* Part 2: Input RIDs declared in descriptor that never arrived.
     * Queue a one-shot GET_REPORT probe for each via the callback so
     * usb_client_task can issue the control transfer. */
    for (uint8_t di = 0; di < s_desc.report_count; di++) {
        if (s_desc.reports[di].input_bytes == 0) continue;
        uint8_t rid = s_desc.reports[di].report_id;
        bool seen = (s_seen_rids[rid >> 3] & (1u << (rid & 7u))) != 0;
        if (!seen) {
            /* Prefer feature_bytes for the probe wLength; fall back to
             * input_bytes. Clamp to 64 - covers large Feature reports
             * (e.g. CyberPower/PowerWalker rid=0x28 declares 63 bytes).
             * wLength < declared size triggers IDF v5.5.4 DWC assert. */
            uint16_t probe_sz = s_desc.reports[di].feature_bytes > 0u
                              ? s_desc.reports[di].feature_bytes
                              : s_desc.reports[di].input_bytes;
            if (probe_sz == 0u) probe_sz = 8u;
            if (probe_sz > 64u) probe_sz = 64u;

            ESP_LOGI(TAG, "[XCHK] rid=0x%02X declared as Input (%u bytes) but never arrived"
                     " - queuing GET_REPORT probe (wlen=%u)",
                     (unsigned)rid, (unsigned)s_desc.reports[di].input_bytes,
                     (unsigned)probe_sz);

            if (s_xchk_probe_cb) {
                s_xchk_probe_cb(rid, probe_sz);
            } else {
                ESP_LOGW(TAG, "[XCHK] no probe callback registered - skipping rid=0x%02X",
                         (unsigned)rid);
            }
            unseen_input_count++;
        }
    }

    ESP_LOGI(TAG, "[XCHK] Summary: %u RIDs seen, %u undeclared (vendor ext), %u declared-but-silent",
             seen_count, undeclared_count, unseen_input_count);
    ESP_LOGI(TAG, "[XCHK] -----------------------------------------------");
}

static void xchk_timer_cb(void *arg)
{
    (void)arg;
    ups_hid_parser_run_xchk();
    /* One-shot - will not re-fire. Handle kept for cleanup in reset(). */
}

/* ---- Helpers --------------------------------------------------------- */

static bool extract_if_matches(const hid_field_t *field,
                                const uint8_t *data, size_t data_len,
                                uint8_t rid, int32_t *out)
{
    if (!field || field->report_id != rid) return false;
    return ups_hid_desc_extract_field(data, data_len, field, out);
}

/* ---- CyberPower direct-decode ---------------------------------------- */
static bool decode_cyberpower_direct(uint8_t rid,
                                      const uint8_t *p, size_t plen,
                                      ups_state_update_t *upd)
{
    bool changed = false;
    switch (rid) {
    case 0x20:
        if (plen >= 1) {
            uint8_t charge = p[0];
            if (charge <= 100) {
                upd->battery_charge_valid = true;
                upd->battery_charge       = charge;
                changed = true;
                ESP_LOGI(TAG, "[CP] battery.charge=%u%%", charge);
            }
        }
        break;
    case 0x23:
        /* rid=0x23 carried input/output voltage on CyberPower — removed.
         * Voltage decode is not used in current NUT/HA integration.
         * Retained as a no-op case to suppress default: log spam. */
        break;
    case 0x80:
        /*
         * rid=0x80 ac_present — value meaning varies by CyberPower model:
         *   CP550HG:   0x01 = AC present (stays 0x01 even on battery - ignore)
         *              0x00 = AC absent  (trust this)
         *   ST Series: 0x02 = AC present (bit 1, not bit 0)
         *              0x00 = AC absent  (trust this)
         * Bug was: checking (p[0] & 0x01u) treated 0x02 as "absent" (bit 0 clear).
         * Fix: only trust AC ABSENT when value is exactly 0x00.
         * Any non-zero value: do not set - rid=0x29 is the authoritative
         * source for on-battery state and must not be overwritten.
         */
        if (plen >= 1) {
            bool ac = (p[0] != 0x00u);
            if (!ac) {
                /* AC definitely lost - value is exactly 0x00 */
                upd->input_utility_present_valid = true;
                upd->input_utility_present       = false;
                changed = true;
            }
            /* non-zero: do not set - rid=0x29 owns this decision */
            ESP_LOGI(TAG, "[CP] ac_present=%u%s", (unsigned)ac,
                     ac ? " (ignored, rid=0x29 authoritative)" : " -> AC LOST");
        }
        break;
    case 0x21:
        /*
         * rid=0x21 = RunTimeToEmpty (16-bit little-endian, seconds).
         * Confirmed from discharge logs: counts down correctly during OB.
         *   0x0D84 = 3460s (~57 min at start of discharge)
         *   0x0A79 = 2681s (~44 min later)
         * This is the authoritative runtime source for CyberPower PID 0501.
         */
        if (plen >= 2) {
            uint16_t runtime_s = (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
            if (runtime_s > 0 && runtime_s < 65000u) {
                upd->battery_runtime_valid = true;
                upd->battery_runtime_s     = runtime_s;
                changed = true;
                ESP_LOGI(TAG, "[CP] battery.runtime=%us (rid=0x21)", runtime_s);
            }
        }
        break;
    case 0x82:
        /*
         * rid=0x82 = RemainingTimeLimit (battery.runtime.low threshold).
         * Value is STATIC at 300s (5m) — this is NOT current runtime.
         * Confirmed from discharge logs: never changes during OB.
         * Do NOT use for battery_runtime — silently ignore.
         */
        break;
    case 0x88:
        if (plen >= 2) {
            uint16_t raw = (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
            uint32_t mv  = (uint32_t)raw * 100u;
            if (mv >= 8000u && mv <= 20000u) {
                upd->battery_voltage_valid = true;
                upd->battery_voltage_mv    = mv;
                changed = true;
                ESP_LOGI(TAG, "[CP] battery.voltage=%"PRIu32"mV (raw=%u×100mV)", mv, raw);
            }
        }
        break;
    case 0x85:
        if (plen >= 1 && p[0] != 0x00u) {
            uint32_t flags = 0;
            if (p[0] & 0x01u) flags |= 0x01u;
            if (p[0] & 0x02u) flags |= 0x02u;
            if (p[0] & 0x04u) flags |= 0x04u;
            upd->ups_flags_valid = true;
            upd->ups_flags      |= flags;
            changed = true;
            ESP_LOGI(TAG, "[CP] flags_byte=0x%02X", p[0]);
        }
        break;
    case 0x29:
        /*
         * CyberPower battery/AC status byte.
         * Observed values:
         *   0x00 = on-line (AC present, not discharging)
         *   0x03 = on-battery (discharging — AC lost)
         * This is more reliable than rid=0x80 (ac_present) which
         * stays 0x01 even during discharge on CP550HG.
         * bit0 = discharging, bit1 = low-battery (observed)
         */
        if (plen >= 1) {
            upd->input_utility_present_valid = true;
            upd->input_utility_present       = (p[0] == 0x00u);
            /*
             * 0x03 = on-battery normal discharge (bit0=on-batt, bit1=discharging)
             * 0x02 = low battery observed on other CyberPower models
             * bit1 alone does NOT mean low battery on CP550HG — it
             * appears set whenever discharging. Only flag LB if
             * rid=0x85 or a dedicated low-batt bit indicates it.
             * Leave ups_flags alone here; rid=0x85 handles LB.
             */
            changed = true;
            ESP_LOGI(TAG, "[CP] status_byte=0x%02X -> %s",
                     p[0], (p[0] == 0x00u) ? "OL" : "OB");
        }
        break;
    case 0x0B:
        /*
         * rid=0x0B - observed on CyberPower 3000R (0764:0601).
         * 1 byte, value 0x13=19 seen on AC (OL) at steady state.
         * Usage/meaning unknown. Fires every 2s alongside rid=0x08.
         * Logged for analysis - need discharge event to identify meaning.
         */
        if (plen >= 1) {
            ESP_LOGI(TAG, "[CP] rid=0x0B raw=0x%02X (%u) - diagnostic only",
                     (unsigned)p[0], (unsigned)p[0]);
        }
        break;
    /* Unresolved reports - silently ignore */
    case 0x22: case 0x25: case 0x28:
    case 0x86: case 0x87:
        break;
    default:
        break;
    }
    return changed;
}

/* ---- APC Back-UPS direct-decode ------------------------------------- */
/*
 * APC Back-UPS (PID 0x0002) live rid map (confirmed from XS 1500M and BR1000G):
 *
 *   rid=06  [3 bytes]  byte0=Charging flag, byte1=Discharging flag,
 *                       byte2=status flags (bit3 observed set at init)
 *                       — handled by standard descriptor path (charging_flag,
 *                         discharging_flag found at rid=06 in both models) ✅
 *   rid=0C  [3 bytes]  byte0 = battery_charge (0–100%) ✅
 *                       byte1:2 = uint16_le = runtime in seconds ✅
 *                         XS 1500M: 0xA0 0x8C → 0x8CA0 = 35,744s (~9.9h)
 *                         BR1000G:  0x60 0x05 → 0x0560 = 1,376s (~23 min)
 *                         (fluctuates as UPS recalculates remaining runtime)
 *   rid=13  [1 byte]   unknown (0x01 observed)
 *   rid=14  [2 bytes]  unknown
 *   rid=16  [4 bytes]  byte0:1 = uint16_le (0x000C=12) — unidentified config
 *   rid=21  [1 byte]   unknown (0x06 observed)
 *
 * VOLTAGES / BATTERY VOLTAGE:
 *   Not present as Input reports in the HID descriptor — likely only
 *   accessible via GET_REPORT (Feature type).  QUIRK_NEEDS_GET_REPORT
 *   is already set in the DB entry; GET_REPORT polling is a future task.
 *
 * STATUS: from rid=06 via standard path (charging/discharging flags) ✅
 * CHARGE: from rid=0C byte0 direct-decode ✅
 * RUNTIME: from rid=0C bytes 1-2 uint16_le direct-decode ✅
 */
static bool decode_apc_backups_direct(uint8_t rid,
                                       const uint8_t *p, size_t plen,
                                       ups_state_update_t *upd)
{
    bool changed = false;
    switch (rid) {
    case 0x0C:
        /*
         * byte0   = battery charge (0–100%)
         * byte1:2 = runtime remaining in seconds (uint16 little-endian)
         * Confirmed on XS 1500M and BR1000G.
         */
        if (plen >= 1) {
            uint8_t charge = p[0];
            if (charge <= 100) {
                upd->battery_charge_valid = true;
                upd->battery_charge       = charge;
                changed = true;
                ESP_LOGI(TAG, "[APC] battery.charge=%u%%", charge);
            }
        }
        if (plen >= 3) {
            uint16_t runtime_s = (uint16_t)(p[1] | ((uint16_t)p[2] << 8));
            /* Sanity: 0 = unknown/not-calculated, cap at ~24h */
            if (runtime_s != 0) {  /* uint16_t max 65535s — no upper cap needed */
                upd->battery_runtime_valid = true;
                upd->battery_runtime_s     = runtime_s;
                changed = true;
                ESP_LOGI(TAG, "[APC] battery.runtime=%us", runtime_s);
            }
        }
        break;
    /* Unresolved reports — silently ignore */
    case 0x13: case 0x14: case 0x16: case 0x21:
        break;
    default:
        break;
    }
    return changed;
}

/* ---- APC Smart-UPS direct-decode ------------------------------------ */
/*
 * APC Smart-UPS C / Smart-UPS (PID 0x0003) live interrupt-IN rid map.
 * Confirmed from issue #1 (Smart-UPS C 1500, v15.13 log).
 *
 * All rids below are NOT declared in the HID descriptor - they arrive
 * as undocumented interrupt-IN reports alongside the descriptor-declared
 * rid=0x0C charge report.
 *
 * rid=0x0D  byte[0:1] = uint16 LE = battery.runtime seconds
 *           Confirmed: 0x1194=4500s, 0x120C=4620s at 100pct charge.
 *           Value oscillates as UPS recalculates remaining runtime.
 *
 * rid=0x07  byte[0] = status flags (confirmed from issue #1 discharge log)
 *           0x0C (on AC):      bit2=1 AC present, bit1=0
 *           0x0A (on battery): bit2=0 AC absent,  bit1=1 discharging
 *           bit3 (0x08) always set - ignore. bit2=AC present, bit1=discharging.
 *
 * rid=0x0C  charge - handled by standard descriptor path (type=0 Input).
 */
static bool decode_apc_smartups_direct(uint8_t rid,
                                        const uint8_t *p, size_t plen,
                                        ups_state_update_t *upd)
{
    bool changed = false;
    switch (rid) {
    case 0x0D:
        if (plen >= 2) {
            uint16_t runtime_s = (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
            if (runtime_s > 0 && runtime_s < 65000u) {
                upd->battery_runtime_valid = true;
                upd->battery_runtime_s     = runtime_s;
                changed = true;
                ESP_LOGI(TAG, "[SMRT] battery.runtime=%us (rid=0x0D)", runtime_s);
            }
        }
        break;
    case 0x07:
        /*
         * Status flags byte - confirmed from issue #1 on-battery log.
         *
         * Observed values:
         *   0x0C (0000 1100) = on AC power       (bit3 + bit2)
         *   0x0A (0000 1010) = on battery/discharging (bit3 + bit1)
         *
         * Bit map:
         *   bit3 (0x08) = always set, purpose unknown - ignore
         *   bit2 (0x04) = AC present (1=AC, 0=on battery)
         *   bit1 (0x02) = discharging (1=discharging, 0=not discharging)
         *   bit0 (0x01) = always clear in observed data - ignore
         *
         * rid=0x07 alone provides full OL/OB status - GET_REPORT rid=0x06
         * charging/discharging flags are redundant for status but kept
         * for completeness (still used for CHRG flag detection).
         */
        if (plen >= 1) {
            bool ac_present  = (p[0] & 0x04u) != 0u;
            bool discharging = (p[0] & 0x02u) != 0u;
            upd->input_utility_present_valid = true;
            upd->input_utility_present       = ac_present;
            if (discharging) upd->ups_flags |= 0x02u;
            upd->ups_flags_valid = true;
            changed = true;
            ESP_LOGI(TAG, "[SMRT] rid=0x07 flags=0x%02X ac=%u discharging=%u",
                     p[0], (unsigned)ac_present, (unsigned)discharging);
        }
        break;
    case 0x0C:
        /* Charge handled by standard descriptor path - no action here */
        break;
    default:
        break;
    }
    return changed;
}

/* ---- derive_status --------------------------------------------------- */
/*
 * Builds NUT compound status string into upd->ups_status.
 *
 * KEY BEHAVIOUR (v15.4 fix):
 *   - If we cannot determine on/off-battery status from THIS report alone,
 *     we leave upd->ups_status[0] = 0 (empty).
 *   - ups_state_apply_update checks upd->ups_status[0] before writing —
 *     empty means "no opinion this cycle, keep existing g_state.ups_status".
 *   - "UNKNOWN" is only written when we have enough data to conclude that
 *     the UPS state is genuinely unknown (utility unknown AND no charge data).
 *
 * This fixes the bug where rid=0x88 (battery voltage) arrived, changed=true,
 * but had no utility/charge flags → derive_status wrote "UNKNOWN" over the
 * valid "OL" already stored in g_state from the previous rid=0x80 cycle.
 */
static void derive_status(ups_state_update_t *upd)
{
    if (!upd || !upd->valid) {
        /* No data at all — leave empty, do not overwrite g_state */
        return;
    }

    bool on_battery    = false;
    bool utility_known = false;
    bool charging      = false;
    bool discharging   = false;
    bool low_battery   = false;

    if (upd->input_utility_present_valid) {
        on_battery    = !upd->input_utility_present;
        utility_known = true;
    }

    if (upd->ups_flags_valid) {
        charging    = (upd->ups_flags & 0x01u) != 0u;
        discharging = (upd->ups_flags & 0x02u) != 0u;
        low_battery = (upd->ups_flags & 0x04u) != 0u;
        if (!utility_known && discharging) { on_battery = true;  utility_known = true; }
        if (!utility_known && charging)    { on_battery = false; utility_known = true; }
    }

    if (!utility_known) {
        if (upd->battery_charge_valid) {
            /* Charge data only — conservatively assume on-line.
             * EXCEPTION: DECODE_APC_SMARTUPS has rid=0x07 as authoritative
             * status source. Charge-only reports (rid=0x0C, rid=0x0D) must
             * not overwrite OB DISCHRG with OL. Leave empty — no opinion. */
            ups_decode_mode_t mode = s_device ? s_device->decode_mode : DECODE_STANDARD;
            if (mode != DECODE_APC_SMARTUPS) {
                strlcpy(upd->ups_status, "OL", sizeof(upd->ups_status));
            }
        }
        /* else: no opinion — leave ups_status empty, g_state unchanged */
        return;
    }

    char buf[16];
    if (on_battery) {
        if (low_battery) strlcpy(buf, "OB DISCHRG LB", sizeof(buf));
        else             strlcpy(buf, "OB DISCHRG",    sizeof(buf));
    } else {
        if (low_battery)   strlcpy(buf, "OL LB",   sizeof(buf));
        else if (charging) strlcpy(buf, "OL CHRG", sizeof(buf));
        else               strlcpy(buf, "OL",       sizeof(buf));
    }
    strlcpy(upd->ups_status, buf, sizeof(upd->ups_status));
    ESP_LOGI(TAG, "derive_status -> '%s' (on_battery=%d utility_known=%d charging=%d)",
             buf, (int)on_battery, (int)utility_known, (int)charging);
}

/* ---- Per-RID interval accessor --------------------------------------- */

uint32_t ups_hid_parser_get_rid_interval(uint8_t rid, uint8_t *samples_out)
{
    if (samples_out) *samples_out = s_rid_samples[rid];
    return s_rid_ema_ms[rid];
}

/* ---- Main decode entry point ----------------------------------------- */

bool ups_hid_parser_decode_report(const uint8_t *data, size_t len,
                                   ups_state_update_t *upd)
{
    if (!data || len == 0 || !upd) return false;
    memset(upd, 0, sizeof(*upd));

    if (!s_cache.valid) {
        ESP_LOGW(TAG, "No descriptor loaded");
        return false;
    }

    uint8_t        rid;
    const uint8_t *payload;
    size_t         payload_len;

    if (s_desc.has_report_ids) {
        rid         = data[0];
        payload     = data + 1;
        payload_len = len - 1;
    } else {
        rid         = 0;
        payload     = data;
        payload_len = len;
    }

    /* Track which RIDs we see in actual traffic (Phase 4 dynamic XCHK) */
    s_seen_rids[rid >> 3] |= (1u << (rid & 7u));

    /* Update per-RID arrival interval tracker */
    {
        uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);
        if (s_rid_last_ms[rid] != 0) {
            uint32_t delta = now_ms - s_rid_last_ms[rid];
            /* Accept intervals 10ms - 60s; ignore bursts and long silences */
            if (delta >= 10 && delta <= 60000) {
                if (s_rid_samples[rid] == 0) {
                    s_rid_ema_ms[rid] = delta;
                } else {
                    /* EMA: 7/8 old + 1/8 new - stable after ~5 samples */
                    s_rid_ema_ms[rid] = (s_rid_ema_ms[rid] * 7u + delta) / 8u;
                }
                if (s_rid_samples[rid] < 31) s_rid_samples[rid]++;
            }
        }
        s_rid_last_ms[rid] = now_ms;
    }

    bool    changed = false;
    int32_t raw     = 0;

    /* Determine decode mode from DB entry. */
    ups_decode_mode_t mode = s_device ? s_device->decode_mode : DECODE_STANDARD;

    /* Heuristic override: if standard but cache is very sparse (no voltage,
       no runtime, no ac_present) treat as CyberPower-style. */
    if (mode == DECODE_STANDARD &&
        !s_cache.ac_present && !s_cache.battery_runtime && !s_cache.battery_charge) {
        mode = DECODE_CYBERPOWER;
        ESP_LOGD(TAG, "Heuristic: sparse cache — treating as DECODE_CYBERPOWER");
    }

    if (mode == DECODE_CYBERPOWER) {
        if (decode_cyberpower_direct(rid, payload, payload_len, upd)) {
            changed = true;
            goto finalize;  /* CP direct decode consumed this RID - skip standard path */
        }
        /* RID not recognized by CP direct path - fall through to standard decode.
         * CyberPower 3000R (0764:0601) sends battery.charge on rid=0x08 which is
         * a standard HID RID handled by the field cache below. */
    } else if (mode == DECODE_APC_BACKUPS) {
        /* APC Back-UPS: direct for vendor rids, then fall through to
           standard path to pick up charging/discharging from descriptor. */
        if (decode_apc_backups_direct(rid, payload, payload_len, upd)) {
            changed = true;
        }
        /* Always fall through to standard path for descriptor fields. */
    } else if (mode == DECODE_APC_SMARTUPS) {
        /* APC Smart-UPS: direct for undocumented interrupt-IN rids,
           then fall through to standard path for descriptor charge field. */
        if (decode_apc_smartups_direct(rid, payload, payload_len, upd)) {
            changed = true;
        }
        /* Fall through - standard path picks up rid=0x0C charge (Input). */
    } else if (mode == DECODE_EATON_MGE) {
        /* Eaton/MGE: undocumented interrupt-IN rids carry all live UPS state.
         * Standard descriptor path yields nothing (all fields MISSING).
         * GET_REPORT supplies battery.charge via rid=0x20 as fallback/initial value
         * (handled in ups_get_report.c). Interrupt-IN is the primary real-time path.
         *
         * Confirmed interrupt-IN rids from Eaton 3S 700 (2026-04-02):
         *   rid=0x06  [6 bytes]  State change notification. Fires on mains events.
         *     data[0] = 0x06 (rid)
         *     data[1] = battery.charge (0-100%)
         *     data[2:3] = battery.runtime_s uint16 LE (seconds)
         *     data[4:5] = flags (always 0x0000 in all submissions; non-zero = OB)
         *
         * Sample: 06 63 B4 10 00 00 (from submission 2026-04-02):
         *   charge=0x63=99%, runtime=0x10B4=4276s (~71 min), flags=0x0000
         */
        if (rid == 0x06 && payload_len >= 5) {
            uint8_t  charge    = payload[0];
            uint16_t runtime_s = (uint16_t)(payload[1] | ((uint16_t)payload[2] << 8));
            uint16_t flags     = (uint16_t)(payload[3] | ((uint16_t)payload[4] << 8));
            if (charge <= 100u) {
                upd->battery_charge_valid = true;
                upd->battery_charge       = charge;
                changed = true;
                ESP_LOGI(TAG, "[EATON] rid=0x06 battery.charge=%u%%", charge);
            }
            if (runtime_s > 0u) {
                upd->battery_runtime_valid = true;
                upd->battery_runtime_s     = runtime_s;
                changed = true;
                ESP_LOGI(TAG, "[EATON] rid=0x06 battery.runtime=%us", runtime_s);
            }
            /* Flags decode: 0x0000 observed in ALL submissions (including
             * events labeled as mains-loss). Do NOT assert OL from 0x0000
             * - the flags field does not reliably indicate AC state.
             * OL/OB now comes from the standard field cache (ACPresent,
             * Charging, Discharging on vendor page 0xFFFF) or from the
             * conservative charge-data default in derive_status().
             * Only trust non-zero flags as an OB indicator (a genuine
             * change from the always-zero baseline). */
            if (flags != 0x0000u) {
                upd->input_utility_present_valid = true;
                upd->input_utility_present       = false;  /* OB */
                changed = true;
                ESP_LOGW(TAG, "[EATON] rid=0x06 flags=0x%04X -> OB (ac_absent)",
                         (unsigned)flags);
            } else {
                ESP_LOGI(TAG, "[EATON] rid=0x06 flags=0x0000 (no status assertion)");
            }
            ESP_LOGI(TAG, "[EATON] rid=0x06 raw: %02X %02X %02X %02X %02X",
                     payload[0], payload[1], payload[2], payload[3], payload[4]);
            goto finalize;  /* v0.29: always skip standard path for Eaton rids (vendor-format payloads) */
        }
        /* rid=0x21: tentative steady-state heartbeat — same byte layout as rid=0x06.
         * rid=0x06 fires on mains events only; rid=0x21 appears in the interrupt-IN
         * stream during the first 30s of normal steady-state operation (confirmed from
         * three Eaton 3S 700 submissions, 2026-03-30/04-02 in ups_db_eaton.c).
         * Treating it as a periodic equivalent of rid=0x06. The sanity guards
         * (charge <= 100, runtime > 0) protect against a wrong format assumption. */
        else if (rid == 0x21 && payload_len >= 5) {
            uint8_t  charge    = payload[0];
            uint16_t runtime_s = (uint16_t)(payload[1] | ((uint16_t)payload[2] << 8));
            uint16_t flags     = (uint16_t)(payload[3] | ((uint16_t)payload[4] << 8));

            ESP_LOGI(TAG, "[EATON] rid=0x21 raw: %02X %02X %02X %02X %02X",
                     payload[0], payload[1], payload[2], payload[3], payload[4]);

            if (charge <= 100u) {
                upd->battery_charge_valid = true;
                upd->battery_charge       = charge;
                changed = true;
                ESP_LOGI(TAG, "[EATON] rid=0x21 battery.charge=%u%%", charge);
            } else {
                ESP_LOGW(TAG, "[EATON] rid=0x21 charge=0x%02X out of range - format mismatch?",
                         charge);
            }
            if (runtime_s > 0u) {
                upd->battery_runtime_valid = true;
                upd->battery_runtime_s     = runtime_s;
                changed = true;
                ESP_LOGI(TAG, "[EATON] rid=0x21 battery.runtime=%us", runtime_s);
            }
            /* Same logic as rid=0x06: only trust non-zero flags for OB.
             * OL comes from standard field cache or charge-data default. */
            if (flags != 0x0000u) {
                upd->input_utility_present_valid = true;
                upd->input_utility_present       = false;  /* OB */
                changed = true;
                ESP_LOGW(TAG, "[EATON] rid=0x21 flags=0x%04X -> OB (ac_absent)",
                         (unsigned)flags);
            } else {
                ESP_LOGI(TAG, "[EATON] rid=0x21 flags=0x0000 (no status assertion)");
            }
            goto finalize;  /* v0.29: always skip standard path for Eaton rids (vendor-format payloads) */
        }

        /* Log all other unrecognised Eaton interrupt-IN rids.
         * 0x2x range = steady-state UPS data. 0x8x range = alarm/event rids:
         * these are the most likely source of OL->OB state change notifications
         * on mains loss. Log ALL bytes (up to 16) at WARN for 0x8x so they
         * remain visible in discharge-event captures even at reduced log levels.
         * Fall through to standard path after logging (finds nothing, harmless). */
        else if (payload_len > 0) {
            char hexbuf[64] = {0};
            int  hpos = 0;
            size_t hn = (payload_len > 16u) ? 16u : payload_len;
            for (size_t hi = 0; hi < hn; hi++) {
                hpos += snprintf(hexbuf + hpos, sizeof(hexbuf) - (size_t)hpos,
                                 "%02X%s", payload[hi], (hi == hn - 1u) ? "" : " ");
            }
            if (rid >= 0x80u && rid <= 0x8Fu) {
                ESP_LOGW(TAG, "[EATON] rid=0x%02X ALARM/EVENT (%u bytes): %s%s",
                         (unsigned)rid, (unsigned)payload_len, hexbuf,
                         payload_len > 16u ? "..." : "");
            } else {
                ESP_LOGI(TAG, "[EATON] rid=0x%02X unrecognised (%u bytes): %s%s",
                         (unsigned)rid, (unsigned)payload_len, hexbuf,
                         payload_len > 16u ? "..." : "");
            }
        }

        /* v0.28: skip standard path for all Eaton rids - vendor page 0xFFFF
         * fields in the field cache cause descriptor bit-extraction to run on
         * vendor-format payloads, corrupting correctly decoded values. */
        goto finalize;
    }

    /* ---- Standard descriptor path ---- */

    /* Battery Charge */
    if (!upd->battery_charge_valid &&
        extract_if_matches(s_cache.battery_charge, payload, payload_len, rid, &raw)) {
        if (raw >= 0 && raw <= 100) {
            upd->battery_charge_valid = true;
            upd->battery_charge       = (uint8_t)raw;
            changed = true;
            ESP_LOGI(TAG, "battery.charge=%"PRId32"%%", raw);
        }
    }

    /* Battery Runtime */
    if (!upd->battery_runtime_valid &&
        extract_if_matches(s_cache.battery_runtime, payload, payload_len, rid, &raw)) {
        int8_t  exp     = s_cache.battery_runtime->unit_exponent;
        int32_t seconds = raw;
        if (exp != 0) {
            int64_t r = (int64_t)raw;
            if (exp > 0) for (int i = 0; i < exp  && i < 9; i++) r *= 10;
            else         for (int i = 0; i < -exp && i < 9; i++) r /= 10;
            seconds = (int32_t)r;
        }
        if (seconds >= 0) {
            upd->battery_runtime_valid = true;
            upd->battery_runtime_s     = (uint32_t)seconds;
            changed = true;
            ESP_LOGI(TAG, "battery.runtime=%"PRId32"s", seconds);
        }
    }

    /* Battery Voltage */
    if (!upd->battery_voltage_valid &&
        extract_if_matches(s_cache.battery_voltage, payload, payload_len, rid, &raw)) {
        int32_t mv = 0;
        if (ups_hid_desc_to_milli(raw, s_cache.battery_voltage->unit_exponent, &mv)
            && mv > 0 && mv < 100000) {
            upd->battery_voltage_valid = true;
            upd->battery_voltage_mv    = (uint32_t)mv;
            changed = true;
        } else if (raw > 0 && raw < 100000) {
            upd->battery_voltage_valid = true;
            upd->battery_voltage_mv    = (uint32_t)raw;
            changed = true;
        }
    }

    /* UPS Load */
    if (extract_if_matches(s_cache.ups_load, payload, payload_len, rid, &raw)) {
        if (raw >= 0 && raw <= 100) {
            upd->ups_load_valid = true;
            upd->ups_load_pct   = (uint8_t)raw;
            changed = true;
        }
    }

    /* AC Present */
    if (!upd->input_utility_present_valid &&
        extract_if_matches(s_cache.ac_present, payload, payload_len, rid, &raw)) {
        upd->input_utility_present_valid = true;
        upd->input_utility_present       = (raw != 0);
        changed = true;
    }

    /* Status Flags */
    {
        uint32_t flags   = 0;
        bool     any_flag = false;
        int32_t  fraw    = 0;
        if (extract_if_matches(s_cache.charging_flag,      payload, payload_len, rid, &fraw)) { if (fraw) flags |= 0x01u; any_flag = true; }
        if (extract_if_matches(s_cache.discharging_flag,   payload, payload_len, rid, &fraw)) { if (fraw) flags |= 0x02u; any_flag = true; }
        if (extract_if_matches(s_cache.low_battery_flag,   payload, payload_len, rid, &fraw)) { if (fraw) flags |= 0x04u; any_flag = true; }
        if (extract_if_matches(s_cache.fully_charged_flag, payload, payload_len, rid, &fraw)) { if (fraw) flags |= 0x08u; any_flag = true; }
        if (extract_if_matches(s_cache.need_replacement,   payload, payload_len, rid, &fraw)) { if (fraw) flags |= 0x10u; any_flag = true; }
        if (any_flag) { upd->ups_flags_valid = true; upd->ups_flags = flags; changed = true; }
    }

finalize:
    if (changed) {
        upd->valid      = true;
        upd->source_rid = rid;

        /* Compute status debounce threshold from learned RID interval.
         * Threshold = 1.5x EMA interval, capped at 3500ms.
         * Requires 3+ samples so debounce is disabled during the first
         * ~10 seconds of operation (warmup). 0 = apply immediately. */
        if (s_rid_samples[rid] >= 3 && s_rid_ema_ms[rid] > 0) {
            uint32_t thresh = (s_rid_ema_ms[rid] * 3u) / 2u;
            upd->status_debounce_ms = (thresh < 3500u) ? thresh : 3500u;
        } else {
            upd->status_debounce_ms = 0;
        }

        derive_status(upd);
    }

    return changed;
}
