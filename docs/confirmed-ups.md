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

## Expected

Devices listed here have known-compatible VID:PID values based on NUT driver tables
or user reports that have not yet gone through full validation.

| Device | VID:PID | Notes |
|--------|---------|-------|
| APC Back-UPS (most models) | 051D:0002 | Standard APC HID profile |
| Eaton 5E / 5S | 0463:FFFF | MGE/Eaton HID profile |
