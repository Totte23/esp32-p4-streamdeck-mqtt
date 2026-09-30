#include "menu.h"
#include "menu_model.h"
#include "icon_store.h"
#include "tile_render.h"
#include "deck_usb.h"
#include "mqtt_bridge.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <stdlib.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#ifndef MENU_STORE_BASE
#define MENU_STORE_BASE "/icons"
#endif
extern const uint8_t default_menu_start[] __asm__("_binary_menu_json_start");
extern const uint8_t default_menu_end[] __asm__("_binary_menu_json_end");
static SemaphoreHandle_t lock;
static QueueHandle_t key_queue;
typedef struct { uint8_t key; uint32_t configuration, connection, gesture; bool pressed; int64_t time_us; } key_message_t;
static atomic_bool back_down;
static atomic_uint gesture_generation;
static bool back_pending, back_long;
static key_message_t back_press;
#define HOME_HOLD_US (2LL * 1000000LL)
static atomic_uint configuration_generation = 1;
static cJSON *config;
static char current[64];
static int64_t last_activity_us; // protected by lock
#define MENU_IDLE_US (180LL * 1000000LL)
static uint8_t rgba[TILE_RGBA_BYTES]; // one scratch image, protected by lock
static struct { char id[64], style[64], value[64]; } states[MENU_MAX_STATES];
static unsigned state_count;
static const cJSON *page(void) { return menu_get(menu_get(config,"pages"),current); }
static cJSON *parse(const char *s,size_t n,char *error,size_t cap)
{
    // Bound parser recursion before cJSON allocates; reject embedded NUL strings.
    unsigned depth=0; bool quote=false,escape=false;
    if(!n||n>MENU_MAX_BYTES||memchr(s,0,n)) goto bad;
    for(size_t i=0;i<n;i++) {
        char c=s[i];
        if(quote) {
            if(escape) { escape=false; if(c=='u'&&i+4<n&&!memcmp(s+i+1,"0000",4)) goto bad; }
            else if(c=='\\') escape=true;
            else if(c=='"') quote=false;
        } else if(c=='"') quote=true;
        else if(c=='{'||c=='[') { if(++depth>12) goto bad; }
        else if(c=='}'||c==']') { if(!depth) goto bad; --depth; }
    }
    if(quote||depth) goto bad;
    const char *end=NULL;
    cJSON *r=cJSON_ParseWithLengthOpts(s,n,&end,false);
    if(!r) goto bad;
    while(end<s+n && (*end==' '||*end=='\n'||*end=='\r'||*end=='\t')) ++end;
    if(end!=s+n) { cJSON_Delete(r); goto bad; }
    if(!menu_validate(r,error,cap)) { cJSON_Delete(r);return NULL; }
    return r;
bad:
    snprintf(error,cap,"Ungültiges JSON (max. 64 KiB, Tiefe 12).");return NULL;
}
static bool assets(const cJSON *r,char *error,size_t cap)
{
    const cJSON *pages=menu_get(r,"pages");
    for(const cJSON *p=pages->child;p;p=p->next) for(unsigned k=0;k<6;k++) {
        const char *icon=menu_string(menu_button(p,k),"icon","");
        if(!*icon) continue;
        char name[33]; menu_icon_name(icon,name);
        if(icon_store_rgba(name,rgba)!=ESP_OK) { snprintf(error,cap,"Icon fehlt oder ist nicht lesbar: %s",name);return false; }
    }
    return true;
}
esp_err_t menu_upload(const char *json,size_t length,char *error,size_t cap)
{
    if(!lock) return ESP_ERR_INVALID_STATE;
    cJSON *r=parse(json,length,error,cap);
    if(!r) return ESP_ERR_INVALID_ARG;
    xSemaphoreTake(lock,portMAX_DELAY);
    esp_err_t err=ESP_FAIL;
    if(!assets(r,error,cap)) err=ESP_ERR_INVALID_ARG;
    else {
        FILE *f=fopen(MENU_STORE_BASE "/.menu.tmp","wb");
        bool ok=false;
        if(f) {
            ok=fwrite(json,1,length,f)==length;
            if(fflush(f)||fsync(fileno(f))) ok=false;
            if(fclose(f)) ok=false;
        }
        if(ok && !rename(MENU_STORE_BASE "/.menu.tmp",MENU_STORE_BASE "/menu.json")) {
            cJSON_Delete(config);config=r;r=NULL;
            atomic_fetch_add(&configuration_generation,1);
            snprintf(current,sizeof(current),"%s",menu_string(config,"startPage",""));
            memset(states,0,sizeof(states)); state_count=0;
            err=ESP_OK;
        } else { unlink(MENU_STORE_BASE "/.menu.tmp");snprintf(error,cap,"Speichern fehlgeschlagen; bisheriges Menü bleibt aktiv."); }
    }
    xSemaphoreGive(lock);cJSON_Delete(r);
    if(err==ESP_OK) { deck_usb_refresh_images(); mqtt_bridge_refresh(); }
    return err;
}
char *menu_export(void)
{
    if(!lock) return NULL;
    xSemaphoreTake(lock,portMAX_DELAY);char *s=cJSON_Print(config);xSemaphoreGive(lock);return s;
}
bool menu_uses_icon(const char *name)
{
    if(!lock) return false;
    xSemaphoreTake(lock,portMAX_DELAY);bool found=false;
    const cJSON *pages=menu_get(config,"pages");
    for(const cJSON *p=pages?pages->child:NULL;p;p=p->next) for(unsigned k=0;k<6;k++) {
        char id[33];menu_icon_name(menu_string(menu_button(p,k),"icon",""),id);
        if(!strcmp(id,name)) found=true;
    }
    xSemaphoreGive(lock);return found;
}
static uint32_t color(const cJSON *o,const char *key,uint32_t fallback)
{ const char *s=menu_string(o,key,"");return *s ? strtoul(s+1,NULL,16) : fallback; }
static void expand(const char *s,const char *value,char out[128])
{
    size_t n=0;
    while(*s&&n<127) {
        if(!strncmp(s,"{value}",7)) { size_t len=strlen(value); if(len>127-n) len=127-n;memcpy(out+n,value,len);n+=len;s+=7; }
        else out[n++]=*s++;
    }
    out[n]=0;
}
bool menu_image(uint8_t key,uint8_t *bmp,void *context)
{
    (void)context;
    if(!lock||key>=6) return false;
    xSemaphoreTake(lock,portMAX_DELAY);
    if(!config) {xSemaphoreGive(lock);return false;}
    const cJSON *p=page(),*b=menu_button(p,key);
    if(key==0||key==3) {
        tile_background(bmp,0x243447);tile_arrow(bmp,key==0);
        if(key==0) {
            // Page title above the arrow; split at a word boundary into two lines.
            char title[64];snprintf(title,sizeof(title),"%s",menu_string(p,"title",""));
            char *second=strchr(title,' ');
            if(second) *second++=0;
            tile_text(bmp,title,3,1,0x7dd3fc);
            if(second) tile_text(bmp,second,13,1,0x7dd3fc);
        } else tile_text(bmp,"2s: Home",3,1,0x7dd3fc);
        tile_text(bmp,key==0?"Weiter":"Zurück",63,1,0xffffff);
    } else {
        const char *sid=menu_string(b,"state",""),*style="unknown",*value="?";
        for(unsigned i=0;i<state_count;i++) if(!strcmp(states[i].id,sid)) {style=states[i].style;value=states[i].value;break;}
        const cJSON *ap=menu_get(menu_get(b,"appearance"),style);
        uint32_t bg=color(ap,"background",color(b,"background",0x18222f));
        uint32_t fg=color(ap,"textColor",color(b,"textColor",0xffffff));
        tile_background(bmp,bg);
        const char *icon=menu_string(b,"icon","");char name[33];menu_icon_name(icon,name);
        const cJSON *size=menu_get(b,"iconSize"),*font=menu_get(b,"fontSize");
        const cJSON *top=menu_get(b,"iconY");
        unsigned icon_size=size?size->valueint:48;
        if(*name&&icon_store_rgba(name,rgba)==ESP_OK) {
            unsigned icon_y=top?top->valueint:(80-icon_size)/2;
            if(menu_get(ap,"iconBackground"))
                tile_rect(bmp,(80-(int)icon_size)/2-1,(int)icon_y-1,icon_size+2,icon_size+2,color(ap,"iconBackground",0x18222f));
            tile_rgba_at(bmp,rgba,icon_size,icon_y);
        }
        const cJSON *labels=menu_get(b,"labels");
        const char *positions[]={"top","center","bottom"};
        unsigned scale=font?font->valueint/8:1;
        int ys[]={3,40-(int)scale*4,75-(int)scale*8};
        for(unsigned i=0;i<3;i++) {
            const char *s=menu_string(labels,positions[i],"");
            if(i==1) s=menu_string(ap,"center",s);
            char text[128];expand(s,value,text);tile_text(bmp,text,ys[i],scale,fg);
        }
    }
    xSemaphoreGive(lock);return true;
}
void menu_state(const char *id,const char *style,const char *value)
{
    if(!lock||!*id||strlen(id)>63||strlen(style)>63||strlen(value)>63) return;
    xSemaphoreTake(lock,portMAX_DELAY);
    // Only configured state IDs enter the cache; arbitrary broker topics cannot fill it.
    bool used=false;const cJSON *pages=menu_get(config,"pages");
    for(const cJSON *p=pages?pages->child:NULL;p;p=p->next) for(unsigned k=0;k<6;k++)
        if(!strcmp(menu_string(menu_button(p,k),"state",""),id)) used=true;
    bool changed=false;
    if(used) {
        unsigned i=0;for(;i<state_count;i++) if(!strcmp(states[i].id,id)) break;
        if(i<MENU_MAX_STATES) {
            if(i==state_count) state_count++;
            changed=strcmp(states[i].style,style)||strcmp(states[i].value,value);
            strcpy(states[i].id,id);strcpy(states[i].style,style);strcpy(states[i].value,value);
        }
    }
    xSemaphoreGive(lock);if(changed) deck_usb_refresh_images();
}
void menu_offline(void)
{
    if(!lock) return;
    xSemaphoreTake(lock,portMAX_DELAY);state_count=0;memset(states,0,sizeof(states));xSemaphoreGive(lock);
    deck_usb_refresh_images();
}
static void handle_key(uint8_t key,uint32_t configuration,uint32_t connection)
{
    char command[64]="";bool changed=false;cJSON *request=NULL;
    xSemaphoreTake(lock,portMAX_DELAY);
    if(configuration!=atomic_load(&configuration_generation)) {xSemaphoreGive(lock);return;}
    last_activity_us=esp_timer_get_time();
    const cJSON *p=page(),*action=menu_get(menu_button(p,key),"onPress");
    const char *target=key==0?menu_string(p,"next",""):key==3?menu_string(p,"previous",""):menu_string(action,"page","");
    if(*target) {snprintf(current,sizeof(current),"%s",target);changed=true;}
    else if(menu_get(action,"entity")) request=cJSON_Duplicate(action,true);
    else snprintf(command,sizeof(command),"%s",menu_string(action,"command",""));
    xSemaphoreGive(lock);
    if(changed) deck_usb_refresh_images();
    if(request) {
        if(!mqtt_bridge_action(request,connection)) ESP_LOGW("menu","Action not sent: %s/%s",menu_string(request,"entity",""),menu_string(request,"action",""));
        cJSON_Delete(request);
    }
    if(*command&&!mqtt_bridge_command(command,connection)) ESP_LOGW("menu","Command not sent (MQTT offline/busy): %s",command);
}
static void return_home_if_idle(void)
{
    bool changed=false;
    xSemaphoreTake(lock,portMAX_DELAY);
    const char *home=menu_string(config,"startPage","");
    if(esp_timer_get_time()-last_activity_us>=MENU_IDLE_US && strcmp(current,home)) {
        snprintf(current,sizeof(current),"%s",home);
        changed=true;
    }
    xSemaphoreGive(lock);
    if(changed) deck_usb_refresh_images();
}
static void hold_home(void)
{
    bool changed=false;
    xSemaphoreTake(lock,portMAX_DELAY);
    if(back_press.configuration==atomic_load(&configuration_generation)) {
        snprintf(current,sizeof(current),"%s",menu_string(config,"startPage",""));
        last_activity_us=esp_timer_get_time();changed=true;
    }
    xSemaphoreGive(lock);
    back_long=true;
    if(changed) deck_usb_refresh_images();
}
static void handle_key_event(key_message_t event)
{
    if(back_pending && back_press.gesture!=atomic_load(&gesture_generation)) back_pending=false;
    if(event.key!=3) {handle_key(event.key,event.configuration,event.connection);return;}
    if(event.gesture!=atomic_load(&gesture_generation)) {back_pending=false;return;}
    if(event.pressed) {
        if(back_pending) return;
        back_press=event;back_pending=true;back_long=false;
        xSemaphoreTake(lock,portMAX_DELAY);
        if(event.configuration==atomic_load(&configuration_generation)) last_activity_us=event.time_us;
        xSemaphoreGive(lock);
    } else if(back_pending) {
        if(!back_long) {
            if(event.time_us-back_press.time_us>=HOME_HOLD_US) hold_home();
            else handle_key(3,back_press.configuration,back_press.connection);
        }
        back_pending=false;
    }
}
static void check_home_hold(void)
{
    if(back_pending && back_press.gesture!=atomic_load(&gesture_generation)) back_pending=false;
    if(back_pending&&!back_long&&atomic_load(&back_down)&&esp_timer_get_time()-back_press.time_us>=HOME_HOLD_US)
        hold_home();
}
static void worker(void *arg)
{
    (void)arg;key_message_t event;
    for(;;) {
        if(xQueueReceive(key_queue,&event,pdMS_TO_TICKS(250))==pdTRUE)
            handle_key_event(event);
        check_home_hold();
        return_home_if_idle();
    }
}

