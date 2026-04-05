# Project State - esp32-s3-nut-node-flex
<!-- Updated: 2026-04-05 -->

## Status
v0.17 - Task Watchdog fix + version string cleanup. Build clean. Ready to push.
- Fix: ups_hid_desc_dump() per-field loop demoted ESP_LOGI -> ESP_LOGD
  (CyberPower 3000R crash-loop: rid=0x29 has 237 fields, INFO loop starved IDLE0)
- Fix: dashboard subtitle "v0.6-flex" replaced with esp_app_get_description()->version
- Fix: /status JSON driver_version "15.13" replaced with esp_app_get_description()->version
- Submission analysis: 2 Eaton 3S (confirmed working), 1 CyberPower 3000R (WDT root cause found)

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
2026-04-05 - v0.17: Task Watchdog crash fix + version string cleanup.
Root cause from CyberPower 3000R submission (sollandk): ups_hid_desc_dump() looped
over 237 fields at ESP_LOGI level (each ~10ms). IDLE0 on core 0 was starved
past the TWDT threshold, triggering watchdog and USB re-enumeration every ~11s.
Fix: per-field loop in ups_hid_desc_dump() changed to ESP_LOGD. Summary line kept
at INFO. Normal builds now emit 1 line per device connection instead of 237.
Bonus: "v0.6-flex" hardcoded subtitle replaced with esp_app_get_description()->version.
driver_version in /status JSON "15.13" literal replaced with app description version.
Submissions analyzed: 2 Eaton 3S (77eaee confirmed working, 9543fe incomplete log),
1 CyberPower 3000R (9b89d6 confirmed goto fix working, WDT root cause identified).
Build: clean.

Previous: 2026-04-04 - v0.16: CyberPower 3000R goto fix (submission 2026-04-04).
ups_hid_parse_report(): changed "if (rid != 0x20) goto finalize" to
"if (changed) goto finalize". Old code silently discarded all RIDs except
0x20 without running standard field-cache path. CyberPower 3000R sends
battery.charge on rid=0x08 (standard HID), which was in the cache but
never applied. rid=0x0B case added to decode_cyberpower_direct() for
diagnostic logging. ups_db_cyberpower.c PID 0601 corrected.
Build: clean.

Previous: 2026-04-03 - v0.15: Eaton/MGE decode update + diag_capture header fix.
diag_capture.c R1: injects "I (0) app_init: App version: vX.Y" and
"I (0) app_init: ESP-IDF: vX.Y" directly into buffer before hook installs.
App_init lines run at ~290ms, capture arms at ~495ms - always missing otherwise.
Confirmed missing from Back-UPS XS 1500M test log. Fix uses esp_app_get_description().
Build clean.

Previous: 2026-04-03 - v0.15: Eaton/MGE decode update from 3-submission analysis.
Three changes, build clean:
1. ups_get_report.c decode_eaton_feature() rid=0x20: changed from state-apply to log-only.
   Confirmed across all 3 submissions: returns 2% on fully charged batteries. Not live charge.
   Real charge comes from rid=0x06 interrupt-IN. Removing the apply prevents 30s GET_REPORT
   poll from overwriting correct value after rid=0x06 fires.
2. ups_hid_parser.c DECODE_EATON_MGE rid=0x06: added flags[3:4] decode.
   flags=0x0000 -> input_utility_present=true (OL confirmed from submission sample).
   Non-zero flags logged as WARN for future OB analysis.
3. ups_db_eaton.c: corrected comments. rid=0x06 documented as primary source.
   rid=0x20 noted as wrong. rid=0xFD (0x29=41) noted as TBD.
R8 added to ups_hid_parser.c version history.

Previous: 2026-04-03 - v0.14: XCHK probe size cap fix - two locations.

Previous (2026-04-03) - v0.12: diag_capture.c/h (new module). Dashboard capture section (radio + button).
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
Flash v0.17 and monitor. CyberPower 3000R user should re-submit to confirm WDT fix.
- Should no longer crash-loop every ~11s
- battery.charge should still read correctly (rid=0x08 from v0.16 fix)
- rid=0x0B value meaning still TBD (need discharge event)

Eaton 3S (9543fe/M5Stack) user: submit longer log (90s+) to diagnose battery.charge issue.
- First submission (77eaee) showed correct battery.charge=89% on standard ESP32-S3
- M5Stack Atom S3 Lite may have USB timing differences - need full operational log

Candidate next tasks:
- D002: Mode 1 fallback when upstream unreachable (Mode 2/3 boot fail)
- Bridge GET_REPORT forwarding (type=0x02) for Feature reports
- APC direct-decode: add input.transfer.low/high from rid=0x52 (D006)
- Eaton: capture OB discharge event log to decode rid=0x06 flags non-zero case
- Eaton: decode rid=0xFD (need second submission at different charge state)
- CyberPower 3000R: need discharge event to decode rid=0x0B (value 0x13=19 seen on AC)
- Wider device testing with additional UPS hardware

## Key Constraint
Never backport experimental changes to esp32-s3-nut-node.
