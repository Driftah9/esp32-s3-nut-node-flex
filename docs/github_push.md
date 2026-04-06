# GitHub Push - esp32-s3-nut-node-flex

> Claude updates this file whenever code changes during a session.
> Keep this current - it is the single source of truth for every push.

---

## Project
esp32-s3-nut-node-flex

## Repo
https://github.com/Driftah9/esp32-s3-nut-node-flex

## Visibility
public

## Branch
main

## Version
v0.26

## Commit Message
v0.26 - Eaton OB diagnostic improvements

OL status on Eaton 3S is correct when on AC power. The unknown is whether
OB transitions correctly on mains loss (no discharge log yet captured from
any Eaton submission).

Changes to improve discharge-event capture and OB probe coverage:
- ups_hid_parser.c: raise 0x8x alarm/event rid log cap from 8 to 16 bytes,
  elevate to WARN level so these rids are visible in reduced-verbosity logs.
  The 0x8x interrupt-IN range is the most likely source of OL->OB notification.
- ups_get_report.c: add rid=0x85 to Eaton recurring probe list and
  decode_eaton_feature() case 0x85 (raw log at WARN for OB analysis).
- ups_usb_hid.c: add rid=0x85 to bootstrap probe queue at enumeration.

When the Eaton user captures a log with a mains-loss event, the 0x8x WARN
lines and rid=0x85 Feature response will identify the OB status byte.

## Files Staged
- src/current/main/ups_hid_parser.c
- src/current/main/ups_get_report.c
- src/current/main/ups_usb_hid.c
- docs/github_push.md
