/*
 * SPDX-License-Identifier: MIT
 * 
 * HTTP server with MJPEG streaming and configuration API
 */

#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "esp_err.h"

/**
 * @brief Start HTTP server on port 80
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t http_server_start(void);

/**
 * @brief Stop HTTP server
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t http_server_stop(void);

#endif // HTTP_SERVER_H
