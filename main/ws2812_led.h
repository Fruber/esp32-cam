/*
 * SPDX-License-Identifier: MIT
 * 
 * WS2812B LED driver for object detection indication
 */

#ifndef WS2812_LED_H
#define WS2812_LED_H

#include "esp_err.h"
#include <stdbool.h>

/**
 * @brief Initialize the WS2812B LED
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ws2812_init(void);

/**
 * @brief Set LED color based on object detection
 * 
 * @param objects_detected true if one or more objects detected (red), false otherwise (green)
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ws2812_set_detection_status(bool objects_detected);

#endif // WS2812_LED_H
