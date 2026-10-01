#include "network.h"
#include "web_ui.h"
#include "mqtt_bridge.h"
#include <string.h>
#include <stdatomic.h>
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_attr.h"
#include "network_watchdog.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_hosted.h"
#include "esp_private/wifi.h"
#include "ping/ping_sock.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "network_credentials.h"

static EventGroupHandle_t events;
static esp_netif_t *station;
static atomic_bool online, associated, recovering, health_seen;
static atomic_uint last_health_s, recovery_request;
static atomic_bool ping_done, ping_ok;
static uint32_t monitored_gateway;
static bool gateway_seen, gateway_fallback_logged;
static RTC_NOINIT_ATTR uint32_t watchdog_marker;
#define WATCHDOG_MARKER 0x57494649u
#define CONNECT_BIT BIT0
#define ONLINE_BIT BIT1
#define RECOVERY_BIT BIT2
static uint64_t reboot_not_before;
static const char *TAG = "wifi";
static void request_recovery(network_recovery_t action)
{
    unsigned old=atomic_load(&recovery_request);
    while (old<(unsigned)action && !atomic_compare_exchange_weak(&recovery_request,&old,(unsigned)action)) {}
    xEventGroupSetBits(events,RECOVERY_BIT);
}
esp_err_t network_recover(bool reset_c6)
{
    if (!events) return ESP_ERR_INVALID_STATE;
    request_recovery(reset_c6 ? NETWORK_RECOVERY_C6 : NETWORK_RECOVERY_RECONNECT);
    return ESP_OK;
}
static void watchdog_task(void *arg)
{
    (void)arg;
    network_watchdog_t policy={0};
    for (;;) {
        uint64_t now=esp_timer_get_time()/1000000;
        bool healthy=network_health_fresh(atomic_load(&online),atomic_load(&health_seen),
                                          (uint32_t)now,atomic_load(&last_health_s));
        network_recovery_t action=network_watchdog_step(&policy,now,reboot_not_before,healthy);
        if (action==NETWORK_RECOVERY_REBOOT) {
            ESP_LOGE(TAG,"Watchdog stage 3: WLAN/C6 unhealthy for 180 s; rebooting P4");
            watchdog_marker=WATCHDOG_MARKER;
            esp_restart();
        } else if (action!=NETWORK_RECOVERY_NONE) {
            ESP_LOGW(TAG,"Watchdog stage %u requested",(unsigned)action);
            request_recovery(action);
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
static void ping_success(esp_ping_handle_t handle, void *arg)
{
    (void)handle; (void)arg; atomic_store(&ping_ok,true);
}
static void ping_end(esp_ping_handle_t handle, void *arg)
{
    (void)handle; (void)arg; atomic_store(&ping_done,true);
}
// This task alone issues RPC/recovery calls. If it blocks, the supervisor still reboots.
static bool health_probe(void)
{
    wifi_ap_record_t ap;
    if (esp_wifi_sta_get_ap_info(&ap)!=ESP_OK) return false;
    esp_netif_ip_info_t ip;
    if (esp_netif_get_ip_info(station,&ip)!=ESP_OK || !ip.ip.addr) return false;
    if (!ip.gw.addr) return true; // No gateway configured: RPC/link checks only.
    if (monitored_gateway!=ip.gw.addr) {
        monitored_gateway=ip.gw.addr; gateway_seen=false; gateway_fallback_logged=false;
    }
    esp_ping_config_t cfg=ESP_PING_DEFAULT_CONFIG();
    cfg.count=1; cfg.interval_ms=100; cfg.timeout_ms=1000;
    cfg.interface=esp_netif_get_netif_impl_index(station);
    cfg.target_addr.type=IPADDR_TYPE_V4; cfg.target_addr.u_addr.ip4.addr=ip.gw.addr;
    esp_ping_callbacks_t callbacks={.on_ping_success=ping_success,.on_ping_end=ping_end};
    esp_ping_handle_t ping=NULL;
    atomic_store(&ping_done,false); atomic_store(&ping_ok,false);
    if (esp_ping_new_session(&cfg,&callbacks,&ping)!=ESP_OK) return false;
    if (esp_ping_start(ping)!=ESP_OK) { esp_ping_delete_session(ping); return false; }
    while (!atomic_load(&ping_done)) vTaskDelay(pdMS_TO_TICKS(50));
    bool replied=atomic_load(&ping_ok);
    esp_ping_delete_session(ping);
    if (replied && !gateway_seen) {
        gateway_seen=true; ESP_LOGI(TAG,"Watchdog: router ping confirmed; data-path monitoring active");
    }
    if (!replied && !gateway_seen && !gateway_fallback_logged) {
        gateway_fallback_logged=true;
        ESP_LOGW(TAG,"Router has not answered ICMP; using C6/link probe until first reply");
    }
    return replied || !gateway_seen;
}

static void event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    if (atomic_load(&recovering)) return;
    if (base == WIFI_EVENT && (id == WIFI_EVENT_STA_START || id == WIFI_EVENT_STA_DISCONNECTED)) {
        atomic_store(&online, false);
        atomic_store(&associated, false);
        atomic_store(&health_seen, false);
        xEventGroupClearBits(events, ONLINE_BIT);
        if (id == WIFI_EVENT_STA_DISCONNECTED) {
            const wifi_event_sta_disconnected_t *lost = data;
            ESP_LOGW(TAG, "Disconnected; reason=%u; retrying", lost ? lost->reason : 0);
        }
        xEventGroupSetBits(events, CONNECT_BIT);
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_CONNECTED) {
        atomic_store(&associated, true);
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *got = data;
        atomic_store(&online, true);
        xEventGroupClearBits(events, CONNECT_BIT);
        ESP_LOGI(TAG, "Open http://" IPSTR "/ in your browser", IP2STR(&got->ip_info.ip));
        xEventGroupSetBits(events, ONLINE_BIT);
    } else if (base == IP_EVENT && id == IP_EVENT_STA_LOST_IP) {
        atomic_store(&online, false);
        atomic_store(&health_seen, false);
        xEventGroupClearBits(events, ONLINE_BIT);
        xEventGroupSetBits(events, CONNECT_BIT);
        ESP_LOGW(TAG, "IP address lost; recovery pending");
    }
}

static esp_err_t initialize_infrastructure(void)
{
    esp_err_t err = nvs_flash_init();
    // Never erase NVS implicitly: it can contain settings belonging to the user.
    if (err != ESP_OK) return err;
    if ((err = esp_netif_init()) != ESP_OK) return err;
    if ((err = esp_event_loop_create_default()) != ESP_OK) return err;
    station = esp_netif_create_default_wifi_sta();
    if (!station) return ESP_ERR_NO_MEM;
    esp_netif_set_hostname(station, "streamdeck-mini");
    if ((err = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, event, NULL)) != ESP_OK) return err;
    if ((err = esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, event, NULL)) != ESP_OK) return err;
    return ESP_OK;
}
static esp_err_t start_wifi(void)
{
    esp_err_t err;
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
    if ((err = esp_wifi_start()) != ESP_OK) return err;
    // Mains-powered control panel: avoid modem-sleep latency; C6 RPC may reject it.
    err = esp_wifi_set_ps(WIFI_PS_NONE);
    if (err != ESP_OK) ESP_LOGW(TAG, "Disable power save: %s", esp_err_to_name(err));
    return ESP_OK;
}

