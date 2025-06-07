#pragma once

#include "esp_err.h"

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
