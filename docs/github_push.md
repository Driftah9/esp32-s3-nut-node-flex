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

## Discord Channel
1489730620337684570

## Version
v0.44

## Status
ready

## Last Push
Commit: 1997541
Tag: v0.44-alpha
Message: v0.44-alpha - Table-driven Feature report architecture foundation
Date: 2026-05-30

## Commit Message
v0.44 - Table-driven Feature report architecture (validated on hardware)

## Changes Summary
- Extended ups_device_db.h with hid_get_report_info_t structure for Feature report polling tables
- Added get_report_table pointer to ups_device_entry_t - each device now references its table
- Moved APC decode functions from ups_get_report.c to ups_db_apc.c (decode_apc_backups_rid_0x17, _0x50, decode_apc_smartups_rid_0x06, _0x0E)
- Built APC Back-UPS and Smart-UPS Feature polling tables matching NUT apc-hid.c hid2nut[] pattern
- All vendor database files updated (APC, CyberPower, Eaton, Standard)
- Hardware validated on APC Back-UPS XS 1500M: rid=0x17 -> input.voltage=120V, rid=0x50 -> ups.load=30%, HA polling confirmed working, no panics

## Files to Stage
src/current/main/ups_device_db.h
src/current/main/ups_db_apc.c
src/current/main/ups_db_cyberpower.c
src/current/main/ups_db_eaton.c
src/current/main/ups_db_standard.c
docs/github_push.md
docs/project_state.md
docs/v0.44-ARCHITECTURE.md

