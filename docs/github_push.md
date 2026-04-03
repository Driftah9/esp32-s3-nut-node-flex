# GitHub Push - esp32-s3-nut-node-flex

> Claude updates this file whenever code changes during a session.
> Keep this current - it is the single source of truth for every push.

---

## Project
esp32-s3-nut-node-flex

## Repo
https://github.com/Driftah9/esp32-s3-nut-node-flex

## Visibility
public

## Branch
main

## Version
v0.8

## Commit Message
v0.8 - esp32-s3-nut-node-flex - Phase 4 complete: NUT mapping table evaluation

Session 008: NUT mge-hid.c mapping table format ported to ESP annotation layer

ups_hid_map.h (NEW):
- ups_hid_map_lookup(usage_page, usage_id) - returns nut_var string or NULL
- ups_hid_map_annotate_report(desc, rid, payload, plen, tag) - logs NUT names per field

ups_hid_map.c (NEW):
- hid_nut_entry_t struct: usage_page, usage_id, nut_var
- Static table ~50 entries covering HID pages 0x84 Power Device and 0x85 Battery System
- Page 0x84: ups.input.voltage, ups.output.voltage, ups.output.current,
  ups.input.frequency, ups.load, ups.temperature, ups.input.voltage.nominal,
  input.transfer.low/high, ups.delay.shutdown/start, AVR boost/buck, overload, utility present
- Page 0x85: battery.charge (multiple usages incl APC variants), battery.runtime,
  battery.voltage, charging/discharging/fully-charged/fully-discharged flags,
  battery.charge.low, battery.replace, battery.cycle.count, input.utility.present
- Vendor pages (>=0xFD00) normalized for lookup
- Design: annotation layer only, does not replace working direct-decode paths

ups_hid_desc.c:
- ups_hid_desc_dump() updated: appends -> nut_var_name or -> unmapped per field
- Added include ups_hid_map.h

ups_get_report.c:
- service_probe_queue(): calls ups_hid_map_annotate_report() after probe response
- Added includes ups_hid_map.h and ups_hid_parser.h

ups_hid_parser.h R7:
- ups_hid_parser_get_desc() accessor declaration added
- Returns const hid_desc_t ptr for probe-path annotation use

ups_hid_parser.c R7:
- ups_hid_parser_get_desc() implemented: returns &s_desc if valid, else NULL

CMakeLists.txt:
- ups_hid_map.c added to SRCS list

docs/DECISIONS.md:
- D004 updated: Implemented and confirmed (v0.6-v0.7) with full XCHK + probe results
- D005 added: NUT mge-hid.c mapping table evaluation - portability verdict, CyberPower result,
  annotation-only approach rationale

Hardware verification (CyberPower ST Series VID:0764 PID:0501):
- All 14 descriptor fields show unmapped (all vendor usage IDs 0x008C-0x00FE)
- Confirms direct-decode requirement for CyberPower
- XCHK: 0 declared-but-silent Input RIDs, 11 undeclared vendor extensions
- System stable, no USB errors, no watchdog triggers

Build: clean, zero warnings, zero errors (ESP-IDF v5.3.1)

## Files Staged
- src/current/main/ups_hid_map.h
- src/current/main/ups_hid_map.c
- src/current/main/ups_hid_desc.c
- src/current/main/ups_get_report.c
- src/current/main/ups_hid_parser.h
- src/current/main/ups_hid_parser.c
- src/current/main/CMakeLists.txt
- docs/DECISIONS.md
- docs/github_push.md
- docs/project_state.md
- docs/next_steps.md
