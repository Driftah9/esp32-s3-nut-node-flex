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
v0.18

## Commit Message
v0.18 - Per-RID interval learning and status debounce

Self-calibrating EMA interval tracker for all interrupt-IN RIDs.
Three parallel arrays track rolling average inter-report interval per
RID. Stabilises in ~5 reports (~10s for a 2s interval device).

Status debounce in ups_state_apply_update(): a new ups_status string
must be seen consistently for min(1.5x learned interval, 3500ms)
before it overwrites the committed g_state status. Prevents false
OL-OB transitions caused by a single anomalous report. During warmup
debounce is disabled and status applies immediately.

data_age_ms added to ups_state_t (now minus last_update_ms at snapshot).
Dashboard shows age indicator below status badge.
/status JSON includes data_age_ms field.

## Files Staged
- src/current/main/ups_hid_parser.h
- src/current/main/ups_hid_parser.c
- src/current/main/ups_state.h
- src/current/main/ups_state.c
- src/current/main/http_portal.c
- src/current/main/http_dashboard.c
- docs/github_push.md
- docs/project_state.md
