#include "beacon.h"

void app_main(void)
{
    ESP_ERROR_CHECK(beacon_init());
    ESP_ERROR_CHECK(wled_init());
    ESP_ERROR_CHECK(beacon_start());
}
