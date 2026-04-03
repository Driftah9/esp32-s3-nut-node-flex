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
v0.11

## Commit Message
v0.11 - esp32-s3-nut-node-flex - Mode renumber 1/2/3 + diag log capture spec

Session 009: Mode numbering fix and standby feature spec

Mode numbering fix (0/1/2 to 1/2/3):
- cfg_store.h: OP_MODE_STANDALONE=1, OP_MODE_NUT_CLIENT=2, OP_MODE_BRIDGE=3
- http_config_page.c: dropdown values, card tags, JS logic, form parse range all updated
- Status page and log strings already used 1/2/3 - no change needed there
- NVS migration: old value 0 falls to default case (STANDALONE) - no data loss

Diagnostic log capture spec (standby - not integrated):
- docs/diag-log-capture.md (NEW): full design spec for opt-in boot log capture
  Checkbox gates button, 90s capture from boot, credential scrub before POST,
  NVS flag + reboot approach, ring buffer 128KB PSRAM, boot loop guard,
  submission endpoint JSON spec, /diag fallback for browser download
- docs/next_steps.md: trimmed to single reference line pointing at spec doc
- docs/DOC-REGISTRY.md: registered diag-log-capture.md, confirmed-ups.md,
  nut-upstream-setup.md (three docs missing from registry)

Build: clean, no warnings.

## Files Staged
- main/cfg_store.h
- main/cfg_store.c
- main/http_config_page.c
- docs/diag-log-capture.md
- docs/DOC-REGISTRY.md
- docs/next_steps.md
- docs/project_state.md
- docs/github_push.md
