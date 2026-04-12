/*============================================================================
 MODULE: cfg_store (public API)

 REVERT HISTORY
 R0  v14.7  modular baseline public interface
 R1  v14.25 adds portal_pass field for HTTP portal access protection
 R2  v14.25 portal_pass defaults to "upsmon"; cfg_store_is_default_pass()
 R3  v0.1-flex  add op_mode, upstream_host, upstream_port, upstream_fallback
 R4  v0.11-flex OP_MODE constants renumbered 1/2/3 (was 0/1/2). NVS with old value 0
                falls to default case in switch = STANDALONE. No migration needed.
 R5  v0.34-flex Add ups_protocol field for dual-protocol devices (Voltronic/Phoenixtec).
                0=HID (default), 1=QS Serial. Toggle via portal on supported devices.

============================================================================*/

#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Operating mode constants (1-based: matches portal labels Mode 1/2/3) */
#define OP_MODE_STANDALONE  1   /* decode HID, serve NUT locally (default) */
#define OP_MODE_NUT_CLIENT  2   /* decode HID, push to upstream upsd */
#define OP_MODE_BRIDGE      3   /* forward raw HID stream to upstream host */

/* UPS protocol for dual-protocol devices (Voltronic/Phoenixtec) */
#define UPS_PROTO_HID       0   /* HID Power Device on Interface 1 (default) */
#define UPS_PROTO_QS        1   /* Voltronic-QS serial on Interface 0 */

typedef struct {
    char sta_ssid[33];
    char sta_pass[65];

    char ap_ssid[33];
    char ap_pass[65]; /* empty -> open AP */

    char ups_name[33];
    char nut_user[33];
    char nut_pass[33];

    char portal_pass[33]; /* default: "upsmon" - change via /config */

    /* Tri-mode operation (flex fork) */
    uint8_t  op_mode;             /* OP_MODE_STANDALONE / NUT_CLIENT / BRIDGE */
    uint8_t  upstream_fallback;   /* 1 = fall back to STANDALONE if upstream unreachable */
    char     upstream_host[64];   /* upstream NUT server or bridge target host */
    uint16_t upstream_port;       /* target port (default 3493 for NUT_CLIENT) */

    /* UPS protocol selection for dual-protocol devices */
    uint8_t  ups_protocol;        /* UPS_PROTO_HID (default) / UPS_PROTO_QS */
} app_cfg_t;

void      cfg_store_load_or_defaults(app_cfg_t *cfg);
void      cfg_store_ensure_ap_ssid(app_cfg_t *cfg);
esp_err_t cfg_store_commit(const app_cfg_t *cfg);

/* Returns true if portal_pass is still the factory default "upsmon".
 * Used by http_portal to display a change-password recommendation. */
bool      cfg_store_is_default_pass(const app_cfg_t *cfg);

#ifdef __cplusplus
}
#endif