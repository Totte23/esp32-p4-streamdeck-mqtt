#include "web_ui.h"
#include "icon_store.h"
#include "deck_usb.h"
#include "menu.h"
#include "menu_model.h"
#include "tile_render.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#include "esp_http_server.h"
#include "esp_log.h"

extern const uint8_t html_start[] asm("_binary_index_html_start");
extern const uint8_t html_end[] asm("_binary_index_html_end");
extern const uint8_t js_start[] asm("_binary_app_js_start");
extern const uint8_t js_end[] asm("_binary_app_js_end");
extern const uint8_t codec_start[] asm("_binary_icon_codec_js_start");
extern const uint8_t codec_end[] asm("_binary_icon_codec_js_end");
extern const uint8_t menu_js_start[] asm("_binary_menu_ui_js_start");
extern const uint8_t menu_js_end[] asm("_binary_menu_ui_js_end");
extern const uint8_t schema_start[] asm("_binary_menu_schema_json_start");
extern const uint8_t schema_end[] asm("_binary_menu_schema_json_end");
extern const uint8_t font_start[] asm("_binary_font_js_start");
extern const uint8_t font_end[] asm("_binary_font_js_end");
static httpd_handle_t server;

static void headers(httpd_req_t *req)
{
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_hdr(req, "X-Content-Type-Options", "nosniff");
    httpd_resp_set_hdr(req, "Content-Security-Policy", "default-src 'self'; script-src 'self'; style-src 'self' 'unsafe-inline'; img-src 'self' blob: data:; object-src 'none'; base-uri 'none'; frame-ancestors 'none'");
}
static esp_err_t error(httpd_req_t *req, const char *status, const char *message)
{
    headers(req); httpd_resp_set_status(req, status);
    httpd_resp_set_type(req, "text/plain; charset=utf-8");
    return httpd_resp_send(req, message, HTTPD_RESP_USE_STRLEN);
}
static esp_err_t result(httpd_req_t *req, esp_err_t err)
{
    if (err == ESP_OK) { headers(req); return httpd_resp_send(req, "OK", 2); }
    if (err == ESP_ERR_INVALID_ARG) return error(req, "400 Bad Request", "Ungültiger Name, Bildinhalt oder Tastenwert.");
    if (err == ESP_ERR_NOT_FOUND) return error(req, "404 Not Found", "Piktogramm nicht gefunden.");
    if (err == ESP_ERR_INVALID_STATE) return error(req, "409 Conflict", "Speicher nicht verfügbar oder Piktogramm noch einer Taste zugewiesen.");
    if (err == ESP_ERR_TIMEOUT) return error(req, "503 Service Unavailable", "Gespeichert, aber USB-Warteschlange voll. Bitte Anzeige erneut aktualisieren.");
    if (err == ESP_ERR_NO_MEM) return error(req, "507 Insufficient Storage", "Zu wenig Speicher oder maximal 64 Piktogramme erreicht.");
    return error(req, "500 Internal Server Error", "Speicherzugriff fehlgeschlagen. Bisherige Datei bleibt bei fehlgeschlagenem Upload erhalten.");
}
static bool write_allowed(httpd_req_t *req)
{
    // Browser cross-origin writes cannot supply this header without preflight;
    // this server deliberately provides no CORS allow headers or OPTIONS route.
    char value[8];
    return httpd_req_get_hdr_value_str(req, "X-Deck-Request", value, sizeof(value)) == ESP_OK && !strcmp(value, "1");
}
static bool receive(httpd_req_t *req, void *buffer, size_t size)
{
    size_t offset = 0; unsigned timeouts = 0;
    while (offset < size) {
        int n = httpd_req_recv(req, (char *)buffer + offset, size - offset);
        if (n == HTTPD_SOCK_ERR_TIMEOUT && ++timeouts < 3) continue;
        if (n <= 0) return false;
        offset += n;
    }
    return true;
}
static esp_err_t reject_body(httpd_req_t *req, const char *status, const char *message)
{
    // Unread request bytes must not be parsed as another keep-alive request.
    error(req, status, message);
    return ESP_FAIL;
}

static esp_err_t state(httpd_req_t *req)
{
    icon_inventory_t *items = calloc(1, sizeof(*items));
    if (!items) return result(req, ESP_ERR_NO_MEM);
    esp_err_t storage = icon_store_inventory(items);
    cJSON *root = cJSON_CreateObject();
    if (!root) { free(items); return result(req, ESP_ERR_NO_MEM); }
    cJSON_AddBoolToObject(root, "ready", storage == ESP_OK);
    cJSON_AddNumberToObject(root, "total", items->total);
    cJSON_AddNumberToObject(root, "used", items->used);
    cJSON *icons = cJSON_AddArrayToObject(root, "icons");
    cJSON *keys = cJSON_AddArrayToObject(root, "keys");
    if (!icons || !keys) { free(items); cJSON_Delete(root); return result(req, ESP_ERR_NO_MEM); }
    for (unsigned i = 0; i < items->count; ++i) cJSON_AddItemToArray(icons, cJSON_CreateString(items->names[i]));
    for (unsigned i = 0; i < MINI_KEYS; ++i) cJSON_AddItemToArray(keys, cJSON_CreateString(items->keys[i]));
    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root); free(items);
    if (!json) return result(req, ESP_ERR_NO_MEM);
    headers(req); httpd_resp_set_type(req, "application/json");
    esp_err_t err = httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
    free(json); return err;
}

