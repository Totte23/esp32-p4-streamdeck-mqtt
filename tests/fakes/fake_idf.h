#pragma once
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

typedef int esp_err_t;
enum { ESP_OK, ESP_FAIL, ESP_ERR_TIMEOUT, ESP_ERR_INVALID_ARG, ESP_ERR_INVALID_STATE, ESP_ERR_NO_MEM, ESP_ERR_NOT_FOUND };
#define ESP_INTR_FLAG_LEVEL1 1
#define ESP_ERROR_CHECK(x) assert((x) == ESP_OK)
#define ESP_LOGI(tag, ...) do { (void)(tag); printf(__VA_ARGS__); putchar('\n'); } while (0)
#define ESP_LOGW ESP_LOGI
#define ESP_LOGE ESP_LOGI
static inline const char *esp_err_to_name(int err) { (void)err; return "mock"; }
int64_t esp_timer_get_time(void);
typedef void *QueueHandle_t;
#define portMAX_DELAY UINT32_MAX
#define pdMS_TO_TICKS(ms) (ms)
#define pdTRUE 1
#define pdPASS 1
QueueHandle_t xQueueCreate(unsigned length, unsigned item_size);
void vQueueDelete(QueueHandle_t q);
int xQueueReceive(QueueHandle_t q, void *item, unsigned wait);
int xQueueSend(QueueHandle_t q, const void *item, unsigned wait);
int xTaskCreate(void (*task)(void *), const char *name, unsigned stack, void *arg, unsigned priority, void *handle);

typedef void *usb_host_client_handle_t;
typedef void *usb_device_handle_t;
typedef struct usb_transfer usb_transfer_t;
struct usb_transfer {
    uint8_t *data_buffer;
    int num_bytes, actual_num_bytes, status;
    usb_device_handle_t device_handle;
    uint8_t bEndpointAddress;
    void (*callback)(usb_transfer_t *);
    bool test_inflight;
};
enum { USB_TRANSFER_STATUS_COMPLETED, USB_TRANSFER_STATUS_CANCELED, USB_TRANSFER_STATUS_ERROR };
enum { USB_HOST_CLIENT_EVENT_NEW_DEV, USB_HOST_CLIENT_EVENT_DEV_GONE };
enum { USB_SPEED_LOW, USB_SPEED_FULL, USB_SPEED_HIGH };
#define USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS 1

typedef struct {
    int event;
    struct { uint8_t address; } new_dev;
    struct { usb_device_handle_t dev_hdl; } dev_gone;
} usb_host_client_event_msg_t;
typedef struct {
    bool is_synchronous;
    int max_num_event_msg;
    struct { void (*client_event_callback)(const usb_host_client_event_msg_t *, void *); } async;
} usb_host_client_config_t;
typedef struct {
    int intr_flags;
    unsigned peripheral_map;
    struct { unsigned rx_fifo_lines, nptx_fifo_lines, ptx_fifo_lines; } fifo_settings_custom;
} usb_host_config_t;
typedef struct { uint16_t idVendor, idProduct; } usb_device_desc_t;
typedef struct { uint8_t bLength, bDescriptorType; uint16_t wTotalLength; uint8_t tail[37]; } usb_config_desc_t;
typedef struct { int speed; } usb_device_info_t;

esp_err_t usb_host_transfer_alloc(size_t size, int iso, usb_transfer_t **out);
esp_err_t usb_host_transfer_free(usb_transfer_t *t);
esp_err_t usb_host_transfer_submit(usb_transfer_t *t);
esp_err_t usb_host_transfer_submit_control(usb_host_client_handle_t c, usb_transfer_t *t);
esp_err_t usb_host_endpoint_halt(usb_device_handle_t d, uint8_t ep);
esp_err_t usb_host_endpoint_flush(usb_device_handle_t d, uint8_t ep);
esp_err_t usb_host_interface_release(usb_host_client_handle_t c, usb_device_handle_t d, uint8_t i);
esp_err_t usb_host_interface_claim(usb_host_client_handle_t c, usb_device_handle_t d, uint8_t i, uint8_t a);
esp_err_t usb_host_device_close(usb_host_client_handle_t c, usb_device_handle_t d);
esp_err_t usb_host_device_open(usb_host_client_handle_t c, uint8_t a, usb_device_handle_t *d);
esp_err_t usb_host_get_device_descriptor(usb_device_handle_t d, const usb_device_desc_t **out);
esp_err_t usb_host_get_active_config_descriptor(usb_device_handle_t d, const usb_config_desc_t **out);
esp_err_t usb_host_device_info(usb_device_handle_t d, usb_device_info_t *info);
esp_err_t usb_host_client_register(const usb_host_client_config_t *c, usb_host_client_handle_t *h);
esp_err_t usb_host_client_handle_events(usb_host_client_handle_t c, unsigned wait);
esp_err_t usb_host_lib_handle_events(unsigned wait, uint32_t *flags);
esp_err_t usb_host_device_free_all(void);
esp_err_t usb_host_install(const usb_host_config_t *c);
