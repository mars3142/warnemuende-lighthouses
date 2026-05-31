#include "beacon.h"

#include "driver/gptimer.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "light.h"
#include "light_priv.h"
#include "persistence.h"
#include "sdkconfig.h"
#include "semaphore.h"

static const char *TAG = "beacon";

static beacon_state_cb_t s_state_cb = NULL;
static bool s_running = false;

// ─── LED helpers ─────────────────────────────────────────────────────────────

static void led_refresh(uint32_t brightness)
{
    LedMatrix_t led_matrix = get_led_matrix();
    if (!led_matrix.led_strip) return;

    for (uint32_t i = 0; i < led_matrix.size; i++)
    {
        led_strip_set_pixel(led_matrix.led_strip, i, 0, brightness, 0);
    }
    led_strip_refresh(led_matrix.led_strip);
}

// ─── Commissioning status LED ─────────────────────────────────────────────────

static esp_timer_handle_t s_status_timer;
static bool s_status_blink_on = false;

static void status_timer_cb(void *arg)
{
    LedMatrix_t lm = get_led_matrix();
    if (!lm.led_strip) return;

    s_status_blink_on = !s_status_blink_on;
    led_refresh(0);  // clear green
    led_strip_set_pixel(lm.led_strip, 0, 0, 0, s_status_blink_on ? 20 : 0);
    led_strip_refresh(lm.led_strip);
}

static void stop_status_timer(void)
{
    if (!s_status_timer)
        return;
    esp_timer_stop(s_status_timer);
    esp_timer_delete(s_status_timer);
    s_status_timer = NULL;
}

static void set_blue(uint8_t brightness)
{
    LedMatrix_t lm = get_led_matrix();
    if (!lm.led_strip) return;
    led_strip_set_pixel(lm.led_strip, 0, 0, 0, brightness);
    led_strip_refresh(lm.led_strip);
}

static void set_red(uint8_t brightness)
{
    LedMatrix_t lm = get_led_matrix();
    if (!lm.led_strip) return;
    led_strip_set_pixel(lm.led_strip, 0, brightness, 0, 0);
    led_strip_refresh(lm.led_strip);
}

void beacon_set_status(beacon_status_t status)
{
    stop_status_timer();
    s_status_blink_on = false;

    switch (status) {
        case BEACON_STATUS_SEARCHING: {
            esp_timer_create_args_t args = {.callback = status_timer_cb, .name = "status_blink"};
            esp_timer_create(&args, &s_status_timer);
            esp_timer_start_periodic(s_status_timer, 500000);
            set_blue(20);
            s_status_blink_on = true;
            break;
        }
        case BEACON_STATUS_PAIRING:
            set_blue(20);
            break;
        case BEACON_STATUS_CONNECTED:
            led_refresh(0);
            break;
        case BEACON_STATUS_ERROR:
            set_red(20);
            break;
    }
}

void beacon_register_state_cb(beacon_state_cb_t cb)
{
    s_state_cb = cb;
}

static SemaphoreHandle_t timer_semaphore;
gptimer_handle_t gptimer = NULL;

static const uint32_t value = 200;
static const uint32_t alarm_value = 2000000;

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

            static bool level = false;
            level = !level;
            led_refresh(level ? value : 0);
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
    esp_err_t ret = gptimer_enable(gptimer);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to enable gptimer: %s", esp_err_to_name(ret));
        return beacon_stop();
    }

    ret = gptimer_set_raw_count(gptimer, alarm_value - 1);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set gptimer raw count: %s", esp_err_to_name(ret));
        return beacon_stop();
    }

    ret = gptimer_start(gptimer);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start gptimer: %s", esp_err_to_name(ret));
    }

    s_running = true;
    ESP_LOGI(TAG, "GPTimer started.");
    return ret;
}

esp_err_t beacon_stop(void)
{
    led_refresh(0);
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
    s_running = false;
    ESP_LOGI(TAG, "GPTimer stopped.");

    ret = gptimer_disable(gptimer);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to enable gptimer: %s", esp_err_to_name(ret));
    }

    return ret;
}

esp_err_t beacon_init(void)
{
    esp_err_t ret = ESP_OK;

    timer_semaphore = xSemaphoreCreateBinary();

    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000,
    };
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

    gptimer_alarm_config_t alarm_config = {
        .alarm_count = 2000000,
        .reload_count = 0,
        .flags.auto_reload_on_alarm = true,
    };
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

bool beacon_is_running(void)
{
    return s_running;
}

esp_err_t beacon_toggle(void)
{
    int8_t beacon_enabled = 0;
    persistence_load(VALUE_TYPE_INT8, "BEACON_ENABLED", &beacon_enabled);

    esp_err_t ret;
    if (beacon_enabled)
    {
        ret = beacon_stop();
    }
    else
    {
        ret = beacon_start();
    }

    beacon_enabled = 1 - beacon_enabled;
    persistence_save(VALUE_TYPE_INT8, "BEACON_ENABLED", &beacon_enabled);

    if (s_state_cb)
        s_state_cb(beacon_enabled);

    return ret;
}
