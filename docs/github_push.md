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
v0.25

## Commit Message
v0.25 - Eaton OB decode fix + SET_IDLE + rid=0x21 heartbeat + bootstrap probes

Fix: Eaton OB status permanently stuck OL after first mains-online event.
ups_hid_parser.c rid=0x06 and rid=0x21 decode, ups_get_report.c rid=0x06
in decode_eaton_feature() all set input_utility_present_valid=true but only
set input_utility_present=true on flags=0x0000. Non-zero flags (OB/mains
loss) fell through with no update - UPS stayed OL forever. Fix: always set
valid=true, let the bool reflect (flags == 0x0000). Any non-zero = OB.

Add: SET_IDLE in ups_usb_hid.c after interface claim. HID SET_IDLE with
duration=4 (16ms) forces periodic interrupt-IN from event-driven devices
(Eaton/MGE). Without this, Eaton 3S only sends on mains events - first
data may not arrive for minutes on stable AC. STALL response ignored.

Add: rid=0x21 decode in DECODE_EATON_MGE path (ups_hid_parser.c). Same
byte layout as rid=0x06 - treated as steady-state heartbeat. Confirmed
from three Eaton 3S 700 submissions. Sanity guards protect format.

Add: Unrecognised Eaton RID raw logging (0x2x, 0x8x range) at INFO level.

Add: XCHK settle timer reduced from 30s to 5s for DECODE_EATON_MGE.
rid=0x06 is event-driven and may never arrive on stable AC - shorter
settle keeps XCHK useful for declared Input RIDs.

Add: ups_usb_hid.c Step 7b - Eaton bootstrap GET_REPORT probes at
enumeration (rid=0x20 charge, rid=0x06 state) without waiting for XCHK.
rid=0x20 is Feature-only and invisible to XCHK - must probe explicitly.

Add: ups_get_report.c service_probe_queue() routes Eaton probe responses
through decode_eaton_feature() - previously all probe responses were logged
only, making bootstrap probes inert.

Add: ups_get_report.c decode_eaton_feature() rid=0x06 case - applies
Feature GET_REPORT response to state with same format as interrupt-IN.

Build: clean, zero warnings. Tested on APC Back-UPS XS 1500M - charge,
runtime, voltage all correct. OB fix requires Eaton 3S test for full
confirmation (no OB event log available on APC).

## Files Staged
- src/current/main/ups_hid_parser.c
- src/current/main/ups_get_report.c
- src/current/main/ups_usb_hid.c
- docs/github_push.md
- docs/project_state.md
