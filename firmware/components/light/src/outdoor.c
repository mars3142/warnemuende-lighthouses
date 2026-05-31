#include "outdoor.h"
#include "persistence.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "outdoor";

#define LEDC_RESOLUTION LEDC_TIMER_10_BIT
#define MAX_DUTY        1023
#define NORMAL_DUTY     (MAX_DUTY * 0.9)
#define FLICKER_COUNT   8

static uint8_t s_flicker_chance = 2;

typedef struct
{
    int             gpio_num;
    ledc_channel_t  channel;
} outdoor_task_args_t;

static TaskHandle_t s_task_left  = NULL;
static TaskHandle_t s_task_right = NULL;

static void outdoor_task(void *pvParameters)
{
    outdoor_task_args_t *args = (outdoor_task_args_t *)pvParameters;
    int            led_pin = args->gpio_num;
    ledc_channel_t channel = args->channel;

    ledc_timer_config_t ledc_timer = {.speed_mode      = LEDC_LOW_SPEED_MODE,
                                      .timer_num       = LEDC_TIMER_0,
                                      .duty_resolution = LEDC_RESOLUTION,
                                      .freq_hz         = 5000,
                                      .clk_cfg         = LEDC_AUTO_CLK};
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    ledc_channel_config_t ledc_channel = {.speed_mode = LEDC_LOW_SPEED_MODE,
                                          .channel    = channel,
                                          .timer_sel  = LEDC_TIMER_0,
                                          .intr_type  = LEDC_INTR_DISABLE,
                                          .gpio_num   = led_pin,
                                          .duty       = 0,
                                          .hpoint     = 0};
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

    while (1)
    {
        ledc_set_duty(LEDC_LOW_SPEED_MODE, channel, NORMAL_DUTY);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, channel);

        uint32_t random_val = esp_random() % 100;

        if (random_val < s_flicker_chance)
        {
            for (int i = 0; i < FLICKER_COUNT; i++)
            {
                uint32_t flicker_duty = (NORMAL_DUTY * 0.3) + (esp_random() % (uint32_t)(NORMAL_DUTY * 0.4));
                ledc_set_duty(LEDC_LOW_SPEED_MODE, channel, flicker_duty);
                ledc_update_duty(LEDC_LOW_SPEED_MODE, channel);

                vTaskDelay(pdMS_TO_TICKS(20 + (esp_random() % 50)));
            }
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

uint8_t outdoor_get_flicker(void)
{
    return s_flicker_chance;
}

esp_err_t outdoor_set_flicker(uint8_t percent)
{
    if (percent > 100) percent = 100;
    s_flicker_chance = percent;
    persistence_save(VALUE_TYPE_INT8, "FLICKER_CHANCE", &s_flicker_chance);
    return ESP_OK;
}

void outdoor_init(void)
{
    persistence_load(VALUE_TYPE_INT8, "FLICKER_CHANCE", &s_flicker_chance);
}

esp_err_t outdoor_start(void)
{
    if (s_task_left != NULL || s_task_right != NULL)
        return ESP_OK;

    ESP_LOGI(TAG, "Simulation of a defective light bulb started.");

    static outdoor_task_args_t args_left  = {.gpio_num = CONFIG_LED_PIN_LEFT,  .channel = LEDC_CHANNEL_0};
    static outdoor_task_args_t args_right = {.gpio_num = CONFIG_LED_PIN_RIGHT, .channel = LEDC_CHANNEL_1};

    xTaskCreate(outdoor_task, "outdoor_left",  2048, &args_left,  5, &s_task_left);
    xTaskCreate(outdoor_task, "outdoor_right", 2048, &args_right, 5, &s_task_right);

    return ESP_OK;
}

esp_err_t outdoor_stop(void)
{
    if (s_task_left == NULL && s_task_right == NULL)
        return ESP_FAIL;

    if (s_task_left)  { vTaskDelete(s_task_left);  s_task_left  = NULL; }
    if (s_task_right) { vTaskDelete(s_task_right); s_task_right = NULL; }

    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);

    return ESP_OK;
}

bool outdoor_is_running(void)
{
    return s_task_left != NULL || s_task_right != NULL;
}
