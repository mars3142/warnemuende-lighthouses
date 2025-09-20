#include "include/light_service.h"
#include "beacon.h"
#include "persistence.h"

static uint8_t g_beacon_enabled = 0;
static int8_t g_led_value = 0;

/// Characteristic Callbacks
int gatt_svr_chr_light_led_access(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt,
                                  void *arg)
{
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR)
    {
        char *data = "To be implemented later";
        os_mbuf_append(ctxt->om, data, strlen(data));
        return 0;
    }
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR)
    {
        int8_t led_value = 0;
        persistence_load(VALUE_TYPE_INT8, "LED_VALUE", &led_value);
    }
    return BLE_ATT_ERR_UNLIKELY;
}

int gatt_svr_chr_light_beacon_access(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt,
                                     void *arg)
{

    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR)
    {
        return os_mbuf_append(ctxt->om, &g_beacon_enabled, sizeof(g_beacon_enabled)) == 0
                   ? 0
                   : BLE_ATT_ERR_INSUFFICIENT_RES;
    }
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR)
    {
        int8_t beacon_enabled = 0;
        persistence_load(VALUE_TYPE_INT8, "BEACON_ENABLED", &beacon_enabled);

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

        if (val == g_beacon_enabled) // value is already set
        {
            return 0;
        }

        g_beacon_enabled = val;

        if (g_beacon_enabled)
        {
            beacon_start();
        }
        else
        {
            beacon_stop();
        }
        persistence_save(VALUE_TYPE_INT8, "BEACON_ENABLED", &g_beacon_enabled);
        return 0;
    }
    return BLE_ATT_ERR_UNLIKELY;
}

// Characteristic User Descriptions
int gatt_svr_desc_led_user_desc_access(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt,
                                       void *arg)
{
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_DSC)
    {
        const char *desc = "Aussenbeleuchtung";
        return os_mbuf_append(ctxt->om, desc, strlen(desc)) == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }
    return BLE_ATT_ERR_READ_NOT_PERMITTED;
}

int gatt_svr_desc_beacon_user_desc_access(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt,
                                          void *arg)
{
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_DSC)
    {
        const char *desc = "Leuchtfeuer";
        return os_mbuf_append(ctxt->om, desc, strlen(desc)) == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }
    return BLE_ATT_ERR_READ_NOT_PERMITTED;
}
