#include "beacon.h"

void app_main(void)
{
    if (initBeacon() != ESP_OK)
    {
        printf("Failed to initialize beacon");
        return;
    }
    if (initWled() != ESP_OK)
    {
        printf("Failed to initialize WLED");
        return;
    }
    if (startBeacon() != ESP_OK)
    {
        printf("Failed to start beacon");
        return;
    }
}
