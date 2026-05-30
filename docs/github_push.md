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
v0.47

## Status
ready

## Last Push
Commit: b8c1ba2
Tag: v0.46
Message: v0.46 - CyberPower 0601 ups.load fix via rid=0x1D interrupt-IN decode
Date: 2026-05-30

## Commit Message
v0.47 - Update /compat page: 6 confirmed devices (APC + CyberPower)

## Changes Summary
- http_compat.c: APC section updated - added XS 1300G and Pro 1000/Pro 1000S as ST_OK confirmed
- http_compat.c: Smart-UPS C 1500 added as confirmed (v0.43+)
- http_compat.c: CyberPower CST150UC added as confirmed (v0.46+, ups.load via rid=0x1D)
- http_compat.c: 0601 series other models promoted from ST_UN to ST_EX (expected to work)
- Vendor/series counts updated to reflect 6 confirmed total (5 APC + 1 CyberPower 0601)
- CMakeLists.txt + main.c: version bumped to v0.47

## Files to Stage
src/current/main/http_compat.c
src/current/CMakeLists.txt
src/current/main/main.c
docs/github_push.md
docs/project_state.md

