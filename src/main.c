#include "deck_usb.h"
#include "icon_store.h"
#include "network.h"
#include "menu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "driver/uart.h"
#include "esp_chip_info.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern const uint8_t starter_start[] asm("_binary_starter_icons_bin_start");
extern const uint8_t starter_end[] asm("_binary_starter_icons_bin_end");

extern const uint8_t menu_default_start[] asm("_binary_menu_json_start");
extern const uint8_t menu_default_end[] asm("_binary_menu_json_end");

static void help(void)
{
    puts("\nStream Deck Mini / ESP32-P4-NANO\n"
         "  b 0..100  Set brightness\n"
         "  demo      Redraw current menu\n"
         "  status    Show USB state\n"
         "  menu-default  Replace saved menu with bundled template\n"
         "  help      Show commands\n"
         "wifi-reconnect: reconnect WLAN; wifi-reset: reset C6/SDIO.\n"
         "Left top: next page. Left bottom: previous page.\n");
}

static void command(char *line)
{
    esp_err_t err = ESP_OK;
    if (!strcmp(line, "help")) help();
    else if (!strcmp(line, "demo")) err = deck_usb_demo();
    else if (!strcmp(line, "menu-default")) {
        char reason[160] = {0};
        size_t length = menu_default_end - menu_default_start;
        if (length && !menu_default_start[length-1]) --length;
        err = menu_upload((const char *)menu_default_start, length, reason, sizeof(reason));
        ESP_LOGI("console", "Bundled menu: %s %s", esp_err_to_name(err), reason);
    }
    else if (!strcmp(line, "wifi-reconnect")) err = network_recover(false);
    else if (!strcmp(line, "wifi-reset")) err = network_recover(true);
    else if (!strcmp(line, "status")) err = deck_usb_status();
    else if (!strncmp(line, "b ", 2)) {
        char *end;
        long value = strtol(line + 2, &end, 10);
        if (end == line + 2 || *end || value < 0 || value > 100) {
            puts("Use: b 0..100"); return;
        }
        err = deck_usb_set_brightness((uint8_t)value);
    } else if (*line) { puts("Unknown command. Use help."); }
    if (err != ESP_OK) ESP_LOGW("console", "%s", esp_err_to_name(err));
}

void app_main(void)
{
    esp_chip_info_t chip;
    esp_chip_info(&chip);
    ESP_LOGI("app", "P4 silicon revision %u.%u, USB Mini firmware 0.3.1",
             chip.revision / 100, chip.revision % 100);
    ESP_LOGI("app", "USB-A power is hardware-enabled on the published P4-NANO schematic");
    const uart_config_t uart = {
        .baud_rate = 115200, .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE, .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE, .source_clk = UART_SCLK_DEFAULT
    };
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_0, &uart));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_0, 37, 38, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_0, 512, 0, 0, NULL, 0));
    esp_err_t storage = icon_store_init();
    if (storage != ESP_OK) ESP_LOGW("app", "Icon storage: %s; demo remains available", esp_err_to_name(storage));
    if (storage == ESP_OK) {
        esp_err_t seed = icon_store_seed(starter_start, starter_end - starter_start);
        if (seed != ESP_OK) ESP_LOGW("app", "Starter icons incomplete: %s", esp_err_to_name(seed));
    }
    ESP_ERROR_CHECK(menu_init());
    ESP_ERROR_CHECK(deck_usb_set_image_provider(menu_image, NULL));
    ESP_ERROR_CHECK(deck_usb_start(menu_key, NULL));
    esp_err_t network = network_start();
    if (network != ESP_OK) ESP_LOGE("app", "Network startup: %s", esp_err_to_name(network));
    help();
    char line[64]; size_t used = 0; bool overflow = false;
    for (;;) {
        uint8_t ch;
        if (uart_read_bytes(UART_NUM_0, &ch, 1, pdMS_TO_TICKS(100)) != 1) continue;
        if (ch == '\r' || ch == '\n') {
            if (overflow) puts("Command too long");
            else { line[used] = 0; command(line); }
            used = 0; overflow = false;
        } else if (ch == 8 || ch == 127) {
            if (used) --used;
        } else if (ch >= 32 && ch < 127 && !overflow) {
            if (used + 1 < sizeof(line)) line[used++] = (char)ch;
            else overflow = true;
        }
    }
}
