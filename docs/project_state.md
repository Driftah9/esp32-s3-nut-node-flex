# Project State - esp32-s3-nut-node-flex
<!-- Updated: 2026-04-02 -->

## Status
Phase 2 COMPLETE. v0.3 ready to push.

## Parent
esp32-s3-nut-node v15.18

## Upstream Sync Baseline
Forked from: v15.18
Last synced: v15.18 (initial copy, 2026-04-02)

## CLI Build Capability
idf-build.ps1 at project root enables CLI-driven build, flash, and monitor.
- Build:         powershell -ExecutionPolicy Bypass -File .\idf-build.ps1 -Target build
- Flash+Monitor: powershell -ExecutionPolicy Bypass -File .\idf-build.ps1 -Target flash-monitor -Duration 35
- Monitor only:  powershell -ExecutionPolicy Bypass -File .\idf-build.ps1 -Target monitor -Duration 60

## Implementation Progress
- [x] Scaffold, v15.18 baseline, README, DOC-REGISTRY, sync rules
- [x] Initial GitHub push - v0.1
- [x] cfg_store.h/c - OP_MODE constants + new fields + defaults
- [x] http_config_page.c - Operating Mode selector + upstream section + JS show/hide
- [x] Build clean (CLI), flash confirmed, portal UI verified
- [x] idf-build.ps1 - CLI build/flash/monitor wrapper
- [x] Push v0.2 to GitHub
- [x] nut_client.c - Mode 2 NUT CLIENT push task (connect, auth, SET VAR loop, fallback)
- [x] main.c - mode dispatch switch
- [x] Tested on nut-test-lxc (10.0.0.18) - connect, auth, push confirmed via upsc
- [x] idf-build.ps1 - flash-monitor combined target
- [ ] Phase 3 - Mode 3 bridge stream

## NUT Test LXC
Host: nut-test-lxc (10.0.0.18)
Device: ups (dummy-ups)
Push user: esppush / esppush123
Verified: upsc shows pushed values after ESP connects

## Last Action
2026-04-02 - Phase 2 complete. Mode 2 NUT CLIENT confirmed end-to-end.
ESP connects to 10.0.0.18:3493, authenticates as esppush, pushes battery.charge /
ups.status / ups.flags / input.utility.present every 10s. Reconnects on socket failure.

## Next Step
Push v0.3. Then begin Phase 3 - Mode 3 bridge stream.

## Key Constraint
Never backport experimental changes to esp32-s3-nut-node.