void menu_key(uint8_t key,bool pressed,void *context)
{
    (void)context;
    if(key==DECK_KEYS_CANCEL) {
        atomic_store(&back_down,false);atomic_fetch_add(&gesture_generation,1);return;
    }
    if(key>=6||!key_queue) return;
    if(key==3) atomic_store(&back_down,pressed);
    // Only Back needs release events. Other buttons still act immediately on DOWN.
    if(!pressed&&key!=3) return;
    key_message_t event={.key=key,.configuration=atomic_load(&configuration_generation),
        .connection=mqtt_bridge_session(),.gesture=atomic_load(&gesture_generation),
        .pressed=pressed,.time_us=esp_timer_get_time()};
    if(xQueueSend(key_queue,&event,0)!=pdTRUE) {
        if(key==3) {atomic_store(&back_down,false);atomic_fetch_add(&gesture_generation,1);}
        ESP_LOGW("menu","Key queue full; ignored");
    }
}
esp_err_t menu_init(void)
{
    lock=xSemaphoreCreateMutex();key_queue=xQueueCreate(16,sizeof(key_message_t));
    if(!lock||!key_queue) return ESP_ERR_NO_MEM;
    char error[128];FILE *f=fopen(MENU_STORE_BASE "/menu.json","rb");
    if(f) {
        fseek(f,0,SEEK_END);long n=ftell(f);rewind(f);
        if(n>0&&n<=MENU_MAX_BYTES) {
            char *s=malloc(n);
            if(s) {if(fread(s,1,n,f)==(size_t)n) config=parse(s,n,error,sizeof(error));free(s);}
        }
        fclose(f);
        if(!config) ESP_LOGW("menu","Saved menu invalid; using built-in menu without overwriting saved file");
    }
    if(!config) config=parse((const char *)default_menu_start,default_menu_end-default_menu_start,error,sizeof(error));
    if(!config) return ESP_FAIL;
    snprintf(current,sizeof(current),"%s",menu_string(config,"startPage",""));
    return xTaskCreate(worker,"menu",6144,NULL,4,NULL)==pdPASS?ESP_OK:ESP_ERR_NO_MEM;
}
