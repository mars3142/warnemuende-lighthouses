#include "light.h"

#include "sdkconfig.h"

static LedMatrix_t led_matrix = {.size = 64};

static const uint32_t value = 10;
static const uint32_t mod = 2;

LedMatrix_t get_led_matrix(void)
{
    return led_matrix;
}

esp_err_t wled_init(void)
{
    led_strip_config_t strip_config = {.strip_gpio_num = CONFIG_WLED_DIN_PIN,
                                       .max_leds = led_matrix.size,
                                       .led_model = LED_MODEL_WS2812,
                                       .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_RGB,
                                       .flags = {
                                           .invert_out = false,
                                       }};

    led_strip_rmt_config_t rmt_config = {.clk_src = RMT_CLK_SRC_DEFAULT,
                                         .resolution_hz = 0,
                                         .mem_block_symbols = 0,
                                         .flags = {
                                             .with_dma = true,
                                         }};

    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_matrix.led_strip));

    for (uint32_t i = 0; i < led_matrix.size; i++)
    {
        if (i % mod != 0)
        {
            led_strip_set_pixel(led_matrix.led_strip, i, value, value, value);
        }
    }
    led_strip_refresh(led_matrix.led_strip);

    return ESP_OK;
}
