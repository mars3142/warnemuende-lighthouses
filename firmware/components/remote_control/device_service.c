#include "include/device_service.h"
#include "esp_app_desc.h"
#include <stdio.h>
#include <string.h>

int device_name_read(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
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

int device_firmware_revision_read(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt,
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

int device_hardware_revision_read(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt,
                                  void *arg)
{
    char *hardware_revision = "rev1";
    os_mbuf_append(ctxt->om, hardware_revision, strlen(hardware_revision));
    return 0;
}

int device_manufacturer_read(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    char *manufacturer = "mars3142";
    os_mbuf_append(ctxt->om, manufacturer, strlen(manufacturer));
    return 0;
}
