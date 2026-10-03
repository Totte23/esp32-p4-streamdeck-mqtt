#include "deck_usb.h"
#include "deck_descriptors.h"
#include "mini_protocol.h"
#include "app_config.h"
#include <inttypes.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "usb/usb_host.h"

static const char *TAG = "deck";
typedef enum { CMD_BRIGHTNESS, CMD_DEMO, CMD_STATUS, CMD_REFRESH, CMD_SLEEP } command_kind_t;
typedef struct { command_kind_t kind; uint8_t value; } command_t;
typedef struct {
    usb_host_client_handle_t client;
    usb_device_handle_t device;
    deck_interface_t interface;
    usb_transfer_t *input, *output, *control;
    bool pending_addresses[128];
    bool claimed, gone, fault, cancel_started;
    bool in_busy, out_busy, ctrl_busy, out_done, ctrl_done;
    bool reset_stream, brightness_pending;
    bool display_sleep, wake_pending;
    uint8_t brightness, pressed, toggled, dirty;
    int image_key;
    size_t page;
    int64_t out_started, ctrl_started;
    uint8_t bmp[MINI_BMP_BYTES];
    deck_key_callback_t key_callback;
    void *key_context;
} deck_t;
static deck_t s;
static QueueHandle_t commands;
static deck_image_provider_t image_provider;
static void *image_context;

static void mark_fault(const char *operation, esp_err_t error)
{
    if (!s.fault && !s.gone)
        ESP_LOGE(TAG, "%s: %s. Unplug/reconnect the Mini to recover.", operation, esp_err_to_name(error));
    s.fault = true;
}

static void input_done(usb_transfer_t *t)
{
    s.in_busy = false;
    if (s.gone || s.fault) return;
    if (t->status != USB_TRANSFER_STATUS_COMPLETED) {
        mark_fault("input transfer", ESP_FAIL);
        return;
    }
    uint8_t mask;
    if (!mini_parse_keys(t->data_buffer, t->actual_num_bytes, &mask)) {
        ESP_LOGW(TAG, "Ignored input report (%d bytes)", t->actual_num_bytes);
        return;
    }
    uint8_t changes = mask ^ s.pressed;
    s.pressed = mask;
    for (unsigned k = 0; k < MINI_KEYS; ++k) {
        if (!(changes & (1u << k))) continue;
        bool down = (mask & (1u << k)) != 0;
        if (down) { s.toggled ^= 1u << k; s.dirty |= 1u << k; }
        if (s.key_callback) s.key_callback(k, down, s.key_context);
    }
}

static void output_done(usb_transfer_t *t)
{
    (void)t;
    s.out_busy = false;
    s.out_done = true;
}

static void control_done(usb_transfer_t *t)
{
    (void)t;
    s.ctrl_busy = false;
    s.ctrl_done = true;
}

static void client_event(const usb_host_client_event_msg_t *event, void *arg)
{
    (void)arg;
    if (event->event == USB_HOST_CLIENT_EVENT_NEW_DEV && event->new_dev.address < 128)
        s.pending_addresses[event->new_dev.address] = true;
    if (event->event == USB_HOST_CLIENT_EVENT_DEV_GONE && event->dev_gone.dev_hdl == s.device)
        { s.gone = true; if(s.key_callback) s.key_callback(DECK_KEYS_CANCEL,false,s.key_context); }
}

static void free_transfers(void)
{
    // Only after every submitted transfer has delivered its callback.
    usb_host_transfer_free(s.input); s.input = NULL;
    usb_host_transfer_free(s.output); s.output = NULL;
    usb_host_transfer_free(s.control); s.control = NULL;
}

static void cancel_transfers(void)
{
    if (s.cancel_started || !s.claimed) return;
    s.cancel_started = true;
    // Endpoint zero belongs to the host library. It cancels control transfers
    // automatically on removal; never free a still-owned control buffer.
    const uint8_t endpoints[] = {s.interface.in_address, s.interface.out_address};
    for (unsigned i = 0; i < 2; ++i) {
        (void)usb_host_endpoint_halt(s.device, endpoints[i]);
        (void)usb_host_endpoint_flush(s.device, endpoints[i]);
    }
}

