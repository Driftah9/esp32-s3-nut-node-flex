# Project State - esp32-s3-nut-node-flex
<!-- Updated: 2026-04-02 -->

## Status
BASELINE READY - v15.18 source copied in. Initial commit pending (Stryder runs git-push.ps1).

## Parent
esp32-s3-nut-node v15.18

## Upstream Sync Baseline
Forked from: v15.18
Last synced: v15.18 (initial copy, 2026-04-02)

## What this fork is
Tri-mode ESP32-S3 UPS NUT node. User selects behavior via web portal:
- Mode 1: Standalone NUT server (current main project behavior)
- Mode 2: NUT client - ESP decodes, pushes to upstream upsd host
- Mode 3: Bridge - ESP is dumb USB-to-network pipe, upstream does all decode

## Implementation Progress
- [x] Scaffold - docs, CLAUDE.md, DECISIONS.md
- [x] v15.18 baseline copied into src\current\
- [x] README.md written
- [x] DOC-REGISTRY.md created
- [x] Upstream sync rules documented in CLAUDE.md
- [ ] Initial GitHub push (run git-push.ps1)
- [ ] Phase 1 - cfg_store + portal mode selector

## Last Action
2026-04-02 - Copied v15.18 baseline, wrote README.md, DOC-REGISTRY.md, git-push.ps1,
upstream sync rules in CLAUDE.md. github_push.md updated for v0.1 initial commit.

## Next Step
Run git-push.ps1 to create GitHub repo esp32-s3-nut-node-flex and push v0.1 initial commit.
Then begin Phase 1: cfg_store op_mode field.

## Key Constraint
Never backport experimental changes to esp32-s3-nut-node.
