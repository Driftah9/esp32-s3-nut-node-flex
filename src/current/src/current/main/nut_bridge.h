/*============================================================================
 MODULE: nut_bridge (public API)
 VERSION: v0.1-flex
 DATE: 2026-04-02

 RESPONSIBILITY
 - Mode 3 BRIDGE: forward raw USB HID stream to upstream host over TCP
 - Launched by main.c when op_mode == OP_MODE_BRIDGE
 - Falls back to nut_server_start() if upstream unreachable and fallback enabled

============================================================================*/

#pragma once
#include "cfg_store.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Start the HID bridge task.
 * Connects to cfg->upstream_host:cfg->upstream_port.
 * On connect: sends HID Report Descriptor as 2-byte-length-prefixed blob.
 * Then streams raw interrupt-IN packets as length-prefixed packets.
 * Falls back to nut_server if unreachable and cfg->upstream_fallback == 1.
 */
void nut_bridge_start(app_cfg_t *cfg);

#ifdef __cplusplus
}
#endif
