#include "outdoor.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "outdoor";

#define LEDC_RESOLUTION LEDC_TIMER_10_BIT // Timer resolution (10 bit = 1024 steps)
#define MAX_DUTY 1023

#define NORMAL_DUTY (MAX_DUTY * 0.9) // 90% brightness

#define FLICKER_CHANCE 2 // 2% chance of flickering per cycle
#define FLICKER_COUNT 8  // Number of brightness changes during a flicker

TaskHandle_t outdoor_task_handle = NULL;

void outdoor_task(void *pvParameters)
{

    int led_pin = *(int *)pvParameters;

    ledc_timer_config_t ledc_timer = {.speed_mode = LEDC_LOW_SPEED_MODE,
                                      .timer_num = LEDC_TIMER_0,
                                      .duty_resolution = LEDC_RESOLUTION,
                                      .freq_hz = 5000,
                                      .clk_cfg = LEDC_AUTO_CLK};
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    ledc_channel_config_t ledc_channel = {.speed_mode = LEDC_LOW_SPEED_MODE,
                                          .channel = LEDC_CHANNEL_0,
                                          .timer_sel = LEDC_TIMER_0,
                                          .intr_type = LEDC_INTR_DISABLE,
                                          .gpio_num = led_pin,
                                          .duty = 0,
                                          .hpoint = 0};
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

    while (1)
    {
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, NORMAL_DUTY);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);

        uint32_t random_val = esp_random() % 100;

        if (random_val < FLICKER_CHANCE)
        {
            for (int i = 0; i < FLICKER_COUNT; i++)
            {
                uint32_t flicker_duty = (NORMAL_DUTY * 0.3) + (esp_random() % (uint32_t)(NORMAL_DUTY * 0.4));
                ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, flicker_duty);
                ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);

                vTaskDelay(pdMS_TO_TICKS(20 + (esp_random() % 50)));
            }
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

esp_err_t outdoor_start(void)
{
    ESP_LOGI(TAG, "Simulation of a defective light bulb started.");

    static const int led_left_pin = CONFIG_LED_PIN_LEFT;
    xTaskCreate(outdoor_task, "outdoor_task_left", 2048, (void *)&led_left_pin, 5, &outdoor_task_handle);

    static const int led_right_pin = CONFIG_LED_PIN_RIGHT;
    xTaskCreate(outdoor_task, "outdoor_task_right", 2048, (void *)&led_right_pin, 5, &outdoor_task_handle);

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
