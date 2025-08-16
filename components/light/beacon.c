#include "beacon.h"

#include "driver/gptimer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip.h"
#include "light.h"
#include "sdkconfig.h"
#include "semaphore.h"

static const char *TAG = "beacon";

static SemaphoreHandle_t timer_semaphore;
gptimer_handle_t gptimer = NULL;

static const uint32_t value = 200;

static bool IRAM_ATTR beacon_timer_callback(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata,
                                            void *userCtx)
{
    BaseType_t high_task_wakeup = pdFALSE;
    xSemaphoreGiveFromISR(timer_semaphore, &high_task_wakeup);
    if (high_task_wakeup)
    {
        portYIELD_FROM_ISR();
    }
    return true;
}

static void beacon_timer_event_task(void *arg)
{
    while (true)
    {
        if (xSemaphoreTake(timer_semaphore, portMAX_DELAY))
        {
            LedMatrix_t led_matrix = get_led_matrix();

            static bool level = false;
            level = !level;
            if (led_matrix.led_strip)
            {
                for (uint32_t i = 0; i < led_matrix.size; i++)
                {
                    led_strip_set_pixel(led_matrix.led_strip, i, 0, (level) ? value : 0, 0);
                }
                led_strip_refresh(led_matrix.led_strip);
            }
            ESP_LOGD(TAG, "Timer Event, LED now %s", level ? "ON" : "OFF");
        }
    }
}

esp_err_t beacon_start(void)
{
    if (gptimer == NULL)
    {
        ESP_LOGE(TAG, "GPTimer not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t ret = gptimer_start(gptimer);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start gptimer: %s", esp_err_to_name(ret));
    }
    else
    {
        ESP_LOGI(TAG, "GPTimer started.");
    }
    return ret;
}

esp_err_t beacon_stop(void)
{
    if (gptimer == NULL)
    {
        ESP_LOGE(TAG, "GPTimer not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t ret = gptimer_stop(gptimer);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to stop gptimer: %s", esp_err_to_name(ret));
    }
    else
    {
        ESP_LOGI(TAG, "GPTimer stopped.");
    }
    return ret;
}

esp_err_t beacon_init(void)
{
    esp_err_t ret = ESP_OK;

    timer_semaphore = xSemaphoreCreateBinary();

    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT, .direction = GPTIMER_COUNT_UP, .resolution_hz = 1000000};
    ret = gptimer_new_timer(&timer_config, &gptimer);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to create new gptimer: %s", esp_err_to_name(ret));
        goto exit;
    }

    gptimer_event_callbacks_t callbacks = {.on_alarm = beacon_timer_callback};
    ret = gptimer_register_event_callbacks(gptimer, &callbacks, NULL);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to register timer callbacks: %s", esp_err_to_name(ret));
        goto cleanupTimer;
    }

    ret = gptimer_enable(gptimer);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to enable gptimer: %s", esp_err_to_name(ret));
        goto cleanupTimer;
    }

    gptimer_alarm_config_t alarm_config = {
        .alarm_count = 2000000, .reload_count = 0, .flags.auto_reload_on_alarm = true};
    ret = gptimer_set_alarm_action(gptimer, &alarm_config);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set gptimer alarm action: %s", esp_err_to_name(ret));
        goto cleanupEnabledTimer;
    }

    BaseType_t task_created = xTaskCreate(beacon_timer_event_task, "beacon_timer_event_task", 4096, NULL, 10, NULL);
    if (task_created != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create timer event task");
        ret = ESP_ERR_NO_MEM;
        goto cleanupEnabledTimer;
    }

    ESP_LOGI(TAG, "Beacon module initialized.");
    goto exit;

cleanupEnabledTimer:
    if (gptimer)
    {
        gptimer_disable(gptimer);
    }
cleanupTimer:
    if (gptimer)
    {
        gptimer_del_timer(gptimer);
        gptimer = NULL;
    }
exit:
    return ret;
}
