/*
 * SPDX-License-Identifier: MIT
 * 
 * Color detection implementation
 */

#include "color_detect.h"
#include "esp_log.h"
#include <string.h>
#include <math.h>

static const char *TAG = "color_detect";
static color_config_t current_config;
static uint32_t frame_counter = 0;

// Convert RGB565 to RGB888
static inline void rgb565_to_rgb888(uint16_t rgb565, uint8_t *r, uint8_t *g, uint8_t *b)
{
    *r = ((rgb565 >> 11) & 0x1F) << 3;
    *g = ((rgb565 >> 5) & 0x3F) << 2;
    *b = (rgb565 & 0x1F) << 3;
}

// Convert RGB to simplified HSV (0-255 range for all channels)
static void rgb_to_hsv(uint8_t r, uint8_t g, uint8_t b, uint8_t *h, uint8_t *s, uint8_t *v)
{
    uint8_t max_val = (r > g) ? ((r > b) ? r : b) : ((g > b) ? g : b);
    uint8_t min_val = (r < g) ? ((r < b) ? r : b) : ((g < b) ? g : b);
    uint8_t delta = max_val - min_val;

    *v = max_val;

    if (max_val == 0) {
        *s = 0;
        *h = 0;
        return;
    }

    *s = (uint8_t)(((uint16_t)delta * 255) / max_val);

    if (delta == 0) {
        *h = 0;
    } else {
        int h_temp;
        if (max_val == r) {
            h_temp = (g - b) * 42 / delta;
        } else if (max_val == g) {
            h_temp = 85 + (b - r) * 42 / delta;
        } else {
            h_temp = 170 + (r - g) * 42 / delta;
        }
        
        if (h_temp < 0) h_temp += 255;
        *h = (uint8_t)h_temp;
    }
}

// Check if HSV values match threshold
static bool hsv_in_range(uint8_t h, uint8_t s, uint8_t v, const hsv_threshold_t *thresh)
{
    // Handle hue wraparound for red color
    bool h_match;
    if (thresh->h_min > thresh->h_max) {
        h_match = (h >= thresh->h_min || h <= thresh->h_max);
    } else {
        h_match = (h >= thresh->h_min && h <= thresh->h_max);
    }

    return h_match && 
           (s >= thresh->s_min && s <= thresh->s_max) &&
           (v >= thresh->v_min && v <= thresh->v_max);
}

esp_err_t color_detect_init(const color_config_t *config)
{
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(&current_config, config, sizeof(color_config_t));
    frame_counter = 0;
    
    ESP_LOGI(TAG, "Color detection initialized");
    ESP_LOGI(TAG, "Min area: %d, Min confidence: %d, Frame decimation: %d",
             current_config.min_area, current_config.min_confidence, current_config.frame_decimation);
    
    return ESP_OK;
}

void color_detect_update_config(const color_config_t *config)
{
    if (config) {
        memcpy(&current_config, config, sizeof(color_config_t));
        ESP_LOGI(TAG, "Configuration updated");
    }
}

