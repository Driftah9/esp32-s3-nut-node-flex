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
v0.22

## Commit Message
v0.22 - Add 300s diagnostic log capture option

Duration type widened from uint8_t to uint16_t throughout (300 > 255).
NVS key diag_dur migrated from nvs_set/get_u8 to nvs_set/get_u16.
Whitelist accepts 90, 120, or 300. Dashboard shows third radio button.

300s is useful for event-driven devices such as Eaton 3S where the
first interrupt-IN report may not arrive until a mains event occurs
after boot. 128KB PSRAM buffer handles 300s at INFO log level easily.

## Files Staged
- src/current/main/diag_capture.h
- src/current/main/diag_capture.c
- src/current/main/http_portal.c
- src/current/main/http_dashboard.c
- docs/github_push.md
- docs/project_state.md
