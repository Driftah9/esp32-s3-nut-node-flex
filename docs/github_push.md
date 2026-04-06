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
v0.25b

## Commit Message
v0.25b - extern C guard on app_main, IDF target updated to v5.5.4

Fix: Users building with a C++ component (or ESP-IDF v5.5.x which links
via g++) get undefined reference to app_main at link time. main.c defines
app_main with C linkage but the C++ linker mangles it. Added extern "C"
guard around app_main - harmless in pure C builds, fixes link error when
any .cpp file is present in the project.

Update: IDF version string in startup banner updated from v5.3.1 to v5.5.4
to reflect current remote build environment.

## Files Staged
- src/current/main/main.c
- docs/github_push.md
