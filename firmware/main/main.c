#include "beacon.h"
#include "light.h"
#include "matter.h"
#include "persistence.h"
#include "touch.h"

void app_main(void)
{
    /// init persistence
    persistence_init("lighthouse");

    init_touch();

    /// init WLED
    if (wled_init() != ESP_OK)
    {
        printf("Failed to initialize WLED task");
        return;
    }

    /// start beacon service
    if (beacon_init() != ESP_OK)
    {
        printf("Failed to initialize beacon task");
        return;
    }

    /// start outdoor light service
    if (outdoor_start() != ESP_OK)
    {
        printf("Failed to start outdoor task");
        return;
    }

    beacon_start();
    /*
    if (matter_init() != ESP_OK)
    {
        printf("Failed to initialize matter task");
        return;
    }
    */
}
