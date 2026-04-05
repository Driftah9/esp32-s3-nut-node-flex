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
v0.21

## Commit Message
v0.21 - Adaptive dashboard poll rate: fast while waiting, normal once live

Dashboard AJAX changed from fixed setInterval(5000) to setTimeout-based
adaptive scheduling driven by ups_valid from /status JSON.

Poll rate: 1500ms while ups_valid=false, 5000ms once ups_valid=true.
XHR errors also fall back to 1500ms.

data_age indicator shows waiting message while ups_valid=false instead
of showing a potentially misleading stale age value.

Addresses visible delay for event-driven devices such as Eaton 3S.
When the first data arrives the page now reflects it within 1.5s.

## Files Staged
- src/current/main/http_dashboard.c
- docs/github_push.md
- docs/project_state.md
