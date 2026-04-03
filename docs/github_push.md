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
v0.13

## Commit Message
v0.13 - esp32-s3-nut-node-flex - Expand diag scrub to cover all private fields

Audit of ESP_LOG output across all modules revealed additional private data
that appears in the captured log beyond passwords:

Fields added to diag_capture_scrub():
- sta_ssid: WiFi network name, logged by IDF WiFi driver during STA connect
- upstream_host: internal IP/hostname, logged 15+ times in main.c,
  nut_client.c, and nut_bridge.c during mode dispatch and connect attempts
- nut_user: NUT username, not in current logs but scrubbed for completeness
- ap_ssid: device AP broadcast name, logged by cfg_store and wifi_mgr

Previously scrubbed (unchanged): sta_pass, ap_pass, nut_pass, portal_pass

Updated UI text in dashboard and log viewer to accurately describe
what is redacted: WiFi credentials, network names, upstream host, and passwords.

diag_capture.h comment updated to document the full scrub field list.

Build: clean, no warnings.

## Files Staged
- main/diag_capture.h
- main/diag_capture.c
- main/http_portal.c
- main/http_dashboard.c
- docs/github_push.md
- docs/project_state.md
