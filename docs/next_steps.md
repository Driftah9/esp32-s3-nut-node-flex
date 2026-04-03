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

## Active Issue - PowerWalker VI 3000 RLE (2026-04-03)

User report: battery.charge reading 0%% confirmed by user to be at 100%%.
VID:0764 PID:0601 - CyberPower rebranded hardware.

Known context from staging submission:
- 256 fields, battery.charge located at rid=08 in field cache
- CyberPower direct-decode path handles this device (vendor usage IDs 0x008C-0x00FE)
- USB disconnect at ~96s was observed in staging submission - may be related

Likely candidates:
- rid=08 field extraction offset wrong for this specific model variant
- Charge value reads correctly on some CyberPower models but not this one
- USB disconnect before charge data is populated (96s disconnect timing vs field settle)

Status: AWAITING LOGS - user will submit diagnostic capture or serial log
When logs arrive: check [XCHK] output for rid=08, check field cache build,
check interrupt-IN packet content for rid=08 at byte offset where charge is expected.
- [ ] Analyse submitted log and identify root cause
- [ ] Fix charge field extraction for this model if offset differs from other CyberPower

---

## Possible Future Additions

- [ ] **In-device diagnostic log capture** - STANDBY. Full design spec in docs/diag-log-capture.md.
      90s boot capture, credential scrub before POST, opt-in checkbox gates button.
      Requires server connector at projects.strydertech.com before submission path can be wired up.
      Fallback: /diag HTTP endpoint on device for browser download (no server needed).
