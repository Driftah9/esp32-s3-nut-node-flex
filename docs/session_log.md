# Session Log - esp32-s3-nut-node-flex

---

## Session 001 - 2026-04-02

Tags: scaffold, baseline, v0.1, initial-commit, readme, sync-rules, doc-registry

Work completed:
- Loaded project context - confirmed src\ did not exist yet
- Copied v15.18 baseline from esp32-s3-nut-node\src\current\ into src\current\
  (38 source files: main.c, cfg_store, http modules, nut_server, ups_* modules, wifi_mgr)
- Created README.md - fork overview, tri-mode mode descriptions, dynamic scanning goals
- Created git-push.ps1 - unified push script (same as main project)
- Created docs\DOC-REGISTRY.md - doc update rules by change type including SYNC type
- Updated CLAUDE.md - added upstream sync rules section with safe/review classifications
- Updated docs\github_push.md - v0.1 initial commit message
- Updated docs\project_state.md - baseline ready status

Problems encountered:
- None

Files changed:
- src\current\ (38 files copied from main project v15.18)
- README.md (new)
- git-push.ps1 (new)
- docs\github_push.md (updated)
- docs\project_state.md (updated)
- docs\DOC-REGISTRY.md (new)
- docs\session_log.md (new)
- CLAUDE.md (upstream sync rules added, source note updated)

Status at end: Ready for initial GitHub push. All docs in place.
Next session starts at: Run git-push.ps1 to create repo + push v0.1. Then begin Phase 1 - cfg_store op_mode field.

---

## Session 002 - 2026-04-02

Tags: phase1, cfg_store, portal, op_mode, cli-build, idf-build, v0.2

Work completed:
- GitHub repo created and v0.1 pushed (git-push.ps1 after user ran git init in project root)
- cfg_store.h: OP_MODE_* constants, op_mode/upstream_host/port/fallback fields added
- cfg_store.c: NVS defaults for new fields (standalone, fallback=1, port=3493)
- http_config_page.c: Operating Mode selector + upstream section + JS show/hide + parse handlers
- Build confirmed clean via CLI idf-build.ps1 wrapper
- Portal UI verified via Chrome MCP (Operating Mode selector visible at 10.0.0.190)
- idf-build.ps1: created CLI build/flash/monitor wrapper - removes MSYSTEM before IDF init
- idf-build.ps1 monitor: fixed cmd.exe host + MessageData scope for output capture
- confirmedtools.md: created at D:\Users\Stryder\Documents\Claude\ClaudeContext\
- Global CLAUDE.md: updated ESP32 build rule + startup sequence
- D001 status updated: Phase 1 implemented

Problems encountered:
- IDF blocked by MSYSTEM=MINGW64: fixed by Remove-Item Env:MSYSTEM inside PowerShell session
- Monitor ProcessStartInfo: UseShellExecute=false cannot launch idf.py (wrapper script)
  Fix: use cmd.exe as FileName with /c idf.py monitor as Arguments
- Register-ObjectEvent action block scope: $stdout not visible in handler runspace
  Fix: -MessageData $stdout so action uses $Event.MessageData.Add()

Files changed:
- src\current\main\cfg_store.h (op_mode fields)
- src\current\main\cfg_store.c (defaults)
- src\current\main\http_config_page.c (mode selector + upstream section)
- idf-build.ps1 (new - CLI build wrapper)
- docs\github_push.md (v0.2 commit message)
- docs\project_state.md (phase 1 complete)
- docs\next_steps.md (push v0.2 checked, phase 2 list)
- docs\DECISIONS.md (D001 status updated)
- docs\session_log.md (this entry)
- README.md (status updated)
- CLAUDE.md (build workflow updated)
- D:\Users\Stryder\Documents\Claude\ClaudeContext\confirmedtools.md (new)
- D:\Users\Stryder\Documents\Claude\.claude\CLAUDE.md (startup sequence + build rule)

