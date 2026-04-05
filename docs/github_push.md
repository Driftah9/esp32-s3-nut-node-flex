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
v0.23

## Commit Message
v0.23 - Remove unused strlcpy0 from http_dashboard.c

strlcpy0 was copied from another module but never called in
http_dashboard.c, producing a -Wunused-function warning on clean
builds. Reported by community user building locally.

## Files Staged
- src/current/main/http_dashboard.c
- docs/github_push.md
- docs/project_state.md
