/*============================================================================
 MODULE: ups_hid_map

 PURPOSE
 Static lookup table: HID usage (page + id) -> NUT variable name.
 Implements the NUT mge-hid.c mapping table pattern for the ESP.

 DESIGN
 The table covers all standard HID Power Device (0x84) and Battery System
 (0x85) usages that appear in the USB HID Power Devices class specification.
 It is used for annotation and logging only - the working decode paths
 (hid_field_cache_t and direct-decode functions) are not replaced.

 This separation lets us:
   1. Annotate probe responses with NUT var names automatically
   2. Add NUT var annotation to ups_hid_desc_dump() output
   3. Serve as the foundation for a future table-driven decode pass

 PORTABILITY FINDING (Phase 4 evaluation)
 NUT mge-hid.c uses: { hidpath, nutname, scale, flags, lkp_table }
 ESP equivalent:     { usage_page, usage_id, nut_var }
 - hidpath dropped: we have usage_page+usage_id directly from descriptor
 - scale dropped: unit_exponent in hid_field_t handles this
 - flags dropped: all entries here are read-only sensor fields
 - lkp_table dropped: enum handling lives in decode functions
 Verdict: standard usages are fully portable. Vendor extensions (undeclared
 rids) still require per-device decode_mode entries in ups_device_db.

 VERSION HISTORY
 R0  v0.7-flex  Initial implementation.

============================================================================*/

#include "ups_hid_map.h"
#include "ups_hid_desc.h"

#include <string.h>
#include "esp_log.h"

/* ---- Mapping table entry ---------------------------------------------- */
typedef struct {
    uint8_t    usage_page;
    uint16_t   usage_id;
    const char *nut_var;
} hid_nut_entry_t;

/* ---- Standard HID Power Device + Battery System -> NUT variable table -- */
/*
 * Source: USB HID Power Devices Class Specification 1.0
 * Verified against NUT usbhid-ups.c and mge-hid.c mapping tables.
 * Entries marked (context) vary by which collection they appear in
 * (e.g. Voltage appears as both input.voltage and output.voltage).
 * We log the general NUT category - the caller sees the context from
 * the bit_offset / collection position in the descriptor.
 */
static const hid_nut_entry_t s_map[] = {

    /* --- Power Device page 0x84 --- */
    { 0x84u, 0x0001u, "ups.id"                   },
    { 0x84u, 0x0016u, "ups.powersummary"          },
    { 0x84u, 0x001Au, "ups.input"                 },
    { 0x84u, 0x001Cu, "ups.output"                },
    { 0x84u, 0x001Eu, "ups.flow"                  },
    { 0x84u, 0x0020u, "ups.outlet"                },
    { 0x84u, 0x0030u, "voltage"                   }, /* input.voltage or output.voltage */
    { 0x84u, 0x0031u, "current"                   },
    { 0x84u, 0x0032u, "frequency"                 }, /* input.frequency or output.frequency */
    { 0x84u, 0x0033u, "ups.power"                 },
    { 0x84u, 0x0034u, "ups.realpower"             },
    { 0x84u, 0x0035u, "ups.load"                  },
    { 0x84u, 0x0036u, "ups.temperature"           },
    { 0x84u, 0x0037u, "ambient.humidity"          },
    { 0x84u, 0x0040u, "input.voltage.nominal"     },
    { 0x84u, 0x0041u, "input.current.nominal"     },
    { 0x84u, 0x0042u, "input.frequency.nominal"   },
    { 0x84u, 0x0043u, "ups.power.nominal"         },
    { 0x84u, 0x0053u, "input.transfer.low"        },
    { 0x84u, 0x0054u, "input.transfer.high"       },
    { 0x84u, 0x0056u, "ups.delay.start"           },
    { 0x84u, 0x0057u, "ups.delay.shutdown"        },
    { 0x84u, 0x0061u, "outlet.switch"             },
    { 0x84u, 0x0062u, "outlet.switchable"         },
    { 0x84u, 0x0065u, "ups.status (overload)"     },
    { 0x84u, 0x0066u, "ups.status (overvoltage)"  },
    { 0x84u, 0x006Eu, "ups.status (boost/AVR)"    },
    { 0x84u, 0x006Fu, "ups.status (buck/AVR)"     },
    { 0x84u, 0x00D0u, "input.utility.present"     },

    /* --- Battery System page 0x85 --- */
    { 0x85u, 0x0001u, "battery.smbmode"           },
    { 0x85u, 0x0013u, "battery.id"                },
    { 0x85u, 0x0027u, "battery.rechargeable"      },
    { 0x85u, 0x002Bu, "battery.charge.warning"    },
    { 0x85u, 0x002Du, "battery.capacity.mode"     },
    { 0x85u, 0x0040u, "battery.status (terminate-charge)" },
    { 0x85u, 0x0041u, "battery.status (terminate-discharge)" },
    { 0x85u, 0x0042u, "battery.charge.low"        },
    { 0x85u, 0x0043u, "battery.runtime.low"       },
    { 0x85u, 0x0044u, "battery.charging"          },
    { 0x85u, 0x0045u, "battery.discharging"       },
    { 0x85u, 0x0046u, "battery.fullycharged"      },
    { 0x85u, 0x0047u, "battery.fullydischarged"   },
    { 0x85u, 0x004Bu, "battery.replace"           },
    { 0x85u, 0x0064u, "battery.charge (relative)" },
    { 0x85u, 0x0065u, "battery.charge (relative)" },
    { 0x85u, 0x0066u, "battery.charge"            },
    { 0x85u, 0x0067u, "battery.charge (APC variant)" },
    { 0x85u, 0x0068u, "battery.runtime"           },
    { 0x85u, 0x0069u, "battery.runtime (average)" },
    { 0x85u, 0x006Bu, "battery.cycle.count"       },
    { 0x85u, 0x0073u, "battery.runtime (APC non-standard)" },
    { 0x85u, 0x0083u, "battery.voltage"           },
    { 0x85u, 0x0085u, "battery.runtime (APC Smart-UPS)" },
    { 0x85u, 0x008Bu, "battery.charging (APC Smart-UPS)" },
    { 0x85u, 0x00D0u, "input.utility.present"     },
};

