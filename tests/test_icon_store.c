#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdbool.h>
static bool rename_fails;
static int test_rename(const char *old, const char *next);
#define rename test_rename
#include "../src/icon_store.c"
#undef rename

static bool blank = true, fail_mount;
static unsigned formatted;
static const esp_partition_t partition = {.size = 4096};
SemaphoreHandle_t xSemaphoreCreateMutex(void) { return (void *)&partition; }
int xSemaphoreTake(SemaphoreHandle_t m, unsigned ticks) { (void)m; (void)ticks; return 1; }
int xSemaphoreGive(SemaphoreHandle_t m) { (void)m; return 1; }
const esp_partition_t *esp_partition_find_first(int t, int s, const char *name)
{ (void)t; (void)s; assert(!strcmp(name, "icons")); return &partition; }
esp_err_t esp_partition_read(const esp_partition_t *p, size_t off, void *data, size_t n)
{ (void)p; (void)off; memset(data, blank ? 0xff : 0x10, n); return ESP_OK; }
esp_err_t esp_vfs_littlefs_register(const esp_vfs_littlefs_conf_t *cfg)
{
    if (cfg->format_if_mount_failed) { ++formatted; blank = false; return ESP_OK; }
    return blank || fail_mount ? ESP_FAIL : ESP_OK;
}
esp_err_t esp_littlefs_info(const char *label, size_t *total, size_t *used)
{ (void)label; *total = 2 * 1024 * 1024; *used = 0; return ESP_OK; }
static int test_rename(const char *old, const char *next)
{ if (rename_fails) { errno = EIO; return -1; } return rename(old, next); }

