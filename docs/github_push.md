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
v0.10

## Commit Message
v0.10 - esp32-s3-nut-node-flex - D006 + all-mode verification on APC XS 1500M

Session 008: APC HID non-compliance documented, all three modes verified

D006 (DECISIONS.md): APC Back-UPS rid=0x52 HID non-compliance
- USB HID spec page=0x84 uid=0x0044 = ConfigActivePower (rated watts)
- APC Back-UPS (VID:051D PID:0002) uses this field for input transfer voltage threshold
- Standard transfer usages are uid=0x0053 (LowVoltageTransfer) and uid=0x0054 (HighVoltageTransfer)
- APC Smart-UPS uses correct spec usages; Back-UPS does not
- Probe values: XS 1500M=132V (medium sensitivity high transfer), RS 1000MS/BR1000G=88V (low sensitivity low transfer)
- APC sensitivity table: High=106/127V, Medium=97/132V, Low=88/136V
- NUT has apc_fix_report_desc() to patch the malformed Back-UPS descriptor
- Decision: do NOT add uid=0x0044 to generic ups_hid_map.c - APC direct-decode path is correct place
- D005 note corrected: removed incorrect ups.output.voltage candidate note, replaced with D006 reference

All-mode verification - APC Back-UPS XS 1500M (VID:051D PID:0002, FW:947.d10):
- Mode 0 STANDALONE: upsc direct on ESP port 3493 confirmed from NUT LXC
  Full variable set: battery.charge/runtime/voltage, input.voltage=120V, output.voltage=120V,
  ups.status=OL, device.model correct, serial/firmware correct
- Mode 1 NUT CLIENT: push to NUT LXC dummy-ups confirmed, all vars flowing
- Mode 2 BRIDGE: 1049B APC descriptor handshake + interrupt-IN stream confirmed
  bridge_receiver.py on LXC port 5493 - 18+ packets logged, multiple RID types

docs/DECISIONS.md: D006 added, D005 uid=0x0044 note corrected
docs/next_steps.md: all-mode verification section added as complete
docs/project_state.md: status updated to v0.10, last action and next steps updated
docs/github_push.md: this file

## Files Staged
- docs/DECISIONS.md
- docs/next_steps.md
- docs/project_state.md
- docs/github_push.md
