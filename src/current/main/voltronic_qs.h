/*============================================================================
 MODULE: voltronic_qs

 RESPONSIBILITY
 Voltronic-QS serial protocol over USB HID Interface 0.
 Used as alternative data source for dual-protocol devices (PowerWalker,
 Phoenixtec) when ups_protocol is set to UPS_PROTO_QS.

 PROTOCOL (Voltronic-QS / Megatec Q* family):
   Commands sent via HID SET_REPORT (Output type) to Interface 0.
   Responses read from Interrupt EP 0x81 on Interface 0.

   QS\r  -> (inV inVfault outV load% Hz battV temp flags\r
   F\r   -> #nomV nomA nomBattV nomHz\r

   Status flags (8 ASCII chars, bit7 first):
     bit7: Utility fail   (1=on battery)
     bit6: Battery low
     bit5: Boost/Buck active
     bit4: UPS fault
     bit3: Line-interactive (type indicator)
     bit2: Self-test running
     bit1: UPS shutdown
     bit0: Beeper on

 VERSION HISTORY
 R0  v0.34  Initial - QS protocol over USB for DECODE_VOLTRONIC devices.

============================================================================*/
#pragma once

#include "usb/usb_host.h"

/**
 * Start QS serial polling on Interface 0.
 * Called from ups_usb_hid.c when ups_protocol == UPS_PROTO_QS and
 * device is DECODE_VOLTRONIC. Spawns a polling task.
 *
 * @param client  USB host client handle (owned by usb_client_task)
 * @param dev     USB device handle
 * @return ESP_OK on success
 */
esp_err_t voltronic_qs_start(usb_host_client_handle_t client,
                              usb_device_handle_t dev);

/**
 * Stop QS polling and release Interface 0.
 */
void voltronic_qs_stop(void);

/**
 * Service the QS request queue. Called from usb_client_task event loop
 * (same pattern as ups_get_report_service_queue).
 */
void voltronic_qs_service(void);

/**
 * Returns true if QS polling is active.
 */
bool voltronic_qs_active(void);