Status at end: All docs updated. v0.2 push ready.
Next session starts at: Phase 2 - Mode 2 NUT client push task. Connect to upstream upsd at 10.0.0.6.

---

## Session 003 - 2026-04-02

Tags: phase2, nut-client, mode2, lxc, set-var, push, flash-monitor, v0.3

Work completed:
- nut-test-lxc created at 10.0.0.18 (Ubuntu 24.04, SSH key nut-test-lxc)
- NUT 2.8.1 installed on LXC: upsd + dummy-ups device named "ups"
- upsd.users: esppush account with SET VAR permission - verified via upsrw
- nut_client.c + nut_client.h: Mode 2 NUT CLIENT push task
  - nc_connect: non-blocking connect with select timeout
  - nc_cmd: send line + read response, returns bool (OK prefix check)
  - nc_auth: USERNAME + PASSWORD sequence
  - nc_set_var: SET VAR with bool return (true=alive, false=socket dead)
  - nc_push_state: push battery.charge, ups.status, input.utility.present, ups.flags + optional fields
  - 5s startup delay for DHCP settle
  - Boot reachability check with fallback to nut_server_start
  - Push-based reconnect: nc_set_var return drives reconnect, no VER keepalive
- main.c: mode dispatch switch (NUT_CLIENT->nut_client, BRIDGE->nut_server, STANDALONE->nut_server)
- idf-build.ps1: flash-monitor combined target + WorkingDirectory fix + pre-flash kill python
- confirmedtools.md: flash, monitor, flash-monitor confirmed CLI + nut-test-lxc added
- Global CLAUDE.md: build workflow updated (all targets CLI-driven, flash is now CLI)

Problems encountered:
- SO_ERROR=113 EHOSTUNREACH: DHCP not settled at connect time - fixed with 5s delay
- UPS name mismatch (NVS="ups", LXC had "esp-ups"): renamed LXC device to "ups"
- VER keepalive false disconnect: VER returns version string not OK - replaced with push-based detection
- COM3 busy on second flash: lingering python monitor process - fixed with pre-flash kill
- ProcessStartInfo WorkingDirectory: Set-Location does not propagate to spawned process - fixed

Files changed:
- src\current\main\nut_client.c (new)
- src\current\main\nut_client.h (new)
- src\current\main\main.c (mode dispatch + op_mode log after NVS)
- src\current\main\CMakeLists.txt (nut_client.c added)
- idf-build.ps1 (flash-monitor, WorkingDirectory, pre-flash kill)
- docs\github_push.md (v0.3)
- docs\project_state.md
- docs\next_steps.md (Phase 2 complete)
- docs\DECISIONS.md (D001 Phase 2 status)
- docs\session_log.md (this entry)
- README.md
- D:\Users\Stryder\Documents\Claude\ClaudeContext\confirmedtools.md
- D:\Users\Stryder\Documents\Claude\.claude\CLAUDE.md

Status at end: Phase 2 complete. Mode 2 confirmed working end-to-end on nut-test-lxc.
Next session starts at: Phase 3 - Mode 3 bridge stream. Or any other priority.

---

## Session 004 - 2026-04-02

Tags: phase3, bridge, mode3, raw-hid, descriptor, stream, v0.4

Work completed:
- ups_usb_hid.h: bridge API - ups_hid_bridge_cb_t typedef, set_bridge_cb(), get_report_descriptor()
- ups_usb_hid.c: s_raw_desc static buffer, descriptor cached after fetch, bridge cb in intr_in_cb (all packets)
- ups_usb_hid.c: set_bridge_cb() and get_report_descriptor() public functions
- nut_bridge.h + nut_bridge.c: Mode 3 bridge task
  - FreeRTOS queue (depth 32, BRIDGE_PKT_MAX=64) for USB task -> bridge task delivery
  - bridge_intr_cb: non-blocking xQueueSend from USB task context
  - bridge_connect: non-blocking connect with select timeout
  - bridge_send_handshake: waits up to 20s for descriptor, sends [2B BE len][desc bytes]
  - bridge_send_packet: [1B type][2B BE len][data]
  - Keepalive: type=0xFF when no packet arrives in 5s
  - Boot reachability check with fallback to nut_server
  - Reconnect: drain queue, reconnect loop
