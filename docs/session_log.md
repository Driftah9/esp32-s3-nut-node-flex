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
