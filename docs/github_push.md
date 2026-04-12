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
v0.33

## Status
pending

## Last Push
Commit: 53f078a
Tag: v0.32
Message: v0.32 - Feature-fallback field cache; PowerWalker GET_REPORT quirk
Date: 2026-04-11

## Previous Push
Commit: 3dc222e
Tag: v0.30
Message: v0.30 - Fix INT-IN buffer truncation; add PowerWalker; Eaton rid=0x06 polling

## Commit Message
v0.33 - DECODE_VOLTRONIC: dedicated decode path for PowerWalker/Voltronic
- ups_device_db.h: add DECODE_VOLTRONIC (mode 5) for dual-protocol HID+QS devices
- ups_hid_parser.c (R19): decode_voltronic_direct() - rid=0x32 status (byte[3] bit4=ACPresent), rid=0x35 input voltage (uint16/10 V AC). Confirmed from user Linux testing.
- ups_get_report.c: decode_voltronic_feature() for Feature reports 0x22 (PresentStatus), 0x21 (load%), plus standard decode for 0x18/0x1B/0x36/0x34. Polling RID list and decode routing added.
- ups_db_standard.c: PowerWalker entry switched from DECODE_STANDARD to DECODE_VOLTRONIC. VID comment corrected (Cypress Semiconductor, not WayTech).

## Files Staged
- src/current/main/ups_device_db.h
- src/current/main/ups_hid_parser.c
- src/current/main/ups_db_standard.c
- src/current/main/ups_get_report.c
- docs/github_push.md
- docs/project_state.md
- docs/next_steps.md
