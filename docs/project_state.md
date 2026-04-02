# Project State - esp32-s3-nut-node-flex
<!-- Updated: 2026-04-02 -->

## Status
Phase 1 COMPLETE. CLI builds enabled via idf-build.ps1. v0.2 push in progress.

## Parent
esp32-s3-nut-node v15.18

## Upstream Sync Baseline
Forked from: v15.18
Last synced: v15.18 (initial copy, 2026-04-02)

## CLI Build Capability
idf-build.ps1 at project root enables CLI-driven builds.
Run: powershell -ExecutionPolicy Bypass -File .\idf-build.ps1 -Target build
Output: docs\build.log

## Implementation Progress
- [x] Scaffold, v15.18 baseline, README, DOC-REGISTRY, sync rules
- [x] Initial GitHub push - v0.1
- [x] cfg_store.h/c - OP_MODE constants + new fields + defaults
- [x] http_config_page.c - Operating Mode selector + upstream section + JS show/hide
- [x] Build clean (CLI), flash confirmed, portal UI verified
- [x] idf-build.ps1 - CLI build wrapper (MSYSTEM fix)
- [ ] Push v0.2 to GitHub
- [ ] Phase 2 - Mode 2 NUT client push task

## Last Action
2026-04-02 - Phase 1 complete. CLI builds now work via idf-build.ps1 (removes MSYSTEM
before IDF init). Build confirmed clean. Portal mode selector verified in Chrome.

## Next Step
Push v0.2 to GitHub. Then begin Phase 2 (NUT client push task).

## Key Constraint
Never backport experimental changes to esp32-s3-nut-node.
