# ESP32-S3 UPS NUT Node - Flex

Fork of [esp32-s3-nut-node](https://github.com/Driftah9/esp32-s3-nut-node) investigating tri-mode operation and dynamic HID scanning.

**Status:** Experimental / Pre-release
**Baseline:** esp32-s3-nut-node v15.18
**Parent project:** [Driftah9/esp32-s3-nut-node](https://github.com/Driftah9/esp32-s3-nut-node) - stable, production use

---

## What This Does (Planned)

This fork adds three selectable operating modes. The mode is chosen via the web portal config page and stored in NVS.

### Mode 1 - STANDALONE (default)
Identical to the main project. ESP decodes USB HID reports, serves NUT protocol on tcp/3493. No external infrastructure required. This is the safe fallback for all other modes.

### Mode 2 - NUT CLIENT
ESP decodes HID locally and pushes UPS data upstream to an existing NUT server (upsd). The ESP acts as a data source, not a server. Useful for integrating into an existing NUT infrastructure without replacing it.

Requires: upstream host IP and port configured in the portal.
Fallback: reverts to Mode 1 if upstream is unreachable at boot.

### Mode 3 - BRIDGE
ESP forwards the raw USB HID stream over TCP to an upstream host. Zero local decoding. On connect, the ESP sends the full HID Report Descriptor as a handshake, then streams raw interrupt-IN packets and GET_REPORT responses as they arrive. The upstream host (running NUT with libusb) handles all decode and NUT serving.

Requires: upstream host IP and port configured in the portal.
Fallback: reverts to Mode 1 if upstream is unreachable at boot.

---

## Dynamic HID Scanning (Investigation)

The main project uses a static pre-seeded list of Report IDs (RIDs) to target with GET_REPORT probes. This fork investigates replacing that with:

- Live RID accumulation from interrupt-IN traffic
- Targeted GET_REPORT probing against only descriptor-declared RIDs
- Evaluation of the NUT mge-hid.c mapping table format for ESP portability

Goal: a device-agnostic decode path that self-builds from any UPS without a static per-device table.

---

## Hardware

Same hardware as the main project.

| Component | Details |
|-----------|---------|
| MCU | Hosyond ESP32-S3-WROOM-1 N16R8 devkit (16MB flash, 8MB PSRAM) |
| UPS connection | USB-A to OTG port (ESP32 acts as USB host) |
| Power | USB-C port (powered independently from UPS USB) |

---

## Build

Requires ESP-IDF v5.3.1. Source is in `src/current/`.

```powershell
# From src/current/
idf.py build flash monitor -p COM3
```

---

## Project Structure

```
src/current/          - Active firmware source (ESP-IDF project)
src/current/main/     - All C source modules
docs/                 - Project state, decisions, session log
docs/DECISIONS.md     - Architectural decisions D001-D004
```

---

## Relationship to Main Project

This fork is one-way. Changes here are never backported to esp32-s3-nut-node.

When the main project is updated (new UPS support, bug fixes, protocol improvements), stable upstream changes can be selectively merged into this fork's baseline modules - subject to manual review against any flex-specific modifications already applied.

See `docs/DECISIONS.md` for full architectural rationale.

---

## License

MIT
