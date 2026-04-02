# Next Steps - esp32-s3-nut-node-flex
<!-- Updated: 2026-04-02 -->

## Immediate
- [x] Push v0.2 to GitHub

## Phase 2 - Mode 2 NUT client push

- [ ] Research NUT upsmon/upsd client protocol (LOGIN, PASSWORD, SET commands)
- [ ] Add upstream_push_task to main.c (launched when op_mode == NUT_CLIENT)
- [ ] Implement TCP connect to upstream_host:upstream_port with retry
- [ ] Implement upstream reachability check at boot
- [ ] Push battery.charge, battery.runtime, ups.status on each state change
- [ ] Implement Mode 1 fallback if upstream unreachable at boot or lost mid-session
- [ ] Test against OrangePi NUT server (10.0.0.6)

## Phase 3 - Mode 3 bridge stream

- [ ] Define wire protocol: length-prefixed descriptor + raw stream format
- [ ] Implement bridge_task (TCP server or client to upstream_host)
- [ ] Send HID Report Descriptor on connect
- [ ] Forward interrupt-IN packets raw as they arrive
- [ ] Forward GET_REPORT responses tagged with rid
- [ ] Implement Mode 1 fallback if upstream unreachable

## Phase 4 - Dynamic scanning (investigation)

- [ ] Replace static expected_rids[] with live seen_rids bitmask
- [ ] Accumulate rids from interrupt-IN in ups_hid_parser_decode_report()
- [ ] Post-enumeration XCHK against seen_rids vs descriptor declared rids
- [ ] Targeted GET_REPORT probe on descriptor-declared rids (not full sweep)
- [ ] Evaluate NUT mge-hid.c mapping table format for portability to ESP
