# ESPHome UPS HID Constants Reference

**Source:** https://github.com/bullshit/esphome-components  
**File:** `components/ups_hid/constants_hid.h`  
**Date Captured:** 2026-05-24  

## Purpose

This directory contains a copy of the ESPHome project's well-documented HID constants file as a cross-reference for USB HID Power Device Class specifications.

The constants define:
- HID Usage Pages (Power Device, Battery System)
- Usage IDs for measurements (voltage, current, frequency, load, temperature)
- Configuration values (nominal/reference ratings)
- Control operations (switch on/off, delays, tests)
- Status indicators (present, good, failures, charging, warnings)
- Battery system metrics (charge level, runtime, capacity)

## USB HID Power Device Class Spec References

The constants align with:
- **USB HID Usage Tables v1.6** — https://usb.org/document-library/hid-usage-tables-16
- **USB Device Class Definition for HID Power Devices v1.1** — https://usb.org/sites/default/files/pdcv11.pdf
- **NUT (Network UPS Tools) hidtypes.h** — Reference implementation in the `drivers/hidtypes.h` header

## Useful Constants for Flex

### Core Voltage/Current Paths
```c
#define HID_USAGE_PAGE_POWER_DEVICE     0x84
#define HID_USAGE_POW_VOLTAGE           0x0030
#define HID_USAGE_POW_CURRENT           0x0031
#define HID_USAGE_POW_FREQUENCY         0x0032
#define HID_USAGE_POW_PERCENT_LOAD      0x0035
#define HID_USAGE_POW_TEMPERATURE       0x0036

// Configuration (nominal/reference values)
#define HID_USAGE_POW_CONFIG_VOLTAGE    0x0040
#define HID_USAGE_POW_CONFIG_CURRENT    0x0041
#define HID_USAGE_POW_CONFIG_FREQUENCY  0x0042
```

### Battery System Page
```c
#define HID_USAGE_PAGE_BATTERY_SYSTEM   0x85

// Battery status
#define HID_USAGE_BAT_CHARGING          0x0044
#define HID_USAGE_BAT_DISCHARGING       0x0045
#define HID_USAGE_BAT_FULLY_CHARGED     0x0046

// Battery measurements
#define HID_USAGE_BAT_REMAINING_CAPACITY    0x0066
#define HID_USAGE_BAT_RUN_TIME_TO_EMPTY     0x0068
#define HID_USAGE_BAT_AVERAGE_TIME_TO_FULL  0x006A
```

### Status Indicators
```c
#define HID_USAGE_POW_PRESENT           0x0060
#define HID_USAGE_POW_GOOD              0x0061
#define HID_USAGE_POW_INTERNAL_FAILURE  0x0062
#define HID_USAGE_POW_VOLTAGE_OUT_OF_RANGE 0x0063
#define HID_USAGE_POW_OVERLOAD          0x0065
#define HID_USAGE_POW_SHUTDOWN_IMMINENT 0x0069
```

### Device Information
```c
#define HID_USAGE_POW_I_MANUFACTURER    0x00FD
#define HID_USAGE_POW_I_PRODUCT         0x00FE
#define HID_USAGE_POW_I_SERIAL_NUMBER   0x00FF
```

## Regional Voltage/Frequency Standards

The file also documents nominal voltages and frequencies by region (useful for validation gates):

```c
#define VOLTAGE_NOMINAL_EU              230.0f    // Europe (IEC 60038)
#define VOLTAGE_NOMINAL_US              120.0f    // North America
#define FREQUENCY_NOMINAL_EU            50.0f     // Europe
#define FREQUENCY_NOMINAL_US            60.0f     // North America

#define VOLTAGE_MIN_VALID               80.0f     // Plausibility gate
#define VOLTAGE_MAX_VALID               300.0f
#define FREQUENCY_MIN_VALID             47.0f
#define FREQUENCY_MAX_VALID             65.0f
```

## How to Use This Reference

1. **HID Report Descriptor Parsing** — When decoding a report descriptor, look up Usage IDs here to confirm what metric a field represents (e.g., 0x0030 = Voltage)

2. **Report Mapping Tables** — When cross-referencing NUT driver behavior or device-specific quirks, these constants align with the official USB spec

3. **Validation Gates** — Use the voltage/frequency ranges to gate implausible values (e.g., reject 0V when AC is present)

4. **Device Coverage** — The standard HID Power Device class is how all major UPS manufacturers (APC, CyberPower, Eaton, Tripp Lite, etc.) expose metrics via USB. These constants cover the standard; vendor-specific extensions are handled separately

## See Also

- `docs/DECISIONS.md` — Architecture decisions on descriptor-driven vs. static mapping
- `docs/project_state.md` — Current implementation status (which metrics are confirmed working)
- `src/current/main/ups_hid_parser.c` — Flex's HID report decoder (already uses these concepts)