static void close_removed(void)
{
    cancel_transfers();
    if (s.in_busy || s.out_busy || s.ctrl_busy) return;
    if (s.claimed) {
        esp_err_t err = usb_host_interface_release(s.client, s.device, s.interface.interface_number);
        if (err != ESP_OK) { ESP_LOGE(TAG, "Release: %s", esp_err_to_name(err)); return; }
        s.claimed = false;
    }
    esp_err_t err = usb_host_device_close(s.client, s.device);
    if (err != ESP_OK) { ESP_LOGE(TAG, "Close: %s", esp_err_to_name(err)); return; }
    free_transfers();
    for (unsigned k = 0; k < MINI_KEYS; ++k)
        if ((s.pressed & (1u << k)) && s.key_callback) s.key_callback(k, false, s.key_context);
    s.device = NULL;
    s.pressed = s.toggled = s.dirty = 0;
    s.out_done = s.ctrl_done = s.gone = s.fault = s.cancel_started = false;
    s.image_key = -1;
    ESP_LOGI(TAG, "Mini disconnected; waiting for a device");
}

static void open_device(uint8_t address)
{
    usb_device_handle_t dev;
    esp_err_t err = usb_host_device_open(s.client, address, &dev);
    if (err != ESP_OK) return; // Device can disappear between enumeration and open.
    const usb_device_desc_t *device_desc;
    const usb_config_desc_t *config;
    if (usb_host_get_device_descriptor(dev, &device_desc) != ESP_OK ||
        !mini_supported(device_desc->idVendor, device_desc->idProduct)) {
        ESP_LOGI(TAG, "Ignoring USB device at address %u (not a supported Mini)", address);
        usb_host_device_close(s.client, dev);
        return;
    }
    deck_interface_t iface;
    if (usb_host_get_active_config_descriptor(dev, &config) != ESP_OK ||
        !deck_find_interface((const uint8_t *)config, config->wTotalLength, &iface)) {
        ESP_LOGE(TAG, "Mini has no supported interrupt IN/OUT HID interface");
        usb_host_device_close(s.client, dev);
        return;
    }
    s.device = dev;
    s.interface = iface;
    err = usb_host_interface_claim(s.client, dev, iface.interface_number, 0);
    if (err != ESP_OK) goto fail;
    s.claimed = true;
    // IN buffers must be a multiple of endpoint MPS, also for short reports.
    size_t input_length = ((65 + iface.in_mps - 1) / iface.in_mps) * iface.in_mps;
    err = usb_host_transfer_alloc(input_length, 0, &s.input);
    if (err != ESP_OK) goto fail;
    err = usb_host_transfer_alloc(MINI_REPORT_BYTES, 0, &s.output);
    if (err != ESP_OK) goto fail;
    err = usb_host_transfer_alloc(8 + MINI_FEATURE_BYTES, 0, &s.control);
    if (err != ESP_OK) goto fail;
    s.input->device_handle = dev;
    s.input->bEndpointAddress = iface.in_address;
    s.input->callback = input_done;
    s.input->num_bytes = input_length;
    s.output->device_handle = dev;
    s.output->bEndpointAddress = iface.out_address;
    s.output->callback = output_done;
    s.output->num_bytes = MINI_REPORT_BYTES;
    s.control->device_handle = dev;
    s.control->bEndpointAddress = 0;
    s.control->callback = control_done;
    s.control->num_bytes = 8 + MINI_FEATURE_BYTES;
    s.reset_stream = s.brightness_pending = true;
    s.dirty = 0x3f;
    s.image_key = -1;
    s.page = 0;
    usb_device_info_t info;
    const char *speed = "unknown";
    if (usb_host_device_info(dev, &info) == ESP_OK)
        speed = info.speed == USB_SPEED_HIGH ? "HIGH (480 Mbps)" :
                info.speed == USB_SPEED_FULL ? "FULL (12 Mbps)" : "LOW";
    ESP_LOGI(TAG, "Mini %04x:%04x connected, %s, HID %u, IN %02x/%u OUT %02x/%u",
             device_desc->idVendor, device_desc->idProduct, speed, iface.interface_number,
             iface.in_address, iface.in_mps, iface.out_address, iface.out_mps);
    return;
fail:
    ESP_LOGE(TAG, "Mini setup failed: %s", esp_err_to_name(err));
    s.gone = true; // Same safe cleanup path; no transfers have been submitted yet.
}

