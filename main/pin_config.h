/*
 * SPDX-License-Identifier: MIT
 * 
 * Pin configuration for ESP32-S3-WROOM-CAM (ESP32-S3-N16R8)
 */

#ifndef PIN_CONFIG_H
#define PIN_CONFIG_H

// Camera pins for OV2640
#define CAM_PIN_PWDN    38
#define CAM_PIN_RESET   -1  // Not connected
#define CAM_PIN_XCLK    15
#define CAM_PIN_SIOD    4   // SDA
#define CAM_PIN_SIOC    5   // SCL

#define CAM_PIN_D0      11  // Y2
#define CAM_PIN_D1      9   // Y3
#define CAM_PIN_D2      8   // Y4
#define CAM_PIN_D3      10  // Y5
#define CAM_PIN_D4      12  // Y6
#define CAM_PIN_D5      18  // Y7
#define CAM_PIN_D6      17  // Y8
#define CAM_PIN_D7      16  // Y9

#define CAM_PIN_VSYNC   6
#define CAM_PIN_HREF    7
#define CAM_PIN_PCLK    13

// WS2812B LED
#define LED_GPIO        48

#endif // PIN_CONFIG_H
