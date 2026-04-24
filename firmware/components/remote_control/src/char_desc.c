#include "char_desc.h"

int gatt_svr_desc_presentation_bool_access(uint16_t conn_handle, uint16_t attr_handle,
                                           struct ble_gatt_access_ctxt *ctxt, void *arg)
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

int gatt_svr_desc_valid_range_bool_access(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt,
                                          void *arg)
{
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_DSC)
    {
        // for bool optional. but here as 1-Byte-Min/Max (0..1)
        uint8_t range[2] = {0x00, 0x01}; // min=0, max=1
        return os_mbuf_append(ctxt->om, range, sizeof(range)) == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }
    return BLE_ATT_ERR_READ_NOT_PERMITTED;
}
