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
v0.20

## Commit Message
v0.20 - Fix GET_REPORT DWC OTG buffer overflow on CyberPower 3000R

CyberPower 3000R (0764:0601) returns more data for rid=0x28 than its
descriptor declares (63 bytes). XCHK probe issues GET_REPORT with
wLength=63, device sends back more bytes. DWC OTG HCI asserts:

  _buffer_parse_ctrl hcd_dwc.c:2341
  (rem_len <= transfer->num_bytes - sizeof(usb_setup_packet_t))

Transfer was allocated as 8 + buf_sz = 71 bytes. With rem_len > 63
the assert fires, aborting and triggering a crash-loop every boot
after the 30s XCHK settle window.

Fix: pad GET_REPORT transfer allocation by 64 bytes beyond declared
report size. wLength in setup packet unchanged - device still told to
send buf_sz bytes. ctrl_cb clips payload to CTRL_PAYLOAD_MAX=24.

Distinct from v0.14 fix (which matched wLength to declared size).
This fix handles devices that ignore wLength and send extra bytes.

Confirmed from submission a0043f: 8803-line crash-loop log, same
assert repeated on every boot after 30s XCHK window.

## Files Staged
- src/current/main/ups_get_report.c
- docs/github_push.md
- docs/project_state.md
- docs/next_steps.md
- docs/session_log.md
