#include "outdoor.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "outdoor";

// Timer resolution (10 bit = 1024 steps)
#define LEDC_RESOLUTION LEDC_TIMER_10_BIT
#define MAX_DUTY 1023

// Constant brightness for the "normal state" (approx. 90%)
#define NORMAL_DUTY (MAX_DUTY * 0.9)

// Parameters for flickering
#define FLICKER_CHANCE 5 // 5% chance of flickering per cycle
#define FLICKER_COUNT 8  // Number of brightness changes during a flicker

TaskHandle_t outdoor_task_handle = NULL;

void outdoor_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Simulation of a defective light bulb started.");

    int led_pin = *(int *)pvParameters;

    // 1. LEDC timer configuration
    ledc_timer_config_t ledc_timer = {.speed_mode = LEDC_LOW_SPEED_MODE,
                                      .timer_num = LEDC_TIMER_0,
                                      .duty_resolution = LEDC_RESOLUTION,
                                      .freq_hz = 5000,
                                      .clk_cfg = LEDC_AUTO_CLK};
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    // 2. LEDC channel configuration
    ledc_channel_config_t ledc_channel = {.speed_mode = LEDC_LOW_SPEED_MODE,
                                          .channel = LEDC_CHANNEL_0,
                                          .timer_sel = LEDC_TIMER_0,
                                          .intr_type = LEDC_INTR_DISABLE,
                                          .gpio_num = led_pin,
                                          .duty = 0,
                                          .hpoint = 0};
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

    // 3. Main loop with flicker logic
    while (1)
    {
        // First, set the LED to its normal brightness
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, NORMAL_DUTY);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);

        // Random trigger for flickering
        uint32_t random_val = esp_random() % 100; // Random number between 0 and 99

        if (random_val < FLICKER_CHANCE)
        {
            // Start flicker sequence
            for (int i = 0; i < FLICKER_COUNT; i++)
            {
                // Set a random, lower brightness (e.g., 30-70% of normal brightness)
                uint32_t flicker_duty = (NORMAL_DUTY * 0.3) + (esp_random() % (uint32_t)(NORMAL_DUTY * 0.4));
                ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, flicker_duty);
                ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);

                // Short, random delay for irregular flickering
                vTaskDelay(pdMS_TO_TICKS(20 + (esp_random() % 50)));
            }
        }

        // A fixed delay in normal operation
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

esp_err_t outdoor_start(void)
{
    static const int led_pin = 13;
    xTaskCreate(outdoor_task, "outdoor_task", 2048, (void *)&led_pin, 5, &outdoor_task_handle);

    return ESP_OK;
}

esp_err_t outdoor_stop(void)
{
    if (outdoor_task_handle == NULL)
    {
        return ESP_FAIL;
    }

    vTaskDelete(outdoor_task_handle);
    outdoor_task_handle = NULL;

    return ESP_OK;
}