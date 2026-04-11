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
v0.32

## Status
pending

## Last Push
Commit: 3dc222e
Tag: v0.30
Message: v0.30 - Fix INT-IN buffer truncation; add PowerWalker; Eaton rid=0x06 polling
Date: 2026-04-09

## Previous Push
Commit: 5ca71cd
Tag: v0.29
Message: v0.29 - Fix Eaton 3S stale data: add rid=0x06 to periodic GET_REPORT polling

## Commit Message
v0.32 - Feature-fallback field cache; PowerWalker GET_REPORT quirk
- ups_hid_parser.c (R18): two-pass field cache - Input first, Feature (type=2) as fallback for NULL slots. Fixes battery.runtime on PowerWalker 0665:5161 (Feature-only declaration, sent on interrupt-IN).
- ups_db_standard.c: add QUIRK_NEEDS_GET_REPORT to PowerWalker VI 3000 SCL. Enables periodic GET_REPORT polling for rid=0x30 status flags (ac_present, charging, discharging).

## Files Staged
- src/current/main/ups_hid_parser.c
- src/current/main/ups_db_standard.c
- docs/github_push.md
- docs/project_state.md
- docs/next_steps.md
