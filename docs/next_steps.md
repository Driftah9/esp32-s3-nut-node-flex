# Next Steps - esp32-s3-nut-node-flex
<!-- Updated: 2026-04-03 -->

## Immediate
- [x] Push v0.4 to GitHub

## Phase 3 - Mode 3 bridge stream - COMPLETE

- [x] ups_usb_hid bridge hooks: raw descriptor cache + interrupt-IN callback
- [x] nut_bridge.c: FreeRTOS queue (USB task -> bridge task), TCP client
- [x] Handshake: 2B-length-prefixed HID Report Descriptor on connect
- [x] Stream: [1B type][2B BE len][data] per interrupt-IN packet
- [x] Keepalive: type=0xFF sent when no packet for 5s
- [x] Boot reachability check with Mode 1 fallback
- [x] bridge_receiver.py on nut-test-lxc port 5493
- [x] Confirmed: 1049B descriptor + 109+ packets streaming, raw data verified

## Mode 2 - Full variable push - COMPLETE (v0.5)
- [x] nc_push_identity() - device identity, nominals, static vars pushed on connect
- [x] nc_push_state() - added input.voltage, output.voltage to dynamic push
- [x] ups.dev comprehensive template - all pushable vars pre-declared on LXC
- [x] nut-upstream-setup.md - full server setup guide for Mode 2 users

## Phase 4 - Dynamic scanning - COMPLETE (v0.6)

- [x] Replace static expected_rids[] with live seen_rids bitmask
- [x] Accumulate rids from interrupt-IN in ups_hid_parser_decode_report()
- [x] 30s settle timer fires ups_hid_parser_run_xchk() after enumeration
- [x] XCHK Part 1: seen but not declared as Input -> WARN (vendor extension)
- [x] XCHK Part 2: declared as Input but never seen -> INFO (silent report)
- [x] ups_hid_parser_run_xchk() public API added (callable manually)
- [x] Static expected_rids[] block removed from ups_hid_desc.c
- [x] Targeted GET_REPORT probe on descriptor-declared rids (not full sweep)
      Callback-driven: run_xchk queues probe per unseen Input RID via
      ups_xchk_probe_fn_t callback registered by ups_usb_hid.
      Probe fires in usb_client_task via ups_get_report_service_queue().
      Raw hex logged as [XCHK Probe] - investigation data for next phase.
- [x] APC Back-UPS hardware validation (v0.8): XS 1500M, RS 1000MS, BR1000G confirmed.
      Mapping table: 9/24 fields annotated (battery.charging x3, battery.discharging,
      battery.runtime x2, battery.replace, ups.status/overload, ups.delay.shutdown, ups.load).
      APC GET_REPORT Feature truncation documented (returns first field only - expected).
      XCHK consistent: 6 RIDs, 5 undeclared vendor ext, 2 declared-but-silent on both
      XS 1500M and RS 1000MS. BR1000G has larger descriptor (1133B, 29 fields, 20 RIDs).
      All clean connect/decode/disconnect. No crashes. 5 confirmed UPS total.
- [x] Evaluate NUT mge-hid.c mapping table format for portability to ESP
      ups_hid_map.c/h: static table { usage_page, usage_id, nut_var } covering
      HID pages 0x84 (Power Device) and 0x85 (Battery System), ~50 entries.
      ups_hid_map_lookup() for single usage, annotate_report() for full RID decode.
      ups_hid_desc_dump() now appends -> nut_var_name (or -> unmapped) per field.
      service_probe_queue() calls annotate_report() on probe responses.
      ups_hid_parser_get_desc() accessor added for probe-path descriptor access.
      Portability verdict: confirmed for APC/Eaton (standard HID usages).
      CyberPower result: all 14 descriptor fields unmapped (all vendor usage IDs
      0x008C-0x00FE) - confirms why direct-decode is required for CyberPower.
      Decision: annotation layer only, no decode migration (D005 in DECISIONS.md).

