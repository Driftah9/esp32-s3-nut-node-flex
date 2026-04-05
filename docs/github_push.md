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
v0.19

## Commit Message
v0.19 - Fix abort in ups_state_apply_update on multi-core ESP32-S3

v0.18 added ESP_LOGI calls inside portENTER_CRITICAL in
ups_state_apply_update(). ESP_LOGI acquires internal mutexes and
cannot be called inside a spinlock critical section on multi-core
ESP32-S3 - causes immediate abort().

Fix: capture all log parameters as local variables inside the critical
section, exit the critical section, then emit the log lines. Pattern
used: log_action enum (0=none, 1=immediate, 2=committed, 3=started)
with log_old, log_new, log_rid, log_stable, log_thresh locals.

Confirmed via APC Back-UPS XS 1500M test: abort at t=1492ms right
after first battery.charge decode. Device crash-looped on every boot.

## Files Staged
- src/current/main/ups_state.c
- docs/github_push.md
- docs/project_state.md
