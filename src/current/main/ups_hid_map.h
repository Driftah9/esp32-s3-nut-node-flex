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
 - ups_hid_map_lookup_ctx(): context-aware lookup using collection_ctx
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
 R1  v0.40      Context-aware lookup: NUT_MAP_CTX_* constants added.
                ups_hid_map_lookup_ctx() resolves ambiguous usages
                (e.g. Voltage 0x0030 -> input/output/battery.voltage)
                using the collection_ctx field from hid_field_t.

============================================================================*/
#pragma once

#include <stdint.h>
#include <stddef.h>
#include "ups_hid_desc.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Context values matching HID collection usage IDs on page 0x84 */
#define NUT_MAP_CTX_ANY           0x0000u  /* fallback: matches any context */
#define NUT_MAP_CTX_BATTERY       0x0010u  /* inside Battery System collection */
#define NUT_MAP_CTX_INPUT         0x001Au  /* inside Input collection */
#define NUT_MAP_CTX_OUTPUT        0x001Cu  /* inside Output collection */
#define NUT_MAP_CTX_POWER_SUMMARY 0x0024u  /* inside PowerSummary collection */

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
 * Context-aware NUT variable lookup.
 * Matches page+usage with collection context; falls back to CTX_ANY on no match.
 *
 * @param usage_page     HID usage page
 * @param usage_id       HID usage ID
 * @param collection_ctx Innermost collection usage ID from hid_field_t.collection_ctx
 * @return               NUT variable name, or NULL if not in table
 */
const char *ups_hid_map_lookup_ctx(uint8_t usage_page, uint16_t usage_id,
                                    uint16_t collection_ctx);

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
