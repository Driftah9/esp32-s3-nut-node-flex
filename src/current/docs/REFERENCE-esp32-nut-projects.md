# Cross-Reference: ESP32 NUT Server Projects

**Compiled:** 2026-05-24  
**Purpose:** Compare architectural approaches across similar projects to identify best practices and pitfalls

---

## Project Matrix

| Project | Transport | Descriptor Strategy | Device Limit | Quirk Handling | Status |
|---------|-----------|---|---|---|---|
| **Flex (this)** | USB HID + serial (RJ45/RJ50) | Dynamic + device DB | 1 (serial), 10+ (USB) | ups_device_db.c profiles | Active investigation |
| **Borchev** | USB HID only | Dynamic discovery | 10+ via hub | Context-aware mapping | Active, production-ready |
| **ludoux** | USB HID only | Hardcoded IDs + quirks | 1 (single model) | SANTAK-specific hacks | Abandoned (architectural debt) |
| **N1k0droid** | GPIO sensors | Hardware integration | 1 (Oukitel P800E) | Physical clamps + relays | Niche use case |
| **syssi** | RS232 serial | APC Smart Protocol | 1 (APC only) | APC protocol decode | Active (ESPHome) |

---

## Key Architectural Insights

### 1. Borchev's Context-Aware Mapping (Most Relevant to Flex)

**The Problem:** HID Usage ID 0x30 = "Voltage" appears in multiple contexts (Input, Output, Battery, PowerSummary). Same ID, different meanings.

**Their Solution:** Mapping table with context discrimination:

```c
typedef struct {
    uint8_t page;              // HID Usage Page (0x84, 0x85, etc.)
    uint8_t usage;             // HID Usage ID (0x30, 0x31, etc.)
    context_t context;         // CTX_INPUT, CTX_OUTPUT, CTX_BATTERY, CTX_POWER_SUMMARY, CTX_NONE
    const char *nut_varname;   // "input.voltage", "output.voltage", "battery.voltage"
    flags_t flags;             // FLAG_MAX (max aggregation), FLAG_VALUE, etc.
} usage_map_t;
```

Example table entries:
```c
{0x84, 0x30, CTX_OUTPUT,        "output.voltage",          FLAG_MAX},
{0x84, 0x30, CTX_INPUT,         "input.voltage",           FLAG_MAX},
{0x84, 0x30, CTX_BATTERY,       "battery.voltage",         FLAG_MAX},
{0x84, 0x30, CTX_POWER_SUMMARY, "battery.voltage",         FLAG_MAX},
{0x84, 0x30, CTX_NONE,          "output.voltage",          FLAG_MAX},  // fallback
```

**Why This Works:**
- Descriptor parsing determines which collections declare 0x30
- Collection type (Input, Output, Battery) = context
- Same table works across all brands
- Fallback (CTX_NONE) handles edge cases
- NUT variable names are standardized (output.*, battery.*, input.*, ups.*)

**Flex's Equivalent:** ups_state_apply_update() with key matching, but less systematic. Borchev's table approach is cleaner for future expansion.

---

### 2. Multi-Device Support (USB Hub)

**Borchev:** Supports 10+ UPS devices simultaneously via USB hub. Each gets a NUT name:
- Device 0 → "ups"
- Device 1 → "ups-2"
- Device 2 → "ups-3"

**Flex:** Currently single-device focus. Multi-UPS support would require:
- Device enumeration loop in usb_client_task
- Per-device state struct array (instead of monolithic ups_state_t)
- NUT server device name multiplexing

---

### 3. HID Usage Reference (Canonical)

Both Borchev and the ESPHome project cross-reference the same USB HID specs:

**Power Device Page (0x84):**
```
0x30 = Voltage (input, output, battery contexts)
0x31 = Current (input, output contexts)
0x32 = Frequency (input, output contexts)
0x33 = ApparentPower (0x84:0x33 @ output = ups.power)
0x34 = ActivePower (0x84:0x34 @ output = ups.realpower)
0x35 = PercentLoad (ups.load)
0x36 = Temperature (battery.temperature or ups.temperature)
0x40 = ConfigVoltage (nominal, differs from 0x30 actual)
0x42 = ConfigFrequency (nominal)
0x53 = LowVoltageTransfer (input transfer low threshold)
0x54 = HighVoltageTransfer (input transfer high threshold)
0x55 = DelayBeforeReboot
0x56 = DelayBeforeStartup
0x57 = DelayBeforeShutdown
0x58 = Test (result status)
0x5A = AudibleAlarmControl (beeper status)
```

**Battery System Page (0x85):**
```
0x44 = Charging (boolean status)
0x45 = Discharging (boolean status)
0x46 = FullyCharged (boolean status)
0x66 = RemainingCapacity (battery charge %)
0x68 = RunTimeToEmpty (battery runtime)
0x6A = AverageTimeToFull
0x87 = ManufacturerName
0x88 = DeviceName
0x89 = DeviceChemistry
```

---

### 4. Pitfalls: ludoux (Cautionary Tale)

**What They Did Right:**
- Got a working implementation (SANTAK TG-BOX 850)
- LED status indicator is nice UX

**What Failed:**
- Hardcoded report IDs → One device only
- No descriptor parsing → Can't extend
- Static quirks → New models = new code
- Project abandoned (author now unmotivated to maintain single-device hack)

**Flex Lesson:** Generic descriptor-driven approach (like Borchev) is essential if expanding to "all RJ45/RJ50/USB" UPS types.

---

### 5. Serial Protocol (syssi - APC SmartPort)

**Completely Different Transport:** RS232 over RJ50 (APC SmartUPS).

**Protocol:** Binary, APC-proprietary (not HID, not NUT native)

**Implementation:** ESPHome component + RS232-to-TTL converter

**Relevance to Flex:**
- If expanding to RJ50 (APC SmartPort), would need similar serial decoder
- Different from USB HID (parallel transport, not descriptor-driven)
- NUT driver equivalent: `apcsmart` (driver for APC Smart UPS)

---

## Best Practices to Adopt

### 1. Context-Aware Mapping (from Borchev)

```c
// Instead of:
if (usage_id == 0x30 && we_guess_context) { ... }

// Use:
usage_map_t mapping = lookup_map(page, usage, context);
char *nut_var = mapping.nut_varname;
```

This scales to new devices without code changes.

### 2. Multi-Device Skeleton (from Borchev)

Even if flex targets 1 device now, the enum pattern supports future expansion:
```c
for (int i = 0; i < hid_ups_get_device_count(); i++) {
    const char *name = hid_ups_get_device_name(i);
    hid_ups_get_vars(name, ...);
}
```

### 3. Avoid Hardcoded IDs (ludoux warning)

Descriptor parsing + fallback is non-negotiable. Hardcoding = dead project.

### 4. NUT Variable Naming (Standard from NUT Spec)

Borchev's mapping table uses official NUT variable names:
- `input.voltage`, `output.voltage`, `battery.voltage`
- `battery.charge`, `battery.runtime`
- `ups.load`, `ups.temperature`
- `ups.status` (OL, OB, CHRG, DISCHRG, etc.)

Use this standard for NUT protocol responses.

---

## References

- **Borchev repo:** https://github.com/Borchev/esp32-usb-nut-server
- **ludoux repo:** https://github.com/ludoux/esp32-nut-server-usbhid (abandoned)
- **syssi ESPHome APC:** https://github.com/syssi/esphome-apc-ups
- **N1k0droid (hardware sensors):** https://github.com/N1k0droid/ESP32-Oukitel-P800-SmartUPS
- **NUT Project:** https://networkupstools.org/docs/man/

