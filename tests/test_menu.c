#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
static bool fail_save;
static int64_t now_us;
int64_t esp_timer_get_time(void) {return now_us;}
static int test_rename(const char *, const char *);
#define rename test_rename
#include "../src/menu.c"
#undef rename
static int test_rename(const char *a,const char *b) {return fail_save ? -1 : rename(a,b);}
const uint8_t default_menu_start[]={0},default_menu_end[]={0};
static unsigned refreshed, queued;
SemaphoreHandle_t xSemaphoreCreateMutex(void) { return (void *)1; }
int xSemaphoreTake(SemaphoreHandle_t m,unsigned t) {(void)m;(void)t;return 1;}
int xSemaphoreGive(SemaphoreHandle_t m) {(void)m;return 1;}
QueueHandle_t xQueueCreate(unsigned n,unsigned s) {(void)n;(void)s;return (void *)1;}
int xQueueSend(QueueHandle_t q,const void *v,unsigned t) {(void)q;(void)v;(void)t;++queued;return pdTRUE;}
int xQueueReceive(QueueHandle_t q,void *v,unsigned t) {(void)q;(void)v;(void)t;return 0;}
int xTaskCreate(void (*task)(void *),const char *name,unsigned stack,void *arg,unsigned pri,void *handle)
{(void)task;(void)name;(void)stack;(void)arg;(void)pri;(void)handle;return pdPASS;}
esp_err_t deck_usb_refresh_images(void) {++refreshed;return ESP_OK;}
static char last_command[64];
void mqtt_bridge_refresh(void) {}
uint32_t mqtt_bridge_session(void) {return 1;}
bool mqtt_bridge_command(const char *s,uint32_t session) {assert(session==1);snprintf(last_command,sizeof(last_command),"%s",s);return true;}
bool mqtt_bridge_action(const cJSON *a,uint32_t session) {assert(session==1);assert(menu_action_valid(a));snprintf(last_command,sizeof(last_command),"%s.%s",menu_string(a,"entity",""),menu_string(a,"action",""));return true;}
esp_err_t icon_store_rgba(const char *name,uint8_t *image)
{
 FILE *f=fopen("src/starter_icons.bin","rb");assert(f);uint8_t header[9];assert(fread(header,1,9,f)==9);
 for(unsigned i=0;i<header[8];i++) {char id[33];assert(fread(id,1,33,f)==33);if(!strcmp(name,id)){assert(fread(image,1,TILE_RGBA_BYTES,f)==TILE_RGBA_BYTES);fclose(f);return ESP_OK;}fseek(f,TILE_RGBA_BYTES,SEEK_CUR);}
 fclose(f);return ESP_ERR_NOT_FOUND;
}
static char *readfile(const char *name)
{
 FILE *f=fopen(name,"rb");assert(f);fseek(f,0,SEEK_END);long n=ftell(f);rewind(f);
 char *s=calloc(n+1,1);assert(s);assert(fread(s,1,n,f)==(size_t)n);fclose(f);return s;
}
static void press(uint8_t key) {handle_key(key,atomic_load(&configuration_generation),1);}
int main(void)
{
 (void)fail_save;lock=(void *)1;key_queue=(void *)1;mkdir(MENU_STORE_BASE,0700);
 char *s=readfile("examples/menu.json"),err[160];
 assert(menu_upload(s,strlen(s),err,sizeof(err))==ESP_OK);
 assert(!strcmp(current,"hauptmenue"));assert(refreshed==1);
 press(0);assert(!strcmp(current,"licht"));press(3);assert(!strcmp(current,"hauptmenue"));
 press(1);assert(!strcmp(current,"licht"));press(1);assert(!strcmp(current,"wohnzimmer"));
 press(1);assert(!strcmp(last_command,"licht.wohnzimmer.decke.toggle"));
 handle_key(0,atomic_load(&configuration_generation)-1,1);assert(!strcmp(current,"wohnzimmer"));
 fail_save=true;assert(menu_upload(s,strlen(s),err,sizeof(err))==ESP_FAIL);assert(!strcmp(current,"wohnzimmer"));fail_save=false;
 char *disk=readfile(MENU_STORE_BASE "/menu.json");assert(!strcmp(disk,s));free(disk);
 char *exported=menu_export();assert(exported);cJSON *copy=cJSON_Parse(exported);assert(menu_validate(copy,err,sizeof(err)));
 cJSON_ReplaceItemInObjectCaseSensitive(copy,"startPage",cJSON_CreateString("missing"));assert(!menu_validate(copy,err,sizeof(err)));cJSON_Delete(copy);
 assert(!parse("{}{}",4,err,sizeof(err)));assert(!parse("{\"x\":\"\\u0000\"}",14,err,sizeof(err)));
 const char *deep="[[[[[[[[[[[[[0]]]]]]]]]]]]]";assert(!parse(deep,strlen(deep),err,sizeof(err)));
 assert(menu_upload("{bad",4,err,sizeof(err))==ESP_ERR_INVALID_ARG);
 char *after=menu_export();assert(!strcmp(after,exported));free(after);
 // Typed actions accept bounded values and reject extra fields and malformed IDs.
 const char *actions[]={
 "{\"entity\":\"licht.flur\",\"action\":\"toggle\"}",
 "{\"entity\":\"licht.flur\",\"action\":\"set\",\"value\":false}",
 "{\"entity\":\"rollo.room_a\",\"action\":\"position\",\"value\":45.5}",
 "{\"entity\":\"rollo.room_a\",\"action\":\"position\",\"value\":101}",
 "{\"entity\":\"licht..flur\",\"action\":\"toggle\"}",
 "{\"entity\":\"licht.flur\",\"action\":\"toggle\",\"value\":true}",
 "{\"entity\":\"licht.flur\",\"action\":\"set\",\"value\":\"true\"}",
 "{\"entity\":\"licht.flur\",\"action\":\"toggle\",\"command\":\"x\"}"};
 for(unsigned i=0;i<sizeof(actions)/sizeof(actions[0]);i++){cJSON *a=cJSON_Parse(actions[i]);assert(menu_action_valid(a)==(i<3));cJSON_Delete(a);}
 // A missing icon must not replace the active configuration.
 copy=cJSON_Parse(s);cJSON *b=(cJSON *)menu_button(menu_get(menu_get(copy,"pages"),"hauptmenue"),1);
 cJSON_ReplaceItemInObjectCaseSensitive(b,"icon",cJSON_CreateString("missing.svg"));char *bad=cJSON_Print(copy);
 assert(menu_upload(bad,strlen(bad),err,sizeof(err))==ESP_ERR_INVALID_ARG);assert(strstr(err,"missing"));free(bad);cJSON_Delete(copy);
 uint8_t image[MINI_BMP_BYTES],unknown[MINI_BMP_BYTES];
 strcpy(current,"wohnzimmer");assert(menu_image(1,unknown,NULL));
 menu_state("licht.wohnzimmer.decke","on","42");assert(state_count==1);assert(menu_image(1,image,NULL));
 assert(memcmp(image,unknown,sizeof(image)));unsigned p=54;assert(image[p]==0xB0 && image[p+1]==0xF3 && image[p+2]==0xFF);
 menu_state("unconfigured","on","1");assert(state_count==1);
 menu_offline();assert(state_count==0);assert(menu_image(1,image,NULL));assert(!memcmp(image,unknown,sizeof(image)));
 assert(menu_image(0,image,NULL));assert(menu_image(3,unknown,NULL));assert(memcmp(image,unknown,sizeof(image)));
 menu_key(1,false,NULL);assert(queued==0);menu_key(1,true,NULL);assert(queued==1);
 // An offset icon leaves the central label area unobstructed and clips safely.
 uint8_t opaque[TILE_RGBA_BYTES];memset(opaque,255,sizeof(opaque));tile_background(image,0);
 tile_rgba_at(image,opaque,32,3);
 assert(image[54+((79-24)*80+2)*3]==0);
 assert(image[54+((79-24)*80+3)*3]==255);
 assert(image[54+((79-24)*80+34)*3]==255);
 assert(image[54+((79-24)*80+35)*3]==0);
 memcpy(unknown,image,sizeof(image));tile_rgba_at(image,opaque,32,60);assert(!memcmp(unknown,image,sizeof(image)));
 // Font supports German characters, and long UTF-8 text remains inside the BMP.
 tile_background(image,0);tile_text(image,"ÄÖÜ äöü ß Küche sehr langer Text",70,2,0xffffff);assert(icon_bmp_valid(image,sizeof(image)));
 // Idle timeout is driven only by valid key presses, not device feedback.
 strcpy(current,"wohnzimmer");now_us=1000000;press(1);
 now_us+=MENU_IDLE_US-1;return_home_if_idle();assert(!strcmp(current,"wohnzimmer"));
 menu_state("licht.wohnzimmer.decke","on","1");
 now_us++;unsigned before_idle=refreshed;return_home_if_idle();
 assert(!strcmp(current,"hauptmenue")&&refreshed==before_idle+1);
 return_home_if_idle();assert(refreshed==before_idle+1);
 press(1);assert(!strcmp(current,"licht"));now_us+=MENU_IDLE_US-1;press(1);
 now_us++;return_home_if_idle();assert(!strcmp(current,"wohnzimmer"));
 now_us+=MENU_IDLE_US;return_home_if_idle();assert(!strcmp(current,"hauptmenue"));
 // Navigation tile shows the current page, even when the same arrow is used.
 strcpy(current,"wohnzimmer");assert(menu_image(0,image,NULL));
 strcpy(current,"esszimmer");assert(menu_image(0,unknown,NULL));
 assert(memcmp(image,unknown,sizeof(image)));
 // Back acts on release; a hold returns Home once without a preceding Back.
 strcpy(current,"wohnzimmer");
 key_message_t back={.key=3,.configuration=atomic_load(&configuration_generation),.connection=1,
     .gesture=atomic_load(&gesture_generation),.pressed=true,.time_us=now_us};
 atomic_store(&back_down,true);handle_key_event(back);assert(!strcmp(current,"wohnzimmer"));
 now_us+=HOME_HOLD_US-1;check_home_hold();assert(!strcmp(current,"wohnzimmer"));
 now_us++;check_home_hold();assert(!strcmp(current,"hauptmenue"));
 unsigned home_refresh=refreshed;check_home_hold();assert(refreshed==home_refresh);
 back.pressed=false;back.time_us=now_us;atomic_store(&back_down,false);handle_key_event(back);
 assert(!strcmp(current,"hauptmenue"));
 strcpy(current,"wohnzimmer");back.pressed=true;back.time_us=now_us;handle_key_event(back);
 back.pressed=false;back.time_us=++now_us;handle_key_event(back);assert(!strcmp(current,"licht"));
 // Disconnect cancels the held button; synthetic release must not navigate.
 strcpy(current,"wohnzimmer");back.pressed=true;handle_key_event(back);
 menu_key(DECK_KEYS_CANCEL,false,NULL);back.gesture=atomic_load(&gesture_generation);back.pressed=false;
 handle_key_event(back);check_home_hold();assert(!strcmp(current,"wohnzimmer"));
 free(s);free(exported);cJSON_Delete(config);config=NULL;unlink(MENU_STORE_BASE "/menu.json");
 puts("PASS: menu schema, references, bounded JSON, atomic validation, states/offline, glyphs, navigation tiles, key edges");
}
