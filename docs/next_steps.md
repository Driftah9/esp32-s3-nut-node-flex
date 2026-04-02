# Next Steps — esp32-s3-nut-node-flex
<!-- Updated: 2026-04-02 -->

## Before any implementation

- [ ] Copy src\ tree from esp32-s3-nut-node v15.18 as starting baseline
- [ ] Create GitHub repo Driftah9/esp32-s3-nut-node-flex
- [ ] Initial commit with copied baseline + this scaffold

## Phase 1 — cfg_store + portal mode selector

- [ ] Add `op_mode` field to cfg_store (uint8, 0=standalone, 1=nut_client, 2=bridge)
- [ ] Add `upstream_host` (char[64]) and `upstream_port` (uint16) to cfg_store
- [ ] Add mode selector to web portal config page (radio buttons or dropdown)
- [ ] Add upstream host/port fields to portal (shown only when mode 2 or 3 selected)
- [ ] Confirm NVS save/load round-trips correctly

## Phase 2 — Mode 2 NUT client push

- [ ] Research NUT upsmon/upsd client protocol (LOGIN, PASSWORD, MONITOR commands)
- [ ] Implement upstream_push task: connect to upstream_host:upstream_port
- [ ] Push battery.charge, battery.runtime, ups.status on each state change
- [ ] Implement upstream reachability check at boot
- [ ] Implement Mode 1 fallback if upstream unreachable

## Phase 3 — Mode 3 bridge stream

- [ ] Define wire protocol: descriptor handshake + raw stream format
- [ ] Implement bridge_task: TCP server or client to upstream_host
- [ ] Send HID Report Descriptor on connect (length-prefixed)
- [ ] Forward interrupt-IN packets raw as they arrive
- [ ] Forward GET_REPORT responses tagged with rid
- [ ] Implement Mode 1 fallback if upstream unreachable

## Phase 4 — Dynamic scanning (investigation)

- [ ] Replace static expected_rids[] with live seen_rids bitmask
- [ ] Accumulate rids from interrupt-IN in ups_hid_parser_decode_report()
- [ ] Post-enumeration XCHK against seen_rids vs descriptor declared rids
- [ ] Targeted GET_REPORT probe on descriptor-declared rids (not full sweep)
- [ ] Evaluate NUT mge-hid.c mapping table format for portability to ESP
