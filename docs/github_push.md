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
v0.2

## Commit Message
- cfg_store.h - add OP_MODE constants (STANDALONE/NUT_CLIENT/BRIDGE)
- cfg_store.h - add op_mode, upstream_host, upstream_port, upstream_fallback to app_cfg_t
- cfg_store.c - defaults: op_mode=0 (standalone), upstream_fallback=1, upstream_port=3493
- http_config_page.c - Operating Mode selector + upstream section with JS show/hide
- http_config_page.c - parse op_mode, upstream_host, upstream_port from POST /save
- idf-build.ps1 - CLI build wrapper (removes MSYSTEM to bypass MinGW rejection)
- idf-build.ps1 - monitor: use cmd.exe host + MessageData for output capture fix
- CLAUDE.md - build workflow updated: CLI now drives builds via idf-build.ps1