static esp_err_t get(httpd_req_t *req)
{
    headers(req);
    const uint8_t *start = NULL, *end = NULL;
    if (!strcmp(req->uri, "/")) {
        start = html_start; end = html_end; httpd_resp_set_type(req, "text/html; charset=utf-8");
    } else if (!strcmp(req->uri, "/app.js")) {
        start = js_start; end = js_end; httpd_resp_set_type(req, "text/javascript; charset=utf-8");
    } else if (!strcmp(req->uri, "/icon-codec.js")) {
        start = codec_start; end = codec_end; httpd_resp_set_type(req, "text/javascript; charset=utf-8");
    } else if (!strcmp(req->uri, "/menu-ui.js")) {
        start = menu_js_start; end = menu_js_end; httpd_resp_set_type(req, "text/javascript; charset=utf-8");
    } else if (!strcmp(req->uri, "/font.js")) {
        start = font_start; end = font_end; httpd_resp_set_type(req, "text/javascript; charset=utf-8");
    } else if (!strcmp(req->uri, "/menu.schema.json")) {
        start = schema_start; end = schema_end; httpd_resp_set_type(req, "application/json");
    } else if (!strcmp(req->uri, "/api/menu")) {
        char *json = menu_export(); if (!json) return result(req, ESP_ERR_NO_MEM);
        httpd_resp_set_type(req, "application/json");
        esp_err_t err = httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN); free(json); return err;
    } else if (!strncmp(req->uri, "/api/assets/", 12)) {
        uint8_t *rgba = malloc(TILE_RGBA_BYTES); if (!rgba) return result(req, ESP_ERR_NO_MEM);
        esp_err_t err = icon_store_rgba(req->uri + 12, rgba);
        if (err == ESP_OK) { httpd_resp_set_type(req, "application/octet-stream"); err = httpd_resp_send(req, (char *)rgba, TILE_RGBA_BYTES); }
        else err = result(req, err);
        free(rgba); return err;
    } else if (!strcmp(req->uri, "/api/state")) return state(req);
    else if (!strncmp(req->uri, "/api/icons/", 11)) {
        uint8_t *bmp = malloc(MINI_BMP_BYTES);
        if (!bmp) return result(req, ESP_ERR_NO_MEM);
        esp_err_t err = icon_store_read(req->uri + 11, bmp);
        if (err == ESP_OK) {
            httpd_resp_set_type(req, "application/octet-stream");
            err = httpd_resp_send(req, (const char *)bmp, MINI_BMP_BYTES);
        } else err = result(req, err);
        free(bmp); return err;
    } else return error(req, "404 Not Found", "Nicht gefunden.");
    return httpd_resp_send(req, (const char *)start, end - start);
}

