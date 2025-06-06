#include "beacon.h"

#include "driver/gpio.h"
#include "driver/gptimer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "inttypes.h"

static const char *TAG = "beacon";

typedef struct
{
    uint32_t tick;
} timer_event_t;

static QueueHandle_t timer_evt_queue = NULL;
gptimer_handle_t gptimer = NULL;

bool IRAM_ATTR timer_callback(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx)
{
    timer_event_t evt = {.tick = edata->count_value};

    BaseType_t high_task_wakeup = pdFALSE;
    xQueueSendFromISR(timer_evt_queue, &evt, &high_task_wakeup);

    return (high_task_wakeup == pdTRUE);
}

void timer_event_task(void *arg)
{
    timer_event_t evt;
    while (true)
    {
        if (xQueueReceive(timer_evt_queue, &evt, portMAX_DELAY))
        {
            static bool level = false;
            level = !level;
            gpio_set_level(CONFIG_WLED_DIN_PIN, level);
            ESP_LOGI(TAG, "Timer Event: count = %" PRIu32 ", GPIO now %s", evt.tick, level ? "HIGH" : "LOW");
        }
    }
}

esp_err_t wled_init(void)
{
    gpio_config_t io_conf = {.pin_bit_mask = (1ULL << CONFIG_WLED_DIN_PIN),
                             .mode = GPIO_MODE_OUTPUT,
                             .pull_up_en = GPIO_PULLUP_DISABLE,
                             .pull_down_en = GPIO_PULLDOWN_DISABLE,
                             .intr_type = GPIO_INTR_DISABLE};
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to configure GPIO: %s", esp_err_to_name(ret));
    }
    return ret;
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

    timer_evt_queue = xQueueCreate(10, sizeof(timer_event_t));
    if (timer_evt_queue == NULL)
    {
        ESP_LOGE(TAG, "Failed to create timer event queue");
        ret = ESP_ERR_NO_MEM;
        goto exit;
    }

    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000, // 1 MHz = 1 µs
    };
    ret = gptimer_new_timer(&timer_config, &gptimer);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to create new gptimer: %s", esp_err_to_name(ret));
        goto cleanup_queue;
    }

    gptimer_event_callbacks_t callbacks = {
        .on_alarm = timer_callback,
    };
    ret = gptimer_register_event_callbacks(gptimer, &callbacks, NULL);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to register timer callbacks: %s", esp_err_to_name(ret));
        goto cleanup_timer;
    }

    ret = gptimer_enable(gptimer);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to enable gptimer: %s", esp_err_to_name(ret));
        goto cleanup_timer; // Timer created but not enabled, can be deleted.
    }

    gptimer_alarm_config_t alarm_config = {
        .alarm_count = 2000000, // 2.000.000 µs = 2 secs
        .reload_count = 0,
        .flags.auto_reload_on_alarm = true,
    };
    ret = gptimer_set_alarm_action(gptimer, &alarm_config);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set gptimer alarm action: %s", esp_err_to_name(ret));
        goto cleanup_enabled_timer; // Timer is enabled, disable before deleting.
    }

    BaseType_t task_created = xTaskCreate(timer_event_task, "timer_event_task", 4096, NULL, 10, NULL);
    if (task_created != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create timer event task");
        ret = ESP_ERR_NO_MEM; // Task creation often fails due to insufficient memory
        goto cleanup_enabled_timer;
    }

    ESP_LOGI(TAG, "Beacon module initialized.");
    goto exit;

cleanup_enabled_timer:
    if (gptimer)
    {
        gptimer_disable(gptimer); // Attempt to disable
    }
cleanup_timer:
    if (gptimer)
    {
        gptimer_del_timer(gptimer);
        gptimer = NULL;
    }
cleanup_queue:
    if (timer_evt_queue)
    {
        vQueueDelete(timer_evt_queue);
        timer_evt_queue = NULL;
    }
exit:
    return ret;
}
