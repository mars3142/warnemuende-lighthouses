#pragma once

#include "host/ble_hs.h"
#include <stdio.h>

// 0x2A00 - Device Name
int gatt_svr_chr_device_name_access(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt,
                                    void *arg);

// 0x2A26 - Firmware Revision String
int gatt_svr_chr_device_firmware_access(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt,
                                        void *arg);

// 0x2A27 - Hardware Revision String
int gatt_svr_chr_device_hardware_access(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt,
                                        void *arg);

// 0x2A29 - Manufacturer Name String
int gatt_svr_chr_device_manufacturer_access(uint16_t conn_handle, uint16_t attr_handle,
                                            struct ble_gatt_access_ctxt *ctxt, void *arg);