## All-mode verification - APC XS 1500M - COMPLETE (v0.10)
- [x] Mode 0 STANDALONE: upsc direct against ESP port 3493 - full variable set confirmed
      battery.charge, runtime, input/output voltage (120V via rid=0x17), status, model,
      serial, firmware - all correct. No input.transfer.high/low (not yet decoded, expected)
- [x] Mode 1 NUT CLIENT: push to NUT LXC dummy-ups confirmed - all vars flowing correctly
- [x] Mode 2 BRIDGE: 1049B APC descriptor + interrupt-IN stream confirmed on LXC port 5493
- [x] rid=0x52 page=0x84 uid=0x0044 researched and documented in D006:
      Spec says ConfigActivePower. APC Back-UPS uses it as input transfer voltage threshold.
      88V = low sensitivity low transfer, 132V = medium sensitivity high transfer.
      Not added to generic map table (APC-specific decode path work - future).

## Mode 3 - future improvements

- [ ] GET_REPORT forwarding (type=0x02) for Feature reports
- [ ] Full upstream NUT libusb integration (replace Python receiver with real driver)
- [ ] Upstream host decodes descriptor + serves NUT on its own tcp/3493

## Cross-mode validation
- [x] Switch connected UPS and verify all three modes still work
  - New UPS: CyberPower VID:0764 PID:0501 (ST/CP/SX Series)
  - Mode 3 BRIDGE: 607B descriptor sent, packets streaming - confirmed
  - Mode 2 NUT CLIENT: authenticated as esppush, battery.charge=100 pushed - confirmed
  - Mode 1 STANDALONE: NUT server on 3493, upsc returns live data - confirmed

---

## Staging Submission Findings (2026-04-03, read-only inspection)

Eaton 3S 700 (VID:0463 PID:FFFF) - 3 user submissions from main repo:
- Field cache misses battery.charge/runtime/voltage - all show rid=FF
- Root cause: data is in undocumented RID 0x06 interrupt-IN packets, not in declared HID descriptor fields
- Decode: byte1=charge%, bytes2-3=runtime uint16 LE, bytes4-5=status flags
- XCHK would surface 0x06 as undeclared vendor extension (seen but not declared as Input)
- Battery charge 2% bug (v15.17) - likely bad field extraction in current Eaton path
- [ ] Add Eaton 3S 700 RID 0x06 decode handler (when users switch to flex repo)

BlueWalker PowerWalker VI 3000 RLE (VID:0764 PID:0601):
- Rebranded CyberPower (USB strings confirm "CyberPower CP1500AVRLCD")
- Already handled by existing CyberPower path - no new code needed
- USB disconnect at ~96s is a hardware behavior, reconnect works correctly

## PowerWalker VI 3000 RLE battery.charge=0 - ROOT CAUSE FOUND (2026-04-03)

User report: battery.charge reading 0%% confirmed by user to be at 100%%.
VID:0764 PID:0601 - CyberPower rebranded hardware.

Root cause (from staging submission e88c29, flex repo, IDF v5.5.4):
- Device has rid=0x28 declared as Input (63 bytes) but never seen in interrupt-IN traffic
- XCHK 30s settle timer fires, queues GET_REPORT probe for rid=0x28
- service_probe_queue() hardcapped wLength at 16 (buf[16] allocation, sz > 16 -> sz=8)
- Device (or USB DWC controller) expects buffer matching declared size (63 bytes)
- IDF v5.5.4 added stricter DWC assert: hcd_dwc.c:2388 rem_len check fires
- Device crash-loops every ~34s (XCHK fires -> probe -> assert -> reboot -> repeat)
- battery.charge=0 is a symptom of the crash-loop, not a decode bug
- User was running IDF v5.5.4 but project targets v5.3.1 - assert is v5.5.4 addition

