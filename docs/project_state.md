# Project State - esp32-s3-nut-node-flex
<!-- Updated: 2026-04-02 -->

## Status
v0.6 ready to push. All session work complete - dashboard redesign, OB fix, issue templates, community links.

## Parent
esp32-s3-nut-node v15.18

## Upstream Sync Baseline
Forked from: v15.18
Last synced: v15.18 (initial copy, 2026-04-02)

## CLI Build Capability
idf-build.ps1 at project root - all targets CLI-driven:
- Build:         powershell -ExecutionPolicy Bypass -File .\idf-build.ps1 -Target build
- Flash:         powershell -ExecutionPolicy Bypass -File .\idf-build.ps1 -Target flash
- Flash+Monitor: powershell -ExecutionPolicy Bypass -File .\idf-build.ps1 -Target flash-monitor -Duration 40
- Monitor only:  powershell -ExecutionPolicy Bypass -File .\idf-build.ps1 -Target monitor -Duration 60

## Implementation Progress
- [x] Scaffold, v15.18 baseline, README, DOC-REGISTRY, sync rules - v0.1
- [x] cfg_store op_mode fields + portal mode selector - v0.2
- [x] Mode 2 NUT CLIENT push task (nut_client.c) - v0.3
- [x] Mode 3 BRIDGE raw HID stream (nut_bridge.c) - v0.4
- [x] Phase 4 - Dynamic RID scanning (seen_rids bitmask + settle XCHK) - v0.6

## Mode Status
- Mode 1 STANDALONE: inherited from v15.18 baseline - confirmed working
- Mode 2 NUT CLIENT: nut_client.c - connect/auth/SET VAR loop - confirmed working
- Mode 3 BRIDGE: nut_bridge.c - descriptor handshake + raw intr-IN stream - confirmed working

## NUT Test LXC (10.0.0.18)
- upsd with dummy-ups "ups" device (esppush/esppush123) - Mode 2 target
- bridge_receiver.py on port 5493 - Mode 3 target
- SSH: nut-test-lxc key

## Last Action
2026-04-02 - Dashboard redesign, CyberPower OB fix, community infrastructure (v0.6).
http_dashboard.c R3: full upsc-style NUT variable table, device group first, AJAX 5s, title renamed.
ups_hid_parser.c: rid=0x80 OB fix - p[0] != 0x00u replaces bit-0 check.
http_portal.c: ups_vendorid/ups_productid added to /status JSON.
.github: issue templates + workflows ported from main project (adapted for flex).
docs/confirmed-ups.md: created with seed entries.
README.md: community section added (Discord, Ko-fi, projects.strydertech.com).
docs/next_steps.md: diagnostic logging added as possible future addition.
docs/github_push.md: updated with all accumulated changes.

## Next Step
Run git-push.ps1 to push v0.6 to GitHub.

## Key Constraint
Never backport experimental changes to esp32-s3-nut-node.
