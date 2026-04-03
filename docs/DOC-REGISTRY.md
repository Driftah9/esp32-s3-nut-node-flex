# DOC-REGISTRY.md - esp32-s3-nut-node-flex
<!-- Updated: 2026-04-03 -->

This file defines which documents must be updated before every GitHub push,
based on the type of change being made.

---

## Change Type Codes

| Code | Meaning |
|------|---------|
| FEAT | New feature or new mode implemented |
| FIX | Bug fix |
| REFACTOR | Code reorganization, no behavior change |
| DEVICE | New UPS device support or quirk |
| DOC | Documentation only |
| INFRA | Build, config, CI, scripts |
| SYNC | Upstream merge from esp32-s3-nut-node |

---

## Required Updates by Change Type

| Document | FEAT | FIX | REFACTOR | DEVICE | DOC | INFRA | SYNC |
|----------|------|-----|----------|--------|-----|-------|------|
| README.md | YES | - | - | YES | YES | - | YES |
| docs/github_push.md | YES | YES | YES | YES | YES | YES | YES |
| docs/project_state.md | YES | YES | YES | YES | - | YES | YES |
| docs/next_steps.md | YES | YES | YES | YES | - | - | YES |
| docs/DECISIONS.md | YES | - | YES | - | - | - | - |
| docs/session_log.md | YES | YES | YES | YES | YES | YES | YES |

---

## Documents

| File | Type | Description | Frozen? |
|------|------|-------------|---------|
| README.md | Required | Fork overview, mode descriptions | No |
| CLAUDE.md | Reference | Project rules for Claude CLI | No |
| docs/github_push.md | Required | Push details, commit message | No |
| docs/project_state.md | Required | Desktop-CLI handoff state | No |
| docs/next_steps.md | Required | Cold-start brief, next steps | No |
| docs/DECISIONS.md | Required | Architectural decisions log | No |
| docs/DOC-REGISTRY.md | Reference | This file | No |
| docs/session_log.md | Required | Chronological session history | No |
| docs/diag-log-capture.md | Reference | Standby feature spec - diagnostic log capture | No |
| docs/confirmed-ups.md | Reference | Confirmed compatible UPS devices | No |
| docs/nut-upstream-setup.md | Reference | NUT server setup guide for Mode 2 users | No |

---

## Notes

- github_push.md MUST be updated and verified before running git-push.ps1
- Never freeze documents during active investigation phase
- SYNC changes: always update README.md to note the upstream baseline version
