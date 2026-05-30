# Decisions Log — esp32-s3-nut-node-flex
<!-- v1.0 | 2026-04-02 | Initial scaffold -->

Fork of esp32-s3-nut-node. Key architectural decisions for this project only.
See parent project DECISIONS.md D019 for the original rationale.

---

## D001 — Tri-mode operation model
**Decision:** Three selectable operating modes, chosen via web portal and stored in NVS.
  Mode 1 STANDALONE: ESP decodes HID, serves NUT on 3493. No external dependency.
  Mode 2 NUT CLIENT: ESP decodes HID, pushes to upstream upsd host.
  Mode 3 BRIDGE: ESP forwards raw HID stream, upstream host decodes.
**Reason:** Allows the device to work standalone out of the box (Mode 1) while
supporting integration with existing NUT infrastructure (Mode 2) and full
dynamic device support via upstream libusb (Mode 3). User controls the tradeoff.
**Status:** Phase 1 + Phase 2 implemented. cfg_store fields, portal UI, and nut_client
push task all confirmed working. SET VAR push to dummy-ups on LXC verified via upsc.

---

## D002 — Mode 1 is always the fallback
**Decision:** If Modes 2 or 3 are configured but the upstream host is unreachable
at boot or connection is lost, the device falls back to Mode 1 automatically.
Fallback behavior is configurable (can be disabled if user wants hard failure).
**Reason:** Prevents a network blip from leaving the UPS completely unmonitored.
Standalone capability is the safety net.
**Status:** Planned. Not implemented.

---

## D003 — Mode 3 bridge protocol: descriptor handshake + raw stream
**Decision:** On TCP connect in Mode 3, ESP sends the full HID Report Descriptor
as a length-prefixed binary blob. Then streams raw interrupt-IN packets and
GET_REPORT responses as they arrive. Upstream host is responsible for all
HID parsing, field mapping, and NUT protocol serving.
**Reason:** Upstream NUT with libusb needs the descriptor to build its field map.
Sending it on connect is the minimal viable handshake. Everything else is raw passthrough.
**Wire format implemented:**
  Handshake: [2B BE: desc_len][desc bytes]
  Stream:    [1B: type][2B BE: data_len][data bytes]
  type 0x01 = interrupt-IN, type 0xFF = keepalive
**Status:** Implemented and confirmed. 1049B descriptor + live interrupt-IN stream
verified on nut-test-lxc (10.0.0.18:5493) bridge_receiver.py. 109+ packets at
UPS poll rate (~4s). GET_REPORT forwarding (type=0x02) deferred to future work.

---

## D004 — Dynamic scanning replaces static XCHK list
**Decision:** The static pre-seeded expected_rids[] list in ups_hid_desc.c is
replaced with a live-accumulated seen_rids bitmask built from interrupt-IN traffic.
XCHK comparison happens against the descriptor at runtime, not against a hardcoded list.
Targeted GET_REPORT probing covers descriptor-declared rids only (not 0x01-0xFF sweep).
**Reason:** Static list requires manual updates per new device. Live accumulation is
device-agnostic and self-building. Full sweep is impractical at ~2s/rid on Eaton.
**Status:** Implemented and confirmed (v0.6-v0.7).
- s_seen_rids[32] bitmask accumulated from interrupt-IN, cleared on disconnect
- 30s settle timer fires run_xchk() comparing seen vs descriptor-declared Input RIDs
- Part 1: undeclared rids logged as WARN (vendor extension)
- Part 2: declared-but-silent Input rids queued for GET_REPORT probe via callback
- Probe fires in usb_client_task via ups_get_report_probe_rid()
- CyberPower ST Series result: 13 seen, 11 undeclared vendor ext, 0 declared-but-silent
- Probe callback mechanism confirmed working end-to-end; waiting for device with silent Input RIDs

---

## D005 — NUT mge-hid.c mapping table: portable for standard usages only
**Decision:** Implement ups_hid_map.c as an annotation layer using NUT mge-hid.c format.
Standard usages (HID pages 0x84 Power Device, 0x85 Battery System) are mapped to NUT
variable names in a static table. Vendor extension usages remain in per-device decode_mode
functions (direct-decode paths). The table is used for annotation only - it does NOT
replace the working field cache or direct-decode paths.
**Reason / Evaluation findings:**
- NUT mge-hid.c format: { hidpath, nutname, scale, flags, lkp_table }
- ESP equivalent:        { usage_page, usage_id, nut_var } - hidpath/scale/flags dropped
  because usage_page+usage_id come directly from parsed descriptor, scale lives in
  unit_exponent from hid_field_t, flags not needed for read-only sensor fields.
- Portability verdict: CONFIRMED for standard usages. CyberPower test showed ALL 14
  descriptor fields use vendor-proprietary usage IDs (0x008C-0x00FE range) - none map
  to standard HID spec. This explains why direct-decode is required for CyberPower:
  its descriptor is vendor-only throughout.
