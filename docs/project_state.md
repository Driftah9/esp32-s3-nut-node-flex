# Project State - esp32-s3-nut-node-flex
<!-- Updated: 2026-04-03 -->

## Status
v0.13 - Diag scrub expanded: sta_ssid, upstream_host, nut_user, ap_ssid added alongside passwords. Button + radio (90s/120s) on dashboard. NVS flag + reboot + ring buffer + vprintf hook. Log served at /diag-log with Copy button. Passwords scrubbed before display.

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

## Last Action
2026-04-03 - v0.12: diag_capture.c/h (new module). Dashboard capture section (radio + button).
/diag-start POST: sets NVS diag_dur key, sends countdown page, reboots.
/diag-log GET: serves captured log as HTML with Copy button (passwords scrubbed).
On next boot with diag_dur set: vprintf hook installs into 128KB PSRAM ring buffer,
FreeRTOS timer task fires at selected duration, marks log ready, restores hook.
Build clean. 3 buffer-size compile errors fixed (PORTAL_CSS removed from simple pages).

Previous (2026-04-03) - v0.11: op_mode constants renumbered 1/2/3. cfg_store.h, cfg_store.c, http_config_page.c updated.
Build clean. Config page now matches status page (Mode 1/2/3 consistent throughout).
NVS note: old value 0 falls to default case (STANDALONE) in switch - no migration required.

Previous (2026-04-03) - Staging submission inspection (main repo user data, read-only).
4 submissions in D:\Users\Stryder\Documents\Claude\Submissions\staging\

Eaton 3S 700 (VID:0463 PID:FFFF) - 3 submissions:
- 926B descriptor, 111 fields, 10 declared RIDs, quirks=0x0040 (EATON/MGE path)
- Field cache: battery.charge/runtime/voltage all MISSING (rid=FF)
- Data lives in undocumented interrupt-IN RID 0x06:
  byte1=charge%, bytes2-3=runtime uint16 LE, bytes4-5=flags
  Example: 06 63 B4 10 00 00 = 99% charge, 4276s runtime
- XCHK submission (sub 3): 12 undeclared vendor extension RIDs (0x21-0x29, 0x80-0x88 range)
- Battery charge 2% bug in v15.17 submission - likely misread field in Eaton decode path
- Stryder will contact submitters about switching to flex repo for testing

BlueWalker PowerWalker VI 3000 RLE (VID:0764 PID:0601) - 1 submission:
- USB strings identify as CyberPower CP1500AVRLCD - rebranded CyberPower hardware
- Already handled by CyberPower OR/PR/RT/UT Series path in device database
- 256 fields, battery.charge found (rid=08), USB disconnect at ~96s then re-enumerated

Previous last action:
2026-04-02 - All-mode verification on APC XS 1500M (v0.10). D006 documented.
Mode 0 STANDALONE: upsc direct against ESP port 3493 - all vars confirmed.
Mode 1 NUT CLIENT: push to LXC dummy-ups confirmed, all vars flowing.
Mode 2 BRIDGE: 1049B descriptor + interrupt-IN stream confirmed on LXC port 5493.
rid=0x52 page=0x84 uid=0x0044 researched: APC non-compliant transfer voltage field.

## Next Step
Active issue: PowerWalker VI 3000 RLE (VID:0764 PID:0601) battery.charge reads 0%% (user confirms 100%%).
Awaiting user log submission for analysis. Likely rid=08 field extraction offset issue or
USB disconnect at ~96s preventing charge data from populating before disconnect.

When ready, candidate next tasks:
- D002: Mode 1 fallback when upstream unreachable (Mode 2/3 boot fail)
- Bridge GET_REPORT forwarding (type=0x02) for Feature reports
- APC direct-decode: add input.transfer.low/high from rid=0x52 (D006)
- Eaton 3S 700 decode path: add RID 0x06 handler for battery.charge/runtime/status
- Wider device testing with additional UPS hardware

## Key Constraint
Never backport experimental changes to esp32-s3-nut-node.
