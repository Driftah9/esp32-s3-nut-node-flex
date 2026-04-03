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
v0.7

## Commit Message
v0.7 - esp32-s3-nut-node-flex - Phase 4 complete: XCHK GET_REPORT probe

Session 008: Targeted GET_REPORT probe for declared-but-silent Input RIDs

Phase 4 probe (v0.7):
- ups_hid_parser.h R6: ups_xchk_probe_fn_t callback typedef
  ups_hid_parser_set_xchk_probe_cb() - register/deregister probe callback
  run_xchk updated doc comment to reflect probe behaviour
- ups_hid_parser.c R6: s_xchk_probe_cb static, set_xchk_probe_cb() implemented
  reset() note: callback NOT cleared on reset (stays valid until disconnect)
  run_xchk Part 2: computes probe_sz per RID (feature_bytes fallback input_bytes,
  clamp to 16), logs queuing intent, calls s_xchk_probe_cb(rid, probe_sz)
  if no callback registered: WARN logged, probe skipped gracefully
- ups_get_report.h: probe_init(), probe_rid(), probe_clear() public API added
  service_queue() doc updated to reflect dual queue behaviour
- ups_get_report.c: probe_req_t struct (rid + size), s_probe_queue/client/dev/intf
  service_probe_queue() static: drains probe queue one entry per service call,
  calls do_get_feature_report() with probe handles, logs [XCHK Probe] hex response
  STALL/error logged as INFO - not a crash, just means no Feature report for that RID
  do_get_feature_report() refactored: takes explicit client/dev/intf_num params
  instead of module-level statics - allows both polling and probe to share the fn
  ups_get_report_service_queue() restructured: recurring polling wrapped in if-block,
  service_probe_queue() always called at end regardless of s_active state
  ups_get_report_stop() calls probe_clear() on recurring stop
  probe_init/rid/clear() implementations added before public API section
- ups_usb_hid.c: xchk_probe_cb() static function routes to ups_get_report_probe_rid()
  Step 7: probe_init() + set_xchk_probe_cb(xchk_probe_cb) always called post-enum
  cleanup_device(): probe_clear() + set_xchk_probe_cb(NULL) on disconnect

Build: clean, zero warnings, zero errors (ESP-IDF v5.3.1)

## Files Staged
- src/current/main/ups_hid_parser.h
- src/current/main/ups_hid_parser.c
- src/current/main/ups_get_report.h
- src/current/main/ups_get_report.c
- src/current/main/ups_usb_hid.c
- docs/github_push.md
- docs/project_state.md
- docs/next_steps.md
