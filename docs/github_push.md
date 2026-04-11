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
v0.31

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
- ups_hid_parser.c / .h: Added ups_hid_parser_get_input_rids() API
- ups_get_report.c: DECODE_STANDARD Feature report decode support (XCHK + recurring poll), CTRL_PAYLOAD_MAX 24->64, buffer 16->64, dynamic RID polling from descriptor

## Files Staged
- src/current/main/ups_hid_parser.c
- src/current/main/ups_hid_parser.h
- src/current/main/ups_get_report.c
- docs/github_push.md
- docs/project_state.md
- docs/next_steps.md
- docs/session_log.md