static void clean(void)
{
    mkdir(ICON_STORE_BASE, 0700);
    DIR *dir = opendir(ICON_STORE_BASE); assert(dir);
    struct dirent *entry;
    while ((entry = readdir(dir))) {
        if (!strcmp(entry->d_name,".") || !strcmp(entry->d_name,"..")) continue;
        char path[512]; snprintf(path,sizeof(path),"%s/%s",ICON_STORE_BASE,entry->d_name); assert(unlink(path)==0);
    }
    closedir(dir);
}
int main(void)
{
    clean();
    assert(icon_store_init() == ESP_OK && formatted == 1);
    uint8_t original[MINI_BMP_BYTES], replacement[MINI_BMP_BYTES], readback[MINI_BMP_BYTES];
    mini_demo_bmp(0,false,original); mini_demo_bmp(1,true,replacement);
    const unsigned offsets[] = {0,2,10,14,18,22,26,28,30,34,46,50};
    for (unsigned i = 0; i < sizeof(offsets)/sizeof(offsets[0]); ++i) {
        unsigned off = offsets[i]; replacement[off] ^= 1;
        assert(!icon_bmp_valid(replacement, sizeof(replacement)));
        replacement[off] ^= 1;
    }
    assert(icon_bmp_valid(replacement, sizeof(replacement)));
    assert(!icon_name_valid("") && !icon_name_valid("a/b") && !icon_name_valid("x.svg"));
    assert(!icon_name_valid("abcdefghijklmnopqrstuvwxyz1234567"));
    assert(icon_store_write("lamp",original,sizeof(original)) == ESP_OK);
    assert(icon_store_assign(0,"lamp") == ESP_OK);
    assert(icon_store_key_image(0,readback,NULL) && !memcmp(original,readback,sizeof(original)));
    assert(icon_store_delete("lamp") == ESP_ERR_INVALID_STATE);
    assert(icon_store_assign(6,"lamp") == ESP_ERR_INVALID_ARG);
    assert(icon_store_write("../bad",original,sizeof(original)) == ESP_ERR_INVALID_ARG);
    assert(icon_store_write("lamp",replacement,sizeof(replacement)-1) == ESP_ERR_INVALID_ARG);
    rename_fails = true;
    assert(icon_store_write("lamp",replacement,sizeof(replacement)) == ESP_FAIL);
    assert(icon_store_read("lamp",readback) == ESP_OK && !memcmp(original,readback,sizeof(original)));
    assert(icon_store_assign(0,"") == ESP_FAIL);
    assert(icon_store_key_image(0,readback,NULL));
    rename_fails = false;
    assert(icon_store_write("lamp",replacement,sizeof(replacement)) == ESP_OK);
    // Simulate process restart: assignments must be loaded from persistent files.
    ready = false; mutex = NULL; memset(keys,0,sizeof(keys));
    assert(icon_store_init() == ESP_OK && formatted == 1);
    assert(icon_store_key_image(0,readback,NULL) && !memcmp(replacement,readback,sizeof(replacement)));
    assert(icon_store_assign(0,"") == ESP_OK && !icon_store_key_image(0,readback,NULL));
    assert(icon_store_delete("lamp") == ESP_OK);
    assert(icon_store_assign(0,"missing") == ESP_ERR_NOT_FOUND);
    // Bundled icons are installed once and user replacements survive reseeding.
    FILE *pack=fopen("src/starter_icons.bin","rb");assert(pack);fseek(pack,0,SEEK_END);size_t pack_size=ftell(pack);rewind(pack);
    uint8_t *bundle=malloc(pack_size);assert(bundle);assert(fread(bundle,1,pack_size,pack)==pack_size);fclose(pack);
    assert(icon_store_seed(bundle,pack_size-1)==ESP_ERR_INVALID_ARG);
    assert(icon_store_seed(bundle,pack_size)==ESP_OK);
    icon_inventory_t seeded;assert(icon_store_inventory(&seeded)==ESP_OK&&seeded.count==42);
    assert(icon_store_write("sd_lampe",original,sizeof(original))==ESP_OK);
    assert(icon_store_seed(bundle,pack_size)==ESP_OK);
    assert(icon_store_read("sd_lampe",readback)==ESP_OK&&!memcmp(original,readback,sizeof(original)));
    assert(icon_store_delete("sd_lampe")==ESP_OK);
    rename_fails=true;assert(icon_store_seed(bundle,pack_size)==ESP_FAIL);rename_fails=false;
    assert(icon_store_seed(bundle,pack_size)==ESP_OK);
    free(bundle);clean();
    for(unsigned i=0;i<64;i++) { char name[16]; snprintf(name,sizeof(name),"icon%u",i); assert(icon_store_write(name,original,sizeof(original)) == ESP_OK); }
    assert(icon_store_write("overflow",original,sizeof(original)) == ESP_ERR_NO_MEM);
    assert(icon_store_write("icon0",replacement,sizeof(replacement)) == ESP_OK);
    icon_inventory_t inventory; assert(icon_store_inventory(&inventory)==ESP_OK && inventory.count==64);
    uint8_t rgba[TILE_RGBA_BYTES], restored[TILE_RGBA_BYTES];
    memset(rgba,0,sizeof(rgba));rgba[0]=255;rgba[3]=128;
    assert(icon_store_write("icon0",rgba,sizeof(rgba))==ESP_OK);
    assert(icon_store_rgba("icon0",restored)==ESP_OK && !memcmp(rgba,restored,sizeof(rgba)));
    assert(icon_store_read("icon0",readback)==ESP_OK && icon_bmp_valid(readback,sizeof(readback)));
    unsigned top_left=54+79*80*3;assert(readback[top_left+2]==(255*128+24*127+127)/255);
    rename_fails=true;assert(icon_store_write("icon0",original,sizeof(original))==ESP_FAIL);rename_fails=false;
    assert(icon_store_rgba("icon0",restored)==ESP_OK && !memcmp(rgba,restored,sizeof(rgba)));
    // A failed mount of nonblank storage must not format away existing data.
    ready = false; mutex = NULL; fail_mount = true;
    assert(icon_store_init() == ESP_FAIL && formatted == 1);
    assert(access(ICON_STORE_BASE "/icon0.bmp", F_OK) == 0);
    clean(); puts("PASS: icon persistence, assignments, atomic replacement, limits, no format of existing data");
    return 0;
}
