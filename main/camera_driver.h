/*
 * SPDX-License-Identifier: MIT
 * 
 * Camera driver for OV2640 on ESP32-S3
 */

#ifndef CAMERA_DRIVER_H
#define CAMERA_DRIVER_H

#include "esp_camera.h"
#include "esp_err.h"

/**
 * @brief Initialize the camera with OV2640 settings
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t camera_init(void);

/**
 * @brief Get a frame buffer from the camera
 * 
 * @return Pointer to camera frame buffer or NULL on error
 */
camera_fb_t* camera_get_fb(void);

/**
 * @brief Return a frame buffer to the camera driver
 * 
 * @param fb Frame buffer to return
 */
void camera_return_fb(camera_fb_t* fb);

#endif // CAMERA_DRIVER_H
