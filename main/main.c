#include "light.h"
#include "persistence.h"
#include "remote_control.h"

void app_main(void)
{
    /// init persistence
    persistence_init("beacon");

    /// init WLED
    if (wled_init() != ESP_OK)
    {
        printf("Failed to initialize WLED");
        return;
    }

    /// start beacon service
    if (beacon_init() != ESP_OK)
    {
        printf("Failed to initialize beacon");
        return;
    }
    if (beacon_start() != ESP_OK)
    {
        printf("Failed to start beacon");
        return;
    }

    /// start outdoor light service
    if (outdoor_init() != ESP_OK)
    {
        printf("Failed to initialize outdoor");
        return;
    }
    if (outdoor_start() != ESP_OK)
    {
        printf("Failed to start outdoor");
        return;
    }

    /// activate BLE functions
    remote_control_init();
}
