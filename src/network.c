#include "network.h"
#include "web_ui.h"
#include "mqtt_bridge.h"
#include <string.h>
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_hosted.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "network_credentials.h"

static EventGroupHandle_t events;
#define CONNECT_BIT BIT0
#define ONLINE_BIT BIT1
static const char *TAG = "wifi";

static void event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    if (base == WIFI_EVENT && (id == WIFI_EVENT_STA_START || id == WIFI_EVENT_STA_DISCONNECTED)) {
        if (id == WIFI_EVENT_STA_DISCONNECTED) ESP_LOGW(TAG, "Disconnected; retrying");
        xEventGroupSetBits(events, CONNECT_BIT);
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *got = data;
        ESP_LOGI(TAG, "Open http://" IPSTR "/ in your browser", IP2STR(&got->ip_info.ip));
        xEventGroupSetBits(events, ONLINE_BIT);
    }
}

static esp_err_t initialize(void)
{
    esp_err_t err = nvs_flash_init();
    // Never erase NVS implicitly: it can contain settings belonging to the user.
    if (err != ESP_OK) return err;
    if ((err = esp_netif_init()) != ESP_OK) return err;
    if ((err = esp_event_loop_create_default()) != ESP_OK) return err;
    esp_netif_t *netif = esp_netif_create_default_wifi_sta();
    if (!netif) return ESP_ERR_NO_MEM;
    esp_netif_set_hostname(netif, "streamdeck-mini");
    if ((err = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, event, NULL)) != ESP_OK) return err;
    if ((err = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, event, NULL)) != ESP_OK) return err;
    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    // Initialize the C6 transport before esp_wifi_remote reconfigures it.
    if ((err = esp_hosted_init()) != ESP_OK) return err;
    // esp_wifi_remote dispatches this API to ESP-Hosted / the SDIO C6.
    if ((err = esp_wifi_init(&init)) != ESP_OK) return err;
    wifi_config_t config = {0};
    memcpy(config.sta.ssid, network_ssid(), strlen(network_ssid()));
    memcpy(config.sta.password, network_password(), strlen(network_password()));
    config.sta.threshold.authmode = strlen(network_password()) ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
    config.sta.pmf_cfg.capable = true;
    if ((err = esp_wifi_set_storage(WIFI_STORAGE_RAM)) != ESP_OK) return err;
    if ((err = esp_wifi_set_mode(WIFI_MODE_STA)) != ESP_OK) return err;
    if ((err = esp_wifi_set_config(WIFI_IF_STA, &config)) != ESP_OK) return err;
    return esp_wifi_start();
}

static void network_task(void *arg)
{
    (void)arg;
    esp_err_t err = initialize();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Wi-Fi initialization failed: %s. Check C6 Hosted firmware and docs/WIFI.md.", esp_err_to_name(err));
        vTaskDelete(NULL); return;
    }
    bool server_started = false;
    for (;;) {
        EventBits_t bits = xEventGroupWaitBits(events, CONNECT_BIT | ONLINE_BIT, pdTRUE, pdFALSE, portMAX_DELAY);
        if (bits & ONLINE_BIT) {
            esp_err_t mqtt = mqtt_bridge_start();
            if (mqtt != ESP_OK) ESP_LOGW(TAG, "MQTT startup: %s", esp_err_to_name(mqtt));
            if (!server_started) {
                err = web_ui_start();
                server_started = err == ESP_OK;
                if (!server_started) {
                    ESP_LOGE(TAG, "Web server start: %s", esp_err_to_name(err));
                    vTaskDelay(pdMS_TO_TICKS(5000));
                    xEventGroupSetBits(events, ONLINE_BIT);
                }
            }
        } else if (bits & CONNECT_BIT) {
            vTaskDelay(pdMS_TO_TICKS(2000));
            err = esp_wifi_connect();
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "Connect: %s", esp_err_to_name(err));
                xEventGroupSetBits(events, CONNECT_BIT);
            }
        }
    }
}

esp_err_t network_start(void)
{
    size_t ssid_len = strlen(network_ssid()), pass_len = strlen(network_password());
    if (!ssid_len) {
        ESP_LOGW(TAG, "Set WIFI_SSID and WIFI_PASSWORD in include/secrets.h, then rebuild/upload for Wi-Fi");
        return ESP_OK;
    }
    if (ssid_len > 32 || pass_len > 63 || (pass_len && pass_len < 8)) return ESP_ERR_INVALID_ARG;
    if (events) return ESP_ERR_INVALID_STATE;
    events = xEventGroupCreate();
    if (!events) return ESP_ERR_NO_MEM;
    if (xTaskCreate(network_task, "wifi_setup", 6144, NULL, 3, NULL) != pdPASS) {
        vEventGroupDelete(events); events = NULL; return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
