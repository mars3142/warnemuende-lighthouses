#include "include/light_service.h"
#include "beacon.h"

static const char *TAG = "light_service";

static uint8_t g_led_enabled = 1; // 0=false, 1=true

/// Characteristic Callbacks
int led_cb(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR)
    {
        char *data = "To be implemented later";
        os_mbuf_append(ctxt->om, data, strlen(data));
        return 0;
    }
    return BLE_ATT_ERR_UNLIKELY;
}

int beacon_cb(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR)
    {
        return os_mbuf_append(ctxt->om, &g_led_enabled, sizeof(g_led_enabled)) == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR)
    {
        // it has to be 1 Byte (0 or 1)
        if (OS_MBUF_PKTLEN(ctxt->om) != 1)
        {
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }
        uint8_t val;
        os_mbuf_copydata(ctxt->om, 0, 1, &val);
        if (val > 1)
        {
            return BLE_ATT_ERR_UNLIKELY; // or BLE_ATT_ERR_INVALID_ATTR_VALUE
        }

        if (val == g_led_enabled) // value is already set
        {
            return 0;
        }

        g_led_enabled = val;

        if (g_led_enabled)
        {
            beacon_start();
        }
        else
        {
            beacon_stop();
        }
        return 0;
    }
    return BLE_ATT_ERR_UNLIKELY;
}

// Characteristic User Descriptions
int led_char_user_desc_cb(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_DSC)
    {
        const char *desc = "Aussenbeleuchtung";
        return os_mbuf_append(ctxt->om, desc, strlen(desc)) == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }
    return BLE_ATT_ERR_READ_NOT_PERMITTED;
}

int beacon_char_user_desc_cb(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_DSC)
    {
        const char *desc = "Leuchtfeuer";
        return os_mbuf_append(ctxt->om, desc, strlen(desc)) == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }
    return BLE_ATT_ERR_READ_NOT_PERMITTED;
}

int bool_char_presentation_cb(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_DSC)
    {
        // 7-Byte Format: [format, exponent, unit(2), namespace, description(2)]
        uint8_t fmt[7] = {
            0x01,       // format = boolean
            0x00,       // exponent
            0x00, 0x00, // unit = none
            0x01,       // namespace = Bluetooth SIG
            0x00, 0x00  // description
        };
        return os_mbuf_append(ctxt->om, fmt, sizeof(fmt)) == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }
    return BLE_ATT_ERR_READ_NOT_PERMITTED;
}

int bool_char_valid_range_cb(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_DSC)
    {
        // for bool optional. but here as 1-Byte-Min/Max (0..1)
        uint8_t range[2] = {0x00, 0x01}; // min=0, max=1
        return os_mbuf_append(ctxt->om, range, sizeof(range)) == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }
    return BLE_ATT_ERR_READ_NOT_PERMITTED;
}
