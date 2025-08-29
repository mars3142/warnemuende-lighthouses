#include "include/remote_control.h"

#include <stdio.h>
#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "host/ble_hs.h"
#include "host/ble_sm.h"
#include "host/ble_uuid.h"
#include "include/char_desc.h"
#include "include/device_service.h"
#include "include/light_service.h"
#include "include/uart_service.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "sdkconfig.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

static const char *TAG = "remote_control";

static const ble_uuid16_t gatt_svr_svc_device_uuid = BLE_UUID16_INIT(0x180A);
static const ble_uuid16_t gatt_svr_svc_light_uuid = BLE_UUID16_INIT(0xA000);
static const ble_uuid16_t gatt_svr_svc_settings_uuid = BLE_UUID16_INIT(0xA999);

uint8_t ble_addr_type;

static void ble_app_advertise(void);

// Descriptors for the Beacon Characteristic
static struct ble_gatt_dsc_def beacon_char_desc[] = {
    {
        // User Description Descriptor
        .uuid = BLE_UUID16_DECLARE(0x2901),
        .att_flags = BLE_ATT_F_READ,
        .access_cb = gatt_svr_desc_beacon_user_desc_access,
    },
    {
        // Presentation Format Descriptor
        .uuid = BLE_UUID16_DECLARE(0x2904),
        .att_flags = BLE_ATT_F_READ,
        .access_cb = gatt_svr_desc_presentation_bool_access,
    },
    {
        // Valid Range Descriptor
        .uuid = BLE_UUID16_DECLARE(0x2906),
        .att_flags = BLE_ATT_F_READ,
        .access_cb = gatt_svr_desc_valid_range_bool_access,
    },
    {0},
};

// Descriptors for the LED Characteristic
static struct ble_gatt_dsc_def led_char_desc[] = {
    {
        // User Description Descriptor
        .uuid = BLE_UUID16_DECLARE(0x2901),
        .att_flags = BLE_ATT_F_READ,
        .access_cb = gatt_svr_desc_led_user_desc_access,
    },
    {
        // Presentation Format Descriptor
        .uuid = BLE_UUID16_DECLARE(0x2904),
        .att_flags = BLE_ATT_F_READ,
        .access_cb = gatt_svr_desc_presentation_bool_access,
    },
    {
        // Valid Range Descriptor
        .uuid = BLE_UUID16_DECLARE(0x2906),
        .att_flags = BLE_ATT_F_READ,
        .access_cb = gatt_svr_desc_valid_range_bool_access,
    },
    {0},
};

// Array of pointers to service definitions
static const struct ble_gatt_svc_def gatt_svcs[] = {
    {
        // Device Information Service
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &gatt_svr_svc_device_uuid.u,
        .characteristics =
            (struct ble_gatt_chr_def[]){
                {
                    // Manufacturer String
                    .uuid = BLE_UUID16_DECLARE(0x2A29),
                    .flags = BLE_GATT_CHR_F_READ,
                    .access_cb = gatt_svr_chr_device_manufacturer_access,
                },
                {
                    // Hardware Revision String
                    .uuid = BLE_UUID16_DECLARE(0x2A27),
                    .flags = BLE_GATT_CHR_F_READ,
                    .access_cb = gatt_svr_chr_device_hardware_access,
                },
                {
                    // Firmware Revision String
                    .uuid = BLE_UUID16_DECLARE(0x2A26),
                    .flags = BLE_GATT_CHR_F_READ,
                    .access_cb = gatt_svr_chr_device_firmware_access,
                },
                {
                    // Device Name
                    .uuid = BLE_UUID16_DECLARE(0x2A00),
                    .flags = BLE_GATT_CHR_F_READ,
                    .access_cb = gatt_svr_chr_device_name_access,
                },
                {0},
            },
    },
    {
        // Light Service
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &gatt_svr_svc_light_uuid.u,
        .characteristics =
            (struct ble_gatt_chr_def[]){
                {
                    // Beacon Characteristic
                    .uuid = BLE_UUID16_DECLARE(0xBEA0),
                    .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
                    .access_cb = gatt_svr_chr_light_beacon_access,
                    .descriptors = beacon_char_desc,
                },
                {
                    // LED Characteristic
                    .uuid = BLE_UUID16_DECLARE(0xF037),
                    .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
                    .access_cb = gatt_svr_chr_light_led_access,
                    .descriptors = led_char_desc,
                },
                {0},
            },
    },
    {
        // UART Service
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &gatt_svr_svc_uart_uuid.u,
        .characteristics =
            (struct ble_gatt_chr_def[]){
                {
                    // Characteristic: RX (Receiving of data)
                    .uuid = &gatt_svr_chr_uart_rx_uuid.u,
                    .access_cb = gatt_svr_chr_uart_access,
                    .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
                },
                {
                    // Characteristic: TX (Sending of data)
                    .uuid = &gatt_svr_chr_uart_tx_uuid.u,
                    .access_cb = gatt_svr_chr_uart_access,
                    .val_handle = &tx_chr_val_handle,
                    .flags = BLE_GATT_CHR_F_NOTIFY,
                },
                {0},
            },
    },
    // Settings Service (empty for now)
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &gatt_svr_svc_settings_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]){{0}},
    },
    {0}};

