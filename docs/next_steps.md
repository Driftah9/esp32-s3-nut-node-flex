# Next Steps - esp32-s3-nut-node-flex
<!-- Updated: 2026-04-02 -->

## Immediate
- [x] Push v0.3 to GitHub

## Phase 2 - Mode 2 NUT client push - COMPLETE

- [x] nut_client.c - TCP connect with timeout, USERNAME/PASSWORD auth, SET VAR push loop
- [x] Boot reachability check with Mode 1 fallback if upstream unreachable
- [x] Push-based reconnect detection (mandatory SET VAR return bool)
- [x] 5s startup delay for DHCP/ARP settle before first connect
- [x] Tested against nut-test-lxc (10.0.0.18) - connect, auth, push confirmed
- [x] idf-build.ps1 flash-monitor target for automated test cycles

## Phase 3 - Mode 3 bridge stream

- [ ] Define wire protocol: length-prefixed descriptor + raw stream format
- [ ] Implement bridge_task (TCP client to upstream_host)
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
