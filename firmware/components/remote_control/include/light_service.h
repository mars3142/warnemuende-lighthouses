#pragma once

#include "host/ble_hs.h"
#include <stdio.h>

// 0xA000 - Light Service
int led_cb(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg);

// 0xBEA0 - Beacon Control
int beacon_cb(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg);

/// Outdoor Light Descriptors
int led_char_user_desc_cb(uint16_t con_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg);

/// Beacon Characteristic Descriptors
int beacon_char_user_desc_cb(uint16_t con_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg);
int bool_char_presentation_cb(uint16_t con_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg);
int bool_char_valid_range_cb(uint16_t con_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg);
