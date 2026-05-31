#pragma once

/**
 * @brief Initializes the touch/button input on GPIO_NUM_2.
 *
 * Configures the pin as a pull-up input with edge-triggered interrupts.
 * A software debounce timer (250 ms) filters out noise; a confirmed falling
 * edge calls beacon_toggle() to start or stop the rotating beacon light.
 *
 * Internally spawns a FreeRTOS task and a one-shot timer; both run for the
 * lifetime of the application.
 */
void init_touch(void);
