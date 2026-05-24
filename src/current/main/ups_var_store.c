/*============================================================================
 MODULE: ups_var_store

 PURPOSE
 Runtime NUT variable key-value store - one slot per connected UPS device.
 Thread-safe via FreeRTOS mutex. Up to UPS_MAX_SLOTS (4) devices supported.

 VERSION HISTORY
 R0  v0.40  Initial implementation.

============================================================================*/

#include "ups_var_store.h"
#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"

static const char *TAG = "ups_var_store";

static ups_slot_t        s_slots[UPS_MAX_SLOTS];
static SemaphoreHandle_t s_lock;

static const char *default_name(uint8_t slot)
{
    static const char * const names[] = { "ups", "ups-2", "ups-3", "ups-4" };
    return (slot < UPS_MAX_SLOTS) ? names[slot] : "ups";
}

void ups_var_store_init(void)
{
    s_lock = xSemaphoreCreateMutex();
    memset(s_slots, 0, sizeof(s_slots));
    for (uint8_t i = 0; i < UPS_MAX_SLOTS; i++) {
        snprintf(s_slots[i].device_name, sizeof(s_slots[i].device_name),
                 "%s", default_name(i));
    }
    ESP_LOGI(TAG, "Initialized %u slots", (unsigned)UPS_MAX_SLOTS);
}

void ups_var_store_activate(uint8_t slot, const char *device_name)
{
    if (slot >= UPS_MAX_SLOTS) return;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_slots[slot].active = true;
    if (device_name && device_name[0]) {
        snprintf(s_slots[slot].device_name, sizeof(s_slots[slot].device_name),
                 "%s", device_name);
    }
    xSemaphoreGive(s_lock);
}

void ups_var_store_reset(uint8_t slot)
{
    if (slot >= UPS_MAX_SLOTS) return;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    char saved[24];
    snprintf(saved, sizeof(saved), "%s", s_slots[slot].device_name);
    memset(&s_slots[slot], 0, sizeof(s_slots[slot]));
    snprintf(s_slots[slot].device_name, sizeof(s_slots[slot].device_name),
             "%s", saved);
    xSemaphoreGive(s_lock);
    ESP_LOGD(TAG, "slot %u reset", (unsigned)slot);
}

void ups_var_store_set(uint8_t slot, const char *name, const char *value)
{
    if (slot >= UPS_MAX_SLOTS || !name || !value) return;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    ups_slot_t *sl = &s_slots[slot];
    for (uint8_t i = 0; i < sl->var_count; i++) {
        if (strncmp(sl->vars[i].name, name, UPS_VAR_NAME_LEN - 1) == 0) {
            snprintf(sl->vars[i].value, UPS_VAR_VAL_LEN, "%s", value);
            xSemaphoreGive(s_lock);
            return;
        }
    }
    if (sl->var_count < UPS_MAX_VARS) {
        snprintf(sl->vars[sl->var_count].name,  UPS_VAR_NAME_LEN, "%s", name);
        snprintf(sl->vars[sl->var_count].value, UPS_VAR_VAL_LEN,  "%s", value);
        sl->var_count++;
    } else {
        ESP_LOGW(TAG, "slot %u full (%u vars), cannot store %s",
                 (unsigned)slot, (unsigned)UPS_MAX_VARS, name);
    }
    xSemaphoreGive(s_lock);
}

bool ups_var_store_get(uint8_t slot, const char *name, char *out, size_t out_max)
{
    if (slot >= UPS_MAX_SLOTS || !name || !out || !out_max) return false;
    bool found = false;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    const ups_slot_t *sl = &s_slots[slot];
    for (uint8_t i = 0; i < sl->var_count; i++) {
        if (strncmp(sl->vars[i].name, name, UPS_VAR_NAME_LEN - 1) == 0) {
            snprintf(out, out_max, "%s", sl->vars[i].value);
            found = true;
            break;
        }
    }
    xSemaphoreGive(s_lock);
    return found;
}

const ups_nut_var_t *ups_var_store_list(uint8_t slot, uint8_t *out_count)
{
    if (slot >= UPS_MAX_SLOTS) {
        if (out_count) *out_count = 0;
        return NULL;
    }
    if (out_count) *out_count = s_slots[slot].var_count;
    return s_slots[slot].vars;
}

uint8_t ups_var_store_active_count(void)
{
    uint8_t n = 0;
    for (uint8_t i = 0; i < UPS_MAX_SLOTS; i++) {
        if (s_slots[i].active) n++;
    }
    return n;
}

const char *ups_var_store_device_name(uint8_t slot)
{
    if (slot >= UPS_MAX_SLOTS || !s_slots[slot].active) return NULL;
    return s_slots[slot].device_name;
}
