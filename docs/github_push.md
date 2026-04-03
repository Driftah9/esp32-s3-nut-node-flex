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
v0.14 - Fix XCHK probe size cap end-to-end (16->64)

Two-location fix for PowerWalker VI 3000 RLE battery.charge=0 crash.

Root cause: XCHK probe for rid=0x28 (63 bytes declared) was sent with
wLength=16. IDF v5.5.4 DWC assert fires (hcd_dwc.c:2388) when wLength
is less than the declared report size. Device crash-loops every ~34s.
battery.charge=0 is a symptom of the crash loop, not a decode bug.

Caps were in two places - both now raised 16->64:
1. ups_hid_parser.c run_xchk: probe_sz clamped at 64 (was 16)
2. ups_get_report.c service_probe_queue: buf[64] cap at 64 (was 16)

Verified on APC XS 1500M: rid=0x07 (50 bytes declared) now probed
with wlen=50 (was wlen=16). No assert. No crash. Device stays up.

## Files Staged
- src/current/main/ups_hid_parser.c
- src/current/main/ups_get_report.c
- docs/next_steps.md
- docs/project_state.md
- docs/github_push.md
