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
v0.12

## Commit Message
v0.12 - esp32-s3-nut-node-flex - Diagnostic log capture

Session 009: Opt-in boot log capture with portal UI and copy-paste viewer

New module - diag_capture.c/h:
- NVS key diag_dur armed via portal, cleared immediately on arm
- 128KB PSRAM ring buffer allocated on armed boot, 32KB heap fallback
- vprintf hook mirrors all log output to ring buffer + UART simultaneously
- FreeRTOS timer task fires at selected duration (90s or 120s)
- Timer removes hook, marks log ready, appends completion marker
- Scrub: sta_pass, ap_pass, nut_pass, portal_pass replaced with asterisks
- Scrub runs once before log is served

Dashboard UI (http_dashboard.c):
- State-aware capture section below nav bar
- Idle: form with radio 90s/120s + Start Capture button
- Armed: progress banner with elapsed/remaining + link to /diag-log
- Ready: capture complete notice + View Log link in new tab

New routes in http_portal.c:
- POST /diag-start: parse duration, write NVS, send countdown page, reboot
- GET  /diag-log: if armed shows progress with 5s auto-refresh
                  if ready shows HTML log viewer with Copy All button
                  if idle shows no-capture-data page

Log viewer: dark-theme pre block, Copy All button with clipboard API fallback
Simple pages use minimal inline CSS instead of PORTAL_CSS to stay within stack buffers

main.c: diag_capture_check_and_arm() called after cfg load, before wifi/USB init

Build: clean, no warnings.

## Files Staged
- main/diag_capture.h
- main/diag_capture.c
- main/main.c
- main/http_portal.c
- main/http_dashboard.c
- main/CMakeLists.txt
- docs/project_state.md
- docs/github_push.md
