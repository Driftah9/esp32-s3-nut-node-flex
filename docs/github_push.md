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
v0.4

## Commit Message
v0.4 - esp32-s3-nut-node-flex - Mode 3 BRIDGE raw HID stream

- nut_bridge.c - new: Mode 3 BRIDGE task (FreeRTOS queue, TCP stream, length-prefixed protocol)
- nut_bridge.h - new: public API for nut_bridge_start()
- ups_usb_hid.h - bridge API: ups_hid_bridge_cb_t, set_bridge_cb(), get_report_descriptor()
- ups_usb_hid.c - bridge hooks: raw descriptor cache (s_raw_desc), bridge callback in intr_in_cb
- ups_usb_hid.c - bridge API implementations: set_bridge_cb, get_report_descriptor
- main.c - Mode 3 dispatch: nut_bridge_start() (was placeholder nut_server_start)
- CMakeLists.txt - add nut_bridge.c
- Wire protocol: [2B BE: desc_len][desc] handshake then [1B type][2B BE: len][data] stream
- LXC: /opt/nut-bridge/bridge_receiver.py on nut-test-lxc port 5493 (test receiver)
- Confirmed: 1049B descriptor received, 109+ interrupt-IN packets streaming at UPS poll rate

## Files Staged
- src/current/main/nut_bridge.c
- src/current/main/nut_bridge.h
- src/current/main/ups_usb_hid.h
- src/current/main/ups_usb_hid.c
- src/current/main/main.c
- src/current/main/CMakeLists.txt
- docs/github_push.md
- docs/project_state.md
- docs/next_steps.md
- docs/DECISIONS.md
- docs/session_log.md
- README.md
