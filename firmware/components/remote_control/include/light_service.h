#pragma once

#include "host/ble_hs.h"
#include <stdio.h>

// 0xA000 - Light Service
int gatt_svr_chr_light_led_access(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt,
                                  void *arg);

// 0xBEA0 - Beacon Control
int gatt_svr_chr_light_beacon_access(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt,
                                     void *arg);

/// Outdoor Light Descriptors
int gatt_svr_desc_led_user_desc_access(uint16_t con_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt,
                                       void *arg);

/// Beacon Characteristic Descriptors
int gatt_svr_desc_beacon_user_desc_access(uint16_t con_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt,
                                          void *arg);
