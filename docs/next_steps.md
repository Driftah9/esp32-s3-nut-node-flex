# Next Steps - esp32-s3-nut-node-flex
<!-- Updated: 2026-04-02 -->

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

## Possible Future Additions

- [ ] **In-device diagnostic log capture** - esp_log_set_vprintf() hook to a ring buffer,
      exposed via /log HTTP endpoint. Optional POST to projects.strydertech.com for issue
      submission. Would allow users to gather a full boot log from within the device without
      needing a serial terminal. Marked as future - evaluate after core modes are stable.
