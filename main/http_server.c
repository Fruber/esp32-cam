/*
 * SPDX-License-Identifier: MIT
 * 
 * HTTP server implementation with MJPEG streaming
 */

#include "http_server.h"
#include "web_ui.h"
#include "camera_driver.h"
#include "color_detect.h"
#include "config_store.h"
#include "ws2812_led.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_camera.h"
#include "cJSON.h"
#include <string.h>

// Forward declaration (provided by esp32-camera component)
bool frame2jpg(camera_fb_t *fb, uint8_t quality, uint8_t **out, size_t *out_len);

static const char *TAG = "http_server";
static httpd_handle_t server = NULL;

#define PART_BOUNDARY "123456789000000000000987654321"
static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

// Handler for root path (web UI)
static esp_err_t root_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Content-Encoding", "identity");
    return httpd_resp_send(req, web_ui_html, strlen(web_ui_html));
}

// Handler for MJPEG stream
static esp_err_t stream_handler(httpd_req_t *req)
{
    camera_fb_t *fb = NULL;
    esp_err_t res = ESP_OK;
    size_t _jpg_buf_len = 0;
    uint8_t *_jpg_buf = NULL;
    char part_buf[64];
    detection_result_t detection;

    res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
    if (res != ESP_OK) {
        return res;
    }

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    while (true) {
        fb = camera_get_fb();
        if (!fb) {
            ESP_LOGE(TAG, "Camera capture failed");
            res = ESP_FAIL;
            break;
        }

        // Process for color detection
        color_detect_process(fb, &detection);
        
        // Update LED based on detection
        ws2812_set_detection_status(detection.rgb_detected);

        // Draw bounding box if detected
        if (detection.rgb_detected) {
            color_detect_draw_bbox(fb, &detection);
        }

        // Convert RGB565 to JPEG
        if (fb->format != PIXFORMAT_JPEG) {
            bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
            if (!jpeg_converted) {
                ESP_LOGE(TAG, "JPEG compression failed");
                camera_return_fb(fb);
                res = ESP_FAIL;
                break;
            }
        } else {
            _jpg_buf_len = fb->len;
            _jpg_buf = fb->buf;
        }

        // Send JPEG frame
        if (res == ESP_OK) {
            res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
        }
        if (res == ESP_OK) {
            size_t hlen = snprintf(part_buf, sizeof(part_buf), _STREAM_PART, _jpg_buf_len);
            res = httpd_resp_send_chunk(req, part_buf, hlen);
        }
        if (res == ESP_OK) {
            res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
        }

        if (fb->format != PIXFORMAT_JPEG) {
            free(_jpg_buf);
            _jpg_buf = NULL;
        }

        camera_return_fb(fb);

        if (res != ESP_OK) {
            break;
        }
    }

    return res;
}

// Handler for GET /api/config
static esp_err_t config_get_handler(httpd_req_t *req)
{
    color_config_t config;
    esp_err_t ret = config_load(&config);
    
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        config_get_defaults(&config);
    }

    cJSON *root = cJSON_CreateObject();
    
    cJSON *red = cJSON_CreateObject();
    cJSON_AddNumberToObject(red, "h_min", config.red.h_min);
    cJSON_AddNumberToObject(red, "h_max", config.red.h_max);
    cJSON_AddNumberToObject(red, "s_min", config.red.s_min);
    cJSON_AddNumberToObject(red, "s_max", config.red.s_max);
    cJSON_AddNumberToObject(red, "v_min", config.red.v_min);
    cJSON_AddNumberToObject(red, "v_max", config.red.v_max);
    cJSON_AddItemToObject(root, "red", red);

    cJSON *green = cJSON_CreateObject();
    cJSON_AddNumberToObject(green, "h_min", config.green.h_min);
    cJSON_AddNumberToObject(green, "h_max", config.green.h_max);
    cJSON_AddNumberToObject(green, "s_min", config.green.s_min);
    cJSON_AddNumberToObject(green, "s_max", config.green.s_max);
    cJSON_AddNumberToObject(green, "v_min", config.green.v_min);
    cJSON_AddNumberToObject(green, "v_max", config.green.v_max);
    cJSON_AddItemToObject(root, "green", green);

    cJSON *blue = cJSON_CreateObject();
    cJSON_AddNumberToObject(blue, "h_min", config.blue.h_min);
    cJSON_AddNumberToObject(blue, "h_max", config.blue.h_max);
    cJSON_AddNumberToObject(blue, "s_min", config.blue.s_min);
    cJSON_AddNumberToObject(blue, "s_max", config.blue.s_max);
    cJSON_AddNumberToObject(blue, "v_min", config.blue.v_min);
    cJSON_AddNumberToObject(blue, "v_max", config.blue.v_max);
    cJSON_AddItemToObject(root, "blue", blue);

    cJSON_AddNumberToObject(root, "min_area", config.min_area);
    cJSON_AddNumberToObject(root, "min_confidence", config.min_confidence);
    cJSON_AddNumberToObject(root, "frame_decimation", config.frame_decimation);

    char *json_str = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);

    free(json_str);
    cJSON_Delete(root);

    return ESP_OK;
}

