#pragma once

#include "led_strip.h"

/**
 * @brief Handle and pixel count for the WS2812 strip.
 *
 * Shared between the light, beacon, and outdoor modules so all of them
 * write through the same RMT handle.
 */
typedef struct
{
    led_strip_handle_t led_strip;
    uint32_t size;
} LedMatrix_t;

/**
 * @brief Returns the global LED matrix descriptor.
 *
 * The descriptor is populated by wled_init() and is valid for the lifetime
 * of the application.
 */
LedMatrix_t get_led_matrix(void);
