/*============================================================================
 MODULE: ups_hid_map (public API)

 PURPOSE
 Maps HID usage (page + id) to NUT variable names.
 Implements the NUT mge-hid.c mapping table pattern for the ESP:
 a static table of standard HID Power Device and Battery System usages
 that translates raw HID field identifiers into meaningful NUT variable
 names for logging and annotation.

 This is the "evaluate NUT mge-hid.c mapping table format" deliverable.
 Key finding: the NUT mapping table format IS directly portable to the ESP
 for all standard usages (pages 0x84 and 0x85). Vendor extension usages
 (not declared in the descriptor) still require per-device knowledge and
 are handled by the existing decode_mode / direct-decode paths.

 USAGE
 - ups_hid_map_lookup(): returns NUT var name for a usage or NULL
 - ups_hid_map_annotate_report(): logs each field in a RID with its
   extracted value and NUT var name - used in probe and dump paths

 PORTABILITY NOTE
 The NUT mge-hid.c format uses { hidpath, nutname, scale, flags, lkp_table }.
 The ESP equivalent drops hidpath (we use usage_page/usage_id directly from
 the parsed descriptor), drops lkp_table (enums handled in decode functions),
 and drops flags (all fields here are read-only sensor values).
 Scale is embedded in unit_exponent from the descriptor - not needed in table.

 VERSION HISTORY
 R0  v0.7-flex  Initial implementation.

============================================================================*/
#pragma once

#include <stdint.h>
#include <stddef.h>
#include "ups_hid_desc.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Look up the NUT variable name for a HID usage.
 *
 * @param usage_page  HID usage page (0x84 Power Device, 0x85 Battery System)
 * @param usage_id    HID usage ID within that page
 * @return            NUT variable name string (e.g. "battery.charge"),
 *                    or NULL if not in the standard mapping table
 */
const char *ups_hid_map_lookup(uint8_t usage_page, uint16_t usage_id);

/**
 * Annotate a raw HID report by walking its descriptor fields.
 *
 * For each field declared in the descriptor for the given report ID,
 * extracts the value from the raw data and logs it with:
 *   - bit offset and size from descriptor
 *   - usage page:id
 *   - NUT variable name (from mapping table, or "unmapped")
 *   - extracted integer value
 *
 * Used by the XCHK probe path (service_probe_queue) and dump functions
 * to provide human-readable annotation of raw report bytes.
 *
 * @param desc     Parsed HID descriptor
 * @param rid      Report ID to annotate
 * @param payload  Report payload bytes NOT including the report-ID byte.
 *                 For GET_REPORT responses: pass data+1, len-1.
 * @param plen     Length of payload in bytes
 * @param log_tag  ESP-IDF log tag to use for output
 */
void ups_hid_map_annotate_report(const hid_desc_t *desc,
                                  uint8_t rid,
                                  const uint8_t *payload, size_t plen,
                                  const char *log_tag);

#ifdef __cplusplus
}
#endif
