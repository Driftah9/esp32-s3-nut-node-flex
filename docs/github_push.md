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
v0.16

## Commit Message
v0.16 - Fix DECODE_CYBERPOWER goto silently discarding standard HID RIDs

Root cause: ups_hid_parse_report() had "if (rid != 0x20) goto finalize"
after calling decode_cyberpower_direct(). This bypassed the standard
field-cache path for ALL RIDs except 0x20, even when direct decode
returned false (RID unrecognized by CP path).

CyberPower 3000R (0764:0601) sends battery.charge on rid=0x08 - a
standard HID RID present in the field cache. Field found at parse
time but silently discarded at decode time due to premature goto.
Fix: goto finalize only when direct decode returns true. Unknown RIDs
fall through to standard field-cache path.

Also adds rid=0x0B diagnostic logging (3000R sends 1 byte every 2s,
value 0x13=19 observed on AC - meaning TBD, need discharge event).

ups_db_cyberpower.c: PID 0601 corrected. Not same decode path as
PID 0501. Uses standard HID RIDs (0x08, 0x0B) not vendor RIDs.
known_good=false pending re-submission confirmation.

Bundles v0.15 Eaton decode corrections:
- ups_get_report.c: Eaton rid=0x20 changed from state-apply to log-only
  (3-submission analysis confirmed: returns 2% on full batteries)
- ups_hid_parser.c: Eaton rid=0x06 flags[3:4] decode added
  (flags=0x0000 -> OL / input_utility_present=true)
- ups_db_eaton.c: corrected comments, rid=0x06 documented as primary source
- diag_capture.c: inject app+IDF version at arm time (was missing before)
- CMakeLists.txt: project renamed to esp32-s3-nut-node-flex
- git-push.ps1: Discord push notification added

## Files Staged
- src/current/CMakeLists.txt
- src/current/main/diag_capture.c
- src/current/main/ups_db_eaton.c
- src/current/main/ups_get_report.c
- src/current/main/ups_hid_parser.c
- src/current/main/ups_db_cyberpower.c
- git-push.ps1
- docs/github_push.md
- docs/project_state.md
- docs/next_steps.md
- docs/session_log.md