Fix applied in v0.14 (ups_get_report.c R2):
- Cap raised from 16 to 64 bytes: `if (sz == 0u || sz > 64u) sz = 8u;`
- buf[16] -> buf[64]: accommodates declared sizes up to 64 bytes
- rid=0x28 (63 bytes declared) now probed with wLength=63, no assert

Additional note: first submission (fb2c24, main repo v15.x, no XCHK) showed
battery.charge=100%% reading correctly throughout. Charge decode is not broken.
The crash-loop was flex-only, caused by XCHK probe firing on rid=0x28.

- [x] Root cause identified from staging submissions
- [x] Fix applied - v0.14 ups_get_report.c
- [ ] User should rebuild with IDF v5.3.1 (project target) or v0.14 flex repo
- [ ] Confirm with user after they update that charge reads correctly

---

## CyberPower 3000R (0764:0601) goto fix - v0.16 (2026-04-04)

Root cause found: ups_hid_parse_report() had "if (rid != 0x20) goto finalize" after
decode_cyberpower_direct(). All RIDs except 0x20 were silently discarded without running
the standard field-cache path. CyberPower 3000R sends battery.charge on rid=0x08
(standard HID) which was in the cache but never applied.

- [x] Bug identified from submission log analysis
- [x] Goto fix applied in ups_hid_parser.c
- [x] rid=0x0B diagnostic logging added (value 0x13=19 on AC, meaning TBD)
- [x] ups_db_cyberpower.c PID 0601 corrected (not same decode path as 0501)
- [x] Build clean
- [ ] Flash v0.17, confirm battery.charge reads correctly (goto fix from v0.16)
- [ ] Need discharge event log to decode rid=0x0B (value 0x13=19 on one device, 0x03 on another)

## CyberPower 3000R (0764:0601) Task Watchdog crash - FIXED v0.17 (2026-04-05)

Root cause found from submission 9b89d6 (sollandk/redandblue, v0.16-dirty):
- rid=0x29 on CyberPower 3000R has 237+ fields in HID descriptor
- ups_hid_desc_dump() looped over all fields at ESP_LOGI level (~10ms per log line)
- 237 fields x 10ms = ~2.4s of continuous ESP_LOGI calls blocking ups_usb task on core 0
- IDLE0 on core 0 starved past TWDT threshold (default 5s) -> watchdog fired at ~t=11.6s
- Device crash-looped on boot: WDT -> USB re-enum -> descriptor dump -> WDT -> repeat
- Manifested as device available 10s / unavailable 10s pattern

Fix applied in v0.17 (ups_hid_desc.c R2):
- Per-field loop in ups_hid_desc_dump() changed ESP_LOGI -> ESP_LOGD
- Summary line "[DUMP] N fields, M reports" kept at INFO (1 line per connection)
- Normal builds: 1 summary line instead of 237 INFO lines

Confirmed from same submission: goto fix from v0.16 is working (battery.charge=93%)

- [x] Root cause identified from submission 9b89d6
- [x] Fix applied - v0.17 ups_hid_desc.c
- [x] Build clean
- [ ] User re-submits to confirm no more crash-loop

## Eaton 3S 700 (0463:FFFF) - v0.17 follow-up (2026-04-05)

Two new submissions from same user (MyDisplayName/Discord):
- 77eaee (07:37): Standard ESP32-S3, v0.16 clean - battery.charge=89%, runtime=1401s,
  status OL all confirmed working. No issues reported.
- 9543fe (10:13): M5Stack Atom S3 Lite, v0.16-dirty, IDF v5.5.3 - debug logging on,
  log cut off during descriptor parse (528 lines, no operational data captured).
  User reports battery.charge and battery.runtime issues.

Status: 9543fe is undiagnosable - log ends before rid=0x06 data appears.
Eaton 3S decode itself is confirmed correct (77eaee).
M5Stack hardware timing may differ. Need longer log from 9543fe user.