static esp_err_t reset_transport(void)
{
    ESP_LOGW(TAG,"Recovery stage 2: reinitializing C6 and SDIO");
    atomic_store(&recovering,true);
    atomic_store(&online,false); atomic_store(&associated,false); atomic_store(&health_seen,false);
    xEventGroupClearBits(events,CONNECT_BIT|ONLINE_BIT);
    // Netif must be stopped even if the stalled C6 cannot emit disconnect events.
    // Ordering follows Espressif's host_shuts_down_slave_to_power_save example.
    (void)esp_wifi_disconnect();
    wifi_event_sta_disconnected_t lost={.reason=WIFI_REASON_ASSOC_LEAVE};
    (void)esp_event_post(WIFI_EVENT,WIFI_EVENT_STA_DISCONNECTED,&lost,sizeof(lost),portMAX_DELAY);
    (void)esp_event_post(WIFI_EVENT,WIFI_EVENT_STA_STOP,NULL,0,portMAX_DELAY);
    vTaskDelay(pdMS_TO_TICKS(500));
    (void)esp_wifi_internal_reg_rxcb(WIFI_IF_STA,NULL);
    (void)esp_wifi_internal_reg_rxcb(WIFI_IF_AP,NULL);
    (void)esp_wifi_stop();
    (void)esp_wifi_deinit();
    esp_err_t err=esp_hosted_deinit();
    if (err==ESP_OK) err=esp_hosted_init();
    if (err==ESP_OK) err=esp_hosted_connect_to_slave(); // Pulses C6 reset GPIO 54.
    atomic_store(&recovering,false);
    if (err==ESP_OK) err=start_wifi();
    ESP_LOGI(TAG,"C6/SDIO reinitialization: %s",esp_err_to_name(err));
    return err;
}
static void network_task(void *arg)
{
    (void)arg;
    esp_err_t err=initialize_infrastructure();
    if (err!=ESP_OK) {
        ESP_LOGE(TAG,"Network infrastructure: %s; watchdog remains active",esp_err_to_name(err));
        vTaskDelete(NULL); return;
    }
    err=start_wifi();
    if (err!=ESP_OK) ESP_LOGE(TAG,"Wi-Fi initialization: %s; recovery pending",esp_err_to_name(err));
    esp_hosted_coprocessor_fwver_t coprocessor={0};
    if (esp_hosted_get_coprocessor_fwversion(&coprocessor)==ESP_OK)
        ESP_LOGI(TAG,"C6 firmware version: %u.%u.%u",(unsigned)coprocessor.major1,
                 (unsigned)coprocessor.minor1,(unsigned)coprocessor.patch1);
    bool server_started=false;
    uint64_t next_probe=0, next_connect=0;
    for (;;) {
        (void)xEventGroupWaitBits(events,CONNECT_BIT|ONLINE_BIT|RECOVERY_BIT,pdTRUE,pdFALSE,pdMS_TO_TICKS(2000));
        unsigned action=atomic_exchange(&recovery_request,NETWORK_RECOVERY_NONE);
        if (action==NETWORK_RECOVERY_C6) {
            (void)reset_transport(); next_connect=0; next_probe=0;
        } else if (action==NETWORK_RECOVERY_RECONNECT) {
            ESP_LOGW(TAG,"Recovery stage 1: disconnect and reconnect WLAN");
            atomic_store(&online,false); atomic_store(&health_seen,false);
            (void)esp_wifi_disconnect();
            vTaskDelay(pdMS_TO_TICKS(500)); // Allow the asynchronous disconnect event to settle.
            atomic_store(&associated,false);
            next_connect=0;
        }
        uint64_t now=esp_timer_get_time()/1000000;
        if (atomic_load(&online)) {
            esp_err_t mqtt=mqtt_bridge_start();
            if (mqtt!=ESP_OK) ESP_LOGW(TAG,"MQTT startup: %s",esp_err_to_name(mqtt));
            if (!server_started) {
                err=web_ui_start(); server_started=err==ESP_OK;
                if (!server_started) ESP_LOGE(TAG,"Web server start: %s",esp_err_to_name(err));
            }
            if (now>=next_probe) {
                bool good=health_probe();
                if (good && atomic_load(&online)) {
                    atomic_store(&last_health_s,(uint32_t)(esp_timer_get_time()/1000000));
                    atomic_store(&health_seen,true);
                } else ESP_LOGW(TAG,"Health probe failed (C6/link/router), MQTT is not part of this check");
                next_probe=esp_timer_get_time()/1000000+10;
            }
        } else if (!atomic_load(&associated) && now>=next_connect) {
            err=esp_wifi_connect();
            if (err!=ESP_OK) ESP_LOGW(TAG,"Connect: %s",esp_err_to_name(err));
            next_connect=esp_timer_get_time()/1000000+10;
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
    bool previous_watchdog = esp_reset_reason() == ESP_RST_SW && watchdog_marker == WATCHDOG_MARKER;
    watchdog_marker = 0;
    reboot_not_before = previous_watchdog ? NETWORK_REBOOT_GUARD_S : 0;
    ESP_LOGI(TAG, "Network watchdog: reconnect 30 s, C6 reset 90 s, reboot 180 s; guard %s; reset reason=%d",
             previous_watchdog ? "600 s after watchdog reboot" : "not active", (int)esp_reset_reason());
    TaskHandle_t watchdog = NULL;
    if (xTaskCreate(watchdog_task, "wifi_watchdog", 3072, NULL, 2, &watchdog) != pdPASS) {
        vEventGroupDelete(events); events = NULL; return ESP_ERR_NO_MEM;
    }
    if (xTaskCreate(network_task, "wifi_setup", 6144, NULL, 3, NULL) != pdPASS) {
        vTaskDelete(watchdog);
        vEventGroupDelete(events); events = NULL; return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