static void process_completions(void)
{
    if (s.out_done) {
        s.out_done = false;
        if (s.output->status != USB_TRANSFER_STATUS_COMPLETED ||
            s.output->actual_num_bytes != MINI_REPORT_BYTES) {
            ESP_LOGE(TAG, "OUT status=%d bytes=%d", s.output->status, s.output->actual_num_bytes);
            mark_fault("image transfer", ESP_FAIL);
        } else if (s.reset_stream) {
            s.reset_stream = false;
        } else if (++s.page == MINI_IMAGE_PAGES) {
            ESP_LOGI(TAG, "Key %d image uploaded", s.image_key + 1);
            s.image_key = -1;
        }
    }
    if (s.ctrl_done) {
        s.ctrl_done = false;
        if (s.control->status != USB_TRANSFER_STATUS_COMPLETED)
            mark_fault("brightness feature report", ESP_FAIL);
        else ESP_LOGI(TAG, "Brightness command delivered");
    }
}

static void service_device(void)
{
    process_completions();
    if (s.fault) { cancel_transfers(); return; }
    if (!s.in_busy) {
        esp_err_t err = usb_host_transfer_submit(s.input);
        if (err != ESP_OK) { mark_fault("submit input", err); return; }
        s.in_busy = true;
    }
    // No IN timeout: an idle keyboard may NAK indefinitely. OUT/feature stalls
    // are faults. Keep buffers alive until cancellation/removal completes.
    int64_t now = esp_timer_get_time();
    if ((s.out_busy && now - s.out_started > 3000000) ||
        (s.ctrl_busy && now - s.ctrl_started > 3000000)) {
        mark_fault("USB output timeout", ESP_ERR_TIMEOUT);
        return;
    }
    if (s.out_busy || s.ctrl_busy) return;
    if (s.wake_pending && !s.dirty && s.image_key < 0 && !s.reset_stream) {
        s.wake_pending = false; s.brightness_pending = true;
    }
    if (s.brightness_pending && !s.wake_pending) {
        uint8_t *p = s.control->data_buffer;
        memset(p, 0, 8 + MINI_FEATURE_BYTES);
        // HID SET_REPORT, Feature(3), report ID 5, recipient Interface.
        p[0] = 0x21; p[1] = 0x09; p[2] = 0x05; p[3] = 0x03;
        p[4] = s.interface.interface_number; p[6] = MINI_FEATURE_BYTES;
        mini_brightness(s.display_sleep ? 0 : s.brightness, p + 8);
        esp_err_t err = usb_host_transfer_submit_control(s.client, s.control);
        if (err != ESP_OK) { mark_fault("submit brightness", err); return; }
        s.brightness_pending = false;
        s.ctrl_busy = true; s.ctrl_started = now;
        return;
    }
    if (s.display_sleep) return; // Keep input polling alive, stop display traffic.
    if (s.reset_stream) {
        memset(s.output->data_buffer, 0, MINI_REPORT_BYTES);
        s.output->data_buffer[0] = 2;
    } else {
        if (s.image_key < 0) {
            if (!s.dirty) return;
            for (unsigned k = 0; k < MINI_KEYS; ++k) {
                if (!(s.dirty & (1u << k))) continue;
                s.image_key = k; s.dirty &= ~(1u << k); s.page = 0;
                if (!image_provider || !image_provider(k, s.bmp, image_context))
                    mini_demo_bmp(k, (s.toggled & (1u << k)) != 0, s.bmp);
                break;
            }
        }
        mini_image_page(s.image_key, s.page, s.bmp, sizeof(s.bmp), s.output->data_buffer);
    }
    esp_err_t err = usb_host_transfer_submit(s.output);
    if (err != ESP_OK) { mark_fault("submit image", err); return; }
    s.out_busy = true; s.out_started = now;
}

