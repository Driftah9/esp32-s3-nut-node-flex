# Project State - esp32-s3-nut-node-flex
<!-- Updated: 2026-05-24 -->

## Last GitHub Push
Version: v0.40
Tag: v0.40
Commit: f9404a0
Message: v0.40 - Fix INT-IN buffer MPS alignment for IDF 5.4.1; 4MB partition table; Linux native build
Result: Success

## Status
v0.40 - IDF 5.4.1 USB host MPS alignment + partition table expansion. Build clean. Pushed.
- ups_usb_hid.c: INT-IN buffer size rounded up to MPS multiple (IDF 5.4.1 stricter validation)
  Fixes interrupt-IN reader init failure on APC (MPS=8, buffer=50 -> 56)
  Result: battery.charge, battery.voltage now flowing (were showing MISSING before)
- partitions.csv: custom table with 4MB app partition + 12MB free for OTA/future
  Replaces default 1MB partition table (96->24% utilization)
- Build environment: Linux native (claude-brain VM) on ESP-IDF 5.4.1
  Previous: Windows SMB + PowerShell build. Now: idf.py direct, cleaner workflow
- APC Back-UPS 1500/XS 1500M (FW:947.d10, PID:0002) re-confirmed on v0.40
- XCHK dynamic scanning: consistent vendor extension pattern (5 undeclared RIDs)
  Logged as WARN for investigation. Plan: suppress for confirmed VID:PIDs (Phase 5)

## Previous Status
v0.39 - Fix HA NUT integration availability (from confirmed working files). Pushed.

## Parent
esp32-s3-nut-node v15.18

## Upstream Sync Baseline
Forked from: v15.18
Last synced: v15.18 (initial copy, 2026-04-02)

## CLI Build Capability
Linux native on claude-brain VM (10.0.0.7):
- Activate: source /home/claude/.espressif/esp-idf/export.sh
- Build:    cd /home/claude/projects/esp32-s3-nut-node-flex/src/current && idf.py build
- Flash:    idf.py -p /dev/esp32_flash flash
- Monitor:  idf.py -p /dev/esp32_flash monitor

## Implementation Progress
- [x] Scaffold, v15.18 baseline, README, DOC-REGISTRY, sync rules - v0.1
- [x] cfg_store op_mode fields + portal mode selector - v0.2
- [x] Mode 2 NUT CLIENT push task (nut_client.c) - v0.3
- [x] Mode 3 BRIDGE raw HID stream (nut_bridge.c) - v0.4
- [x] Phase 4 - Dynamic RID scanning (seen_rids bitmask + settle XCHK) - v0.6
- [x] Phase 4 - Targeted GET_REPORT probe for declared-but-silent Input RIDs - v0.7
- [x] Phase 4 - NUT mge-hid.c mapping table evaluation + annotation layer - v0.8
- [x] Phase 5 - XCHK logging suppression for confirmed VID:PIDs (ups_var_store) - v0.40

## Mode Status
- Mode 1 STANDALONE: inherited from v15.18 baseline - confirmed working
- Mode 2 NUT CLIENT: nut_client.c - connect/auth/SET VAR loop - confirmed working
- Mode 3 BRIDGE: nut_bridge.c - descriptor handshake + raw intr-IN stream - confirmed working

## NUT Test LXC (10.0.0.18)
- upsd with dummy-ups "ups" device (esppush/esppush123) - Mode 2 target
- bridge_receiver.py on port 5493 - Mode 3 target
- SSH: nut-test-lxc key

## Last Action
2026-05-24 - v0.40: INT-IN buffer MPS alignment fix + 4MB partition table + Linux native build.
ups_usb_hid.c: buffer size rounded up to MPS multiple (IDF 5.4.1 stricter validation).
partitions.csv: new custom partition table, 4MB app + 12MB free.
sdkconfig: CONFIG_PARTITION_TABLE_CUSTOM enabled.
CLAUDE.md: Linux native build as primary workflow.
ups_var_store.c/h: new module (variable store for XCHK suppression groundwork).
Build: clean. Pushed as v0.40.

Previous: 2026-04-15 - v0.39: Fix HA NUT integration availability.

Previous: 2026-04-06 - v0.27: Eaton OL/OB fix: vendor page 0xFFFF, pre-seed OL, OB-only flags.
ups_hid_parser.c: Eaton 3S has 111 fields all on vendor page 0xFFFF - parser was
  filtering them out. Fix: include 0xFFFF in descriptor interesting filter and field
  cache scan. OL/OB now from standard-path field cache (same UIDs as standard pages).
  Demote flags-based OL from rid=0x06/0x21: flags=0x0000 in ALL submissions including
  mains-loss events. Only non-zero flags now trigger OB.
ups_usb_hid.c: Pre-seed OL status at enumeration. rid=0x21 heartbeat takes 20-30s
  before first arrival; without the seed NUT returns UNKNOWN during that window.
ups_get_report.c: Add rid=0x85 to Eaton bootstrap probe queue (speculative OB probe).
Build: clean.

Previous: 2026-04-06 - v0.26: Eaton OB diagnostic improvements.
Previous: 2026-04-06 - v0.25b: extern C guard on app_main, IDF target v5.5.4.
Previous: 2026-04-06 - v0.25a: Fix send_set_idle() forward declaration ordering.
Previous: 2026-04-06 - v0.25: Eaton OB decode fix, SET_IDLE, heartbeat, probes.
Previous: 2026-04-05 - v0.24: Config mode validation + NUT server battery.charge valid gate.
Previous: 2026-04-05 - v0.23: Remove unused strlcpy0 from http_dashboard.c.
Previous: 2026-04-05 - v0.22: Add 300s diagnostic log capture option.
Previous: 2026-04-05 - v0.21: Adaptive dashboard poll rate.
Previous: 2026-04-05 - v0.20: Fix GET_REPORT DWC OTG buffer overflow assert (CyberPower 3000R).
Previous: 2026-04-05 - v0.19: Fix abort in ups_state_apply_update (multi-core ESP32-S3).
Previous: 2026-04-05 - v0.18: Per-RID interval learning + status debounce.
Previous: 2026-04-05 - v0.17: Task Watchdog crash fix + version string cleanup.
Previous: 2026-04-04 - v0.16: CyberPower 3000R goto fix.
Previous: 2026-04-03 - v0.15: Eaton/MGE decode update + diag_capture header fix.
Previous: 2026-04-03 - v0.14: XCHK probe size cap fix.
Previous (2026-04-03) - v0.12: diag_capture.c/h (new module).
Previous (2026-04-03) - v0.11: op_mode constants renumbered 1/2/3.
Previous (2026-04-02) - v0.10: All-mode verification on APC XS 1500M.

## Next Step
Candidate next tasks:
- Phase 5 complete: XCHK suppression for confirmed VID:PIDs (ups_var_store groundwork in place)
- CyberPower 3000R ups.status: decode rid=0x0B bits to extract ac_present (0x03/0x04 observed)
- CyberPower 3000R battery.runtime/voltage: add Feature report polling for rid=0x08 byte 2 and rid=0x07
- D002: Mode 1 fallback when upstream unreachable (Mode 2/3 boot fail)
- Bridge GET_REPORT forwarding (type=0x02) for Feature reports
- APC direct-decode: add input.transfer.low/high from rid=0x52 (D006)
- Eaton: capture OB discharge event log to decode rid=0x06 flags non-zero case
- Eaton: decode rid=0xFD (need second submission at different charge state)
- Wider device testing with additional UPS hardware

## Key Constraint
Never backport experimental changes to esp32-s3-nut-node.
