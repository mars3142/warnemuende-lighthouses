#include "include/remote_control.h"

#include <stdio.h>
#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
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

void ble_store_config_init(void);

static const char *TAG = "remote_control";

static const ble_uuid16_t gatt_svr_svc_device_uuid = BLE_UUID16_INIT(0x180A);
static const ble_uuid16_t gatt_svr_svc_light_uuid = BLE_UUID16_INIT(0xA000);
static const ble_uuid16_t gatt_svr_svc_settings_uuid = BLE_UUID16_INIT(0xA999);

uint8_t ble_addr_type;

ble_connection_t g_connections[CONFIG_BT_NIMBLE_MAX_CONNECTIONS];

static void init_connection_pool()
{
    for (int i = 0; i < CONFIG_BT_NIMBLE_MAX_CONNECTIONS; i++)
    {
        g_connections[i].conn_handle = BLE_HS_CONN_HANDLE_NONE;
        g_connections[i].is_connected = false;
    }
}

bool is_any_device_connected(void)
{
    for (int i = 0; i < CONFIG_BT_NIMBLE_MAX_CONNECTIONS; i++)
    {
        if (g_connections[i].is_connected)
        {
            return true;
        }
    }
    return false;
}

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
                    .flags =
                        BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_READ_ENC | BLE_GATT_CHR_F_WRITE_ENC,
                    .access_cb = gatt_svr_chr_light_beacon_access,
                    .descriptors = beacon_char_desc,
                },
                {
                    // LED Characteristic
                    .uuid = BLE_UUID16_DECLARE(0xF037),
                    .flags =
                        BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_READ_ENC | BLE_GATT_CHR_F_WRITE_ENC,
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

inline static void format_addr(char *addr_str, uint8_t addr[])
{
    sprintf(addr_str, "%02X:%02X:%02X:%02X:%02X:%02X", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
}

static void print_conn_desc(struct ble_gap_conn_desc *desc)
{
    /* Local variables */
    char addr_str[18] = {0};

    /* Connection handle */
    ESP_LOGI(TAG, "connection handle: %d", desc->conn_handle);

    /* Local ID address */
    format_addr(addr_str, desc->our_id_addr.val);
    ESP_LOGI(TAG, "device id address: type=%d, value=%s", desc->our_id_addr.type, addr_str);

    /* Peer ID address */
    format_addr(addr_str, desc->peer_id_addr.val);
    ESP_LOGI(TAG, "peer id address: type=%d, value=%s", desc->peer_id_addr.type, addr_str);

    /* Connection info */
    ESP_LOGI(TAG,
             "conn_itvl=%d, conn_latency=%d, supervision_timeout=%d, "
             "encrypted=%d, authenticated=%d, bonded=%d\n",
             desc->conn_itvl, desc->conn_latency, desc->supervision_timeout, desc->sec_state.encrypted,
             desc->sec_state.authenticated, desc->sec_state.bonded);
}

// BLE event handling
static int ble_gap_event(struct ble_gap_event *event, void *arg)
{
    esp_err_t rc;
    struct ble_gap_conn_desc desc;

    switch (event->type)
    {
    case BLE_GAP_EVENT_CONNECT:
        /* A new connection was established or a connection attempt failed. */
        ESP_LOGI(TAG, "connection %s; status=%d", event->connect.status == 0 ? "established" : "failed",
                 event->connect.status);

        /* Connection succeeded */
        if (event->connect.status == 0)
        {
            bool found_slot = false;
            for (int i = 0; i < CONFIG_BT_NIMBLE_MAX_CONNECTIONS; i++)
            {
                if (!g_connections[i].is_connected)
                {
                    g_connections[i].conn_handle = event->connect.conn_handle;
                    g_connections[i].is_connected = true;
                    found_slot = true;
                    ESP_LOGI(TAG, "Connection stored in slot %d", i);
                    break;
                }
            }
            if (!found_slot)
            {
                ESP_LOGW(TAG, "No free connection slot available!");
            }

            /* Check connection handle */
            rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
            if (rc != 0)
            {
            }

            print_conn_desc(&desc);

            /* Try to update connection parameters */
            struct ble_gap_upd_params params = {.itvl_min = desc.conn_itvl,
                                                .itvl_max = desc.conn_itvl,
                                                .latency = 3,
                                                .supervision_timeout = desc.supervision_timeout};
            rc = ble_gap_update_params(event->connect.conn_handle, &params);
            if (rc != 0)
            {
                ESP_LOGE(TAG, "failed to update connection parameters, error code: %d", rc);
                return rc;
            }
        }
        /* Connection failed, restart advertising */
        else
        {
            ble_app_advertise();
        }
        return rc;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "Disconnected; reason=%d", event->disconnect.reason);
        for (int i = 0; i < CONFIG_BT_NIMBLE_MAX_CONNECTIONS; i++)
        {
            if (g_connections[i].conn_handle == event->disconnect.conn.conn_handle)
            {
                g_connections[i].is_connected = false;
                g_connections[i].conn_handle = BLE_HS_CONN_HANDLE_NONE;
                ESP_LOGI(TAG, "Connection from slot %d removed", i);
                ble_app_advertise(); // Restart advertising to allow new connections
                break;
            }
        }
        break;

    case BLE_GAP_EVENT_PASSKEY_ACTION:
        ESP_LOGI(TAG, "Passkey action required: %d", event->passkey.params.action);
        struct ble_sm_io pkey = {0};

        switch (event->passkey.params.action)
        {
        case BLE_SM_IOACT_DISP:
            pkey.action = BLE_SM_IOACT_DISP;
            pkey.passkey = CONFIG_BONDING_PASSPHRASE;
            ESP_LOGI(TAG, "Displaying passkey: %06d", pkey.passkey);
            rc = ble_sm_inject_io(event->passkey.conn_handle, &pkey);
            if (rc != 0)
            {
                ESP_LOGE(TAG, "failed to inject security manager io, error code: %d", rc);
                return rc;
            }
            break;

        default:
            ESP_LOGE(TAG, "Unknown passkey action: %d", event->passkey.params.action);
            return 0;
        }
        break;

    case BLE_GAP_EVENT_ENC_CHANGE:
        ESP_LOGI(TAG, "Encryption change event; status=%d", event->enc_change.status);

        if (event->enc_change.status != 0)
        {
            ESP_LOGW(TAG, "Encryption failed with status %d", event->enc_change.status);

            rc = ble_gap_conn_find(event->enc_change.conn_handle, &desc);
            if (rc == 0)
            {
                char addr_str[18] = {0};
                format_addr(addr_str, desc.peer_id_addr.val);
                ESP_LOGI(TAG, "Deleting bond for peer: %s", addr_str);

                ble_store_util_delete_peer(&desc.peer_id_addr);

                ble_gap_terminate(event->enc_change.conn_handle, BLE_ERR_REM_USER_CONN_TERM);
            }
        }
        else
        {
            ESP_LOGI(TAG, "Encryption successfully established");
        }
        break;

    case BLE_GAP_EVENT_REPEAT_PAIRING:
        ESP_LOGI(TAG, "Repeat pairing requested");

        rc = ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc);
        if (rc == 0)
        {
            char addr_str[18] = {0};
            format_addr(addr_str, desc.peer_id_addr.val);
            ESP_LOGI(TAG, "Deleting old bond for specific peer: %s", addr_str);

            ble_store_util_delete_peer(&desc.peer_id_addr);
        }

        return BLE_GAP_REPEAT_PAIRING_RETRY;

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

static void on_stack_reset(int reason)
{
    /* On reset, print reset reason to console */
    ESP_LOGI(TAG, "nimble stack reset, reset reason: %d", reason);
}

static void on_stack_sync(void)
{
    esp_err_t ret;
    uint8_t ble_addr[6] = {0};

    /* Figure out address to use while advertising (no privacy for now) */
    ret = ble_hs_id_infer_auto(0, &ble_addr_type);
    if (ret != 0)
    {
        ESP_LOGE(TAG, "error determining address type; rc=%d", ret);
        return;
    }

    ret = ble_hs_id_copy_addr(ble_addr_type, ble_addr, NULL);
    if (ret != 0)
    {
        ESP_LOGE(TAG, "Failed to get BLE MAC address (err: %d)", ret);
    }

    char formatted_name[32];
    snprintf(formatted_name, sizeof(formatted_name), "Lighthouse %02X%02X", ble_addr[4], ble_addr[5]);
    ble_svc_gap_device_name_set(formatted_name);

    // Start Advertising
    ble_app_advertise();
}

static esp_err_t gatt_svc_init(void)
{
    esp_err_t ret;
    ble_svc_gatt_init();

    ret = ble_gatts_count_cfg(gatt_svcs);
    if (ret != ESP_OK)
    {
        return ret;
    }

    ret = ble_gatts_add_svcs(gatt_svcs);
    if (ret != ESP_OK)
    {
        return ret;
    }

    return ESP_OK;
}

static esp_err_t gap_init(void)
{
    ble_svc_gap_init();

    return ESP_OK;
}

// The infinite task
static void host_task(void *param)
{
    nimble_port_run(); // This function will return only when nimble_port_stop() is executed
}

static void nimble_host_config_init(void)
{
    // callbacks
    ble_hs_cfg.reset_cb = on_stack_reset;
    ble_hs_cfg.sync_cb = on_stack_sync;

    ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_DISP_ONLY;
    ble_hs_cfg.sm_bonding = 1;
    ble_hs_cfg.sm_mitm = 1;
    ble_hs_cfg.sm_our_key_dist |= BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist |= BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;

    // Initialize BLE store configuration
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    ble_store_config_init();
}

void remote_control_init(void)
{
    esp_err_t ret;

    ret = nimble_port_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize nimble stack (err: %s)", esp_err_to_name(ret));
        return;
    }

    init_connection_pool();

    ret = gap_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize GAP service (err: %s)", esp_err_to_name(ret));
        return;
    }

    ret = gatt_svc_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize GATT server (err: %s)", esp_err_to_name(ret));
        return;
    }

    nimble_host_config_init();

    nimble_port_freertos_init(host_task); // Start BLE host task

    xTaskCreate(uart_tx_task, "uart_tx", 2048, NULL, 1, NULL);
}
