# Confirmed Compatible UPS Devices - esp32-s3-nut-node-flex

This file tracks UPS devices confirmed working with the flex firmware.
Entries are added automatically when a compatibility report issue receives the `confirmed` label.

For submission instructions, open a [UPS Compatibility Report](https://github.com/Driftah9/esp32-s3-nut-node-flex/issues/new?template=ups-compatibility-report.yml).

---

## Confirmed

| Device | VID:PID | Firmware | Status | Submitted by | Issue | Date |
|--------|---------|----------|--------|--------------|-------|------|
| CyberPower CP550HG | 0764:0501 | v0.6 | Confirmed | @Driftah9 | - | 2026-04-02 |
| CyberPower ST Series (VID:0764 PID:0501) | 0764:0501 | v0.6 | Confirmed | @Driftah9 | - | 2026-04-02 |
| APC Back-UPS XS 1500M (FW:947.d10) | 051D:0002 | v0.8 | Confirmed | @Driftah9 | - | 2026-04-02 |
| APC Back-UPS RS 1000MS (FW:950.e3) | 051D:0002 | v0.8 | Confirmed | @Driftah9 | - | 2026-04-02 |
| APC Back-UPS BR1000G (FW:868.L2) | 051D:0002 | v0.8 | Confirmed | @Driftah9 | - | 2026-04-02 |

## Expected

Devices listed here have known-compatible VID:PID values based on NUT driver tables
or user reports that have not yet gone through full validation.

| Device | VID:PID | Notes |
|--------|---------|-------|
| APC Back-UPS (additional models with PID:0002) | 051D:0002 | Same HID profile as confirmed models above - expected compatible |
| Eaton 5E / 5S | 0463:FFFF | MGE/Eaton HID profile |
