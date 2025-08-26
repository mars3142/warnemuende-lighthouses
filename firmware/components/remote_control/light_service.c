#include "include/light_service.h"

static const char *TAG = "light_service";

/// Capabilities of Device
int led_capabilities_read(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    char *data = "To be implemented later";
    os_mbuf_append(ctxt->om, data, strlen(data));
    return 0;
}

// Write data to ESP32 defined as server
int led_write(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    const char *received_payload = (const char *)ctxt->om->om_data;
    uint16_t payload_len = ctxt->om->om_len;

    // Define command strings
    const char CMD_LIGHT_ON[] = "LIGHT ON";
    const char CMD_LIGHT_OFF[] = "LIGHT OFF";
    const char CMD_FAN_ON[] = "FAN ON";
    const char CMD_FAN_OFF[] = "FAN OFF";

    if (payload_len == (sizeof(CMD_LIGHT_ON) - 1) && strncmp(received_payload, CMD_LIGHT_ON, payload_len) == 0)
    {
        ESP_LOGI(TAG, "LIGHT ON");
        // for (int i = 0; i < led_matrix_get_size(); i++)
        {
            // led_matrix_set_pixel(i, 10, 10, 0);
        }
    }
    else if (payload_len == (sizeof(CMD_LIGHT_OFF) - 1) && strncmp(received_payload, CMD_LIGHT_OFF, payload_len) == 0)
    {
        ESP_LOGI(TAG, "LIGHT OFF");
        // for (int i = 0; i < led_matrix_get_size(); i++)
        {
            // led_matrix_set_pixel(i, 0, 0, 0);
        }
    }
    else if (payload_len == (sizeof(CMD_FAN_ON) - 1) && strncmp(received_payload, CMD_FAN_ON, payload_len) == 0)
    {
        ESP_LOGI(TAG, "FAN ON");
        // TODO: Implement action for FAN ON
    }
    else if (payload_len == (sizeof(CMD_FAN_OFF) - 1) && strncmp(received_payload, CMD_FAN_OFF, payload_len) == 0)
    {
        ESP_LOGI(TAG, "FAN OFF");
        // TODO: Implement action for FAN OFF
    }
    else
    {
        char temp_buffer[payload_len + 1];
        memcpy(temp_buffer, received_payload, payload_len);
        temp_buffer[payload_len] = '\0';

        ESP_LOGI(TAG, "Unknown command from client: %s", temp_buffer);
    }

    return 0;
}

int led_char_a000_user_desc(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    const char *desc = "Capabilities of Device";
    os_mbuf_append(ctxt->om, desc, strlen(desc));
    return 0;
}

int led_char_dead_user_desc(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    const char *desc = "Readable Data from Server";
    os_mbuf_append(ctxt->om, desc, strlen(desc));
    return 0;
}

int led_char_dead_presentation(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op != BLE_GATT_ACCESS_OP_READ_DSC)
    {
        return BLE_ATT_ERR_READ_NOT_PERMITTED;
    }

    // GATT Presentation Format (0x2904), 7 Bytes:
    // [format, exponent, unit(2), namespace, description(2)]
    // format 0x04 = uint8, exponent 0, unit 0x2700 (unitless), namespace 1 (Bluetooth SIG Assigned Numbers),
    // description 0
    const uint8_t fmt[7] = {
        0x04,       // format = uint8
        0x00,       // exponent
        0x00, 0x27, // unit = org.bluetooth.unit.unitless (0x2700)
        0x01,       // namespace = 1 (SIG)
        0x00, 0x00  // description
    };
    return os_mbuf_append(ctxt->om, fmt, sizeof(fmt)) == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

int led_char_dead_valid_range(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op != BLE_GATT_ACCESS_OP_READ_DSC)
    {
        return BLE_ATT_ERR_READ_NOT_PERMITTED;
    }

    // Valid Range Descriptor (0x2906) Payload:
    // - Für numerische Werte: [min .. max] im "nativen" Datentyp der Characteristic.
    //   Hier: uint8, also 1 Byte min + 1 Byte max.
    const uint8_t range[2] = {0x00, 0x01}; // min=0, max=1

    return os_mbuf_append(ctxt->om, range, sizeof(range)) == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}
