#pragma once

#include "esp_err.h"

esp_err_t beacon_init(void);
esp_err_t beacon_start(void);
esp_err_t beacon_stop(void);
esp_err_t wled_init(void);
