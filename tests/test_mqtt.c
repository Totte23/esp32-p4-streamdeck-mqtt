#include <assert.h>
#include "../src/mqtt_bridge.c"
static unsigned publishes, subscriptions, offline_calls, state_calls;
static char last_payload[512],last_id[64],last_style[64],last_value[64];
const char *network_mqtt_uri(void){return "mqtt://test.invalid";}
const char *network_mqtt_username(void){return "";}
const char *network_mqtt_password(void){return "";}
const char *network_mqtt_client_id(void){return "test-mini";}
const char *network_mqtt_prefix(void){return "streamdeck/mini";}
uint32_t esp_random(void){return 42;}
esp_mqtt_client_handle_t esp_mqtt_client_init(const esp_mqtt_client_config_t *c)
{assert(!strcmp(c->session.last_will.msg,"offline")&&c->session.last_will.retain);return (void *)1;}
esp_err_t esp_mqtt_client_register_event(esp_mqtt_client_handle_t c,int id,void (*f)(void *,esp_event_base_t,int32_t,void *),void *ctx)
{(void)c;(void)id;(void)f;(void)ctx;return ESP_OK;}
esp_err_t esp_mqtt_client_start(esp_mqtt_client_handle_t c){(void)c;return ESP_OK;}
esp_err_t esp_mqtt_client_destroy(esp_mqtt_client_handle_t c){(void)c;return ESP_OK;}
int esp_mqtt_client_subscribe(esp_mqtt_client_handle_t c,const char *t,int qos)
{(void)c;assert(!strcmp(t,"streamdeck/mini/state/+")&&qos==0);++subscriptions;return 0;}
int esp_mqtt_client_publish(esp_mqtt_client_handle_t c,const char *t,const char *p,int n,int qos,int retain)
{(void)c;(void)n;assert(qos==0);if(strstr(t,"/command"))assert(!retain);else assert(retain);snprintf(last_payload,sizeof(last_payload),"%s",p);++publishes;return 0;}
void menu_offline(void){++offline_calls;}
void menu_state(const char *id,const char *style,const char *value)
{++state_calls;snprintf(last_id,sizeof(last_id),"%s",id);snprintf(last_style,sizeof(last_style),"%s",style);snprintf(last_value,sizeof(last_value),"%s",value);}
static void data(const char *payload,int split)
{
 esp_mqtt_event_t e={.topic="streamdeck/mini/state/flur",.topic_len=25,.data=(char *)payload,.data_len=(int)strlen(payload),.total_data_len=(int)strlen(payload)};
 e.topic_len=strlen(e.topic);
 if(split>0)e.data_len=split;
 event(NULL,NULL,MQTT_EVENT_DATA,&e);
 if(split>0){e.current_data_offset=split;e.data=(char *)payload+split;e.data_len=e.total_data_len-split;e.topic=NULL;e.topic_len=0;event(NULL,NULL,MQTT_EVENT_DATA,&e);}
}
int main(void)
{
 assert(!mqtt_bridge_command("flur.toggle",mqtt_bridge_session()));assert(publishes==0);
 assert(mqtt_bridge_start()==ESP_OK);assert(!mqtt_bridge_command("flur.toggle",mqtt_bridge_session()));
 event(NULL,NULL,MQTT_EVENT_CONNECTED,NULL);assert(offline_calls==1&&subscriptions==1);
 assert(mqtt_bridge_command("flur.toggle",mqtt_bridge_session()));cJSON *r=cJSON_Parse(last_payload);assert(!strcmp(menu_string(r,"command",""),"flur.toggle"));assert(!strcmp(menu_string(r,"id",""),"0000002a-1"));cJSON_Delete(r);
 cJSON *action=cJSON_Parse("{\"entity\":\"licht.flur\",\"action\":\"set\",\"value\":false}");
 assert(mqtt_bridge_action(action,mqtt_bridge_session()));r=cJSON_Parse(last_payload);
 assert(menu_get(r,"v")->valueint==1&&!strcmp(menu_string(r,"entity",""),"licht.flur"));
 assert(cJSON_IsFalse(menu_get(r,"value"))&&!menu_get(r,"command"));cJSON_Delete(r);
 cJSON_AddStringToObject(action,"extra","bad");unsigned count=publishes;assert(!mqtt_bridge_action(action,mqtt_bridge_session())&&publishes==count);cJSON_Delete(action);
 data("{\"state\":\"on\",\"value\":45}",8);assert(state_calls==1&&!strcmp(last_style,"on")&&!strcmp(last_value,"45")&&!strcmp(last_id,"flur"));
 data("{\"state\":\"off\",\"value\":\"Aus\"}",0);assert(state_calls==2&&!strcmp(last_value,"Aus"));
 data("{\"state\":\"on\"} trailing",0);assert(state_calls==2);
 data("{\"state\":\"on\",\"value\":{}}",0);assert(state_calls==2);
 data("[[[[[{}]]]]]",0);assert(state_calls==2);
 data("{\"state\":\"on\\u0000bad\"}",0);assert(state_calls==2);
 mqtt_bridge_refresh();assert(subscriptions==2);
 uint32_t old_session=mqtt_bridge_session();unsigned before=publishes;event(NULL,NULL,MQTT_EVENT_DISCONNECTED,NULL);assert(offline_calls==2);
 assert(!mqtt_bridge_command("flur.toggle",mqtt_bridge_session())&&publishes==before);mqtt_bridge_refresh();assert(subscriptions==2);
 event(NULL,NULL,MQTT_EVENT_CONNECTED,NULL);assert(publishes==before+1); // availability only, no replay
 assert(!mqtt_bridge_command("flur.toggle",old_session));assert(publishes==before+1);
 puts("PASS: MQTT command format, QoS0/nonretained, offline drop/no replay, fragmented state, malformed payloads, resubscribe");
}
