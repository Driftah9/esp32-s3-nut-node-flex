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
v0.9

## Commit Message
v0.9 - esp32-s3-nut-node-flex - APC Back-UPS hardware validation complete

Session 008: APC Back-UPS validation on 3 models, mapping table confirmed working

Hardware tested (3-minute monitor, 4 connect/disconnect cycles):
- APC Back-UPS XS 1500M (VID:051D PID:0002, FW:947.d10) - confirmed
- APC Back-UPS RS 1000MS (VID:051D PID:0002, FW:950.e3) - confirmed
- APC Back-UPS BR1000G (VID:051D PID:0002, FW:868.L2) - confirmed
- CyberPower ST Series (VID:0764 PID:0501) - previously confirmed

Mapping table results (APC XS 1500M + RS 1000MS, identical 1049B descriptor):
- 9/24 fields annotated vs 0/14 on CyberPower - confirms table works on standard usages
- Mapped: battery.charging x3 variants, battery.discharging, battery.runtime x2,
  battery.replace, ups.status/overload, ups.delay.shutdown, ups.load
- Unmapped: 15 APC proprietary IDs (expected)
- Note: page=0x84 uid=0x0044 (rid=0x52) is likely ups.output.voltage - future addition

XCHK results (consistent across both APC models):
- 6 RIDs seen, 5 undeclared vendor extensions, 2 declared-but-silent
- Declared-but-silent: rid=0x07 and rid=0x52 (Feature-only on APC)
- Probe fired for both - rid=0x07 returns battery.runtime (3 bytes, APC truncates)
- extract FAILED WARNs for bit_off >= 16 are expected (APC Feature response truncation)

BR1000G notes:
- Larger descriptor: 1133B, 29 fields, 20 RIDs (5 extra Feature fields in high RIDs)
- Extra rids 0x80/0x8D-0x90 on page=0x84 uid=0x0092-0x0096 - all unmapped
- Disconnected at ~22.8s - XCHK 30s timer did not fire (expected edge case)

All 3 models: clean enumerate/decode/disconnect, no crashes, no USB errors

docs/confirmed-ups.md:
- Added APC Back-UPS XS 1500M, RS 1000MS, BR1000G (all v0.8, 2026-04-02)
- APC moved from Expected to Confirmed (additional models note kept in Expected)
- Total confirmed: 5 devices

docs/DECISIONS.md:
- D005 updated with full APC validation results, probe truncation behavior,
  BR1000G descriptor differences, candidate future mapping (uid=0x0044)

docs/next_steps.md: APC validation results added under Phase 4 completed items
docs/project_state.md: status and last action updated

## Files Staged
- docs/confirmed-ups.md
- docs/DECISIONS.md
- docs/github_push.md
- docs/project_state.md
- docs/next_steps.md
