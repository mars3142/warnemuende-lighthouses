#include "include/device_service.h"
#include "esp_app_desc.h"
#include <stdio.h>
#include <string.h>

int gatt_svr_chr_device_name_access(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt,
                                    void *arg)
{
    char firmware_revision_str[33];
    const esp_app_desc_t *app_desc = esp_app_get_description();

    if (app_desc->project_name[0] != '\0')
    {
        snprintf(firmware_revision_str, sizeof(firmware_revision_str), "%s", app_desc->project_name);
    }
    else
    {
        snprintf(firmware_revision_str, sizeof(firmware_revision_str), "undefined");
    }

    os_mbuf_append(ctxt->om, firmware_revision_str, strlen(firmware_revision_str));
    return 0;
}

int gatt_svr_chr_device_firmware_access(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt,
                                        void *arg)
{
    char firmware_revision_str[33];
    const esp_app_desc_t *app_desc = esp_app_get_description();

    if (app_desc->version[0] != '\0')
    {
        snprintf(firmware_revision_str, sizeof(firmware_revision_str), "%s", app_desc->version);
    }
    else
    {
        snprintf(firmware_revision_str, sizeof(firmware_revision_str), "undefined");
    }

    os_mbuf_append(ctxt->om, firmware_revision_str, strlen(firmware_revision_str));
    return 0;
}

int gatt_svr_chr_device_hardware_access(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt,
                                        void *arg)
{
    char *hardware_revision = "rev1";
    os_mbuf_append(ctxt->om, hardware_revision, strlen(hardware_revision));
    return 0;
}

int gatt_svr_chr_device_manufacturer_access(uint16_t conn_handle, uint16_t attr_handle,
                                            struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    char *manufacturer = "mars3142";
    os_mbuf_append(ctxt->om, manufacturer, strlen(manufacturer));
    return 0;
}
