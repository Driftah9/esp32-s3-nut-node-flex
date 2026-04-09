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
v0.29

## Status
pending push

## Last Push
Commit: d4fba70
Message: docs: correct Linux paths in CLAUDE.md, add v0.27 push record to project_state
Tag: none (docs-only commit, no version bump)
Date: 2026-04-06

## Previous Push
Commit: a354858
Tag: v0.27
Message: v0.27 - Eaton OL/OB fix: vendor page 0xFFFF, pre-seed OL, OB-only flags

## Commit Message
v0.29 - Fix Eaton 3S stale data: add rid=0x06 to periodic GET_REPORT polling

- ups_get_report.c (R7): add rid=0x06 to s_eaton_rids[] periodic polling list - Eaton 3S only sends rid=0x06 on mains events, not periodically - data went stale after boot burst
- ups_hid_parser.c (R16): make goto finalize unconditional for rid=0x06/0x21 Eaton blocks - was conditional on `changed`, edge case could fall through to standard path
- Root cause: GET_REPORT polled rids 0x20/0xFD/0x85 but none applied data. decode_eaton_feature case 0x06 already handles and applies charge/runtime/flags
- Source: Eaton 3S submissions 30b6f9 (v0.27) + 713d7c (v0.28) from MyDisplayName

## Files Staged
- src/current/main/ups_get_report.c (R7: add 0x06 to s_eaton_rids[])
- src/current/main/ups_hid_parser.c (R16: unconditional goto finalize)
- docs/github_push.md (version bump)
- docs/project_state.md (v0.29 status)
- docs/next_steps.md (v0.29 section)
