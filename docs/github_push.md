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
v0.46

## Status
ready

## Last Push
Commit: 249ee2e
Tag: v0.45
Message: v0.45 - Table-driven Feature report polling (generic walker replaces hardcoded dispatch)
Date: 2026-05-30

## Commit Message
v0.46 - CyberPower 0601 ups.load fix via rid=0x1D interrupt-IN decode

## Changes Summary
- ups_hid_parser.c: Add rid=0x1D tentative ups.load decode for CyberPower 0601
  (field cache MISSING - descriptor puts PercentLoad on non-standard page)
  byte[0] = ups.load percentage (0-100). Source: CST150UC submission 2026-05-30.
- ups_hid_parser.c: rid=0x19 silently consumed (candidates TBD, needs load-test)
- CMakeLists.txt: version v0.45 -> v0.46
- main.c: banner v0.45 -> v0.46

## Files to Stage
src/current/main/ups_hid_parser.c
src/current/CMakeLists.txt
src/current/main/main.c
docs/project_state.md
docs/github_push.md

