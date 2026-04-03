# Feature Spec - In-Device Diagnostic Log Capture
<!-- Status: STANDBY - not scheduled, not integrated. Ready to implement when needed. -->
<!-- Created: 2026-04-03 -->

## Purpose

Allow users to capture a full boot log from within the device portal and submit it
to projects.strydertech.com for debugging. No serial terminal required. Opt-in only.
Sensitive credentials are scrubbed before any data leaves the device.

---

## Portal UI

Location: Status page or Config page (TBD at implementation time)

Controls:
- Checkbox: "Enable diagnostic capture" (default: unchecked)
- Button: "Capture and Submit Log" (disabled until checkbox is checked)

Behaviour:
- Checkbox must be checked before the button becomes clickable
- User must actively opt in - nothing runs passively
- After a successful capture + submit, both controls reset to their default state
- If submission fails, an error message appears in the portal (checkbox stays checked
  so the user can retry or uncheck to cancel)

---

## Capture Sequence

Triggered when the user checks the box and clicks the button.

1. Portal sends POST to /diag-start (or equivalent internal endpoint)
2. ESP sets NVS key "diag_cap" = 1 and reboots immediately
3. On next boot, app_main() checks "diag_cap" before anything else:
   - If 0: normal boot, no hook, zero overhead
   - If 1: install esp_log_set_vprintf() hook into a RAM ring buffer, then continue
     normal boot sequence (WiFi, USB, portal, mode dispatch all run as usual)
4. 90s capture timer starts at boot
5. At 90s: timer fires
   - Log hook removed (ring buffer frozen)
   - Scrub pass runs on ring buffer (see Sensitive Data Scrubbing below)
   - HTTP POST sent to projects.strydertech.com/submit
6. On HTTP 200 response:
   - NVS "diag_cap" cleared to 0
   - Portal status updated: "Log submitted successfully"
   - Checkbox and button reset to default state
7. On failure:
   - Retry once after 10s
   - If second attempt fails: NVS "diag_cap" cleared, portal shows error
   - User can re-enable and try again

---

## Why 90 Seconds

The capture window needs to cover the full boot + enumeration + decode + XCHK cycle:

| Event                                 | Approx time from boot |
|---------------------------------------|-----------------------|
| WiFi STA connect                      | ~2-5s                 |
| USB device enumeration                | ~1s after connect     |
| HID descriptor parse + field cache    | ~1-2s                 |
| First interrupt-IN packet decoded     | ~2-5s                 |
| XCHK settle timer fires               | ~31s                  |
| GET_REPORT probe queue drains         | ~32-35s               |
| Mode 2/3 upstream connect (if used)   | ~2-10s (concurrent)   |
| Comfortable headroom                  | to 90s                |

90s gives a complete picture of boot behaviour including XCHK and probe results,
with headroom for slow enumerators or flaky upstream connections.
Adjust if real-world data shows the window needs to be longer or shorter.

---

## Ring Buffer Sizing

Estimated log output at typical IDF INFO verbosity over 90s: 32-64KB.
Verbose debug mode could reach 128KB.

Recommended allocation: 128KB from PSRAM (device has 8MB - negligible cost).
If PSRAM is unavailable: fall back to 32KB from heap (may truncate on verbose builds).

Ring buffer behaviour when full: overwrite oldest data (circular). Most critical
events (XCHK results, probe output, decode errors) occur after the first 30s so
tail data is more valuable than head data in most cases. Alternatively: stop writing
when full and log a truncation marker. Decision deferred to implementation.

---

## Sensitive Data Scrubbing

Runs on the ring buffer after the 90s timer fires, before the POST is built.

Method: exact string search-and-replace on the flat ring buffer char array.
No regex. At submit time app_cfg_t is in memory - the exact password strings are known.

Strings replaced with "[REDACTED]":
- cfg->sta_pass         (WiFi STA password)
- cfg->ap_pass          (SoftAP password - if non-empty)
- cfg->nut_pass         (NUT protocol password)
- cfg->portal_pass      (HTTP portal login password)

