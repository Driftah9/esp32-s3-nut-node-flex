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

## Phase 4 - Dynamic scanning (investigation)

- [ ] Replace static expected_rids[] with live seen_rids bitmask
- [ ] Accumulate rids from interrupt-IN in ups_hid_parser_decode_report()
- [ ] Post-enumeration XCHK against seen_rids vs descriptor declared rids
- [ ] Targeted GET_REPORT probe on descriptor-declared rids (not full sweep)
- [ ] Evaluate NUT mge-hid.c mapping table format for portability to ESP

## Mode 3 - future improvements

- [ ] GET_REPORT forwarding (type=0x02) for Feature reports
- [ ] Full upstream NUT libusb integration (replace Python receiver with real driver)
- [ ] Upstream host decodes descriptor + serves NUT on its own tcp/3493

## Cross-mode validation
- [ ] Switch connected UPS and verify all three modes still work
