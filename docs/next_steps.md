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
- [ ] Evaluate NUT mge-hid.c mapping table format for portability to ESP
      (Input to this: raw probe responses from declared-but-silent RIDs)

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