// BLE event handling
static int ble_gap_event(struct ble_gap_event *event, void *arg)
{
    switch (event->type)
    {
    case BLE_GAP_EVENT_CONNECT:
        ESP_LOGI(TAG, "Connection established; status=%d", event->connect.status);
        conn_handle = event->connect.conn_handle;
        ESP_LOGI(TAG, "Connection handle: %d", conn_handle);
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "Disconnected; reason=%d", event->disconnect.reason);
        conn_handle = 0;
        ble_app_advertise();
        break;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        ESP_LOGI(TAG, "Advertising complete");
        ble_app_advertise();
        break;

    default:
        break;
    }
    return 0;
}

// Define the BLE connection
static void ble_app_advertise(void)
{
    int ret;

    // GAP - advertising definition
    struct ble_hs_adv_fields fields;
    memset(&fields, 0, sizeof(fields));
    uint8_t mfg_data[] = {0xDE, 0xC0, 0x05, 0x10, 0x20, 0x25};
    static const ble_uuid16_t services[] = {gatt_svr_svc_device_uuid, gatt_svr_svc_light_uuid,
                                            gatt_svr_svc_settings_uuid};

    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.uuids16 = services;
    fields.num_uuids16 = sizeof(services) / sizeof(services[0]);
    fields.uuids16_is_complete = 1;
    fields.mfg_data = mfg_data;
    fields.mfg_data_len = sizeof(mfg_data);

    ret = ble_gap_adv_set_fields(&fields);
    if (ret != 0)
    {
        ESP_LOGE(TAG, "Failed to set advertising data (err: %d)", ret);
        return;
    }

    // GAP - device connectivity definition
    struct ble_gap_adv_params adv_params;
    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND; // connectable or non-connectable
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN; // discoverable or non-discoverable
    ret = ble_gap_adv_start(ble_addr_type, NULL, BLE_HS_FOREVER, &adv_params, ble_gap_event, NULL);
    if (ret != 0)
    {
        ESP_LOGE(TAG, "Advertising failed to start (err %d)", ret);
        return;
    }

    // --- Configure Scan Response Data (SCAN_RSP) ---
    struct ble_hs_adv_fields scan_rsp_fields;
    memset(&scan_rsp_fields, 0, sizeof(scan_rsp_fields));

    // Get the device name
    const char *device_name;
    device_name = ble_svc_gap_device_name();
    scan_rsp_fields.name = (uint8_t *)device_name;
    scan_rsp_fields.name_len = strlen(device_name);
    scan_rsp_fields.name_is_complete = 1;

    // Optionally, add TX power level to scan response
    scan_rsp_fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;
    scan_rsp_fields.tx_pwr_lvl_is_present = 1;

    ret = ble_gap_adv_rsp_set_fields(&scan_rsp_fields);
    if (ret != 0)
    {
        ESP_LOGE(TAG, "Error setting scan response data; rc=%d", ret);
        return;
    }
}

// The application
static void ble_app_on_sync(void)
{
    uint8_t ble_addr[6] = {0};
    int ret = ble_hs_id_copy_addr(BLE_ADDR_PUBLIC, ble_addr, NULL);
    if (ret != 0)
    {
        ESP_LOGE(TAG, "Failed to get BLE MAC address (err: %d)", ret);
        return;
    }

    char formatted_name[32];
    snprintf(formatted_name, sizeof(formatted_name), "Lighthouse %02X%02X", ble_addr[4], ble_addr[5]);
    ble_svc_gap_device_name_set(formatted_name);

    // Start Advertising
    ble_hs_id_infer_auto(0, &ble_addr_type); // Determines the best address type automatically
    ble_app_advertise();
}

// The infinite task
static void host_task(void *param)
{
    nimble_port_run(); // This function will return only when nimble_port_stop() is executed
}

void remote_control_init(void)
{
    nimble_port_init();
    ble_svc_gap_init();
    ble_svc_gatt_init();
    ble_gatts_count_cfg(gatt_svcs);
    ble_gatts_add_svcs(gatt_svcs);

    // Callback for synchronization
    ble_hs_cfg.sync_cb = ble_app_on_sync;

    nimble_port_freertos_init(host_task); // Start BLE host task

    xTaskCreate(uart_tx_task, "uart_tx", 2048, NULL, 1, NULL);
}