- [x] 77eaee confirmed working (first confirmed Eaton 3S on flex repo)
- [ ] 9543fe user: capture 90s+ log without debug logging enabled
- [ ] Confirm battery.charge on M5Stack Atom S3 Lite once proper log received

## CyberPower 3000R (0764:0601) DWC assert crash - FIXED v0.20 (2026-04-05)

Submission a0043f (sollandk/redandblue, v0.17-dirty, IDF v5.3.1) analyzed.
8803-line log shows crash-loop starting at first boot after XCHK fires (30s settle).

Root cause: do_get_feature_report() allocates transfer as 8 + buf_sz bytes exactly.
CyberPower 3000R returns more data for rid=0x28 than the 63 bytes declared in descriptor.
DWC OTG HCI asserts: rem_len <= (transfer->num_bytes - sizeof(usb_setup_packet_t))
With alloc=71, assertion fires when device sends >63 bytes -> abort -> reset -> loop.

This is distinct from the v0.14 fix (which fixed wLength being too small: 16 vs declared 63).
v0.14 made wLength match the declared size. v0.20 adds 64-byte overflow buffer padding
so rem_len fits even if the device sends extra bytes beyond its own declaration.

Fix applied in v0.20 (ups_get_report.c R3):
- alloc = 8 + buf_sz + 64 (was 8 + buf_sz)
- wLength in setup packet unchanged (device told to send buf_sz bytes)
- ctrl_cb clips payload to CTRL_PAYLOAD_MAX=24, callers unaffected

Remaining issues for this device (no fix yet):
- ups.status: no rid=0x80 (ac_present) sent by this specific hardware
  Only rid=0x08 (battery.charge) and rid=0x0B seen in traffic
  rid=0x0B: 0x03 during init, transitions to 0x04 after ~20s on AC - meaning TBD
- battery.runtime: only in Feature report rid=0x08 byte 2 (uid=0x0068). Not polled.
- battery.voltage: only in Feature report rid=0x07 (uid=0x0083). Not polled.
- ups.load, input.voltage, output.voltage: absent from descriptor. Device does not expose them.
- WDT on boot 3 in submitter log: submitter built with DEBUG log level, all LOGD lines print.
  v0.17 LOGD demotion fix is ineffective when compiled with CONFIG_LOG_DEFAULT_LEVEL_DEBUG=y.
  Submitter should use INFO log level (default release config).

- [x] Root cause identified from submission a0043f
- [x] Fix applied - v0.20 ups_get_report.c
- [x] Build clean
- [ ] sollandk/redandblue: re-submit on v0.20 to confirm no more crash loop
- [ ] Decode rid=0x0B to determine ac_present bit for ups.status
- [ ] Consider Feature report polling for battery.runtime (rid=0x08 byte 2) and battery.voltage (rid=0x07)

## Possible Future Additions

- [ ] **OTA (Over-the-Air) firmware updates** - FUTURE.
      ESP-IDF supports OTA via esp_https_ota / esp_ota_ops with automatic rollback.
      Requires: custom partitions.csv with dual OTA slots (ota_0 + ota_1 + otadata).
      With 16MB flash two 2MB app slots fit easily alongside NVS.
      Portal addition: /update page - user uploads .bin or pastes URL, device flashes
      inactive slot, calls esp_ota_mark_app_valid_cancel_rollback() on clean boot,
      rolls back automatically if new firmware fails to validate.
      Constraint: existing devices need one manual reflash to adopt the new partition
      layout. All subsequent updates would be OTA.
      Pattern: same as ESPHome web OTA - fits naturally with existing /reboot + auth.

- [ ] **In-device diagnostic log capture** - STANDBY. Full design spec in docs/diag-log-capture.md.
      90s boot capture, credential scrub before POST, opt-in checkbox gates button.
      Requires server connector at projects.strydertech.com before submission path can be wired up.
      Fallback: /diag HTTP endpoint on device for browser download (no server needed).