// Handler for POST /api/config
static esp_err_t config_post_handler(httpd_req_t *req)
{
    char buf[1024];
    int ret, remaining = req->content_len;

    if (remaining >= sizeof(buf)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Content too long");
        return ESP_FAIL;
    }

    ret = httpd_req_recv(req, buf, remaining);
    if (ret <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (root == NULL) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    color_config_t config;
    config_get_defaults(&config);

    cJSON *red = cJSON_GetObjectItem(root, "red");
    if (red && cJSON_IsObject(red)) {
        cJSON *item;
        if ((item = cJSON_GetObjectItem(red, "h_min")) && cJSON_IsNumber(item)) config.red.h_min = item->valueint;
        if ((item = cJSON_GetObjectItem(red, "h_max")) && cJSON_IsNumber(item)) config.red.h_max = item->valueint;
        if ((item = cJSON_GetObjectItem(red, "s_min")) && cJSON_IsNumber(item)) config.red.s_min = item->valueint;
        if ((item = cJSON_GetObjectItem(red, "s_max")) && cJSON_IsNumber(item)) config.red.s_max = item->valueint;
        if ((item = cJSON_GetObjectItem(red, "v_min")) && cJSON_IsNumber(item)) config.red.v_min = item->valueint;
        if ((item = cJSON_GetObjectItem(red, "v_max")) && cJSON_IsNumber(item)) config.red.v_max = item->valueint;
    }

    cJSON *green = cJSON_GetObjectItem(root, "green");
    if (green && cJSON_IsObject(green)) {
        cJSON *item;
        if ((item = cJSON_GetObjectItem(green, "h_min")) && cJSON_IsNumber(item)) config.green.h_min = item->valueint;
        if ((item = cJSON_GetObjectItem(green, "h_max")) && cJSON_IsNumber(item)) config.green.h_max = item->valueint;
        if ((item = cJSON_GetObjectItem(green, "s_min")) && cJSON_IsNumber(item)) config.green.s_min = item->valueint;
        if ((item = cJSON_GetObjectItem(green, "s_max")) && cJSON_IsNumber(item)) config.green.s_max = item->valueint;
        if ((item = cJSON_GetObjectItem(green, "v_min")) && cJSON_IsNumber(item)) config.green.v_min = item->valueint;
        if ((item = cJSON_GetObjectItem(green, "v_max")) && cJSON_IsNumber(item)) config.green.v_max = item->valueint;
    }

    cJSON *blue = cJSON_GetObjectItem(root, "blue");
    if (blue && cJSON_IsObject(blue)) {
        cJSON *item;
        if ((item = cJSON_GetObjectItem(blue, "h_min")) && cJSON_IsNumber(item)) config.blue.h_min = item->valueint;
        if ((item = cJSON_GetObjectItem(blue, "h_max")) && cJSON_IsNumber(item)) config.blue.h_max = item->valueint;
        if ((item = cJSON_GetObjectItem(blue, "s_min")) && cJSON_IsNumber(item)) config.blue.s_min = item->valueint;
        if ((item = cJSON_GetObjectItem(blue, "s_max")) && cJSON_IsNumber(item)) config.blue.s_max = item->valueint;
        if ((item = cJSON_GetObjectItem(blue, "v_min")) && cJSON_IsNumber(item)) config.blue.v_min = item->valueint;
        if ((item = cJSON_GetObjectItem(blue, "v_max")) && cJSON_IsNumber(item)) config.blue.v_max = item->valueint;
    }

    cJSON *min_area = cJSON_GetObjectItem(root, "min_area");
    if (min_area && cJSON_IsNumber(min_area)) config.min_area = min_area->valueint;

    cJSON *min_confidence = cJSON_GetObjectItem(root, "min_confidence");
    if (min_confidence && cJSON_IsNumber(min_confidence)) config.min_confidence = min_confidence->valueint;

    cJSON *frame_decimation = cJSON_GetObjectItem(root, "frame_decimation");
    if (frame_decimation && cJSON_IsNumber(frame_decimation)) config.frame_decimation = frame_decimation->valueint;

    cJSON_Delete(root);

    // Save to NVS
    esp_err_t err = config_save(&config);
    if (err != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to save config");
        return ESP_FAIL;
    }

    // Update runtime config
    color_detect_update_config(&config);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");

    return ESP_OK;
}

esp_err_t http_server_start(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.ctrl_port = 32768;
    config.max_uri_handlers = 8;
    config.max_resp_headers = 8;
    config.stack_size = 8192;

    ESP_LOGI(TAG, "Starting HTTP server on port %d", config.server_port);

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t root_uri = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = root_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &root_uri);

        httpd_uri_t stream_uri = {
            .uri = "/stream",
            .method = HTTP_GET,
            .handler = stream_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &stream_uri);

        httpd_uri_t config_get_uri = {
            .uri = "/api/config",
            .method = HTTP_GET,
            .handler = config_get_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &config_get_uri);

        httpd_uri_t config_post_uri = {
            .uri = "/api/config",
            .method = HTTP_POST,
            .handler = config_post_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &config_post_uri);

        ESP_LOGI(TAG, "HTTP server started successfully");
        return ESP_OK;
    }

    ESP_LOGE(TAG, "Failed to start HTTP server");
    return ESP_FAIL;
}

esp_err_t http_server_stop(void)
{
    if (server) {
        httpd_stop(server);
        server = NULL;
        ESP_LOGI(TAG, "HTTP server stopped");
    }
    return ESP_OK;
}
