#include "include/uart_service.h"

static const char *TAG = "uart_service";

// Service UUID: 6E400001-B5A3-F393-E0A9-E50E24DCCA9E
const ble_uuid128_t gatt_svr_svc_uart_uuid =
    BLE_UUID128_INIT(0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0, 0x93, 0xF3, 0xA3, 0xB5, 0x01, 0x00, 0x40, 0x6E);

// RX Characteristic UUID: 6E400002-B5A3-F393-E0A9-E50E24DCCA9E
const ble_uuid128_t gatt_svr_chr_uart_rx_uuid =
    BLE_UUID128_INIT(0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0, 0x93, 0xF3, 0xA3, 0xB5, 0x02, 0x00, 0x40, 0x6E);

// TX Characteristic UUID: 6E400003-B5A3-F393-E0A9-E50E24DCCA9E
const ble_uuid128_t gatt_svr_chr_uart_tx_uuid =
    BLE_UUID128_INIT(0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0, 0x93, 0xF3, 0xA3, 0xB5, 0x03, 0x00, 0x40, 0x6E);

uint16_t conn_handle;
uint16_t tx_chr_val_handle;

// Callback function for GATT events (read/write on characteristics)
int gatt_svr_chr_uart_access(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    switch (ctxt->op)
    {
    case BLE_GATT_ACCESS_OP_WRITE_CHR:
        // Check if the RX characteristic is being written to
        // if (ble_uuid_cmp((const ble_uuid_t *)&ctxt->chr->uuid, (const ble_uuid_t *)&gatt_svr_chr_uart_rx_uuid) == 0)
        {
            // Get data from the buffer
            uint16_t data_len = OS_MBUF_PKTLEN(ctxt->om);
            char *data = (char *)malloc(data_len + 1);
            if (data)
            {
                int rc = ble_hs_mbuf_to_flat(ctxt->om, data, data_len, NULL);
                if (rc == 0)
                {
                    data[data_len] = '\0';
                    ESP_LOGI(TAG, "Received data: %s", data);
                }
                free(data);
            }
            return 0;
        }
        break;
    default:
        break;
    }
    return BLE_ATT_ERR_UNLIKELY;
}

// Function to send data via the TX characteristic
void send_ble_data(const char *data)
{
    if (conn_handle != 0)
    { // Only send when connected
        struct os_mbuf *om = ble_hs_mbuf_from_flat(data, strlen(data));
        if (om)
        {
            int rc = ble_gatts_notify_custom(conn_handle, tx_chr_val_handle, om);
            if (rc == 0)
            {
                ESP_LOGI(TAG, "Sent data: %s", data);
            }
            else
            {
                ESP_LOGE(TAG, "Error sending data: %d", rc);
            }
        }
    }
}
