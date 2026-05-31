#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

void      outdoor_init(void);
esp_err_t outdoor_start(void);
esp_err_t outdoor_stop(void);
bool      outdoor_is_running(void);

uint8_t   outdoor_get_flicker(void);
esp_err_t outdoor_set_flicker(uint8_t percent);
