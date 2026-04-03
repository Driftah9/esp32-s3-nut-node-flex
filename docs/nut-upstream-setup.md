# NUT Upstream Setup - Mode 2 NUT CLIENT

This document covers what needs to run on the upstream NUT server when the
ESP32 operates in Mode 2 (NUT CLIENT push mode).

---

## Why dummy-ups is Required

NUT's dummy-ups driver does not allow SET VAR to create new variables.
Variables MUST be pre-declared in the .dev file before the ESP can push them.

The comprehensive ups.dev template below pre-declares every variable the ESP
can push. Users copy it to their NUT server. No manual variable hunting needed.

---

## Upstream Server Requirements

- NUT 2.8.x (tested on Ubuntu 24.04, NUT 2.8.1)
- dummy-ups driver
- upsd listening on tcp/3493
- esppush user with actions=SET in upsd.users

---

## /etc/nut/nut.conf

```
MODE=netserver
```

---

## /etc/nut/upsd.conf

```
LISTEN 0.0.0.0 3493
```

---

## /etc/nut/ups.conf

```
[ups]
    driver = dummy-ups
    port = ups.dev
    desc = "ESP32 UPS NUT Node - Mode 2 push receiver"
```

---

## /etc/nut/upsd.users

```
[admin]
    password = nutadmin
    actions = SET
    instcmds = ALL

[esppush]
    password = esppush123
    actions = SET
    instcmds = NONE

[upsmon]
    password = upsmon
    upsmon primary
```

---

## /etc/nut/ups.dev

Copy this file to /etc/nut/ups.dev on the NUT server.
All variables pre-declared so the ESP can push any of them via SET VAR.

Values shown are safe defaults. The ESP overwrites everything it knows
about on first connect (identity push) and on each poll cycle (state push).

Variables the connected UPS does not support retain their default values.

```
# --- Identity (overwritten by ESP on connect) ---
device.mfr: UNKNOWN
device.model: UNKNOWN
device.serial: UNKNOWN
device.type: ups
ups.mfr: UNKNOWN
ups.model: UNKNOWN
ups.firmware: unknown
ups.vendorid: 0000
ups.productid: 0000
ups.type: line-interactive

# --- Battery status (pushed every 10s) ---
battery.charge: 0
battery.charge.low: 20
battery.charge.warning: 35
battery.runtime: 0
battery.runtime.low: 300
battery.type: PbAc
battery.voltage: 0.000
battery.voltage.nominal: 12.0

# --- UPS status (pushed every 10s) ---
ups.status: OL
ups.flags: 0x00000000
ups.load: 0

# --- Input / Output (pushed when available - GET_REPORT devices) ---
input.utility.present: 1
input.voltage: 0.0
input.voltage.nominal: 120
output.voltage: 0.0

# --- Static NUT housekeeping (pushed once on connect) ---
ups.test.result: No test initiated
ups.delay.shutdown: 20
ups.delay.start: 30
ups.timer.reboot: -1
ups.timer.shutdown: -1
```

---

## Deploy Commands

Run these on the upstream NUT server:

```bash
# Install NUT
apt-get install -y nut

# After writing config files above, restart NUT
systemctl restart nut-server

# Verify upsd is listening
ss -tlnp | grep 3493

# Test once ESP is connected and pushing
upsc ups@localhost
```

---

## Notes

- device.mfr / device.model / ups.vendorid / ups.productid will be set
  to real values by the ESP identity push on first connect.
- input.voltage and output.voltage are only pushed on devices with
  QUIRK_NEEDS_GET_REPORT (Eaton/MGE type devices). For most UPS they stay 0.0.
- battery.charge.warning / battery.runtime.low / battery.voltage.nominal /
  input.voltage.nominal are pushed from the ESP device database entry.
  If the device is not in the DB, safe defaults remain.
- driver.* and server.* are NOT in the .dev file - dummy-ups sets these itself.
