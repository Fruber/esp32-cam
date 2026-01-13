/*
 * SPDX-License-Identifier: MIT
 * 
 * Configuration storage in NVS for color detection parameters
 */

#ifndef CONFIG_STORE_H
#define CONFIG_STORE_H

#include "esp_err.h"
#include <stdint.h>

// HSV threshold structure for each color
typedef struct {
    uint8_t h_min;
    uint8_t h_max;
    uint8_t s_min;
    uint8_t s_max;
    uint8_t v_min;
    uint8_t v_max;
} hsv_threshold_t;

// Complete color detection configuration
typedef struct {
    hsv_threshold_t red;
    hsv_threshold_t green;
    hsv_threshold_t blue;
    uint16_t min_area;          // Minimum area in pixels
    uint8_t min_confidence;     // Minimum confidence (0-100)
    uint8_t frame_decimation;   // Process every Nth frame
} color_config_t;

/**
 * @brief Initialize configuration storage
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t config_store_init(void);

/**
 * @brief Load configuration from NVS
 * 
 * @param config Pointer to config structure to fill
 * @return ESP_OK on success, ESP_ERR_NVS_NOT_FOUND if not found, other error codes
 */
esp_err_t config_load(color_config_t *config);

/**
 * @brief Save configuration to NVS
 * 
 * @param config Pointer to config structure to save
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t config_save(const color_config_t *config);

/**
 * @brief Get default configuration
 * 
 * @param config Pointer to config structure to fill with defaults
 */
void config_get_defaults(color_config_t *config);

#endif // CONFIG_STORE_H
