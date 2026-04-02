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
**Status:** Phase 1 implemented. cfg_store: op_mode field + OP_MODE_* constants +
upstream_host/port/fallback fields + NVS defaults. Portal: Operating Mode selector
with upstream section (JS show/hide). Build confirmed clean. Portal UI verified.

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
**Status:** Planned. Protocol wire format TBD when implementation begins.

---

## D004 — Dynamic scanning replaces static XCHK list
**Decision:** The static pre-seeded expected_rids[] list in ups_hid_desc.c is
replaced with a live-accumulated seen_rids bitmask built from interrupt-IN traffic.
XCHK comparison happens against the descriptor at runtime, not against a hardcoded list.
Targeted GET_REPORT probing covers descriptor-declared rids only (not 0x01-0xFF sweep).
**Reason:** Static list requires manual updates per new device. Live accumulation is
device-agnostic and self-building. Full sweep is impractical at ~2s/rid on Eaton.
**Status:** Planned. Not implemented.

---

## Template for new decisions
## DXXX - Short title
**Decision:** What was decided.
**Reason:** Why.
**Status:** Planned / Implemented / Confirmed stable.
