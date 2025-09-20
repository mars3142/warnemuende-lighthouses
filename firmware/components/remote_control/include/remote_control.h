#pragma once

#include "host/ble_hs.h"
#include "sdkconfig.h"

typedef struct
{
    uint16_t conn_handle;
    bool is_connected;
} ble_connection_t;

extern ble_connection_t g_connections[CONFIG_BT_NIMBLE_MAX_CONNECTIONS];

void remote_control_init(void);
bool is_any_device_connected(void);
