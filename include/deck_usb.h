#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

// Called on the USB client task; callbacks must be short and non-blocking.
// Disconnect first emits DECK_KEYS_CANCEL (pressed=false), then held-key releases.
#define DECK_KEYS_CANCEL UINT8_MAX
typedef void (*deck_key_callback_t)(uint8_t key, bool pressed, void *context);
esp_err_t deck_usb_start(deck_key_callback_t callback, void *context);
// Thread-safe asynchronous commands. Return ESP_ERR_TIMEOUT if queue is full.
esp_err_t deck_usb_set_brightness(uint8_t percent);
esp_err_t deck_usb_demo(void);
esp_err_t deck_usb_status(void);

// Register before start. Provider runs on the client task, outside callbacks.
// Write exactly MINI_BMP_BYTES of native BMP; false selects the demo tile.
typedef bool (*deck_image_provider_t)(uint8_t key, uint8_t *bmp, void *context);
esp_err_t deck_usb_set_image_provider(deck_image_provider_t provider, void *context);
esp_err_t deck_usb_refresh_images(void);