Strings kept (useful debug context, not credentials):
- cfg->sta_ssid         (network name - helps identify environment)
- cfg->ap_ssid          (device identity - already in submission metadata)
- cfg->nut_user         (username only, not a secret on its own)
- cfg->upstream_host    (internal IP or hostname - useful for mode 2/3 context)

Implementation notes:
- Skip scrub pass for any field that is an empty string (open AP, unset password)
- Append to ring buffer after scrub: "[diag] scrub complete: N fields redacted"
- The scrub runs in-place on the buffer - no second allocation needed
- Passwords are typically short (under 64 chars) - scan is fast even on 128KB

---

## NVS Flag

Namespace: "cfg" (same as main config, separate key)
Key: "diag_cap"
Type: uint8

| Value | Meaning                          |
|-------|----------------------------------|
| 0     | Normal boot - no capture         |
| 1     | Capture pending - install hook   |

Set to 1 by portal on button press.
Cleared to 0 after successful submit or after retry failure.
Never left at 1 permanently - boot loop protection: if reboot count > 3 with
diag_cap=1 still set, clear it and boot normally (guard against crash during capture).

---

## Submission Endpoint

Target: https://projects.strydertech.com/submit
Method: POST
Content-Type: application/json

Payload fields:

```
{
  "fw_version":  "v0.11",
  "vid":         "051D",
  "pid":         "0002",
  "model":       "Back-UPS XS 1500M",
  "serial":      "...",
  "op_mode":     1,
  "log":         "... scrubbed log text (plain or base64) ..."
}
```

Server behaviour (separate project - not part of this firmware):
- Stores submission with timestamp
- Links to device fingerprint (VID/PID/serial)
- Open intake - no auth required to submit
- Review and triage on server side

If the server connector is not yet built at implementation time: write the log
to a /diag HTTP endpoint on the device instead, so it can be downloaded via browser.
This requires no server infrastructure and is a viable first-pass alternative.

---

## Implementation Order

When ready to build this feature:

1. **NVS flag + ring buffer + vprintf hook** (firmware - diag_capture.c new module)
   - diag_capture_check_and_arm() called early in app_main()
   - diag_capture_timer_start() called after normal init completes
   - diag_capture_get_buffer() / diag_capture_get_len() for POST path

2. **Scrub pass** (diag_capture.c)
   - diag_capture_scrub(app_cfg_t *cfg) - runs in place on ring buffer

3. **Portal UI** (http_portal.c or http_config_page.c)
   - Checkbox + button with JS enable gating
   - POST handler for /diag-start
   - Status polling or redirect after reboot

4. **HTTP POST to endpoint** (diag_capture.c or new diag_submit.c)
   - esp_http_client POST with JSON body
   - Retry logic + NVS clear on completion

5. **Server-side intake** (separate project - projects.strydertech.com)

---

## Files That Would Be Added or Modified

| File | Change |
|------|--------|
| main/diag_capture.c (NEW) | Ring buffer, vprintf hook, scrub, NVS flag, submit |
| main/diag_capture.h (NEW) | Public API |
| main/main.c | Call diag_capture_check_and_arm() near top of app_main() |
| main/http_portal.c or http_config_page.c | Checkbox, button, /diag-start handler |
| main/CMakeLists.txt | Add diag_capture.c to SRCS |
| cfg_store.h | No change - diag_cap stored as separate NVS key, not in app_cfg_t |

---

## Notes

- This feature has zero impact on normal operation when diag_cap NVS key is 0
- The device is not persistent-logging during normal use - capture is a deliberate,
  single-shot, user-initiated action
- 90s timer means the device is back to normal operation within 2 minutes of reboot
- If the user unplugs during capture: NVS flag persists, next boot will re-arm.
  This is intentional - the capture will just start over. Timer resets on each boot.
- Boot loop guard (see NVS Flag section) prevents the device from getting stuck if
  the capture itself causes a crash
