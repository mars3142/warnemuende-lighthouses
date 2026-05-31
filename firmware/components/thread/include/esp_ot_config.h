#pragma once

#include "esp_openthread_types.h"

// ESP32-H2 has a native IEEE 802.15.4 radio.
#define ESP_OPENTHREAD_DEFAULT_RADIO_CONFIG() \
    {                                         \
        .radio_mode = RADIO_MODE_NATIVE,      \
    }

// No OpenThread CLI — the application uses CoAP directly.
#define ESP_OPENTHREAD_DEFAULT_HOST_CONFIG()                    \
    {                                                           \
        .host_connection_mode = HOST_CONNECTION_MODE_NONE,      \
    }

#define ESP_OPENTHREAD_DEFAULT_PORT_CONFIG() \
    {                                        \
        .storage_partition_name = "nvs",     \
        .netif_queue_size = 32,              \
        .task_queue_size = 32,              \
    }
