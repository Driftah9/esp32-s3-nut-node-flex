# Project State - esp32-s3-nut-node-flex
<!-- Updated: 2026-04-09 -->

## Last GitHub Push
Version: v0.30
Tag: v0.30
Commit: 3dc222e
Message: v0.30 - Fix INT-IN buffer truncation; add PowerWalker; Eaton rid=0x06 polling
Result: Success

## Previous Push
Version: v0.29
Tag: v0.29
Commit: 5ca71cd
Message: v0.29 - Fix Eaton 3S stale data: add rid=0x06 to periodic GET_REPORT polling
Result: Success

## Status
v0.30 - INT-IN buffer fix + PowerWalker support. Pushed to GitHub.
- Fix: ups_usb_hid.c (R16): INT-IN buffer now max(MPS, largest_input_report) capped at 64.
  PowerWalker VI 3000 SCL rid=0x30 is 24 bytes; MPS=8 was truncating Charging/Discharging.
- Add: ups_hid_parser.c (R17): ups_hid_parser_max_input_bytes() API
- Add: ups_db_standard.c: PowerWalker VI 3000 SCL (0665:5161, 230V, line-interactive)
- Fix (v0.29): ups_hid_parser.c (R16): goto finalize unconditional for all Eaton rids
- Fix (v0.29): ups_get_report.c (R7): add rid=0x06 to Eaton periodic GET_REPORT polling

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
Command: idf-build.ps1 -Target build (via SSH to Stryder@10.0.0.2) - 2026-04-10
Result: SUCCESS
Output summary: Build clean. Zero errors, zero warnings. Binary: 0xee7f0 bytes (7% free in app partition). Bootloader: 0x5260 bytes (36% free).
Next step: Flash v0.30 when ready.

## Last Action
2026-04-09 - v0.30: Fix INT-IN buffer truncation; add PowerWalker VI 3000 SCL support.
Root cause: MPS=8 for PowerWalker caused INT-IN buffer to allocate only 8 bytes.
rid=0x30 is 24 bytes, so Charging/Discharging bits were truncated and never decoded.
Fix: buffer = max(MPS, largest_input_report_size) capped at 64.
New API: ups_hid_parser_max_input_bytes() exposes the pre-computed max to usb_hid.
Also includes v0.29 changes (never independently pushed): Eaton rid=0x06 polling fix.
Push: 3dc222e tagged v0.30.

## Next Step
- Flash v0.30 and test with PowerWalker VI 3000 SCL
- Eaton user (MyDisplayName): re-submit on v0.30 to confirm data refresh fix
- Request Eaton discharge log (unplug from mains for 10s) to capture OB event

Candidate next tasks:
- CyberPower 3000R ups.status: decode rid=0x0B bits to extract ac_present
- CyberPower 3000R battery.runtime/voltage: add Feature report polling
- D002: Mode 1 fallback when upstream unreachable (Mode 2/3 boot fail)
- Bridge GET_REPORT forwarding (type=0x02) for Feature reports
- APC direct-decode: add input.transfer.low/high from rid=0x52 (D006)
- Eaton: decode OB event from discharge log once captured

## Key Constraint
Never backport experimental changes to esp32-s3-nut-node.
