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
v0.40

## Status
pending

## Last Push
Commit: 3cff4c8
Tag: v0.39
Message: v0.39 - Fix HA NUT integration availability (from confirmed working files)
Date: 2026-04-15

## Commit Message
v0.40 - Fix INT-IN buffer MPS alignment for IDF 5.4.1; 4MB partition table; Linux native build

## Changes Summary
- ups_usb_hid.c: Round interrupt-IN buffer size up to MPS multiple
  Fixes ESP_ERR_INVALID_ARG / "num_bytes not integer multiple of MPS" in IDF 5.4.1
  MPS=8, buf=50 was rejected; now rounds to 56 - restores battery data flow
  
- partitions.csv (new): Custom 4MB app partition (replaces 1MB default)
  Reduces utilization from 95% to 24%, leaves 12MB for OTA/future storage
  
- sdkconfig: CONFIG_PARTITION_TABLE_CUSTOM enabled, points to partitions.csv

- CLAUDE.md: Linux native build as primary workflow
  source ~/.espressif/esp-idf/export.sh + idf.py build/flash/monitor
  Windows MCP retained as deferred fallback (board on Proxmox USB passthrough)

- docs/project_state.md: Updated to v0.40 status
- docs/next_steps.md: Added Phase 5 - XCHK logging suppression for confirmed devices
- docs/confirmed-ups.md: APC Back-UPS 1500/XS 1500M re-confirmed on v0.29

## Files to Stage
- src/current/main/ups_usb_hid.c
- src/current/partitions.csv
- src/current/sdkconfig
- src/current/CLAUDE.md
- src/current/docs/project_state.md
- src/current/docs/next_steps.md
- src/current/docs/confirmed-ups.md
- src/current/main/CMakeLists.txt
- src/current/main/http_config_page.h
- src/current/main/main.c
- src/current/main/ups_get_report.c
- src/current/main/ups_hid_desc.c
- src/current/main/ups_hid_desc.h
- src/current/main/ups_hid_map.c
- src/current/main/ups_hid_map.h
- src/current/main/ups_hid_parser.c
- src/current/main/ups_var_store.c
- src/current/main/ups_var_store.h
- src/current/.gitignore
- CLAUDE.md
- docs/github_push.md
