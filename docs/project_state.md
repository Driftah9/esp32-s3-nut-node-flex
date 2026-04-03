# Project State - esp32-s3-nut-node-flex
<!-- Updated: 2026-04-02 -->

## Status
v0.10 - all-mode verification complete on APC XS 1500M. D006 documented (APC HID non-compliance). Phase 4 fully closed out.

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
2026-04-02 - All-mode verification on APC XS 1500M (v0.10). D006 documented.
Mode 0 STANDALONE: upsc direct against ESP port 3493 - all vars confirmed.
Mode 1 NUT CLIENT: push to LXC dummy-ups confirmed, all vars flowing.
Mode 2 BRIDGE: 1049B descriptor + interrupt-IN stream confirmed on LXC port 5493.
rid=0x52 page=0x84 uid=0x0044 researched: APC non-compliant transfer voltage field.
uid=0x0044 per spec = ConfigActivePower, but APC Back-UPS uses it for
input transfer voltage threshold (88V/132V). Not added to generic map table.
ups_hid_map.h/c (NEW): hid_nut_entry_t { usage_page, usage_id, nut_var } static table,
~50 entries covering HID pages 0x84 (Power Device) and 0x85 (Battery System).
ups_hid_map_lookup(): linear scan, vendor page normalization.
ups_hid_map_annotate_report(): per-field decode + NUT name for a given RID.
ups_hid_desc.c: ups_hid_desc_dump() appends -> nut_var or -> unmapped per field.
ups_get_report.c: service_probe_queue() calls annotate_report() on probe responses.
ups_hid_parser.h/c: ups_hid_parser_get_desc() accessor returns stored hid_desc_t ptr.
CMakeLists.txt: ups_hid_map.c added to SRCS.
DECISIONS.md: D004 updated (implemented+confirmed), D005 added (mapping table decision).
Flashed + confirmed: CyberPower all 14 fields unmapped (all vendor usage IDs 0x008C-0x00FE).
XCHK: 0 declared-but-silent Input RIDs (both Input RIDs seen in traffic). Phase 4 complete.

## Next Step
Phase 4 closed. Consider next work:
- D002: Mode 1 fallback when upstream unreachable (Mode 2/3 boot fail)
- Bridge GET_REPORT forwarding (type=0x02) for Feature reports
- APC direct-decode: add input.transfer.low/high from rid=0x52 (D006)
- Wider device testing with additional UPS hardware

## Key Constraint
Never backport experimental changes to esp32-s3-nut-node.
