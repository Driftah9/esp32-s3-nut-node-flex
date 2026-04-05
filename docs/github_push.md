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
v0.17

## Commit Message
v0.17 - Fix Task Watchdog crash on large HID descriptors + version string cleanup

Root cause (CyberPower 3000R submission 9b89d6):
ups_hid_desc_dump() looped over all descriptor fields at ESP_LOGI level.
CyberPower 3000R rid=0x29 has 237 fields. At 10ms per ESP_LOGI call the
loop blocked the ups_usb task on core 0 for ~2.4s, starving IDLE0 past
the TWDT threshold. Watchdog fired at ~t=11.6s, device crash-looped on
every boot.

Fix: per-field loop in ups_hid_desc_dump() changed from ESP_LOGI to
ESP_LOGD. Summary line kept at INFO. Normal builds emit 1 line per
connection instead of 237.

Version string cleanup:
- http_dashboard.c: hardcoded v0.6-flex subtitle replaced with
  esp_app_get_description version (tracks git tag automatically)
- http_portal.c: hardcoded 15.13 driver_version in /status JSON
  replaced with esp_app_get_description version

Eaton 3S analysis - 77eaee confirmed working (battery.charge=89%,
runtime=1401s, OL). 9543fe incomplete log, undiagnosable.

## Files Staged
- src/current/main/ups_hid_desc.c
- src/current/main/http_dashboard.c
- src/current/main/http_portal.c
- docs/github_push.md
- docs/project_state.md
- docs/next_steps.md
