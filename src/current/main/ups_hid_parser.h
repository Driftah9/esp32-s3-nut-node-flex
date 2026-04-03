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
 *   - Declared but never seen -> INFO (descriptor claims Input, never arrived)
 */
void ups_hid_parser_run_xchk(void);

#ifdef __cplusplus
}
#endif