- For APC/Eaton devices using standard HID usages, the table would annotate correctly.
- Full decode migration (replacing field cache with table-driven decode) deferred:
  risk of regression, current decode paths are tested and stable.
**Status:** Implemented and confirmed (v0.8).
ups_hid_map.c/h: lookup table + annotate_report().
Integrated into ups_hid_desc_dump() and ups_get_report probe path.
ups_hid_parser_get_desc() added as accessor for probe annotation.
APC Back-UPS validation (v0.8, three models - XS 1500M, RS 1000MS, BR1000G):
- All three use VID:051D PID:0002 and share the same APC Back-UPS HID profile
- XS 1500M + RS 1000MS: identical 1049B descriptor, 24 fields, 16 RIDs
- BR1000G: 1133B descriptor, 29 fields, 20 RIDs (5 extra Feature-only fields
  in rids 0x80/0x8D-0x90 using page=0x84 uid=0x0092-0x0096, all unmapped)
- Mapping table result: 9/24 fields annotated with NUT names (vs 0/14 CyberPower)
  Mapped: battery.charging (×3 variants), battery.discharging, battery.runtime (×2),
  battery.replace, ups.status/overload, ups.delay.shutdown, ups.load
  Unmapped: 15 APC proprietary IDs (0x00FE, 0x00FF, 0x0089, 0x008F, 0x00DB,
  0x002A, 0x0052, 0x007C, 0x007D, and page=0x84 uid=0x0044)
  Note: page=0x84 uid=0x0044 (rid=0x52) - APC non-compliant usage, see D006
- XCHK result (XS 1500M + RS 1000MS): 6 RIDs seen, 5 undeclared vendor ext,
  2 declared-but-silent (rid=0x07 and rid=0x52 - confirmed Feature-only on APC)
- Probe result: rid=0x07 returns 3 bytes only (battery.runtime field only).
  APC truncates GET_REPORT Feature response to just the first field.
  annotate_report() correctly extracts battery.runtime (val=20881 / 19663),
  emits extract FAILED WARN for fields at bit_off >= 16 (expected, payload short).
  This is APC behavior, not a firmware bug.
- BR1000G disconnected at ~22.8s (before 30s XCHK timer) - no XCHK result captured
- All 3 models: clean enumerate/decode/disconnect cycle, no crashes, no USB errors

---

## D006 - APC Back-UPS non-compliant HID usage: rid=0x52 page=0x84 uid=0x0044
**Decision:** Treat APC Back-UPS (VID:051D PID:0002) rid=0x52 as an APC-specific
transfer voltage threshold field, NOT as ConfigActivePower per HID spec.
Do NOT add page=0x84 uid=0x0044 to the generic ups_hid_map.c table.
Document as APC vendor deviation for future APC-specific decode path work.
**Reason / Research findings:**
- USB HID Power Devices spec (USB-IF Release 1.0/1.1) defines:
    page=0x84 uid=0x0044 = ConfigActivePower (nominal rated active power in watts)
    page=0x84 uid=0x0053 = LowVoltageTransfer
    page=0x84 uid=0x0054 = HighVoltageTransfer
- NUT apc-hid.c maps ConfigActivePower -> ups.realpower.nominal
- APC Smart-UPS uses 0x0053/0x0054 correctly for transfer thresholds
- APC Back-UPS (PID:0002) does NOT follow the spec. rid=0x52 with uid=0x0044
  returns input transfer voltage thresholds, not rated watts.
- GET_REPORT probe results across 3 models:
    XS 1500M: 132V = medium sensitivity high transfer threshold
    RS 1000MS: 88V  = low sensitivity low transfer threshold
    BR1000G:   88V  = low sensitivity low transfer threshold
- Standard APC 120V sensitivity table:
    High sensitivity:   low=106V, high=127V
    Medium sensitivity: low=97V,  high=132V
    Low sensitivity:    low=88V,  high=136V
- Same usage ID returns high OR low threshold depending on model sensitivity config.
  Single register, model-dependent value.
- NUT works around APC descriptor issues via apc_fix_report_desc() which patches
  the raw descriptor bytes before parsing. This is why NUT handles it correctly
  despite the non-compliant usage assignments.
- Adding uid=0x0044 to the generic map as ups.realpower.nominal would be WRONG
  for APC Back-UPS - the value is a voltage, not watts.
**Status:** Research complete. No code change. APC direct-decode path
(ups_db_apc.c or equivalent) is the correct place to handle this field
when input.transfer.low/high support is added in future work.

---

## Template for new decisions
## DXXX - Short title
**Decision:** What was decided.
**Reason:** Why.
**Status:** Planned / Implemented / Confirmed stable.
