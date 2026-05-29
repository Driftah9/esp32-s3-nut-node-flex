/*============================================================================
 MODULE: ups_hid_map

 PURPOSE
 Static lookup table: HID usage (page + id + collection context) -> NUT variable name.
 Implements the NUT mge-hid.c mapping table pattern for the ESP.

 DESIGN
 The table covers all standard HID Power Device (0x84) and Battery System
 (0x85) usages that appear in the USB HID Power Devices class specification.
 It is used for annotation, the var store pass, and logging. The working
 typed decode paths (hid_field_cache_t and direct-decode functions) are
 not replaced.

 Context-specific entries appear FIRST in the table. CTX_ANY fallback
 entries appear last. ups_hid_map_lookup_ctx() does two passes:
   Pass 1 - exact context match (ctx != ANY)
   Pass 2 - CTX_ANY fallback

 This separation lets us:
   1. Annotate probe responses with NUT var names automatically
   2. Populate ups_var_store from all mapped fields per report
   3. Correctly distinguish input.voltage / output.voltage / battery.voltage
      by collection context rather than requiring per-device knowledge

 PORTABILITY FINDING (Phase 4 evaluation)
 NUT mge-hid.c uses: { hidpath, nutname, scale, flags, lkp_table }
 ESP equivalent:     { usage_page, usage_id, ctx, nut_var }
 - hidpath dropped: we have usage_page+usage_id directly from descriptor
 - scale dropped: unit_exponent in hid_field_t handles this
 - flags dropped: all entries here are read-only sensor fields
 - lkp_table dropped: enum handling lives in decode functions
 Verdict: standard usages are fully portable. Vendor extensions (undeclared
 rids) still require per-device decode_mode entries in ups_device_db.

 VERSION HISTORY
 R0  v0.7-flex  Initial implementation.
 R1  v0.40      Add ctx field to hid_nut_entry_t. Context-specific entries
                for ambiguous usages (Voltage, Current, Frequency, etc.).
                ups_hid_map_lookup_ctx() with two-pass exact/fallback logic.
                ups_hid_map_lookup() delegates to lookup_ctx with CTX_ANY.
                ups_hid_map_annotate_report() uses lookup_ctx with collection_ctx.
 R2  v0.42      annotate_report(): demote "payload too short" from WARN to DEBUG
                when field bit range exceeds actual received payload. APC rid=0x07
                declares 50 bytes but returns 3; fields beyond byte 2 spammed WARN
                on every XCHK probe cycle. Genuine extraction failures (within
                payload bounds) remain at WARN.

============================================================================*/

#include "ups_hid_map.h"
#include "ups_hid_desc.h"

#include <string.h>
#include "esp_log.h"

/* ---- Mapping table entry ---------------------------------------------- */
typedef struct {
    uint8_t    usage_page;
    uint16_t   usage_id;
    uint16_t   ctx;          /* NUT_MAP_CTX_* constant; 0 = any context */
    const char *nut_var;
} hid_nut_entry_t;

/* ---- Standard HID Power Device + Battery System -> NUT variable table -- */
/*
 * Source: USB HID Power Devices Class Specification 1.0
 * Verified against NUT usbhid-ups.c and mge-hid.c mapping tables.
 *
 * Context-specific entries (ctx != CTX_ANY) appear FIRST.
 * CTX_ANY fallback entries appear LAST per usage.
 * ups_hid_map_lookup_ctx() does exact-ctx pass before CTX_ANY pass.
 */
