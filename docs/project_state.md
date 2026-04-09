# Project State - esp32-s3-nut-node-flex
<!-- Updated: 2026-04-08 -->

## Last GitHub Push
Version: v0.29
Tag: v0.29
Commit: 5ca71cd
Message: v0.29 - Fix Eaton 3S stale data: add rid=0x06 to periodic GET_REPORT polling
Result: Success

## Previous Push
Version: v0.28
Tag: v0.28
Commit: 6e2dc96
Message: v0.28 - Fix Eaton stale-data regression: add goto finalize after direct-decode
Result: Success

## Status
v0.29 - Eaton 3S periodic data refresh fix. Pushed to GitHub.
- Fix: ups_get_report.c add rid=0x06 to s_eaton_rids[] for periodic GET_REPORT polling
- Fix: ups_hid_parser.c make goto finalize unconditional for rid=0x06/0x21 Eaton blocks
- Root cause: Eaton 3S only sends rid=0x06 on mains events. After boot burst, data went stale.
  GET_REPORT polling ran every 30s on rids 0x20/0xFD/0x85 but none applied data to state.
  decode_eaton_feature case 0x06 already applies charge/runtime/flags - just needed polling.
- OL/OB still unresolved: Eaton vendor UIDs (0x0074/0x0075/0x006B) don't match standard
  ACPresent/Charging/Discharging. Need discharge event data from user.

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
- [x] Phase 4 - Targeted GET_REPORT probe for declared-but-silent Input RIDs - v0.7
- [x] Phase 4 - NUT mge-hid.c mapping table evaluation + annotation layer - v0.8

## Mode Status
- Mode 1 STANDALONE: inherited from v15.18 baseline - confirmed working
- Mode 2 NUT CLIENT: nut_client.c - connect/auth/SET VAR loop - confirmed working
- Mode 3 BRIDGE: nut_bridge.c - descriptor handshake + raw intr-IN stream - confirmed working

## NUT Test LXC (10.0.0.18)
- upsd with dummy-ups "ups" device (esppush/esppush123) - Mode 2 target
- bridge_receiver.py on port 5493 - Mode 3 target
- SSH: nut-test-lxc key

## Last CLI Run
Command: idf-build.ps1 -Target build (via SSH to Stryder@10.0.0.2) - 2026-04-09
Result: SUCCESS
Output summary: Build clean. Zero errors, zero warnings. Binary: 0xee7f0 bytes (7% free in app partition). Bootloader: 0x5260 bytes (36% free).
Next step: Flash v0.29 when ready, or request re-submission from Eaton user.

## Last Action
2026-04-08 - v0.29: Fix Eaton 3S stale data (periodic GET_REPORT refresh).
Analyzed submissions 30b6f9 (v0.27, 2026-04-07) and 713d7c (v0.28, 2026-04-08) from
MyDisplayName. Both logs truncated during descriptor parse (DEBUG level + 32KB buffer).
Root cause identified from code analysis: Eaton 3S sends rid=0x06 as interrupt-IN only
on mains events - after boot burst data goes stale because the 30s GET_REPORT polling
cycle only polled rids 0x20/0xFD/0x85, none of which apply data.
Fix: add 0x06 to s_eaton_rids[] so periodic GET_REPORT refreshes charge/runtime.
Also made goto finalize unconditional for Eaton rid=0x06/0x21 blocks.
OL/OB issue remains: vendor UIDs don't match standard ACPresent - need discharge log.
Build: clean. Push: 5ca71cd tagged v0.29.

## Next Step
Eaton user (MyDisplayName): re-submit on v0.29 to confirm:
1. Data refreshes periodically (data_age resets every 30s instead of climbing)
2. battery.charge and battery.runtime update over time
3. OL/OB still won't work (known limitation - need discharge event data)

Request: ask user to unplug UPS from mains for 10s and submit that log.
This will capture the rid=0x06 payload during an actual OB event and reveal
whether non-zero flags or a different rid carries the AC status.

Candidate next tasks:
- CyberPower 3000R ups.status: decode rid=0x0B bits to extract ac_present
- CyberPower 3000R battery.runtime/voltage: add Feature report polling
- D002: Mode 1 fallback when upstream unreachable (Mode 2/3 boot fail)
- Bridge GET_REPORT forwarding (type=0x02) for Feature reports
- APC direct-decode: add input.transfer.low/high from rid=0x52 (D006)
- Eaton: decode OB event from discharge log once captured

## Key Constraint
Never backport experimental changes to esp32-s3-nut-node.