static const size_t s_map_n = sizeof(s_map) / sizeof(s_map[0]);

/* ----------------------------------------------------------------------- */

const char *ups_hid_map_lookup(uint8_t usage_page, uint16_t usage_id)
{
    /* Normalise APC vendor pages (0xFF84/0xFF85 stored as 0x84/0x85) */
    uint8_t pg = usage_page & 0x7Fu;
    if (pg == 0x04u) pg = 0x84u;  /* in case stored raw */

    for (size_t i = 0; i < s_map_n; i++) {
        if (s_map[i].usage_page == usage_page && s_map[i].usage_id == usage_id) {
            return s_map[i].nut_var;
        }
    }
    /* Try with normalised page (vendor variant) */
    if (pg != usage_page) {
        for (size_t i = 0; i < s_map_n; i++) {
            if (s_map[i].usage_page == pg && s_map[i].usage_id == usage_id) {
                return s_map[i].nut_var;
            }
        }
    }
    return NULL;
}

/* ----------------------------------------------------------------------- */

void ups_hid_map_annotate_report(const hid_desc_t *desc,
                                  uint8_t rid,
                                  const uint8_t *payload, size_t plen,
                                  const char *log_tag)
{
    if (!desc || !desc->valid || !payload || plen == 0) return;

    uint16_t field_count = 0;
    for (uint16_t i = 0; i < desc->field_count; i++) {
        const hid_field_t *f = &desc->fields[i];
        if (f->report_id != rid) continue;
        field_count++;

        int32_t raw = 0;
        bool ok = ups_hid_desc_extract_field(payload, plen, f, &raw);

        const char *nut = ups_hid_map_lookup(f->usage_page, f->usage_id);

        if (ok) {
            ESP_LOGI(log_tag,
                     "  [MAP] rid=%02X type=%u bit_off=%3u size=%2u "
                     "page=%02X uid=%04X val=%-8"PRId32" -> %s",
                     (unsigned)rid, (unsigned)f->report_type,
                     (unsigned)f->bit_offset, (unsigned)f->bit_size,
                     (unsigned)f->usage_page, (unsigned)f->usage_id,
                     raw,
                     nut ? nut : "unmapped");
        } else {
            ESP_LOGW(log_tag,
                     "  [MAP] rid=%02X bit_off=%3u size=%2u page=%02X uid=%04X"
                     " extract FAILED (payload too short?) -> %s",
                     (unsigned)rid,
                     (unsigned)f->bit_offset, (unsigned)f->bit_size,
                     (unsigned)f->usage_page, (unsigned)f->usage_id,
                     nut ? nut : "unmapped");
        }
    }

    if (field_count == 0) {
        ESP_LOGI(log_tag, "  [MAP] rid=%02X: no fields declared in descriptor"
                 " (vendor extension - no standard annotation available)", (unsigned)rid);
    }
}
