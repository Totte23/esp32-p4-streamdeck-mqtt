#pragma once
#include "fake_idf.h"
typedef const char *esp_event_base_t;
#define ESP_EVENT_ANY_ID -1
enum {MQTT_EVENT_CONNECTED=1,MQTT_EVENT_DISCONNECTED,MQTT_EVENT_DATA};
typedef void *esp_mqtt_client_handle_t;
typedef struct {int current_data_offset,total_data_len,topic_len,data_len;char *topic,*data;} esp_mqtt_event_t;
typedef esp_mqtt_event_t *esp_mqtt_event_handle_t;
typedef struct {
 struct {struct {const char *uri;} address;} broker;
 struct {const char *username,*client_id;struct {const char *password;} authentication;} credentials;
 struct {struct {const char *topic,*msg;int qos;bool retain;} last_will;} session;
 struct {unsigned timeout_ms;} network;
 struct {unsigned size;} buffer;
} esp_mqtt_client_config_t;
esp_mqtt_client_handle_t esp_mqtt_client_init(const esp_mqtt_client_config_t *);
esp_err_t esp_mqtt_client_register_event(esp_mqtt_client_handle_t,int,void (*)(void *,esp_event_base_t,int32_t,void *),void *);
esp_err_t esp_mqtt_client_start(esp_mqtt_client_handle_t);
esp_err_t esp_mqtt_client_destroy(esp_mqtt_client_handle_t);
int esp_mqtt_client_subscribe(esp_mqtt_client_handle_t,const char *,int);
int esp_mqtt_client_publish(esp_mqtt_client_handle_t,const char *,const char *,int,int,int);
