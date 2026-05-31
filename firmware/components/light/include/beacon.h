#pragma once

#include "esp_err.h"
#include <stdbool.h>

typedef enum
{
    BEACON_STATUS_SEARCHING,  // blue, 500ms blink
    BEACON_STATUS_PAIRING,    // blue, steady
    BEACON_STATUS_CONNECTED,  // off
    BEACON_STATUS_ERROR,      // red, steady
} beacon_status_t;

/**
 * @brief Sets the commissioning status LED.
 *
 * Controls the status LED to reflect the current Thread commissioning state.
 * Any active blink timer is stopped before applying the new state.
 *
 * @param status  Desired status:
 *                - BEACON_STATUS_SEARCHING: blue, 500 ms blink (joiner scanning)
 *                - BEACON_STATUS_PAIRING:   blue, steady (joiner handshake in progress)
 *                - BEACON_STATUS_CONNECTED: off (joined successfully)
 *                - BEACON_STATUS_ERROR:     red, steady (max join attempts exceeded)
 */
void beacon_set_status(beacon_status_t status);

/**
 * @brief Initializes the beacon module.
 *
 * This function sets up the beacon module, configuring it for subsequent operations
 * such as starting or stopping the broadcast functionality.
 *
 * @return
 *     - ESP_OK: Initialization completed successfully.
 *     - Error codes in case of failure, indicating the specific issue.
 */
esp_err_t beacon_init(void);

/**
 * @brief Starts the beacon functionality for broadcasting signals.
 *
 * This function initiates the process required for starting the beacon.
 * It ensures the beacon is prepared to transmit signals effectively.
 *
 * @return
 *     - ESP_OK: Beacon started successfully.
 *     - Error codes in case of failure, indicating the specific issue.
 */
esp_err_t beacon_start(void);

/**
 * @brief Stops the beacon broadcasting functionality.
 *
 * This function terminates the ongoing broadcasting process of the beacon
 * and releases any resources allocated during the operation.
 *
 * @return
 *     - ESP_OK: Broadcasting stopped successfully.
 *     - Error codes in case of failure, indicating the specific issue.
 */
esp_err_t beacon_stop(void);

/**
 * @brief Toggles the beacon and persists the new state.
 *
 * Reads the current enabled flag from NVS, inverts it, starts or stops the
 * beacon accordingly, and writes the updated flag back to NVS.
 *
 * @return
 *     - ESP_OK: Toggle completed successfully.
 *     - Error codes in case of failure, indicating the specific issue.
 */
esp_err_t beacon_toggle(void);

/**
 * @brief Returns whether the beacon is currently running.
 *
 * @return true if the beacon timer is active, false otherwise.
 */
bool beacon_is_running(void);

typedef void (*beacon_state_cb_t)(bool enabled);
void beacon_register_state_cb(beacon_state_cb_t cb);
