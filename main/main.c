#include "beacon.h"
#include "persistence.h"
#include "remote_control.h"

void app_main(void)
{
    persistence_init("lighthouse");

    if (beacon_init() != ESP_OK)
    {
        printf("Failed to initialize beacon");
        return;
    }
    if (wled_init() != ESP_OK)
    {
        printf("Failed to initialize WLED");
        return;
    }
    if (beacon_start() != ESP_OK)
    {
        printf("Failed to start beacon");
        return;
    }

    remote_control_init();
}
