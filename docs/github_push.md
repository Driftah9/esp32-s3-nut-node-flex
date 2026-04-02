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
v0.3

## Commit Message
v0.3 - esp32-s3-nut-node-flex - Mode 2 NUT CLIENT push task

- nut_client.c - new: Mode 2 NUT CLIENT task (connect, auth, SET VAR push loop)
- nut_client.h - new: public API for nut_client_start()
- main.c - mode dispatch: OP_MODE_NUT_CLIENT calls nut_client_start, STANDALONE/BRIDGE call nut_server_start
- main.c - op_mode logged after NVS load (was before, showed wrong value)
- nut_client.c - 5s startup delay for DHCP settle before first connect attempt
- nut_client.c - push-based reconnect detection (nc_set_var returns bool, no VER keepalive)
- idf-build.ps1 - flash-monitor combined target (flash then immediate boot capture)
- idf-build.ps1 - WorkingDirectory fix for ProcessStartInfo monitor (was using project root not src/current)
- idf-build.ps1 - kill lingering python processes before flash to free COM3
- CMakeLists.txt - add nut_client.c to build

## Files Staged
- src/current/main/nut_client.c
- src/current/main/nut_client.h
- src/current/main/main.c
- src/current/main/CMakeLists.txt
- idf-build.ps1
- docs/github_push.md
- docs/project_state.md
- docs/next_steps.md
- docs/DECISIONS.md
- docs/session_log.md
- README.md
