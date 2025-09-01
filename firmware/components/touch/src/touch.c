#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "freertos/timers.h"

static const char *TAG = "TOUCH";

#define TOUCH_GPIO_PIN GPIO_NUM_2
#define DEBOUNCE_TIME_MS 250 // Debounce time in milliseconds
#define ESP_INTR_FLAG_DEFAULT 0

static QueueHandle_t gpio_evt_queue = NULL;
static TimerHandle_t debounce_timer = NULL;
static int last_stable_state = 1; // Last stable state (1 = not pressed)
static int pending_state = 1;     // Pending state after debouncing

// Timer callback for debouncing
static void debounce_timer_callback(TimerHandle_t xTimer)
{
    int current_level = gpio_get_level(TOUCH_GPIO_PIN);

    // Check if state has remained stable
    if (current_level == pending_state && current_level != last_stable_state)
    {
        last_stable_state = current_level;

        if (current_level == 0)
        {
            ESP_LOGI(TAG, "Touch detected (debounced)!");
        }
        else
        {
            ESP_LOGI(TAG, "Touch released (debounced)!");
        }
    }
}

// Interrupt Service Routine
static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    uint32_t gpio_num = (uint32_t)arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

// Task for processing GPIO events
static void gpio_task(void *arg)
{
    uint32_t io_num;

    for (;;)
    {
        if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY))
        {
            pending_state = gpio_get_level(TOUCH_GPIO_PIN);

            // Restart timer for debouncing
            xTimerReset(debounce_timer, 0);
        }
    }
}

void init_touch(void)
{
    gpio_config_t io_conf = {.intr_type = GPIO_INTR_ANYEDGE,
                             .mode = GPIO_MODE_INPUT,
                             .pin_bit_mask = (1ULL << TOUCH_GPIO_PIN),
                             .pull_down_en = GPIO_PULLDOWN_DISABLE,
                             .pull_up_en = GPIO_PULLUP_ENABLE};

    gpio_config(&io_conf);

    // Create debounce timer
    debounce_timer = xTimerCreate("debounce_timer", pdMS_TO_TICKS(DEBOUNCE_TIME_MS),
                                  pdFALSE, // One-shot timer
                                  NULL, debounce_timer_callback);

    // Create event queue
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));

    // Start task
    xTaskCreate(gpio_task, "gpio_task", 2048, NULL, 10, NULL);

    // Install interrupt service
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    gpio_isr_handler_add(TOUCH_GPIO_PIN, gpio_isr_handler, (void *)TOUCH_GPIO_PIN);

    ESP_LOGI(TAG, "Touch GPIO on pin %d initialized with debounce", TOUCH_GPIO_PIN);
}