esp_err_t color_detect_process(camera_fb_t *fb, detection_result_t *result)
{
    if (!fb || !result) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(result, 0, sizeof(detection_result_t));

    // Frame decimation
    frame_counter++;
    if (frame_counter % current_config.frame_decimation != 0) {
        return ESP_OK;
    }

    if (fb->format != PIXFORMAT_RGB565) {
        ESP_LOGE(TAG, "Unsupported pixel format");
        return ESP_ERR_NOT_SUPPORTED;
    }

    uint16_t width = fb->width;
    uint16_t height = fb->height;
    uint16_t *pixels = (uint16_t *)fb->buf;

    // Simple blob detection: scan image for color regions
    // Track bounding boxes for each color
    uint16_t red_x_min = width, red_x_max = 0, red_y_min = height, red_y_max = 0;
    uint16_t green_x_min = width, green_x_max = 0, green_y_min = height, green_y_max = 0;
    uint16_t blue_x_min = width, blue_x_max = 0, blue_y_min = height, blue_y_max = 0;
    
    uint32_t red_pixels = 0, green_pixels = 0, blue_pixels = 0;

    // Scan image
    for (uint16_t y = 0; y < height; y++) {
        for (uint16_t x = 0; x < width; x++) {
            uint16_t pixel = pixels[y * width + x];
            uint8_t r, g, b, h, s, v;
            
            rgb565_to_rgb888(pixel, &r, &g, &b);
            rgb_to_hsv(r, g, b, &h, &s, &v);

            if (hsv_in_range(h, s, v, &current_config.red)) {
                red_pixels++;
                if (x < red_x_min) red_x_min = x;
                if (x > red_x_max) red_x_max = x;
                if (y < red_y_min) red_y_min = y;
                if (y > red_y_max) red_y_max = y;
            } else if (hsv_in_range(h, s, v, &current_config.green)) {
                green_pixels++;
                if (x < green_x_min) green_x_min = x;
                if (x > green_x_max) green_x_max = x;
                if (y < green_y_min) green_y_min = y;
                if (y > green_y_max) green_y_max = y;
            } else if (hsv_in_range(h, s, v, &current_config.blue)) {
                blue_pixels++;
                if (x < blue_x_min) blue_x_min = x;
                if (x > blue_x_max) blue_x_max = x;
                if (y < blue_y_min) blue_y_min = y;
                if (y > blue_y_max) blue_y_max = y;
            }
        }
    }

    // Check if all three colors detected with minimum area
    bool red_ok = red_pixels >= current_config.min_area;
    bool green_ok = green_pixels >= current_config.min_area;
    bool blue_ok = blue_pixels >= current_config.min_area;

    if (!red_ok || !green_ok || !blue_ok) {
        return ESP_OK;
    }

    // Calculate centroids
    uint16_t red_cx = (red_x_min + red_x_max) / 2;
    uint16_t green_cx = (green_x_min + green_x_max) / 2;
    uint16_t blue_cx = (blue_x_min + blue_x_max) / 2;

    // Check if bands are in order R-G-B (left to right)
    bool ordered = (red_cx < green_cx) && (green_cx < blue_cx);
    
    if (!ordered) {
        return ESP_OK;
    }

    // Calculate confidence based on color separation and alignment
    uint16_t red_cy = (red_y_min + red_y_max) / 2;
    uint16_t green_cy = (green_y_min + green_y_max) / 2;
    uint16_t blue_cy = (blue_y_min + blue_y_max) / 2;

    // Vertical alignment check (should be roughly on same horizontal line)
    uint16_t max_cy = (red_cy > green_cy) ? ((red_cy > blue_cy) ? red_cy : blue_cy) : 
                                            ((green_cy > blue_cy) ? green_cy : blue_cy);
    uint16_t min_cy = (red_cy < green_cy) ? ((red_cy < blue_cy) ? red_cy : blue_cy) : 
                                            ((green_cy < blue_cy) ? green_cy : blue_cy);
    uint16_t vertical_diff = max_cy - min_cy;

    // Confidence calculation: better alignment = higher confidence
    uint8_t alignment_score = (vertical_diff < height / 10) ? 100 : 
                              (vertical_diff < height / 5) ? 70 : 40;

    result->confidence = alignment_score;

    if (result->confidence >= current_config.min_confidence) {
        result->rgb_detected = true;

        // Combined bounding box
        result->bbox_x = red_x_min < green_x_min ? (red_x_min < blue_x_min ? red_x_min : blue_x_min) :
                                                    (green_x_min < blue_x_min ? green_x_min : blue_x_min);
        result->bbox_y = red_y_min < green_y_min ? (red_y_min < blue_y_min ? red_y_min : blue_y_min) :
                                                    (green_y_min < blue_y_min ? green_y_min : blue_y_min);
        
        uint16_t max_x = red_x_max > green_x_max ? (red_x_max > blue_x_max ? red_x_max : blue_x_max) :
                                                    (green_x_max > blue_x_max ? green_x_max : blue_x_max);
        uint16_t max_y = red_y_max > green_y_max ? (red_y_max > blue_y_max ? red_y_max : blue_y_max) :
                                                    (green_y_max > blue_y_max ? green_y_max : blue_y_max);

        result->bbox_w = max_x - result->bbox_x;
        result->bbox_h = max_y - result->bbox_y;

        ESP_LOGI(TAG, "RGB detected! Confidence: %d%%, BBox: (%d,%d,%d,%d)",
                 result->confidence, result->bbox_x, result->bbox_y, result->bbox_w, result->bbox_h);
    }

    return ESP_OK;
}

void color_detect_draw_bbox(camera_fb_t *fb, const detection_result_t *result)
{
    if (!fb || !result || !result->rgb_detected) {
        return;
    }

    if (fb->format != PIXFORMAT_RGB565) {
        return;
    }

    uint16_t width = fb->width;
    uint16_t height = fb->height;
    uint16_t *pixels = (uint16_t *)fb->buf;
    
    // Yellow color in RGB565 (RGB 255,255,0 -> 0xFFE0)
    uint16_t color = 0xFFE0;
    
    uint16_t x1 = result->bbox_x;
    uint16_t y1 = result->bbox_y;
    uint16_t x2 = result->bbox_x + result->bbox_w;
    uint16_t y2 = result->bbox_y + result->bbox_h;

    // Clamp to frame boundaries
    if (x2 >= width) x2 = width - 1;
    if (y2 >= height) y2 = height - 1;

    // Draw top and bottom horizontal lines
    for (uint16_t x = x1; x <= x2 && x < width; x++) {
        if (y1 < height) pixels[y1 * width + x] = color;
        if (y2 < height) pixels[y2 * width + x] = color;
    }

    // Draw left and right vertical lines
    for (uint16_t y = y1; y <= y2 && y < height; y++) {
        if (x1 < width) pixels[y * width + x1] = color;
        if (x2 < width) pixels[y * width + x2] = color;
    }
}
