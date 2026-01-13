/*
 * SPDX-License-Identifier: MIT
 * 
 * Configuration storage implementation
 */

#include "config_store.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "config";
static const char *NVS_NAMESPACE = "color_cfg";
static const char *NVS_KEY = "cfg";

esp_err_t config_store_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition was truncated and needs to be erased");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "NVS initialized");
    }
    return ret;
}

void config_get_defaults(color_config_t *config)
{
    if (!config) return;

    // Red HSV thresholds (simplified 0-255 scale)
    // Default uses lower hue range (0-10) for simplicity.
    // Red color can also appear in upper range (240-255 in standard HSV).
    // For full wraparound detection, users can configure h_min=240, h_max=10 via web UI.
    // The detection algorithm supports wraparound when h_min > h_max.
    config->red.h_min = 0;
    config->red.h_max = 10;
    config->red.s_min = 100;
    config->red.s_max = 255;
    config->red.v_min = 100;
    config->red.v_max = 255;

    // Green HSV thresholds (Hue: 40-80 in simplified range)
    config->green.h_min = 40;
    config->green.h_max = 80;
    config->green.s_min = 100;
    config->green.s_max = 255;
    config->green.v_min = 100;
    config->green.v_max = 255;

    // Blue HSV thresholds (Hue: 100-140 in simplified range)
    config->blue.h_min = 100;
    config->blue.h_max = 140;
    config->blue.s_min = 100;
    config->blue.s_max = 255;
    config->blue.v_min = 100;
    config->blue.v_max = 255;

    config->min_area = 900;        // ~30x30 pixels
    config->min_confidence = 60;   // 60%
    config->frame_decimation = 15; // Process every 15th frame (~2 FPS at 30 FPS)
}

esp_err_t config_load(color_config_t *config)
{
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(ret));
        return ret;
    }

    size_t required_size = sizeof(color_config_t);
    ret = nvs_get_blob(nvs_handle, NVS_KEY, config, &required_size);
    
    nvs_close(nvs_handle);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Configuration loaded from NVS");
    } else if (ret == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "Configuration not found in NVS, using defaults");
    } else {
        ESP_LOGE(TAG, "Failed to read config from NVS: %s", esp_err_to_name(ret));
    }

    return ret;
}

esp_err_t config_save(const color_config_t *config)
{
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace for writing: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = nvs_set_blob(nvs_handle, NVS_KEY, config, sizeof(color_config_t));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write config to NVS: %s", esp_err_to_name(ret));
        nvs_close(nvs_handle);
        return ret;
    }

    ret = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Configuration saved to NVS");
    } else {
        ESP_LOGE(TAG, "Failed to commit config to NVS: %s", esp_err_to_name(ret));
    }

    return ret;
}
