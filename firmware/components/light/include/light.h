#pragma once

#include "beacon.h"
#include "outdoor.h"

#include "esp_err.h"

/**
 * @brief Initializes the WS2812 LED strip via RMT.
 *
 * Configures the RMT peripheral for the data pin defined by
 * `CONFIG_WLED_DIN_PIN` and blanks all pixels. Must be called once before
 * any other LED operations.
 *
 * @return
 *     - ESP_OK: Initialization completed successfully.
 *     - Error codes in case of failure, indicating the specific issue.
 */
esp_err_t wled_init(void);
