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
v0.24

## Commit Message
v0.24 - Config mode validation + HA zero data fix

http_config_page.c: chkSave() blocks Save when Mode 2 or 3 is selected
with no upstream_host filled in. Shows inline error and focuses the field.
onModeChange() clears the error when mode switches back to 1.

nut_server.c: battery.charge now gated on st->valid. Prevents HA from
receiving "0" during the boot window before first UPS data arrives.
All other gated variables (runtime, voltage, load) already had valid checks.

## Files Staged
- src/current/main/http_config_page.c
- src/current/main/nut_server.c
- docs/github_push.md
- docs/project_state.md
- docs/next_steps.md
- docs/session_log.md