- main.c: Mode 3 now calls nut_bridge_start() (was placeholder)
- nut-test-lxc: bridge_receiver.py at /opt/nut-bridge/bridge_receiver.py, port 5493
- Chrome MCP: set portal to Mode 3, upstream host 10.0.0.18, port 5493, submitted
- Confirmed: 1049B HID descriptor received on LXC, 109+ interrupt-IN packets streaming

Problems encountered:
- Python receiver SCP: nested quotes in PowerShell heredoc - fixed by writing file locally then SCP
- bridge_receiver.log empty (nohup stdout): actual output went to /tmp/bridge.log (script's LOG_FILE)

Files changed:
- src\current\main\nut_bridge.c (new)
- src\current\main\nut_bridge.h (new)
- src\current\main\ups_usb_hid.h (bridge API)
- src\current\main\ups_usb_hid.c (bridge hooks + API)
- src\current\main\main.c (Mode 3 dispatch fixed)
- src\current\main\CMakeLists.txt (nut_bridge.c)
- docs\github_push.md (v0.4)
- docs\project_state.md
- docs\next_steps.md (Phase 3 complete)
- docs\DECISIONS.md (D003 wire format + status)
- docs\session_log.md (this entry)
- README.md

Status at end: Phase 3 complete. All three modes confirmed working.
Next session starts at: UPS swap validation. Then Phase 4 dynamic scanning or other priorities.

---

## Session 005 - 2026-04-02

Tags: validation, ups-swap, cyberpower, cross-mode, post-config

Work completed:
- UPS swapped by user to CyberPower VID:0764 PID:0501 (ST/CP/SX Series)
- Diagnosed bridge_receiver.py stuck on zombie connection from previous session
  Fix: pkill + restart on LXC, receiver re-listening on port 5493
- Diagnosed portal chrome-computer tool blocked by extension URL restriction
  Fix: created post_config.ps1 at project root - uses GET /save with Basic auth
  Allows CLI-driven portal config without Chrome interaction
- Mode 3 BRIDGE confirmed on new UPS:
  607B HID descriptor sent on connect, interrupt-IN packets streaming
  LXC bridge.log confirmed 231+ packets with CyberPower RID pattern
- Mode 2 NUT CLIENT confirmed on new UPS:
  Authenticated as esppush on 10.0.0.18:3493
  battery.charge=100, battery.runtime=48127, battery.voltage=12.000 pushed to upsd
  upsc confirms values live
- Mode 1 STANDALONE confirmed on new UPS:
  NUT server on tcp/3493, CyberPower enumerated (14 fields, 13 RIDs)
  upsc returns full variable list, real client (10.0.0.10) connected and polled

Problems encountered:
- bridge_receiver.py single-connection - zombie connection blocked new ESP connects
  Fix: restart receiver on LXC before each Mode 3 test
- Chrome computer tool "Cannot access chrome-extension:// URL of different extension"
  Fix: post_config.ps1 script for direct portal config via PowerShell HTTP
- Mode 2 connected to wrong port (5493 bridge receiver) on first attempt
  Fix: set upstream_port to 3493 via post_config.ps1 before Mode 2 test

Files changed:
- post_config.ps1 (new - portal config utility)
- docs/project_state.md
- docs/next_steps.md
- docs/session_log.md (this entry)

Status at end: Cross-mode validation complete. All three modes confirmed on CyberPower device.
v0.4 push pending (includes all Phase 1-3 work + validation results).
Next session starts at: Push v0.4. Then Phase 4 dynamic scanning investigation.

---

## Session 006 - 2026-04-02

Tags: mode2, nut-client, identity-push, full-variable, ups-dev, dummy-ups, v0.5

Work completed:
- Researched NUT documentation: usbhid-ups, dummy-ups, clone, variable namespace
  Key finding: dummy-ups cannot create new variables via SET VAR - must pre-declare in .dev file
  Key finding: no standard NUT driver receives pushed data from a remote device
  dummy-ups + pre-declared .dev file is the correct upstream for Mode 2
- nut_client.c v0.2-flex: added nc_push_identity() function
  Pushes all static/identity variables once per connection after auth:
  device.mfr, device.model, device.serial, device.type
  ups.mfr, ups.model, ups.firmware, ups.vendorid, ups.productid
  battery.type, battery.charge.low
  DB-sourced: battery.charge.warning, battery.runtime.low, battery.voltage.nominal,
              input.voltage.nominal, ups.type
  Static: ups.delay.shutdown, ups.delay.start, ups.timer.reboot, ups.timer.shutdown,
          ups.test.result
- nut_client.c v0.2-flex: expanded nc_push_state()
  Added input.voltage and output.voltage (when valid, for GET_REPORT devices)
- ups.dev template: comprehensive pre-declaration of all ESP-pushable variables
  Deployed to /etc/nut/ups.dev on nut-test-lxc
- nut-upstream-setup.md: full upstream NUT server setup guide (new doc)
- Fixed ups.test.result "No test initiated" - ERR TOO-LONG from dummy-ups
  Changed to "None" (standard NUT value)
- Confirmed: full upsc output from Mode 2 shows all variables correctly
  device.mfr=CyberPower, device.model=ST/CP/SX Series (PID 0501), vid=0764, pid=0501
  battery.charge=100, battery.runtime=50794, battery.voltage=12.000
  ups.status=OB DISCHRG, ups.type=line-interactive, all nominals present

Problems encountered:
- op_mode=0 in NVS from previous Mode 1 test - post_config.ps1 still had op_mode=0
  Fix: corrected post_config.ps1 to op_mode=1 before flash
- ups.test.result "No test initiated" rejected by dummy-ups: ERR TOO-LONG
  Fix: changed to "None"

Files changed:
- src/current/main/nut_client.c (v0.2-flex - nc_push_identity, expanded nc_push_state)
- src/current/main/http_config_page.c (v0.2-flex - two-column layout, mode description cards)
- docs/nut-upstream-setup.md (new)
- docs/project_state.md
- docs/next_steps.md
- docs/github_push.md
- docs/session_log.md (this entry)
- LXC: /etc/nut/ups.dev (comprehensive variable template)

Status at end: Mode 2 now pushes full variable set. Any NUT client polling upsd gets
complete real data: identity, live values, nominals, housekeeping. Zero dummy leftovers.

Config page updated with mode description panel (right column, live-switching cards).
v0.5 ready to push.
Next session starts at: Push v0.5. Then Phase 4 dynamic scanning or other priorities.

---

## Session 007 - 2026-04-02

Tags: phase4, dynamic-scanning, seen-rids, xchk, bitmask, settle-timer, v0.6

Work completed:
- Phase 4 dynamic RID cross-check implemented
- ups_hid_parser.c: added s_seen_rids[32] bitmask (256-bit, one bit per RID 0x00-0xFF)
  - Cleared in ups_hid_parser_reset() on disconnect
  - Bit set in decode_report() immediately after RID byte is extracted from data[0]
  - s_xchk_timer (esp_timer_handle_t) one-shot, created and started in set_descriptor()
  - Timer fires after 30s -> xchk_timer_cb -> ups_hid_parser_run_xchk()
- ups_hid_parser_run_xchk() public function:
  - Part 1: iterates seen RIDs, checks each against s_desc.reports[] for Input declaration
    - Not declared: ESP_LOGW "[XCHK] rid=0xXX seen in traffic but NOT declared as Input"
    - Declared: ESP_LOGI "[XCHK] rid=0xXX seen, declared - OK"
  - Part 2: iterates descriptor Input reports, checks if each RID was seen in traffic
    - Never seen: ESP_LOGI "[XCHK] rid=0xXX declared as Input but never arrived"
  - Summary: counts seen / undeclared / declared-but-silent
- ups_hid_parser.h: ups_hid_parser_run_xchk() added to public API with full doc comment
- ups_hid_desc.c: static expected_rids[] block removed (lines 619-635)
  Old static list: 0x20,0x21,0x22,0x23,0x25,0x28,0x29,0x80,0x82,0x85,0x86,0x87,0x88
  Replaced with comment explaining Phase 4 supersedes it
- Build confirmed clean (idf-build.ps1 -Target build)

Problems encountered:
- None

Files changed:
- src/current/main/ups_hid_parser.c (R5 - seen_rids bitmask, xchk timer, run_xchk)
- src/current/main/ups_hid_parser.h (run_xchk public API)
- src/current/main/ups_hid_desc.c (static expected_rids[] removed)
- docs/github_push.md (v0.6)
- docs/project_state.md (Phase 4 complete)
- docs/next_steps.md (Phase 4 checked)
- docs/session_log.md (this entry)

Reboot page improvement:
- http_portal.c: /reboot now serves styled HTML with 20s JS countdown
  Ticks down live, at 0 redirects browser to /
  Device restarts immediately after page is sent (200ms delay then esp_restart)
  Added http_portal_css.h include to http_portal.c for PORTAL_CSS

Files changed (additional):
- src/current/main/http_portal.c (reboot countdown page)

Status at end: Phase 4 + reboot UX complete. Build clean. v0.6 ready to push.
Next session starts at: Push v0.6. Flash + monitor to verify reboot countdown and XCHK at 30s.

---

## Session 008 - 2026-04-03

Tags: phase4-probe, mge-hid, annotation, xchk-fix, diag-capture, v0.7-v0.14

Work completed (reconstructed from project_state - multiple sub-sessions):
- v0.7: Targeted GET_REPORT probe on descriptor-declared Input RIDs only
  xchk queues probe per unseen Input RID via ups_xchk_probe_fn_t callback
  Probe fires in usb_client_task via ups_get_report_service_queue()
  Raw hex logged as [XCHK Probe]
- v0.8: NUT mge-hid.c mapping table pattern evaluated for ESP portability
  ups_hid_map.c/h: static table covering HID pages 0x84 and 0x85, ~50 entries
  ups_hid_map_lookup() + annotate_report() functions
  ups_hid_desc_dump() appends -> nut_var_name per field
  Portability confirmed for APC/Eaton standard HID usages
  CyberPower: all 14 fields unmapped (vendor usage IDs) - direct-decode required
  D005 decision: annotation layer only, no decode migration
- v0.10: All-mode verification on APC XS 1500M (Mode 1/2/3 confirmed)
  rid=0x52 page=0x84 uid=0x0044 researched: APC non-compliant transfer voltage
  D006 documented. 5 confirmed UPS total.
- v0.11: op_mode constants renumbered 1/2/3 (was 0/1/2)
  cfg_store.h, cfg_store.c, http_config_page.c updated
  NVS old value 0 falls to default STANDALONE - no migration needed
- v0.12: diag_capture.c/h new module - in-device log capture
  /diag-start POST: sets NVS diag_dur, countdown page, reboots
  /diag-log GET: captured log as HTML with Copy button (passwords scrubbed)
  128KB PSRAM ring buffer via vprintf hook
  FreeRTOS timer fires at selected duration, marks log ready, restores hook
- v0.14: XCHK probe size cap fix (two locations: 16->64 bytes)
  Root cause: PowerWalker VI 3000 RLE (0764:0601) crash-loop every ~34s
  rid=0x28 declared 63 bytes, probed with wLength=16, IDF v5.5.4 DWC assert fires
  Fix: buf[16]->buf[64], cap 16->64 in both ups_hid_parser.c and ups_get_report.c
  Confirmed on APC XS 1500M: rid=0x07 (50 bytes) probed with wlen=50, no crash

Problems encountered:
- PORTAL_CSS inclusion caused stack overflow on simple pages - removed from 3 pages

Files changed:
- src/current/main/ups_hid_parser.c (R5-R7, seen_rids, xchk probe, mge annotation, probe cap)
- src/current/main/ups_hid_desc.c (expected_rids removed, dump annotation)
- src/current/main/ups_get_report.c (R1-R2, probe queue service, cap fix)
- src/current/main/ups_usb_hid.c (xchk probe callback registration)
- src/current/main/ups_hid_map.c + ups_hid_map.h (new - mge-hid annotation layer)
- src/current/main/diag_capture.c + diag_capture.h (new - log capture module)
- src/current/main/http_portal.c (diag endpoints, reboot countdown)
- src/current/main/cfg_store.h + cfg_store.c (op_mode renumber, diag_dur NVS key)
- src/current/main/http_config_page.c (mode renumber, two-column layout)
- src/current/main/CMakeLists.txt (ups_hid_map, diag_capture added)
- docs/ (multiple updates)

Status at end: v0.14 pushed. Dynamic scanning, annotation, diag-capture all working.
Next session: Eaton 3S 700 submission analysis.

---

## Session 009 - 2026-04-03 to 2026-04-04

Tags: eaton-decode, cyberpower-goto, submission-analysis, v0.15, v0.16

Work completed:
- Inspected 4 staging submissions (read-only, main repo and flex repo data)
- Eaton 3S 700 (VID:0463 PID:FFFF) - 3 submissions analyzed:
  rid=0x06 interrupt-IN identified as primary data source (byte1=charge%, bytes2-3=runtime LE)
  rid=0x20 GET_REPORT confirmed returning wrong value (2% on full batteries across all 3 subs)
  flags[3:4]=0x0000 -> OL/input_utility_present=true confirmed from interrupt-IN sample
- v0.15 Eaton decode changes:
  ups_get_report.c decode_eaton_feature() rid=0x20: state-apply removed, log-only
  ups_hid_parser.c DECODE_EATON_MGE rid=0x06: flags[3:4] decode added
  ups_db_eaton.c: corrected comments (rid=0x06 as primary, rid=0x20 as log-only)
  diag_capture.c: inject app+IDF version lines at arm time (t=290ms lines missed otherwise)
  CMakeLists.txt: project renamed to esp32-s3-nut-node-flex
  git-push.ps1: Discord push notification added (channel_id per-project)
- CyberPower 3000R (VID:0764 PID:0601) submission analyzed:
  DB match confirmed: DECODE_CYBERPOWER, quirks=0x002f
  Interrupt-IN: rid=0x08 byte[0]=0x64=100% every 2s, rid=0x0B byte[0]=0x13 every 2s
  Bug found: "if (rid != 0x20) goto finalize" silently discarded rid=0x08 battery.charge
  Field cache had battery.charge at rid=0x08 (correct), but decode path never reached it
  PID 0601 vs 0501 distinction: 0501 uses vendor RIDs 0x20-0x88, 0601 uses standard HIDs
- v0.16 CyberPower goto fix:
  ups_hid_parser.c: "if (rid != 0x20) goto finalize" changed to
  "if (changed) goto finalize" - CP direct decode returns false for unknown RIDs,
  those now fall through to standard field-cache path
  ups_hid_parser.c: rid=0x0B case added to decode_cyberpower_direct() for diagnostic logging
  R9 version history entry added
  ups_db_cyberpower.c: PID 0601 corrected, known_good=false, comment updated
- Build clean (idf-build.ps1 -Target build, zero warnings)

Problems encountered:
- Previous session context compacted - resumed from summary

Files changed:
- src/current/main/ups_hid_parser.c (R8 Eaton, R9 CyberPower goto fix + rid=0x0B)
- src/current/main/ups_get_report.c (Eaton rid=0x20 log-only)
- src/current/main/ups_db_eaton.c (comment corrections)
- src/current/main/ups_db_cyberpower.c (PID 0601 corrected)
- src/current/main/diag_capture.c (version injection at arm time)
- src/current/CMakeLists.txt (project rename)
- git-push.ps1 (Discord notification)
- docs/github_push.md (v0.16)
- docs/project_state.md (v0.16 status)
- docs/next_steps.md (CyberPower 3000R follow-up)
- docs/session_log.md (this entry)

Status at end: v0.16 build clean. Pushed to GitHub.
Next session: Flash v0.16, monitor for rid=0x08 battery.charge applying correctly.

---

## Session 006 - 2026-04-05

Tags: v0.17, v0.18, v0.19, v0.20, cyberpower, crash-fix, debounce, dwc-assert

Work completed (context-compacted session - covers v0.17 through v0.20):

v0.17 - CyberPower 3000R WDT fix + version string cleanup:
- ups_hid_desc.c: per-field loop demoted ESP_LOGI -> ESP_LOGD (rid=0x29 has 237 fields)
- http_dashboard.c: hardcoded "v0.6-flex" subtitle replaced with esp_app_get_description()
- http_portal.c: hardcoded "15.13" driver_version replaced with app description version

v0.18 - Per-RID EMA interval learning + status debounce:
- ups_hid_parser.c: three new static arrays (s_rid_last_ms, s_rid_ema_ms, s_rid_samples)
  track inter-report interval per RID. EMA = (ema * 7 + delta) / 8
- ups_state_update_t: source_rid + status_debounce_ms fields added
- ups_state.c: s_pending_status debounce logic - new status must persist 1.5x EMA before commit
- ups_state_t: data_age_ms added, computed at snapshot time
- http_dashboard.c: "UPS data: Xs old" below status badge
- http_portal.c: data_age_ms in /status JSON

v0.19 - Fix abort in ups_state_apply_update (multi-core ESP32-S3):
- v0.18 added ESP_LOGI inside portENTER_CRITICAL -> abort on first battery.charge decode
- Confirmed from APC Back-UPS XS 1500M test: crash at t=1492ms
- Fix: capture log params as locals inside CS, exit CS, then emit log via switch

v0.20 - GET_REPORT DWC OTG buffer overflow fix:
- Analyzed CyberPower 3000R submission a0043f (sollandk/redandblue, v0.17-dirty)
- 8803-line log: crash-loop every boot after XCHK 30s settle fires rid=0x28 probe
- Root cause: device returns more data than descriptor declares (63 bytes) for rid=0x28
  DWC assert: _buffer_parse_ctrl hcd_dwc.c:2341 rem_len check fires
- ups_get_report.c: alloc = 8 + buf_sz + 64 (was 8 + buf_sz), wLength unchanged
- Also analyzed: ups.status MISSING (no rid=0x80 ac_present), battery.runtime/voltage
  only in Feature reports, ups.load/input.voltage/output.voltage absent from descriptor

Problems encountered:
- v0.18 introduced ESP_LOGI inside portENTER_CRITICAL (fixed in v0.19)
- CyberPower 3000R DWC assert was a second layer beyond the v0.14 wLength fix

Files changed:
- src/current/main/ups_hid_desc.c (v0.17 LOGD demotion)
- src/current/main/ups_hid_parser.c (v0.18 EMA tracking + v0.20 no change)
- src/current/main/ups_state.h (v0.18 new fields)
- src/current/main/ups_state.c (v0.18 debounce + v0.19 deferred logging)
- src/current/main/http_dashboard.c (v0.17 version, v0.18 data_age)
- src/current/main/http_portal.c (v0.17 version, v0.18 data_age_ms JSON)
- src/current/main/ups_get_report.c (v0.20 alloc padding)
- docs/github_push.md (v0.20)
- docs/project_state.md (v0.20)
- docs/next_steps.md (v0.20 analysis)
- docs/session_log.md (this entry)

Status at end: v0.20 build clean. Ready to push.
Next session: Flash v0.20, request re-submission from sollandk for CyberPower 3000R.

---

## Session 007 - 2026-04-05

Tags: v0.21, v0.22, v0.23, v0.24, dashboard-poll, diag-capture, community-fix, mode-validation, ha-zero-data

Work completed (context-compacted session - covers v0.21 through v0.24):

v0.21 - Adaptive dashboard poll rate:
- http_dashboard.c: replaced fixed setInterval(5000) with setTimeout-based adaptive polling
  While ups_valid=false: poll every 1500ms. Once valid: poll every 5000ms.
  schedPoll(ms) wrapper manages timer with clearTimeout/setTimeout
  XHR errors fall back to 1500ms. onerror handler added.
  data_age block: shows "Waiting for UPS data..." while !ups_valid, age otherwise
  Motivation: Eaton 3S and other event-driven devices may not send data until mains event
  Dashboard now reflects first data the moment it arrives without user refresh

v0.22 - 300s diagnostic log capture option:
- diag_capture.h: description updated, get_duration() return type uint8_t -> uint16_t
- diag_capture.c: s_duration uint8_t -> uint16_t, dur uint8_t -> uint16_t
  nvs_get_u8 -> nvs_get_u16, nvs_set_u8 -> nvs_set_u16
  Whitelist: dur != 90 && dur != 120 now also checks dur != 300
- http_portal.c: dur uint8_t -> uint16_t, 300 case added, nvs_set_u8 -> nvs_set_u16
- http_dashboard.c: 300s radio button added, 120s label style updated
  NVS note: old u8 key returns ESP_ERR_NVS_TYPE_MISMATCH, falls to dur=0 (no capture) safely

v0.23 - Remove unused strlcpy0 from http_dashboard.c:
- http_dashboard.c: removed static strlcpy0 helper (5 lines, function never called)
  Reported by Eaton community user building locally (IDF v5.5.3) - -Wunused-function warning

v0.24 - Config mode validation + NUT server battery.charge valid gate:
- http_config_page.c: form onsubmit='return chkSave()' added
  upstream_host input given id='upstream_host'
  Inline error div added (upstream_err, red, hidden by default)
  chkSave() function: blocks save if Mode 2 or 3 with blank upstream_host
  onModeChange() updated to clear upstream_err on mode change
- nut_server.c: battery.charge VAR send wrapped in if (st->valid)
  Prevents HA from receiving "0" during boot window before first UPS data arrives
  All other gated variables (runtime, voltage, load) already had valid checks

Problems encountered:
- v0.21 initial push: ellipsis "..." in commit message body caused PowerShell parse error
  Fix: simplified commit message to remove problematic characters

Submissions reviewed this session:
- a0043f (CyberPower 3000R, sollandk/redandblue): DWC assert root cause confirmed, v0.20 fix
- bc8a09 (Eaton 3S 700, MyDisplayName): NOT a firmware bug - user had Mode 3 Bridge configured
  with no upstream_host set. Bridge has nowhere to connect. v0.24 validation prevents this.
- 9543fe (Eaton M5Stack Atom S3 Lite, MyDisplayName): incomplete log, needs 90s+ without DEBUG level
- 77eaee (Eaton 3S 700, standard ESP32-S3): confirmed working, battery.charge=89% correct

Files changed:
- src/current/main/http_dashboard.c (v0.21 adaptive poll, v0.22 300s button, v0.23 strlcpy0 removed)
- src/current/main/diag_capture.h (v0.22 uint16_t return type)
- src/current/main/diag_capture.c (v0.22 uint16_t throughout, nvs_set/get_u16)
- src/current/main/http_portal.c (v0.22 uint16_t, 300 case, nvs_set_u16)
- src/current/main/http_config_page.c (v0.24 chkSave validation)
- src/current/main/nut_server.c (v0.24 battery.charge valid gate)
- docs/github_push.md (v0.24)
- docs/project_state.md (v0.24)
- docs/next_steps.md (v0.24 follow-up items)
- docs/session_log.md (this entry)

Status at end: v0.24 build clean. Push in progress.
Next session: Flash v0.24. Request re-submission from sollandk (CyberPower 3000R, confirm no crash loop).
