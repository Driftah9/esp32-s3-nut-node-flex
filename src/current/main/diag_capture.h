/*============================================================================
 MODULE: diag_capture (public API)

 RESPONSIBILITY
 - Opt-in boot log capture for user debugging
 - User selects duration (90s or 120s) via portal, device reboots,
   captures full boot log to a ring buffer, then makes it available
   at GET /diag-log for copy/paste. Passwords are redacted before display.

 REVERT HISTORY
 R0  v0.12-flex  Initial implementation

============================================================================*/

#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "cfg_store.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Call early in app_main - before wifi/USB start - reads NVS flag and arms
 * the vprintf hook + timer task if a capture was requested. No-op if idle. */
void diag_capture_check_and_arm(void);

/* State query functions - safe to call at any time */
bool     diag_capture_is_armed(void);       /* true while capture is running */
bool     diag_capture_is_ready(void);       /* true after timer fires, log available */
uint8_t  diag_capture_get_duration(void);   /* requested duration in seconds */
uint32_t diag_capture_get_elapsed_s(void);  /* seconds since capture started */

/* Returns pointer to captured log buffer and sets *len_out to byte count.
 * Returns NULL if capture not ready. Buffer is valid until next reboot. */
const char *diag_capture_get_log(size_t *len_out);

/* Scrub password fields from the ring buffer in-place (replace with asterisks).
 * Call once before serving /diag-log. Idempotent - safe to call multiple times. */
void diag_capture_scrub(const app_cfg_t *cfg);

#ifdef __cplusplus
}
#endif
