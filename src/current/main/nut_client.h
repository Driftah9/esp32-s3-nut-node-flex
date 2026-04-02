/*============================================================================
 MODULE: nut_client (public API)
 VERSION: v0.1-flex
 DATE: 2026-04-02

 RESPONSIBILITY
 - Mode 2 NUT CLIENT: connect to upstream upsd, push live UPS data via SET VAR
 - Launched by main.c when op_mode == OP_MODE_NUT_CLIENT
 - Falls back to nut_server_start() if upstream unreachable and fallback enabled

============================================================================*/

#pragma once
#include "cfg_store.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Start the NUT client push task.
 * Attempts to connect to cfg->upstream_host:cfg->upstream_port.
 * If unreachable and cfg->upstream_fallback == 1, starts nut_server instead.
 * Call once from app_main when op_mode == OP_MODE_NUT_CLIENT.
 */
void nut_client_start(app_cfg_t *cfg);

#ifdef __cplusplus
}
#endif