static void client_task(void *arg)
{
    (void)arg;
    const usb_host_client_config_t cfg = {
        .is_synchronous = false, .max_num_event_msg = 10,
        .async = {.client_event_callback = client_event}
    };
    ESP_ERROR_CHECK(usb_host_client_register(&cfg, &s.client));
    for (;;) {
        usb_host_client_handle_events(s.client, pdMS_TO_TICKS(10));
        command_t command;
        while (xQueueReceive(commands, &command, 0) == pdTRUE) {
            switch (command.kind) {
            case CMD_SLEEP:
                s.display_sleep = command.value != 0;
                s.wake_pending = !s.display_sleep;
                s.brightness_pending = s.display_sleep;
                if (!s.display_sleep) s.dirty = 0x3f;
                break;
            case CMD_BRIGHTNESS:
                s.brightness = command.value; s.brightness_pending = true; break;
            case CMD_DEMO:
                s.toggled = 0; s.dirty = 0x3f; break;
            case CMD_REFRESH:
                s.dirty = 0x3f; break;
            case CMD_STATUS:
                ESP_LOGI(TAG, "connected=%d fault=%d brightness=%u pressed=0x%02x dirty=0x%02x",
                         s.device != NULL && !s.gone, s.fault, s.brightness, s.pressed, s.dirty);
                break;
            }
        }
        if (s.device) {
            if (s.gone) close_removed();
            else service_device();
        } else {
            for (unsigned address = 1; address < 128; ++address) {
                if (!s.pending_addresses[address]) continue;
                s.pending_addresses[address] = false;
                open_device(address);
                if (s.device) break;
            }
        }
    }
}

static void host_task(void *arg)
{
    (void)arg;
    for (;;) {
        uint32_t flags;
        ESP_ERROR_CHECK(usb_host_lib_handle_events(portMAX_DELAY, &flags));
        if (flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) usb_host_device_free_all();
    }
}

esp_err_t deck_usb_start(deck_key_callback_t callback, void *context)
{
    if (commands) return ESP_ERR_INVALID_STATE;
    commands = xQueueCreate(DECK_USB_COMMAND_QUEUE, sizeof(command_t));
    if (!commands) return ESP_ERR_NO_MEM;
    const usb_host_config_t config = {
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
        .peripheral_map = 1, // P4 USB-OTG High Speed, dedicated D+/D- -> USB-A.
        // IN MPS is 512 bytes: reserve THREE status words, not IDF's two.
        // 130 RX words causes zero-length IN storms and interrupt-OUT asserts.
        // https://github.com/espressif/esp-idf/issues/19143
        .fifo_settings_custom = {.rx_fifo_lines = 131, .nptx_fifo_lines = 64, .ptx_fifo_lines = 256}

    };
    esp_err_t err = usb_host_install(&config);
    if (err != ESP_OK) { vQueueDelete(commands); commands = NULL; return err; }
    s.key_callback = callback; s.key_context = context;
    s.brightness = DECK_INITIAL_BRIGHTNESS; s.image_key = -1;
    // A task allocation failure at boot is fatal rather than leaving a partial host.
    ESP_ERROR_CHECK(xTaskCreate(host_task, "usb_host", 4096, NULL, 5, NULL) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(xTaskCreate(client_task, "deck_client", DECK_USB_TASK_STACK, NULL, 4, NULL) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
    return ESP_OK;
}

static esp_err_t post(command_kind_t kind, uint8_t value)
{
    if (!commands) return ESP_ERR_INVALID_STATE;
    command_t command = {.kind = kind, .value = value};
    return xQueueSend(commands, &command, 0) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}
esp_err_t deck_usb_set_brightness(uint8_t percent)
{
    return percent <= 100 ? post(CMD_BRIGHTNESS, percent) : ESP_ERR_INVALID_ARG;
}
esp_err_t deck_usb_set_sleep(bool asleep) { return post(CMD_SLEEP, asleep); }
esp_err_t deck_usb_demo(void) { return post(CMD_DEMO, 0); }
esp_err_t deck_usb_status(void) { return post(CMD_STATUS, 0); }

esp_err_t deck_usb_set_image_provider(deck_image_provider_t provider, void *context)
{
    if (commands) return ESP_ERR_INVALID_STATE;
    image_provider = provider; image_context = context;
    return ESP_OK;
}
esp_err_t deck_usb_refresh_images(void) { return post(CMD_REFRESH, 0); }
