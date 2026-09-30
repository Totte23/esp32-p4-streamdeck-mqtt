#pragma once
// Copy to secrets.h. Empty MQTT_URI disables MQTT until ioBroker is configured.
// Use a 2.4-GHz WLAN; password: 8-63 characters (empty only for open WLAN).
#define WIFI_SSID ""
#define WIFI_PASSWORD ""
#define MQTT_URI "" // Example: "mqtt://192.168.1.10:1883"
#define MQTT_USERNAME ""
#define MQTT_PASSWORD ""
#define MQTT_CLIENT_ID "esp32-p4-streamdeck"
#define MQTT_TOPIC_PREFIX "streamdeck/mini"