static const hid_nut_entry_t s_map[] = {

    /* --- Power Device page 0x84 - context-specific (voltage, current, frequency) --- */
    { 0x84u, 0x0030u, NUT_MAP_CTX_INPUT,         "input.voltage"            },
    { 0x84u, 0x0030u, NUT_MAP_CTX_OUTPUT,         "output.voltage"           },
    { 0x84u, 0x0030u, NUT_MAP_CTX_BATTERY,        "battery.voltage"          },
    { 0x84u, 0x0030u, NUT_MAP_CTX_POWER_SUMMARY,  "battery.voltage"          },
    { 0x84u, 0x0030u, NUT_MAP_CTX_ANY,            "output.voltage"           },

    { 0x84u, 0x0031u, NUT_MAP_CTX_INPUT,         "input.current"            },
    { 0x84u, 0x0031u, NUT_MAP_CTX_OUTPUT,         "output.current"           },
    { 0x84u, 0x0031u, NUT_MAP_CTX_ANY,            "output.current"           },

    { 0x84u, 0x0032u, NUT_MAP_CTX_INPUT,         "input.frequency"          },
    { 0x84u, 0x0032u, NUT_MAP_CTX_OUTPUT,         "output.frequency"         },
    { 0x84u, 0x0032u, NUT_MAP_CTX_ANY,            "output.frequency"         },

    { 0x84u, 0x0036u, NUT_MAP_CTX_BATTERY,        "battery.temperature"      },
    { 0x84u, 0x0036u, NUT_MAP_CTX_ANY,            "ups.temperature"          },

    { 0x84u, 0x0040u, NUT_MAP_CTX_INPUT,         "input.voltage.nominal"    },
    { 0x84u, 0x0040u, NUT_MAP_CTX_OUTPUT,         "output.voltage.nominal"   },
    { 0x84u, 0x0040u, NUT_MAP_CTX_BATTERY,        "battery.voltage.nominal"  },
    { 0x84u, 0x0040u, NUT_MAP_CTX_POWER_SUMMARY,  "battery.voltage.nominal"  },
    { 0x84u, 0x0040u, NUT_MAP_CTX_ANY,            "output.voltage.nominal"   },

    { 0x84u, 0x0042u, NUT_MAP_CTX_INPUT,         "input.frequency.nominal"  },
    { 0x84u, 0x0042u, NUT_MAP_CTX_OUTPUT,         "output.frequency.nominal" },
    { 0x84u, 0x0042u, NUT_MAP_CTX_ANY,            "output.frequency.nominal" },

    /* --- Power Device page 0x84 - unambiguous entries (ctx=ANY) --- */
    { 0x84u, 0x0001u, NUT_MAP_CTX_ANY,  "ups.id"                     },
    { 0x84u, 0x0016u, NUT_MAP_CTX_ANY,  "ups.powersummary"           },
    { 0x84u, 0x001Au, NUT_MAP_CTX_ANY,  "ups.input"                  },
    { 0x84u, 0x001Cu, NUT_MAP_CTX_ANY,  "ups.output"                 },
    { 0x84u, 0x001Eu, NUT_MAP_CTX_ANY,  "ups.flow"                   },
    { 0x84u, 0x0020u, NUT_MAP_CTX_ANY,  "ups.outlet"                 },
    { 0x84u, 0x0033u, NUT_MAP_CTX_ANY,  "ups.power"                  },
    { 0x84u, 0x0034u, NUT_MAP_CTX_ANY,  "ups.realpower"              },
    { 0x84u, 0x0035u, NUT_MAP_CTX_ANY,  "ups.load"                   },
    { 0x84u, 0x0037u, NUT_MAP_CTX_ANY,  "ambient.humidity"           },
    { 0x84u, 0x0041u, NUT_MAP_CTX_ANY,  "input.current.nominal"      },
    { 0x84u, 0x0043u, NUT_MAP_CTX_ANY,  "ups.power.nominal"          },
    { 0x84u, 0x0044u, NUT_MAP_CTX_ANY,  "ups.realpower.nominal"      },
    { 0x84u, 0x0053u, NUT_MAP_CTX_ANY,  "input.transfer.low"         },
    { 0x84u, 0x0054u, NUT_MAP_CTX_ANY,  "input.transfer.high"        },
    { 0x84u, 0x0055u, NUT_MAP_CTX_ANY,  "ups.delay.reboot"           },
    { 0x84u, 0x0056u, NUT_MAP_CTX_ANY,  "ups.delay.start"            },
    { 0x84u, 0x0057u, NUT_MAP_CTX_ANY,  "ups.delay.shutdown"         },
    { 0x84u, 0x0058u, NUT_MAP_CTX_ANY,  "ups.test.result"            },
    { 0x84u, 0x005Au, NUT_MAP_CTX_ANY,  "ups.beeper.status"          },
    { 0x84u, 0x0061u, NUT_MAP_CTX_ANY,  "outlet.switch"              },
    { 0x84u, 0x0062u, NUT_MAP_CTX_ANY,  "outlet.switchable"          },
    { 0x84u, 0x0065u, NUT_MAP_CTX_ANY,  "ups.status.overload"        },
    { 0x84u, 0x0066u, NUT_MAP_CTX_ANY,  "ups.status.overvoltage"     },
    { 0x84u, 0x006Eu, NUT_MAP_CTX_ANY,  "ups.status.boost"           },
    { 0x84u, 0x006Fu, NUT_MAP_CTX_ANY,  "ups.status.buck"            },
    { 0x84u, 0x00D0u, NUT_MAP_CTX_ANY,  "input.utility.present"      },

    /* --- Battery System page 0x85 - all unambiguous --- */
    { 0x85u, 0x0001u, NUT_MAP_CTX_ANY,  "battery.smbmode"                    },
    { 0x85u, 0x0013u, NUT_MAP_CTX_ANY,  "battery.id"                         },
    { 0x85u, 0x0027u, NUT_MAP_CTX_ANY,  "battery.rechargeable"               },
    { 0x85u, 0x002Bu, NUT_MAP_CTX_ANY,  "battery.charge.warning"             },
    { 0x85u, 0x002Du, NUT_MAP_CTX_ANY,  "battery.capacity.mode"              },
    { 0x85u, 0x002Cu, NUT_MAP_CTX_ANY,  "battery.discharging"                },
    { 0x85u, 0x0040u, NUT_MAP_CTX_ANY,  "battery.status.terminate-charge"    },
    { 0x85u, 0x0041u, NUT_MAP_CTX_ANY,  "battery.status.terminate-discharge" },
    { 0x85u, 0x0042u, NUT_MAP_CTX_ANY,  "battery.charge.low"                 },
    { 0x85u, 0x0043u, NUT_MAP_CTX_ANY,  "battery.runtime.low"                },
    { 0x85u, 0x0044u, NUT_MAP_CTX_ANY,  "battery.charging"                   },
    { 0x85u, 0x0045u, NUT_MAP_CTX_ANY,  "battery.discharging"                },
    { 0x85u, 0x0046u, NUT_MAP_CTX_ANY,  "battery.fullycharged"               },
    { 0x85u, 0x0047u, NUT_MAP_CTX_ANY,  "battery.fullydischarged"            },
    { 0x85u, 0x004Bu, NUT_MAP_CTX_ANY,  "battery.replace"                    },
    { 0x85u, 0x0064u, NUT_MAP_CTX_ANY,  "battery.charge"                     },
    { 0x85u, 0x0065u, NUT_MAP_CTX_ANY,  "battery.charge"                     },
    { 0x85u, 0x0066u, NUT_MAP_CTX_ANY,  "battery.charge"                     },
    { 0x85u, 0x0067u, NUT_MAP_CTX_ANY,  "battery.charge"                     },
    { 0x85u, 0x0068u, NUT_MAP_CTX_ANY,  "battery.runtime"                    },
    { 0x85u, 0x0069u, NUT_MAP_CTX_ANY,  "battery.runtime"                    },
    { 0x85u, 0x006Bu, NUT_MAP_CTX_ANY,  "battery.cycle.count"                },
    { 0x85u, 0x0073u, NUT_MAP_CTX_ANY,  "battery.runtime"                    },
    { 0x85u, 0x0083u, NUT_MAP_CTX_ANY,  "battery.voltage"                    },
    { 0x85u, 0x0085u, NUT_MAP_CTX_ANY,  "battery.runtime"                    },
    { 0x85u, 0x008Bu, NUT_MAP_CTX_ANY,  "battery.charging"                   },
    { 0x85u, 0x00D0u, NUT_MAP_CTX_ANY,  "input.utility.present"              },
};

