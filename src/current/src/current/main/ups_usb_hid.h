/*============================================================================
 MODULE: ups_usb_hid (public API)

 RESPONSIBILITY
 - USB Host lifecycle
 - Enumerate attached USB devices (on boot + hotplug)
 - Parse HID interface and stream interrupt reports
 - Feed decoded reports into ups_state via ups_hid_parser

 REVERT HISTORY
 R0  v14.7 USB skeleton API
 R1  v14.8 scan + identify API
 R2  v14.9 parser/state integration

============================================================================*/
#pragma once

#include "cfg_store.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void ups_usb_hid_start(const app_cfg_t *cfg);

/* ---------- Bridge mode API (Mode 3) ----------
 *
 * ups_usb_hid_set_bridge_cb()
 *   Register a callback invoked from the USB client task on every
 *   interrupt-IN packet (ALL packets, not just changed ones).
 *   Callback must be fast and non-blocking - enqueue and return.
 *   Pass NULL to deregister.
 *
 * ups_usb_hid_get_report_descriptor()
 *   Retrieve a pointer to the raw HID Report Descriptor bytes fetched
 *   during enumeration.  Returns false if not yet available (USB not
 *   enumerated) or if the descriptor buffer is empty.
 *   The pointer is valid until the next USB device disconnect.
 */
typedef void (*ups_hid_bridge_cb_t)(const uint8_t *data, uint16_t len);

void ups_usb_hid_set_bridge_cb(ups_hid_bridge_cb_t cb);
bool ups_usb_hid_get_report_descriptor(const uint8_t **buf_out, uint16_t *len_out);

#ifdef __cplusplus
}
#endif
