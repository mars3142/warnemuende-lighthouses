#pragma once

#include "host/ble_gatt.h"
#include <stdint.h>

int gatt_svr_desc_presentation_bool_access(uint16_t con_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt,
                                           void *arg);
int gatt_svr_desc_valid_range_bool_access(uint16_t con_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt,
                                          void *arg);
