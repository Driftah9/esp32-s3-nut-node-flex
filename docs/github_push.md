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
v0.6

## Commit Message
v0.6 - esp32-s3-nut-node-flex - dashboard redesign, OB fix, issue templates, community links

Session 007/008: Full NUT variable dashboard + CyberPower fix + community infrastructure

Dashboard redesign:
- http_dashboard.c R3: full upsc-style NUT variable table, all groups visible
- Groups in order: device, driver, battery, input, output, ups
- device group moved to first position - identifies what is plugged in
- AJAX polls /status every 5s, all cells update live
- fmtRt formats runtime as Xh Xm Xs - human readable with seconds fallback
- stCls color-codes status badge ob/ol/unknown
- Lightbox removed - full table direct on page
- Title changed from ESP32-S3 UPS Node to ESP32 UPS Node - board-agnostic
- Subtitle reads cfg->op_mode and shows active mode string

CyberPower OB DISCHRG fix:
- ups_hid_parser.c: rid=0x80 AC-present decode fixed
- Old p[0] & 0x01u broke on ST Series which sends 0x02 for AC present
- New p[0] != 0x00u - only exact zero means AC absent
- CP550HG uses 0x01, ST Series uses 0x02, both now read correctly

Status JSON additions:
- http_portal.c: ups_vendorid and ups_productid added to /status JSON
- Formatted as 4-digit lowercase hex, JSON buffer bumped to 1100

Phase 4 dynamic RID scanning - complete:
- ups_hid_parser.c R5: s_seen_rids[32] bitmask + 30s settle timer + run_xchk API
- ups_hid_parser.h: ups_hid_parser_run_xchk declaration added
- ups_hid_desc.c: static expected_rids block removed

Reboot countdown page:
- http_portal.c: /reboot returns full HTML with 20s JS countdown then redirects to /

Community and issue infrastructure:
- .github/ISSUE_TEMPLATE/bug_report.yml - adapted from main project
- .github/ISSUE_TEMPLATE/ups-compatibility-report.yml - adapted from main project
- .github/workflows/label-new-issue.yml - auto-label and checklist comment
- .github/workflows/update-compat-list.yml - auto-update confirmed-ups.md on confirmed label
- docs/confirmed-ups.md - stub with two seed entries (CP550HG, ST Series)
- README.md: Ko-fi button, Discord link, projects.strydertech.com, issue report links
- docs/next_steps.md: diagnostic logging added as possible future addition

## Files Staged
- src/current/main/ups_hid_parser.c
- src/current/main/ups_hid_parser.h
- src/current/main/ups_hid_desc.c
- src/current/main/http_portal.c
- src/current/main/http_dashboard.c
- .github/ISSUE_TEMPLATE/bug_report.yml
- .github/ISSUE_TEMPLATE/ups-compatibility-report.yml
- .github/workflows/label-new-issue.yml
- .github/workflows/update-compat-list.yml
- docs/confirmed-ups.md
- docs/github_push.md
- docs/project_state.md
- docs/next_steps.md
- docs/session_log.md
- README.md
