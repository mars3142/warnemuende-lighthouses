#pragma once

#include "host/ble_hs.h"
#include <stdio.h>

// 0x2A00 Device Name
int device_name_read(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg);

// 0x2A26 Firmware Revision String
int device_firmware_revision_read(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt,
                                  void *arg);

// 0x2A27 Hardware Revision String
int device_hardware_revision_read(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt,
                                  void *arg);

// 0x2A29 Manufacturer Name String
int device_manufacturer_read(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg);
