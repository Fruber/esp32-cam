/*
 * SPDX-License-Identifier: MIT
 * 
 * WS2812B LED driver implementation
 */

#include "ws2812_led.h"
#include "pin_config.h"
#include "led_strip.h"
#include "esp_log.h"

static const char *TAG = "ws2812";
static led_strip_handle_t led_strip = NULL;

esp_err_t ws2812_init(void)
{
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_GPIO,
        .max_leds = 1,
        .led_pixel_format = LED_PIXEL_FORMAT_GRB,
        .led_model = LED_MODEL_WS2812,
        .flags.invert_out = false,
    };

    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
        .flags.with_dma = false,
    };

    esp_err_t ret = led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create LED strip: %s", esp_err_to_name(ret));
        return ret;
    }

    // Clear LED on init
    led_strip_clear(led_strip);
    
    ESP_LOGI(TAG, "WS2812B LED initialized on GPIO%d", LED_GPIO);
    return ESP_OK;
}

esp_err_t ws2812_set_detection_status(bool objects_detected)
{
    if (led_strip == NULL) {
        ESP_LOGE(TAG, "LED strip not initialized");
        return ESP_FAIL;
    }

    if (objects_detected) {
        // Red for objects detected
        led_strip_set_pixel(led_strip, 0, 255, 0, 0);
    } else {
        // Green for no objects
        led_strip_set_pixel(led_strip, 0, 0, 255, 0);
    }

    return led_strip_refresh(led_strip);
}
