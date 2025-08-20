#pragma once

#include "beacon.h"
#include "led_strip.h"
#include "outdoor.h"

#include "esp_err.h"

typedef struct
{
    led_strip_handle_t led_strip;
    uint32_t size;
} LedMatrix_t;

LedMatrix_t get_led_matrix(void);

/**
 * @brief Initializes the WLED module.
 *
 * This function configures the WLED module for operation, preparing it for subsequent
 * usage such as enabling lighting effects or communication.
 *
 * @return
 *     - ESP_OK: Initialization completed successfully.
 *     - Error codes in case of failure, indicating the specific issue.
 */
esp_err_t wled_init(void);
