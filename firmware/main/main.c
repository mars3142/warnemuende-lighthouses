#include "light.h"
#include "persistence.h"
#include "remote_control.h"
#include "touch.h"

void init_touch_gpio(void);

void app_main(void)
{
    /// init persistence
    persistence_init("lighthouse");

    init_touch();

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
    /// start beacon service
    uint8_t beacon_enabled = 1;
    persistence_load(VALUE_TYPE_INT32, "BEACON_ENABLED", &beacon_enabled);
    if (beacon_enabled && beacon_start() != ESP_OK)
    {
        printf("Failed to start beacon");
        return;
    }

    /// start outdoor light service
    if (outdoor_start() != ESP_OK)
    {
        printf("Failed to start outdoor");
        return;
    }

    /// activate BLE functions
    remote_control_init();
}