static const size_t s_map_n = sizeof(s_map) / sizeof(s_map[0]);

/* ----------------------------------------------------------------------- */

const char *ups_hid_map_lookup_ctx(uint8_t usage_page, uint16_t usage_id,
                                    uint16_t collection_ctx)
{
    /* Pass 1: exact context match (ctx != ANY) */
    for (size_t i = 0; i < s_map_n; i++) {
        if (s_map[i].usage_page == usage_page &&
            s_map[i].usage_id  == usage_id   &&
            s_map[i].ctx       == collection_ctx &&
            s_map[i].ctx       != NUT_MAP_CTX_ANY) {
            return s_map[i].nut_var;
        }
    }
    /* Pass 2: fallback to CTX_ANY */
    for (size_t i = 0; i < s_map_n; i++) {
        if (s_map[i].usage_page == usage_page &&
            s_map[i].usage_id  == usage_id   &&
            s_map[i].ctx       == NUT_MAP_CTX_ANY) {
            return s_map[i].nut_var;
        }
    }
    return NULL;
}

/* ----------------------------------------------------------------------- */

const char *ups_hid_map_lookup(uint8_t usage_page, uint16_t usage_id)
{
    return ups_hid_map_lookup_ctx(usage_page, usage_id, NUT_MAP_CTX_ANY);
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

        /* Use context-aware lookup so annotation shows the correct NUT var */
        const char *nut = ups_hid_map_lookup_ctx(f->usage_page, f->usage_id,
                                                   f->collection_ctx);

        if (ok) {
            ESP_LOGI(log_tag,
                     "  [MAP] rid=%02X type=%u bit_off=%3u size=%2u "
                     "page=%02X uid=%04X ctx=%04X val=%-8"PRId32" -> %s",
                     (unsigned)rid, (unsigned)f->report_type,
                     (unsigned)f->bit_offset, (unsigned)f->bit_size,
                     (unsigned)f->usage_page, (unsigned)f->usage_id,
                     (unsigned)f->collection_ctx,
                     raw,
                     nut ? nut : "unmapped");
        } else {
            uint32_t field_end_bits  = (uint32_t)f->bit_offset + (uint32_t)f->bit_size;
            uint32_t payload_bits    = (uint32_t)plen * 8u;
            if (field_end_bits > payload_bits) {
                /* Field is beyond the actual received payload - device returned a
                 * shorter response than its descriptor declares. Silent at DEBUG. */
                ESP_LOGD(log_tag,
                         "  [MAP] rid=%02X bit_off=%3u size=%2u -> %s"
                         " (beyond %u-byte payload, skip)",
                         (unsigned)rid,
                         (unsigned)f->bit_offset, (unsigned)f->bit_size,
                         nut ? nut : "unmapped",
                         (unsigned)plen);
            } else {
                ESP_LOGW(log_tag,
                         "  [MAP] rid=%02X bit_off=%3u size=%2u page=%02X uid=%04X ctx=%04X"
                         " extract FAILED -> %s",
                         (unsigned)rid,
                         (unsigned)f->bit_offset, (unsigned)f->bit_size,
                         (unsigned)f->usage_page, (unsigned)f->usage_id,
                         (unsigned)f->collection_ctx,
                         nut ? nut : "unmapped");
            }
        }
    }

    if (field_count == 0) {
        ESP_LOGI(log_tag, "  [MAP] rid=%02X: no fields declared in descriptor"
                 " (vendor extension - no standard annotation available)", (unsigned)rid);
    }
}
