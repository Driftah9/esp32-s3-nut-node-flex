/*============================================================================
 MODULE: ups_hid_parser (public API)

 REVERT HISTORY
 R0  v14.9  scaffold
 R1  v14.10 candidate voltage/load decoding
 R2  v14.13 v14.3 stable
 R3  v15.0  Complete rewrite — usage-based, vendor-agnostic.
            Replaced model-hint approach with hid_desc_t field map.
            New API: ups_hid_parser_set_descriptor() must be called after
            USB enumeration before decode_report() will produce data.
 R5  v0.31  Add ups_hid_parser_get_input_rids() - returns list of report IDs
            that have Input reports, for dynamic GET_REPORT polling.
 R4  v0.18  Add ups_hid_parser_get_rid_interval() - exposes per-RID EMA
            inter-report interval learned from live interrupt-IN traffic.

============================================================================*/
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "ups_state.h"
#include "ups_hid_desc.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Reset all parser state.  Called on USB disconnect.
 */
void ups_hid_parser_reset(void);

/**
 * Supply the parsed HID report descriptor to the parser.
 * Must be called once after USB enumeration, before decode_report().
 * The descriptor is copied internally.
 *
 * @param desc  Parsed descriptor from ups_hid_desc_parse()
 */
void ups_hid_parser_set_descriptor(const hid_desc_t *desc);

/**
 * Decode one raw USB HID interrupt-IN report.
 *
 * @param data   Raw report bytes (including report ID byte if device uses them)
 * @param len    Number of bytes
 * @param upd    Output update structure - filled on success
 * @return       true if any field was decoded
 */
bool ups_hid_parser_decode_report(const uint8_t *data, size_t len,
                                   ups_state_update_t *upd);

/**
 * Run the dynamic RID cross-check immediately.
 *
 * Normally called automatically 30s after ups_hid_parser_set_descriptor()
 * via internal one-shot timer.  Can also be called manually for testing.
 *
 * Compares the set of report IDs seen in actual interrupt-IN traffic
 * against the Input reports declared in the HID descriptor:
 *   - Seen but not declared -> WARN (undocumented vendor extension)
 *   - Declared but never seen -> INFO + queued for GET_REPORT probe
 */
void ups_hid_parser_run_xchk(void);

/**
 * Callback type for XCHK one-shot GET_REPORT probes.
 *
 * Called from ups_hid_parser_run_xchk() for each Input RID that is
 * declared in the HID descriptor but never arrived in interrupt-IN
 * traffic during the 30s settle window.
 *
 * Implementation in ups_usb_hid.c routes to ups_get_report_probe_rid()
 * so the actual USB control transfer runs in usb_client_task.
 *
 * @param rid        Report ID to probe via GET_REPORT (Feature type)
 * @param probe_size wLength to use - feature_bytes from descriptor,
 *                   or input_bytes as fallback, clamped to 16
 */
typedef void (*ups_xchk_probe_fn_t)(uint8_t rid, uint16_t probe_size);

/**
 * Register (or deregister) the XCHK probe callback.
 * Called by ups_usb_hid after enumeration. Pass NULL to deregister.
 */
void ups_hid_parser_set_xchk_probe_cb(ups_xchk_probe_fn_t fn);

/**
 * Return a pointer to the internally stored parsed HID descriptor.
 * Valid after ups_hid_parser_set_descriptor() has been called.
 * Returns NULL if no valid descriptor has been loaded.
 * Used by the probe path to annotate GET_REPORT responses with NUT var names.
 */
const hid_desc_t *ups_hid_parser_get_desc(void);

/**
 * Return the learned EMA inter-report interval for a given RID.
 *
 * @param rid         Report ID to query
 * @param samples_out If non-NULL, receives the number of samples collected
 *                    (0 = no data yet; debounce is disabled below 3 samples)
 * @return            EMA interval in milliseconds, or 0 if not yet learned
 */
uint32_t ups_hid_parser_get_rid_interval(uint8_t rid, uint8_t *samples_out);

/**
 * @brief Return the largest Input report payload size from the parsed descriptor.
 *
 * Used by the interrupt-IN reader to allocate a transfer buffer large enough
 * for the largest HID Input report. Returns 0 if no descriptor has been set.
 */
uint16_t ups_hid_parser_max_input_bytes(void);

/**
 * @brief Copy the list of report IDs that have Input reports into caller buffer.
 *
 * Used by the periodic GET_REPORT polling path to dynamically determine which
 * RIDs to poll for DECODE_STANDARD devices (instead of hardcoded vendor lists).
 *
 * @param out_rids  Caller buffer to receive report IDs
 * @param max_rids  Size of caller buffer
 * @return          Number of RIDs written (0 if no descriptor loaded)
 */
uint8_t ups_hid_parser_get_input_rids(uint8_t *out_rids, uint8_t max_rids);

#ifdef __cplusplus
}
#endif