static esp_err_t upload(httpd_req_t *req)
{
    if (!write_allowed(req)) return reject_body(req, "403 Forbidden", "Bitte die Weboberfläche verwenden.");
    const char *name = req->uri + 11;
    if (!icon_name_valid(name) || req->content_len != MINI_BMP_BYTES)
        return reject_body(req, "400 Bad Request", "Erwartet: gültiger Name und ein konvertiertes 80×80-Bild.");
    uint8_t *bmp = malloc(MINI_BMP_BYTES);
    if (!bmp) return reject_body(req, "503 Service Unavailable", "Zu wenig Arbeitsspeicher.");
    if (!receive(req, bmp, MINI_BMP_BYTES)) {
        free(bmp); return reject_body(req, "408 Request Timeout", "Upload unvollständig; bisheriges Bild bleibt erhalten.");
    }
    esp_err_t err = icon_store_write(name, bmp, MINI_BMP_BYTES);
    free(bmp);
    if (err == ESP_OK) err = deck_usb_refresh_images();
    return result(req, err);
}
static esp_err_t upload_asset(httpd_req_t *req)
{
    if (!write_allowed(req) || !icon_name_valid(req->uri + 12) || req->content_len != TILE_RGBA_BYTES)
        return reject_body(req, "400 Bad Request", "Erwartet: RGBA-Bild mit 80×80 Pixeln.");
    uint8_t *rgba = malloc(TILE_RGBA_BYTES);
    if (!rgba) return reject_body(req, "503 Service Unavailable", "Zu wenig Speicher.");
    if (!receive(req, rgba, TILE_RGBA_BYTES)) { free(rgba); return reject_body(req, "408 Request Timeout", "Upload unvollständig."); }
    esp_err_t err = icon_store_write(req->uri + 12, rgba, TILE_RGBA_BYTES); free(rgba);
    if (err == ESP_OK) err = deck_usb_refresh_images();
    return result(req, err);
}
static esp_err_t upload_menu(httpd_req_t *req)
{
    if (!write_allowed(req) || !req->content_len || req->content_len > MENU_MAX_BYTES)
        return reject_body(req, "400 Bad Request", "Erwartet: menu.json, maximal 64 KiB.");
    char *json = malloc(req->content_len); if (!json) return reject_body(req, "503 Service Unavailable", "Zu wenig Speicher.");
    if (!receive(req, json, req->content_len)) { free(json); return reject_body(req, "408 Request Timeout", "Upload unvollständig."); }
    char reason[160] = "Menü konnte nicht gespeichert werden.";
    esp_err_t err = menu_upload(json, req->content_len, reason, sizeof(reason)); free(json);
    if (err != ESP_OK) return error(req, err == ESP_ERR_INVALID_ARG ? "400 Bad Request" : "500 Internal Server Error", reason);
    return result(req, ESP_OK);
}
static esp_err_t remove_icon(httpd_req_t *req)
{
    if (!write_allowed(req) || req->content_len)
        return reject_body(req, "403 Forbidden", "Ungültige Anfrage.");
    if (menu_uses_icon(req->uri + 11)) return error(req, "409 Conflict", "Dieses Icon wird im Menü verwendet.");
    return result(req, icon_store_delete(req->uri + 11));
}
static esp_err_t assign(httpd_req_t *req)
{
    if (!write_allowed(req)) return reject_body(req, "403 Forbidden", "Bitte die Weboberfläche verwenden.");
    const char *key = req->uri + 10; // /api/keys/1 ... /api/keys/6
    if (strlen(key) != 1 || *key < '1' || *key > '6' || req->content_len > ICON_NAME_MAX)
        return reject_body(req, "400 Bad Request", "Ungültige Taste oder Icon-ID.");
    char name[ICON_NAME_MAX + 1] = {0};
    if (!receive(req, name, req->content_len)) return reject_body(req, "408 Request Timeout", "Anfrage unvollständig.");
    if (strlen(name) != req->content_len) return result(req, ESP_ERR_INVALID_ARG);
    esp_err_t err = icon_store_assign(*key - '1', name);
    if (err == ESP_OK) err = deck_usb_refresh_images();
    return result(req, err);
}
static esp_err_t brightness(httpd_req_t *req)
{
    if (!write_allowed(req) || !req->content_len || req->content_len > 3)
        return reject_body(req, "400 Bad Request", "Erwartet: 0 bis 100.");
    char value[4] = {0};
    if (!receive(req, value, req->content_len)) return reject_body(req, "408 Request Timeout", "Anfrage unvollständig.");
    for (size_t i = 0; i < req->content_len; ++i)
        if (value[i] < '0' || value[i] > '9') return result(req, ESP_ERR_INVALID_ARG);
    unsigned n = (unsigned)strtoul(value, NULL, 10);
    return result(req, n <= 100 ? deck_usb_set_brightness(n) : ESP_ERR_INVALID_ARG);
}

esp_err_t web_ui_start(void)
{
    if (server) return ESP_OK;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.stack_size = 8192;
    config.max_open_sockets = 4;
    config.lru_purge_enable = true;
    config.recv_wait_timeout = 3;
    config.send_wait_timeout = 5;
    esp_err_t err = httpd_start(&server, &config);
    if (err != ESP_OK) return err;
    const httpd_uri_t routes[] = {
        {.uri = "/*", .method = HTTP_GET, .handler = get},
        {.uri = "/api/icons/*", .method = HTTP_PUT, .handler = upload},
        {.uri = "/api/icons/*", .method = HTTP_DELETE, .handler = remove_icon},
        {.uri = "/api/keys/*", .method = HTTP_POST, .handler = assign},
        {.uri = "/api/menu", .method = HTTP_PUT, .handler = upload_menu},
        {.uri = "/api/assets/*", .method = HTTP_PUT, .handler = upload_asset},
        {.uri = "/api/brightness", .method = HTTP_POST, .handler = brightness},
    };
    for (unsigned i = 0; i < sizeof(routes) / sizeof(routes[0]); ++i) {
        err = httpd_register_uri_handler(server, &routes[i]);
        if (err != ESP_OK) { httpd_stop(server); server = NULL; return err; }
    }
    ESP_LOGI("web", "Icon manager listening on port 80");
    return ESP_OK;
}
