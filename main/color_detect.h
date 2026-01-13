/*
 * SPDX-License-Identifier: MIT
 * 
 * Color detection module for RGB band detection
 */

#ifndef COLOR_DETECT_H
#define COLOR_DETECT_H

#include "esp_camera.h"
#include "config_store.h"
#include <stdbool.h>
#include <stdint.h>

// Detection result structure
typedef struct {
    bool rgb_detected;      // True if R-G-B bands detected in order
    uint8_t confidence;     // Detection confidence (0-100)
    uint16_t bbox_x;        // Bounding box top-left X
    uint16_t bbox_y;        // Bounding box top-left Y
    uint16_t bbox_w;        // Bounding box width
    uint16_t bbox_h;        // Bounding box height
} detection_result_t;

/**
 * @brief Initialize color detection module
 * 
 * @param config Pointer to color configuration
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t color_detect_init(const color_config_t *config);

/**
 * @brief Update color detection configuration
 * 
 * @param config Pointer to new color configuration
 */
void color_detect_update_config(const color_config_t *config);

/**
 * @brief Process frame for color detection
 * 
 * @param fb Camera frame buffer (RGB565 format)
 * @param result Pointer to store detection result
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t color_detect_process(camera_fb_t *fb, detection_result_t *result);

/**
 * @brief Draw bounding box on RGB565 frame buffer
 * 
 * @param fb Frame buffer to draw on
 * @param result Detection result with bounding box coordinates
 */
void color_detect_draw_bbox(camera_fb_t *fb, const detection_result_t *result);

#endif // COLOR_DETECT_H
