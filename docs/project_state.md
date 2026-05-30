# Project State - esp32-s3-nut-node-flex
<!-- Updated: 2026-05-30 -->

## Last CLI Run
<!-- Updated: 2026-05-30 -->
Command: idf.py build + flash + monitor (v0.44-alpha)
Result: Success
Output summary:
- Build: CLEAN - binary 0xf3d40 bytes (1003840 bytes), 76% of 4MB partition free, bootloader 0x5220 bytes 36% free
- Flash: All 3 segments verified (Hash of data verified x3), MAC 10:20:ba:4a:e4:9c
- Monitor (30s, from t=18s post-boot):
  - APC Back-UPS enumerated and decoding interrupt-IN correctly
  - battery.charge=100%, battery.runtime active (ranging 969s-1104s)
  - XCHK fired at t=31.5s: 6 RIDs seen, 5 undeclared vendor ext, 2 declared-but-silent
  - XCHK probes: rid=0x07 (battery.runtime 3-byte truncated response), rid=0x52 (2-byte response)
  - Feature polling at t=33.7s: rid=0x17 -> input.voltage=120V, rid=0x50 -> ups.load=30%
  - NUT server connection from 10.0.0.10:44554 at t=32s (HA polling confirmed working)
  - No panics, no assert fails, no WDT events
Next step: v0.44 table-driven Feature report decode implementation (APC decode funcs)

## Last GitHub Push
Version: v0.43
Tag: v0.43
Commit: cdb6d98
Message: v0.43 - Add rid=0x50 GET_REPORT polling for ups.load on APC Back-UPS
Result: Success

## Previous Push
Version: v0.42
Tag: v0.42
Commit: 1fccf39
Message: v0.42 - Silence annotate_report payload-too-short spam for truncated GET_REPORT responses
Result: Success

## Status
v0.43 - rid=0x50 (PowerConverter.PercentLoad) added to APC Back-UPS periodic GET_REPORT polling. ups.load now sourced from Feature report for PID 0x0002 (XS 1400U). Decode confirmed from NUT apc-hid.c and live debug dump.

## Parent
esp32-s3-nut-node v15.18

## Upstream Sync Baseline
Forked from: v15.18
Last synced: v15.18 (initial copy, 2026-04-02)

## CLI Build Capability
build.sh at project root - all targets CLI-driven:
- Build:         ./build.sh build
- Flash:         ./build.sh flash
- Flash+Monitor: ./build.sh flash monitor

idf.py direct (after source /home/claude/scripts/idf-activate.sh):
- Build:         idf.py build
- Flash:         idf.py -p /dev/esp32_flash flash
- Monitor:       idf.py -p /dev/esp32_flash monitor

## Implementation Progress
- [x] Scaffold, v15.18 baseline, README, DOC-REGISTRY, sync rules - v0.1
- [x] cfg_store op_mode fields + portal mode selector - v0.2
- [x] Mode 2 NUT CLIENT push task (nut_client.c) - v0.3
- [x] Mode 3 BRIDGE raw HID stream (nut_bridge.c) - v0.4
- [x] Phase 4 - Dynamic RID scanning (seen_rids bitmask + settle XCHK) - v0.6
- [x] Phase 4 - Targeted GET_REPORT probe for declared-but-silent Input RIDs - v0.7
- [x] Phase 4 - NUT mge-hid.c mapping table evaluation + annotation layer - v0.8
- [x] IDF 5.4.1 INT-IN buffer MPS alignment fix; 4MB partition table; Linux native build - v0.40
- [x] Version string fix in CMakeLists.txt; parser/get_report sync confirmed - v0.41
- [x] Silence annotate_report payload-too-short WARN spam for truncated GET_REPORT responses - v0.42
- [x] Add rid=0x50 GET_REPORT polling for ups.load on APC Back-UPS PID 0x0002 - v0.43
- [x] v0.44-alpha: Table-driven Feature report architecture (database + APC decode funcs) - In progress

## Mode Status
- Mode 1 STANDALONE: inherited from v15.18 baseline - confirmed working
- Mode 2 NUT CLIENT: nut_client.c - connect/auth/SET VAR loop - confirmed working
- Mode 3 BRIDGE: nut_bridge.c - descriptor handshake + raw intr-IN stream - confirmed working

## NUT Test LXC (10.0.0.18)
- upsd with dummy-ups "ups" device (esppush/esppush123) - Mode 2 target
- bridge_receiver.py on port 5493 - Mode 3 target
- SSH: nut-test-lxc key

## v0.43 Baseline Test (2026-05-29)
ESP32 NUT server queried via `upsc ups-test@10.0.0.190:3493`:
- Device: APC Back-UPS XS 1500M (FW:947.d10) at IP 10.0.0.190
- **ups.load: PRESENT, reporting 0%** (rid=0x50 polling working)
- All expected vars: battery.charge, battery.runtime, input.voltage, output.voltage, ups.status, device info
- Driver version: 15.19 (esp32-nut-hid)
- Status: OL (on line), no active load

Ready for: Physical USB capture comparison with NUT library direct USB access.

## Key Constraint
Never backport experimental changes to esp32-s3-nut-node.
