# Project State - esp32-s3-nut-node-flex
<!-- Updated: 2026-05-30 v0.47 -->

## Last CLI Run
<!-- Updated: 2026-05-30 -->
Command: idf.py build (v0.47 - /compat page update, version bump)
Result: Pending build
Output summary:
- Version bump: v0.46 -> v0.47
- Changed files: http_compat.c, CMakeLists.txt, main.c
- No errors expected (content-only + version string)
Next step: Push to GitHub

## Last GitHub Push
Version: v0.47
Tag: v0.47
Commit: pending
Message: v0.47 - Update /compat page: 6 confirmed devices (APC + CyberPower)
Result: Pending

## Previous Push
Version: v0.46
Tag: v0.46
Commit: b8c1ba2
Message: v0.46 - CyberPower 0601 ups.load fix via rid=0x1D interrupt-IN decode
Result: Success

## Status
v0.47 - /compat page updated: 6 confirmed devices listed (APC Back-UPS XS 1300G, XS 1500M, Pro 1000/1000S, Smart-UPS C 1500, CyberPower CP550HG/SX550G, CST150UC). 0601 rackmount series other models promoted to ST_EX. Version bump only - no firmware logic changes from v0.46.

v0.46 baseline: CyberPower 0601 ups.load fix: rid=0x1D interrupt-IN byte[0] decoded as tentative ups.load (field cache MISSING for this device - descriptor puts PercentLoad on non-standard page filtered by our parser). rid=0x19 silently consumed. Needs load-test confirmation.

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
- [x] Table-driven Feature report architecture (database + APC decode funcs, hardware validated) - v0.44
- [x] Table-driven Feature report polling (generic walker replaces hardcoded dispatch) - v0.45
- [x] CyberPower 0601 ups.load fix: rid=0x1D interrupt-IN decode (tentative, needs load-test) - v0.46
- [x] /compat page update: 6 confirmed devices, 0601 series ST_EX promotion - v0.47

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
