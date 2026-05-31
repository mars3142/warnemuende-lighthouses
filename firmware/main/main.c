#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_vfs_eventfd.h"
#include "esp_mac.h"

#include "beacon.h"
#include "light.h"
#include "thread_control.h"
#include "persistence.h"
#include "touch.h"

void app_main(void)
{
    uint8_t base_mac[6];
    esp_read_mac(base_mac, ESP_MAC_EFUSE_FACTORY);
    ESP_LOGI("H2", "Factory MAC: %02X:%02X:%02X:%02X:%02X:%02X", 
             base_mac[0], base_mac[1], base_mac[2], base_mac[3], base_mac[4], base_mac[5]);

    ESP_LOGI("H2", "Reset Reason: %d", esp_reset_reason());

    /// init persistence (calls nvs_flash_init internally)
    persistence_init("lighthouse");
    outdoor_init();

    // Required by OpenThread: event loop, netif subsystem, and eventfd VFS
    // (OpenThread task queue uses eventfd internally)
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_netif_init());
    esp_vfs_eventfd_config_t eventfd_config = {
        .max_fds = 5,  // Increased for OT task queue, radio driver and potentially others
    };
    ESP_ERROR_CHECK(esp_vfs_eventfd_register(&eventfd_config));

    // init_touch(); // Temporarily disabled for diagnostics

    if (wled_init() != ESP_OK)
    {
        printf("Failed to initialize WLED");
        return;
    }

    if (beacon_init() != ESP_OK)
    {
        printf("Failed to initialize beacon task");
        return;
    }

    if (thread_init() != ESP_OK)
    {
        printf("Failed to initialize thread task");
        return;
    }
}
