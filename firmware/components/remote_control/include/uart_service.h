#pragma once

#include "host/ble_hs.h"
#include "host/ble_sm.h"
#include "host/ble_uuid.h"

// Unique UUIDs for UART-Service (compatible with Nordic UART Service)
// Service UUID: 6E400001-B5A3-F393-E0A9-E50E24DCCA9E
extern const ble_uuid128_t gatt_svr_svc_uart_uuid;

// RX Characteristic UUID: 6E400002-B5A3-F393-E0A9-E50E24DCCA9E
extern const ble_uuid128_t gatt_svr_chr_uart_rx_uuid;

// TX Characteristic UUID: 6E400003-B5A3-F393-E0A9-E50E24DCCA9E
extern const ble_uuid128_t gatt_svr_chr_uart_tx_uuid;

extern uint16_t conn_handle;
extern uint16_t tx_chr_val_handle;

int gatt_svr_chr_uart_access(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg);
void send_ble_data(const char *data);
