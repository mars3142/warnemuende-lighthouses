#pragma once

#include <esp_err.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Starts the OpenThread stack and begins network commissioning.
     *
     * Spawns the `thread_task` FreeRTOS task which:
     *  - initialises the OpenThread platform (radio, host, port)
     *  - configures the device as a Full Thread Device (FTD)
     *  - starts the Joiner if no dataset is commissioned yet, or enables the
     *    Thread interface directly when a dataset is already stored
     *
     * The Joiner retries commissioning up to `JOINER_MAX_RETRIES` times.
     * After all attempts fail the status LED turns solid red and the joiner
     * stops permanently until the next reboot.
     *
     * @return
     *     - ESP_OK: Task created successfully.
     *     - ESP_ERR_NO_MEM: FreeRTOS task creation failed.
     */
    esp_err_t thread_init(void);

#ifdef __cplusplus
}
#endif
