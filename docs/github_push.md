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
v0.27

## Commit Message
v0.27 - Eaton OL/OB fix: vendor page 0xFFFF, pre-seed OL, OB-only flags

Root cause: Eaton 3S HID descriptor has 111 fields ALL on vendor page
0xFFFF. Parser only kept pages 0x84/0x85/0xFF84/0xFF85 - all Eaton
fields were discarded. Standard field cache (ac_present, charging_flag,
discharging_flag) was always empty - no standard-path OL/OB source.

flags=0x0000 in rid=0x06/0x21 was observed in ALL submissions including
events labeled as mains-loss. Asserting OL from flags=0x0000 was false
certainty. Eaton also had a 20-30s boot window where status was empty
and NUT returned UNKNOWN (rid=0x21 heartbeat takes that long to arrive).

Fixes:
- ups_hid_desc.c: include page 0xFFFF in interesting filter so Eaton
  fields reach the field table
- ups_hid_parser.c: add page 0xFF to field cache scan (BS and PD usage
  IDs). MGE HID uses same usage IDs as standard pages.
- ups_hid_parser.c / ups_get_report.c: demote flags-based OL. Only
  non-zero flags trigger OB assertion. OL comes from field cache or
  conservative charge-data default in derive_status().
- ups_get_report.c: add rid=0x85 handler (OB status probe, log raw bytes)
- ups_usb_hid.c: pre-seed OL at enumeration (eliminates UNKNOWN window).
  Add rid=0x85 to bootstrap probe queue.

## Files Staged
- src/current/main/ups_hid_desc.c
- src/current/main/ups_hid_parser.c
- src/current/main/ups_get_report.c
- src/current/main/ups_usb_hid.c
- src/current/main/ups_db_eaton.c
- docs/github_push.md
- docs/next_steps.md
