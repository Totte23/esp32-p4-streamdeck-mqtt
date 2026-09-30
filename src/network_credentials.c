#include "network_credentials.h"
#if __has_include("secrets.h")
#include "secrets.h"
#else
#include "secrets.example.h"
#endif
// Separate translation unit keeps the complete networking and MQTT implementations in
// builds even with empty credentials, so linker validation is not skipped.
const char *network_ssid(void) { return WIFI_SSID; }
const char *network_password(void) { return WIFI_PASSWORD; }
const char *network_mqtt_uri(void) { return MQTT_URI; }
const char *network_mqtt_username(void) { return MQTT_USERNAME; }
const char *network_mqtt_password(void) { return MQTT_PASSWORD; }
const char *network_mqtt_client_id(void) { return MQTT_CLIENT_ID; }
const char *network_mqtt_prefix(void) { return MQTT_TOPIC_PREFIX; }
