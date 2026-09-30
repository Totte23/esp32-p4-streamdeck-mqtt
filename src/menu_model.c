#include "menu_model.h"
#include "icon_format.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
const cJSON *menu_get(const cJSON *o, const char *k) { return cJSON_GetObjectItemCaseSensitive(o,k); }
const char *menu_string(const cJSON *o,const char *k,const char *fallback)
{ const cJSON *v=menu_get(o,k); return cJSON_IsString(v)?v->valuestring:fallback; }
void menu_icon_name(const char *filename, char name[33])
{
    size_t n=strlen(filename);
    if(n>4 && (!strcmp(filename+n-4,".svg") || !strcmp(filename+n-4,".png"))) n-=4;
    if(n>32) n=32;
    memcpy(name,filename,n);name[n]=0;
}
const cJSON *menu_button(const cJSON *page,unsigned key)
{
    const char *slots[]={NULL,"topLeft","topRight",NULL,"bottomLeft","bottomRight"};
    return key<6 && slots[key] ? menu_get(menu_get(page,"buttons"),slots[key]) : NULL;
}
static bool fields(const cJSON *o,const char *allowed)
{
    if(!cJSON_IsObject(o)) return false;
    for(const cJSON *v=o->child;v;v=v->next) {
        char needle[96];
        if(!v->string || strlen(v->string)>90) return false;
        snprintf(needle,sizeof(needle),"|%s|",v->string);
        if(!strstr(allowed,needle)) return false;
        for(const cJSON *p=o->child;p!=v;p=p->next) if(!strcmp(p->string,v->string)) return false;
    }
    return true;
}
static bool str(const cJSON *o,const char *k,size_t max,bool required)
{ const cJSON *v=menu_get(o,k); return !v ? !required : cJSON_IsString(v)&&strlen(v->valuestring)<=max&&(!required||*v->valuestring); }
static bool id(const char *s)
{
    if(!*s || strlen(s)>63) return false;
    for(;*s;s++) if(!isalnum((unsigned char)*s)&&*s!='_'&&*s!='-'&&*s!='.') return false;
    return true;
}
static bool color(const cJSON *o,const char *k)
{
    const cJSON *v=menu_get(o,k); if(!v) return true;
    if(!cJSON_IsString(v)||strlen(v->valuestring)!=7||v->valuestring[0]!='#') return false;
    for(int i=1;i<7;i++) if(!isxdigit((unsigned char)v->valuestring[i])) return false;
    return true;
}
static bool labels(const cJSON *v)
{ return !v || (fields(v,"|top||center||bottom|")&&str(v,"top",63,false)&&str(v,"center",63,false)&&str(v,"bottom",63,false)); }
static bool style(const cJSON *v)
{
    return fields(v,"|background||textColor||center||iconBackground|")&&color(v,"background")&&color(v,"textColor")&&color(v,"iconBackground")&&str(v,"center",63,false);
}
bool menu_action_valid(const cJSON *a)
{
    if(!fields(a,"|entity||action||value|")||!str(a,"entity",63,true)||!str(a,"action",16,true)) return false;
    const char *entity=menu_string(a,"entity",""),*action=menu_string(a,"action","");
    bool segment_start=true, dot=false;
    for(const char *p=entity;*p;p++) {
        if(*p=='.') {if(segment_start) return false; segment_start=true;dot=true;}
        else {if(segment_start&&(*p<'a'||*p>'z')) return false;
            if(!(*p>='a'&&*p<='z')&&!(*p>='0'&&*p<='9')&&*p!='_'&&*p!='-') return false;
            segment_start=false;}
    }
    if(segment_start||!dot) return false;
    const cJSON *v=menu_get(a,"value");
    if(!strcmp(action,"set")) return cJSON_IsBool(v);
    if(!strcmp(action,"position")) return cJSON_IsNumber(v)&&isfinite(v->valuedouble)&&v->valuedouble>=0&&v->valuedouble<=100;
    const char *no_value[]={"toggle","up","down","stop","slatUp","slatDown","tilt","trigger"};
    for(unsigned i=0;i<sizeof(no_value)/sizeof(no_value[0]);i++) if(!strcmp(action,no_value[i])) return !v;
    return false;
}
bool menu_validate(const cJSON *r,char *e,size_t cap)
{
#define FAIL(msg) do { snprintf(e,cap,"%s",msg); return false; } while(0)
    if(!fields(r,"|$schema||version||startPage||pages|") || !str(r,"$schema",200,false)) FAIL("Unbekanntes Feld im Menü.");
    const cJSON *version=menu_get(r,"version"), *pages=menu_get(r,"pages");
    if(!cJSON_IsNumber(version)||version->valuedouble!=1) FAIL("version muss 1 sein.");
    if(!cJSON_IsObject(pages)||cJSON_GetArraySize(pages)<1||cJSON_GetArraySize(pages)>MENU_MAX_PAGES) FAIL("Erwartet: 1 bis 48 Seiten.");
    if(!str(r,"startPage",63,true)||!menu_get(pages,menu_string(r,"startPage",""))) FAIL("startPage fehlt oder existiert nicht.");
    for(const cJSON *p=pages->child;p;p=p->next) {
        if(!id(p->string)||!fields(p,"|title||previous||next||buttons|")||!str(p,"title",63,true)) FAIL("Ungültige Seite oder Seitentitel.");
        for(const cJSON *q=pages->child;q!=p;q=q->next) if(!strcmp(p->string,q->string)) FAIL("Doppelte Seiten-ID.");
        const char *links[]={"previous","next"};
        for(unsigned i=0;i<2;i++) if(!str(p,links[i],63,true)||!menu_get(pages,menu_string(p,links[i],""))) FAIL("Blätterziel fehlt oder existiert nicht.");
        const cJSON *buttons=menu_get(p,"buttons");
        if(!fields(buttons,"|topLeft||topRight||bottomLeft||bottomRight|")) FAIL("Nur vier rechte Inhaltstasten sind erlaubt.");
        for(const cJSON *b=buttons->child;b;b=b->next) {
            if(cJSON_IsNull(b)) continue;
            if(!fields(b,"|id||icon||iconSize||iconY||labels||fontSize||background||textColor||onPress||state||appearance|")||!str(b,"id",63,true)||!id(menu_string(b,"id",""))) FAIL("Ungültige Taste oder unbekanntes Tastenfeld.");
            const char *is=menu_string(b,"icon","");char name[33];menu_icon_name(is,name);
            size_t iconlen=strlen(is);
            if(iconlen>4&&(!strcmp(is+iconlen-4,".svg")||!strcmp(is+iconlen-4,".png"))) iconlen-=4;
            if(!str(b,"icon",36,false)||iconlen>32||(*is&&!icon_name_valid(name))) FAIL("Ungültiger Icon-Dateiname.");
            if(!labels(menu_get(b,"labels"))||!color(b,"background")||!color(b,"textColor")) FAIL("Ungültige Beschriftung oder Farbe (#RRGGBB).");
            const cJSON *size=menu_get(b,"iconSize"),*font=menu_get(b,"fontSize");
            if(size&&(!cJSON_IsNumber(size)||size->valuedouble<1||size->valuedouble>80||size->valueint!=size->valuedouble)) FAIL("iconSize: 1 bis 80 Pixel.");
            const cJSON *top=menu_get(b,"iconY");
            if(top&&(!cJSON_IsNumber(top)||top->valuedouble<0||top->valueint!=top->valuedouble||top->valuedouble>80-(size?size->valueint:48))) FAIL("iconY und iconSize müssen innerhalb 80 Pixel bleiben.");
            if(font&&(!cJSON_IsNumber(font)||(font->valuedouble!=8&&font->valuedouble!=16))) FAIL("fontSize: 8 oder 16 Pixel.");
            if(!str(b,"state",63,false)||(*menu_string(b,"state","")&&!id(menu_string(b,"state","")))) FAIL("Ungültige Status-ID.");
            const cJSON *a=menu_get(b,"onPress");
            if(a) {
                if(menu_get(a,"entity")) {
                    if(!menu_action_valid(a)) FAIL("onPress: ungültige entity/action/value-Kombination.");
                } else {
                    if(!fields(a,"|page||command|")||cJSON_GetArraySize(a)!=1) FAIL("onPress: page oder entity/action (command nur für Altmenüs).");
                    if(menu_get(a,"page")&&(!str(a,"page",63,true)||!menu_get(pages,menu_string(a,"page","")))) FAIL("Tastenziel existiert nicht.");
                    if(menu_get(a,"command")&&(!str(a,"command",63,true)||!id(menu_string(a,"command","")))) FAIL("Ungültiger MQTT-Aktionsname.");
                }
            }
            const cJSON *ap=menu_get(b,"appearance");
            if(ap) {
                if(!cJSON_IsObject(ap)||cJSON_GetArraySize(ap)>16) FAIL("appearance: maximal 16 Zustände.");
                for(const cJSON *v=ap->child;v;v=v->next) {
                    if(!id(v->string)||!style(v)) FAIL("Ungültige Zustandsdarstellung.");
                    for(const cJSON *q=ap->child;q!=v;q=q->next) if(!strcmp(q->string,v->string)) FAIL("Doppelter Darstellungszustand.");
                }
            }
        }
    }
    if(cap) *e=0;
    return true;
#undef FAIL
}
