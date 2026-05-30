#!/bin/bash
# Linux build/flash/monitor wrapper for esp32-s3-nut-node-flex
# Run from: /home/claude/projects/esp32-s3-nut-node-flex/
#
# Usage:
#   ./build.sh build
#   ./build.sh flash
#   ./build.sh monitor
#   ./build.sh flash monitor
#   ./build.sh build flash monitor

set -e

IDF_PATH=/home/claude/.espressif/esp-idf
SRC_DIR=/home/claude/projects/esp32-s3-nut-node-flex/src/current
DOCS_DIR=/home/claude/projects/esp32-s3-nut-node-flex/docs
PORT_SCRIPT=/home/claude/scripts/find-esp32-port.sh

. "$IDF_PATH/export.sh" > /dev/null 2>&1

cd "$SRC_DIR"

PORT=$("$PORT_SCRIPT")
echo "Port: $PORT"

for cmd in "$@"; do
    case "$cmd" in
        build)
            echo "--- BUILD ---"
            idf.py build 2>&1 | tee "$DOCS_DIR/build.log"
            ;;
        flash)
            echo "--- FLASH ---"
            idf.py -p "$PORT" flash 2>&1 | tee "$DOCS_DIR/flash.log"
            ;;
        monitor)
            echo "--- MONITOR ---"
            idf.py -p "$PORT" monitor 2>&1 | tee "$DOCS_DIR/monitor.log"
            ;;
        *)
            echo "Unknown command: $cmd"
            echo "Usage: $0 [build] [flash] [monitor]"
            exit 1
            ;;
    esac
done
