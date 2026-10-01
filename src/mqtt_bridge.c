#include <stdbool.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "mqtt_bridge.h"
#include "menu.h"
#include "menu_model.h"
#include "network_credentials.h"
#include "mqtt_client.h"
#include "esp_log.h"
#include "esp_random.h"
static esp_mqtt_client_handle_t client;
static atomic_uint session;
static unsigned generation;
static char base[128], command_topic[160], status_topic[160], availability_topic[160];
static char incoming[512], topic[256];
static size_t received, expected;
static uint32_t boot_id, sequence;
static void event(void *arg,esp_event_base_t event_base,int32_t id,void *data)
{
    (void)arg;(void)event_base;
    esp_mqtt_event_handle_t e=data;
    if(id==MQTT_EVENT_CONNECTED) {
        ESP_LOGI("mqtt", "Connected to broker; subscribing to states");
        received=expected=0; menu_offline();
        snprintf(status_topic,sizeof(status_topic),"%s/state/+",base);
        esp_mqtt_client_subscribe(client,status_topic,0);
        esp_mqtt_client_publish(client,availability_topic,"online",0,0,true);
        if(!++generation) ++generation;
        atomic_store(&session,generation);
    } else if(id==MQTT_EVENT_DISCONNECTED) {
        ESP_LOGW("mqtt", "Disconnected from broker; automatic reconnect enabled");
        atomic_store(&session,0);received=expected=0;menu_offline();
    } else if(id==MQTT_EVENT_DATA) {
        if(e->current_data_offset==0) {
            received=expected=0;
            if(e->total_data_len<1||e->total_data_len>=(int)sizeof(incoming)||e->topic_len<1||e->topic_len>=(int)sizeof(topic)) return;
            expected=e->total_data_len;
            memcpy(topic,e->topic,e->topic_len);topic[e->topic_len]=0;
        }
        if(!expected||e->current_data_offset!=(int)received||e->data_len<0||received+(size_t)e->data_len>expected) {expected=0;return;}
        memcpy(incoming+received,e->data,e->data_len);received+=e->data_len;
        if(received!=expected) return;
        incoming[received]=0;expected=0;
        char prefix[160];snprintf(prefix,sizeof(prefix),"%s/state/",base);
        if(strncmp(topic,prefix,strlen(prefix))||memchr(incoming,0,received)) return;
        const char *sid=topic+strlen(prefix);
        // Only a shallow object is part of the protocol; cap parser recursion.
        unsigned depth=0; bool quoted=false, escaped=false;
        for(size_t i=0;i<received;i++) {
            char c=incoming[i];
            if(quoted) { if(escaped) { escaped=false; if(c=='u'&&i+4<received&&!memcmp(incoming+i+1,"0000",4)) return; } else if(c=='\\') escaped=true; else if(c=='"') quoted=false; }
            else if(c=='"') quoted=true;
            else if(c=='{'||c=='[') {if(++depth>4) return;}
            else if(c=='}'||c==']') {if(!depth) return;--depth;}
        }
        if(quoted||depth) return;
        cJSON *r=cJSON_ParseWithLengthOpts(incoming,received+1,NULL,true);
        if(r) {
            const cJSON *s=menu_get(r,"state"),*v=menu_get(r,"value");
            if(cJSON_IsString(s)&&(!v||cJSON_IsString(v)||cJSON_IsNumber(v)||cJSON_IsBool(v))) {
                char value[64]="";
                if(cJSON_IsString(v)) snprintf(value,sizeof(value),"%s",v->valuestring);
                else if(cJSON_IsNumber(v)) snprintf(value,sizeof(value),"%.6g",v->valuedouble);
                else if(cJSON_IsBool(v)) snprintf(value,sizeof(value),"%s",cJSON_IsTrue(v)?"true":"false");
                menu_state(sid,s->valuestring,value);
            }
            cJSON_Delete(r);
        }
    }
}
esp_err_t mqtt_bridge_start(void)
{
    if(client) return ESP_OK;
    if(!*network_mqtt_uri()) {ESP_LOGI("mqtt","Disabled: MQTT_URI is empty");return ESP_OK;}
    if(strlen(network_mqtt_prefix())>120||!*network_mqtt_prefix()||strpbrk(network_mqtt_prefix(),"+#")) return ESP_ERR_INVALID_ARG;
    snprintf(base,sizeof(base),"%s",network_mqtt_prefix());
    snprintf(command_topic,sizeof(command_topic),"%s/command",base);
    snprintf(availability_topic,sizeof(availability_topic),"%s/availability",base);
    esp_mqtt_client_config_t cfg={
        .broker.address.uri=network_mqtt_uri(),
        .credentials.username=network_mqtt_username(),
        .credentials.client_id=network_mqtt_client_id(),
        .credentials.authentication.password=network_mqtt_password(),
        .session.last_will.topic=availability_topic,
        .session.last_will.msg="offline",.session.last_will.qos=0,.session.last_will.retain=true,
        .network.timeout_ms=2000,.buffer.size=1024,
    };
    client=esp_mqtt_client_init(&cfg);if(!client) return ESP_ERR_NO_MEM;
    boot_id=esp_random();
    esp_err_t err=esp_mqtt_client_register_event(client,ESP_EVENT_ANY_ID,event,NULL);
    if(err==ESP_OK) err=esp_mqtt_client_start(client);
    if(err!=ESP_OK) {esp_mqtt_client_destroy(client);client=NULL;}
    return err;
}
uint32_t mqtt_bridge_session(void) { return atomic_load(&session); }
static bool publish_request(cJSON *r, uint32_t expected_session)
{
    if(!r) return false;
    if(!expected_session||atomic_load(&session)!=expected_session||!client) {cJSON_Delete(r);return false;}
    char id[32];snprintf(id,sizeof(id),"%08lx-%lu",(unsigned long)boot_id,(unsigned long)++sequence);
    if(!cJSON_AddStringToObject(r,"id",id)) {cJSON_Delete(r);return false;}
    char *s=cJSON_PrintUnformatted(r);cJSON_Delete(r);
    if(!s) return false;
    // QoS 0, no retain and no enqueue: offline commands are never replayed.
    int result=atomic_load(&session)==expected_session ? esp_mqtt_client_publish(client,command_topic,s,0,0,false) : -1;
    free(s);return result>=0;
}
bool mqtt_bridge_action(const cJSON *action, uint32_t expected_session)
{
    if(!menu_action_valid(action)) return false;
    cJSON *r=cJSON_Duplicate(action,true);if(!r) return false;
    if(!cJSON_AddNumberToObject(r,"v",1)) {cJSON_Delete(r);return false;}
    return publish_request(r,expected_session);
}
bool mqtt_bridge_command(const char *command, uint32_t expected_session)
{
    // Compatibility for previously stored menus. New menus use entity/action.
    cJSON *r=cJSON_CreateObject();if(!r) return false;
    if(!cJSON_AddStringToObject(r,"command",command)) {cJSON_Delete(r);return false;}
    return publish_request(r,expected_session);
}

void mqtt_bridge_refresh(void)
{
    if(client && atomic_load(&session)) esp_mqtt_client_subscribe(client,status_topic,0);
}
