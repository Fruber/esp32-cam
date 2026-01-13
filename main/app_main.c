/*
 * SPDX-License-Identifier: MIT
 * 
 * Main application for ESP32-S3 Camera with color detection
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"

#include "wifi_provisioning/manager.h"
#include "wifi_provisioning/scheme_softap.h"

#include "camera_driver.h"
#include "ws2812_led.h"
#include "config_store.h"
#include "color_detect.h"
#include "http_server.h"

static const char *TAG = "main";

#define PROV_QR_VERSION         "v1"
#define PROV_TRANSPORT_SOFTAP   "softap"
#define QRCODE_BASE_URL         "https://espressif.github.io/esp-jumpstart/qrcode.html"

static EventGroupHandle_t wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_PROV_EVENT) {
        switch (event_id) {
            case WIFI_PROV_START:
                ESP_LOGI(TAG, "Provisioning started");
                break;
            case WIFI_PROV_CRED_RECV: {
                wifi_sta_config_t *wifi_sta_cfg = (wifi_sta_config_t *)event_data;
                ESP_LOGI(TAG, "Received Wi-Fi credentials SSID:%s", wifi_sta_cfg->ssid);
                break;
            }
            case WIFI_PROV_CRED_FAIL: {
                wifi_prov_sta_fail_reason_t *reason = (wifi_prov_sta_fail_reason_t *)event_data;
                ESP_LOGE(TAG, "Provisioning failed! Reason: %s",
                        (*reason == WIFI_PROV_STA_AUTH_ERROR) ? "Wi-Fi auth error" : "Wi-Fi AP not found");
                break;
            }
            case WIFI_PROV_CRED_SUCCESS:
                ESP_LOGI(TAG, "Provisioning successful");
                break;
            case WIFI_PROV_END:
                wifi_prov_mgr_deinit();
                break;
            default:
                break;
        }
    } else if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                esp_wifi_connect();
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                ESP_LOGI(TAG, "Disconnected. Reconnecting...");
                esp_wifi_connect();
                xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
                break;
            default:
                break;
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Connected with IP Address:" IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static void get_device_service_name(char *service_name, size_t max)
{
    uint8_t eth_mac[6];
    const char *ssid_prefix = "PROV_";
    esp_wifi_get_mac(WIFI_IF_STA, eth_mac);
    snprintf(service_name, max, "%s%02X%02X%02X",
             ssid_prefix, eth_mac[3], eth_mac[4], eth_mac[5]);
}

static void wifi_init_sta(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_prov_mgr_config_t config = {
        .scheme = wifi_prov_scheme_softap,
        .scheme_event_handler = WIFI_PROV_EVENT_HANDLER_NONE
    };

    ESP_ERROR_CHECK(wifi_prov_mgr_init(config));

    bool provisioned = false;
    ESP_ERROR_CHECK(wifi_prov_mgr_is_provisioned(&provisioned));

    if (!provisioned) {
        ESP_LOGI(TAG, "Starting provisioning");

        char service_name[12];
        get_device_service_name(service_name, sizeof(service_name));

        wifi_prov_security_t security = WIFI_PROV_SECURITY_1;
        const char *pop = "abcd1234";

        const char *service_key = NULL;

        wifi_prov_scheme_softap_set_httpd_handle(NULL);

        ESP_ERROR_CHECK(wifi_prov_mgr_start_provisioning(security, pop, service_name, service_key));

        ESP_LOGI(TAG, "Provisioning started with SSID: %s, POP: %s", service_name, pop);
    } else {
        ESP_LOGI(TAG, "Already provisioned, starting Wi-Fi STA");
        wifi_prov_mgr_deinit();
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_start());
    }

    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, false, true, portMAX_DELAY);
}

void app_main(void)
{
    ESP_LOGI(TAG, "ESP32-S3 Camera with Color Detection");
    ESP_LOGI(TAG, "Build: %s %s", __DATE__, __TIME__);

    // Initialize NVS
    ESP_ERROR_CHECK(config_store_init());

    // Load or initialize configuration
    color_config_t config;
    esp_err_t ret = config_load(&config);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "No configuration found, using defaults");
        config_get_defaults(&config);
        config_save(&config);
    }

    // Initialize WS2812B LED
    ESP_ERROR_CHECK(ws2812_init());
    ws2812_set_detection_status(false);

    // Initialize camera
    ESP_LOGI(TAG, "Initializing camera...");
    ESP_ERROR_CHECK(camera_init());

    // Initialize color detection
    ESP_ERROR_CHECK(color_detect_init(&config));

    // Initialize Wi-Fi and provisioning
    ESP_LOGI(TAG, "Starting Wi-Fi provisioning...");
    wifi_init_sta();

    // Start HTTP server
    ESP_LOGI(TAG, "Starting HTTP server...");
    ESP_ERROR_CHECK(http_server_start());

    ESP_LOGI(TAG, "System ready! Access web UI at http://<device-ip>/");

    // Main loop
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
