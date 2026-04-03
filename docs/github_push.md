# GitHub Push - esp32-s3-nut-node-flex

> Claude updates this file whenever code changes during a session.
> Keep this current - it is the single source of truth for every push.

---

## Project
esp32-s3-nut-node-flex

## Repo
https://github.com/Driftah9/esp32-s3-nut-node-flex

## Visibility
public

## Branch
main

## Version
v0.5

## Commit Message
v0.5 - esp32-s3-nut-node-flex - Mode 3 BRIDGE + cross-mode validation + Mode 2 full variable push

Sessions 004-006: Phase 3 BRIDGE + validation + Mode 2 expanded push

Mode 3 BRIDGE (v0.4):
- nut_bridge.c - new: BRIDGE task (FreeRTOS queue, TCP stream, length-prefixed wire protocol)
- nut_bridge.h - new: public API nut_bridge_start()
- ups_usb_hid.h/c - bridge API: descriptor cache, bridge callback in intr_in_cb
- main.c - Mode 3 dispatch: nut_bridge_start()
- CMakeLists.txt - add nut_bridge.c
- Wire format: [2B BE desc_len][desc] handshake, [1B type][2B BE len][data] stream
- LXC: /opt/nut-bridge/bridge_receiver.py on port 5493 (Python test receiver)
- post_config.ps1 - portal config utility (GET /save with Basic auth, CLI-driven)
- Cross-mode validation on CyberPower VID:0764 PID:0501 (ST/CP/SX Series) - all confirmed

Mode 2 full variable push (v0.5):
- nut_client.c v0.2-flex - nc_push_identity(): full static/identity push on connect
  Pushes: device.mfr/model/serial/type, ups.mfr/model/firmware/vendorid/productid
  DB-sourced: battery.voltage.nominal, battery.runtime.low, battery.charge.warning,
              input.voltage.nominal, ups.type
  Static: battery.type, battery.charge.low, all housekeeping vars
- nut_client.c - nc_push_state(): added input.voltage and output.voltage
- http_config_page.c v0.2-flex - two-column layout: mode description panel right column
  Live-switching mode cards (Standalone / NUT Client / Bridge) via JS
  Each card shows: what the mode does, requirements, notes
  Responsive: stacks vertically on narrow screens
- docs/nut-upstream-setup.md - new: full upstream NUT server setup guide
- LXC /etc/nut/ups.dev - comprehensive variable template (all pushable vars pre-declared)
- Confirmed: upsc on LXC returns complete real data, zero dummy identity leftovers

## Files Staged
- src/current/main/nut_bridge.c
- src/current/main/nut_bridge.h
- src/current/main/ups_usb_hid.h
- src/current/main/ups_usb_hid.c
- src/current/main/main.c
- src/current/main/nut_client.c
- src/current/main/http_config_page.c
- src/current/main/CMakeLists.txt
- post_config.ps1
- docs/github_push.md
- docs/project_state.md
- docs/next_steps.md
- docs/DECISIONS.md
- docs/session_log.md
- docs/nut-upstream-setup.md
- README.md
