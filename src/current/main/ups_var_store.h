/*============================================================================
 MODULE: ups_var_store (public API)

 PURPOSE
 Runtime NUT variable key-value store, one slot per connected UPS device.
 Populated by the HID decode pass using the context-aware mapping table.

 Provides the foundation for multi-device (USB hub) support:
   slot 0  -> "ups"    (first or only UPS)
   slot 1  -> "ups-2"  (second UPS via hub)
   slot 2  -> "ups-3"
   slot 3  -> "ups-4"

 Variables are stored as strings in NUT format. The NUT server reads from
 this store when serving LIST VAR, GET VAR, and STATUS commands.

 VERSION HISTORY
 R0  v0.40  Initial implementation. Multi-device skeleton with 4 slots.
            Context-aware HID mapping populates variables per slot.

============================================================================*/
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UPS_MAX_SLOTS    4
#define UPS_MAX_VARS    64
#define UPS_VAR_NAME_LEN 48
#define UPS_VAR_VAL_LEN  64

typedef struct {
    char name[UPS_VAR_NAME_LEN];
    char value[UPS_VAR_VAL_LEN];
    bool rw;
} ups_nut_var_t;

typedef struct {
    ups_nut_var_t vars[UPS_MAX_VARS];
    uint8_t       var_count;
    bool          active;
    char          device_name[24];
} ups_slot_t;

void     ups_var_store_init(void);
void     ups_var_store_activate(uint8_t slot, const char *device_name);
void     ups_var_store_reset(uint8_t slot);
void     ups_var_store_set(uint8_t slot, const char *name, const char *value);
bool     ups_var_store_get(uint8_t slot, const char *name, char *out, size_t out_max);
const ups_nut_var_t *ups_var_store_list(uint8_t slot, uint8_t *out_count);
uint8_t  ups_var_store_active_count(void);
const char *ups_var_store_device_name(uint8_t slot);

#ifdef __cplusplus
}
#endif
