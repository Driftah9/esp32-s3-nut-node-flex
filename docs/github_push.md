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
v0.14

## Commit Message
v0.14 - Fix XCHK probe buffer crash on large Feature reports

Root cause of PowerWalker VI 3000 RLE battery.charge=0 identified
from staging submissions: XCHK probe for rid=0x28 (63 bytes declared)
was sent with wLength=16 due to hardcap. IDF v5.5.4 DWC assert fires
(hcd_dwc.c:2388 rem_len check), device crash-loops every ~34s,
battery.charge=0 is a crash-loop symptom not a decode bug.

Fix in ups_get_report.c: probe buffer cap raised 16->64 bytes.
buf[16]->buf[64]. Covers declared Feature report sizes up to 64 bytes.
Also applies on v5.3.1 where large wLength would read wrong data.

Docs updated with root cause analysis and fix checklist.

## Files Staged
- src/current/main/ups_get_report.c
- docs/next_steps.md
- docs/project_state.md
- docs/github_push.md
