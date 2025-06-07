#include "beacon.h"

#include "driver/gptimer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "inttypes.h"
#include "led_strip.h"
#include "sdkconfig.h"
#include "semaphore.h"

static const char *TAG = "beacon";

typedef struct
{
    led_strip_handle_t ledStrip;
    uint32_t size;
} LedMatrix;

static LedMatrix ledMatrix = {.size = 64};
static SemaphoreHandle_t timer_semaphore;
gptimer_handle_t gpTimer = NULL;

bool IRAM_ATTR timerCallback(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *userCtx)
{
    BaseType_t high_task_wakeup = pdFALSE;
    xSemaphoreGiveFromISR(timer_semaphore, &high_task_wakeup);
    if (high_task_wakeup)
    {
        portYIELD_FROM_ISR();
    }
    return true;
}

void timerEventTask(void *arg)
{
    while (true)
    {
        if (xSemaphoreTake(timer_semaphore, portMAX_DELAY))
        {
            static bool level = false;
            level = !level;
            if (ledMatrix.ledStrip)
            {
                for (uint32_t i = 0; i < (ledMatrix.size / 2); i++)
                {
                    led_strip_set_pixel(ledMatrix.ledStrip, i, 0, (level) ? 100 : 0, 0);
                }
                led_strip_refresh(ledMatrix.ledStrip);
            }
            ESP_LOGD(TAG, "Timer Event, LED now %s", level ? "ON" : "OFF");
        }
    }
}

esp_err_t initWled(void)
{
    led_strip_config_t stripConfig = {.strip_gpio_num = CONFIG_WLED_DIN_PIN,
                                      .max_leds = ledMatrix.size,
                                      .led_model = LED_MODEL_WS2812,
                                      .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_RGB,
                                      .flags = {
                                          .invert_out = false,
                                      }};

    led_strip_rmt_config_t rmtConfig = {.clk_src = RMT_CLK_SRC_DEFAULT,
                                        .resolution_hz = 0,
                                        .mem_block_symbols = 0,
                                        .flags = {
                                            .with_dma = true,
                                        }};
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&stripConfig, &rmtConfig, &ledMatrix.ledStrip));

    const uint32_t value = 25;
    for (uint32_t i = (ledMatrix.size / 2); i < ledMatrix.size; i++)
    {
        led_strip_set_pixel(ledMatrix.ledStrip, i, value, value, value);
    }
    led_strip_refresh(ledMatrix.ledStrip);

    return ESP_OK;
}

esp_err_t startBeacon(void)
{
    if (gpTimer == NULL)
    {
        ESP_LOGE(TAG, "GPTimer not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t ret = gptimer_start(gpTimer);
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

esp_err_t stopBeacon(void)
{
    if (gpTimer == NULL)
    {
        ESP_LOGE(TAG, "GPTimer not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t ret = gptimer_stop(gpTimer);
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

esp_err_t initBeacon(void)
{
    esp_err_t ret = ESP_OK;

    timer_semaphore = xSemaphoreCreateBinary();

    gptimer_config_t timerConfig = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT, .direction = GPTIMER_COUNT_UP, .resolution_hz = 1000000};
    ret = gptimer_new_timer(&timerConfig, &gpTimer);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to create new gptimer: %s", esp_err_to_name(ret));
        goto exit;
    }

    gptimer_event_callbacks_t callbacks = {.on_alarm = timerCallback};
    ret = gptimer_register_event_callbacks(gpTimer, &callbacks, NULL);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to register timer callbacks: %s", esp_err_to_name(ret));
        goto cleanupTimer;
    }

    ret = gptimer_enable(gpTimer);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to enable gptimer: %s", esp_err_to_name(ret));
        goto cleanupTimer;
    }

    gptimer_alarm_config_t alarmConfig = {
        .alarm_count = 2000000, .reload_count = 0, .flags.auto_reload_on_alarm = true};
    ret = gptimer_set_alarm_action(gpTimer, &alarmConfig);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set gptimer alarm action: %s", esp_err_to_name(ret));
        goto cleanupEnabledTimer;
    }

    BaseType_t taskCreated = xTaskCreate(timerEventTask, "timer_event_task", 4096, NULL, 10, NULL);
    if (taskCreated != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create timer event task");
        ret = ESP_ERR_NO_MEM;
        goto cleanupEnabledTimer;
    }

    ESP_LOGI(TAG, "Beacon module initialized.");
    goto exit;

cleanupEnabledTimer:
    if (gpTimer)
    {
        gptimer_disable(gpTimer);
    }
cleanupTimer:
    if (gpTimer)
    {
        gptimer_del_timer(gpTimer);
        gpTimer = NULL;
    }
exit:
    return ret;
}
