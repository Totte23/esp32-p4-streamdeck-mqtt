// Driver state-machine simulation. No USB electrical/hardware behavior is emulated.
#include "../src/deck_usb.c"

static int64_t now_us;
static unsigned allocations, releases, closes, events, last_key;
static bool last_pressed;
static int device_token;
static const uint8_t config_bytes[] = {
    9,2,41,0,1,1,0,0x80,100,
    9,4,0,0,2,3,0,0,0,
    9,0x21,0x11,1,0,1,0x22,42,0,
    7,5,0x81,3,64,0,1,
    7,5,0x02,3,0,4,1
};
static usb_config_desc_t fake_config;

int64_t esp_timer_get_time(void) { return now_us; }
QueueHandle_t xQueueCreate(unsigned n, unsigned size) { (void)n; (void)size; return &device_token; }
void vQueueDelete(QueueHandle_t q) { (void)q; }
int xQueueReceive(QueueHandle_t q, void *p, unsigned w) { (void)q; (void)p; (void)w; return 0; }
int xQueueSend(QueueHandle_t q, const void *p, unsigned w) { (void)q; (void)p; (void)w; return pdTRUE; }
int xTaskCreate(void (*t)(void *), const char *n, unsigned st, void *a, unsigned p, void *h)
{ (void)t; (void)n; (void)st; (void)a; (void)p; (void)h; return pdPASS; }

esp_err_t usb_host_transfer_alloc(size_t size, int iso, usb_transfer_t **out)
{
    (void)iso;
    *out = calloc(1, sizeof(**out)); assert(*out);
    (*out)->data_buffer = calloc(1, size); assert((*out)->data_buffer);
    ++allocations; return ESP_OK;
}
esp_err_t usb_host_transfer_free(usb_transfer_t *t)
{
    if (t) { assert(!t->test_inflight); free(t->data_buffer); free(t); --allocations; }
    return ESP_OK;
}
esp_err_t usb_host_transfer_submit(usb_transfer_t *t)
{ assert(!t->test_inflight); t->test_inflight = true; return ESP_OK; }
esp_err_t usb_host_transfer_submit_control(usb_host_client_handle_t c, usb_transfer_t *t)
{ (void)c; return usb_host_transfer_submit(t); }
esp_err_t usb_host_endpoint_halt(usb_device_handle_t d, uint8_t ep)
{ (void)d; assert(ep); return ESP_OK; }
esp_err_t usb_host_endpoint_flush(usb_device_handle_t d, uint8_t ep)
{ (void)d; assert(ep); return ESP_OK; } // Callbacks are deliberately delivered later.
esp_err_t usb_host_interface_release(usb_host_client_handle_t c, usb_device_handle_t d, uint8_t i)
{ (void)c; (void)d; (void)i; assert(!s.in_busy && !s.out_busy && !s.ctrl_busy); ++releases; return ESP_OK; }
esp_err_t usb_host_interface_claim(usb_host_client_handle_t c, usb_device_handle_t d, uint8_t i, uint8_t a)
{ (void)c; (void)d; assert(i == 0 && a == 0); return ESP_OK; }
esp_err_t usb_host_device_close(usb_host_client_handle_t c, usb_device_handle_t d)
{ (void)c; (void)d; ++closes; return ESP_OK; }
esp_err_t usb_host_device_open(usb_host_client_handle_t c, uint8_t a, usb_device_handle_t *d)
{ (void)c; (void)a; *d = &device_token; return ESP_OK; }
esp_err_t usb_host_get_device_descriptor(usb_device_handle_t d, const usb_device_desc_t **out)
{ (void)d; static const usb_device_desc_t desc = {0x0fd9,0x0063}; *out = &desc; return ESP_OK; }
esp_err_t usb_host_get_active_config_descriptor(usb_device_handle_t d, const usb_config_desc_t **out)
{ (void)d; memcpy(&fake_config, config_bytes, sizeof(config_bytes)); *out = &fake_config; return ESP_OK; }
esp_err_t usb_host_device_info(usb_device_handle_t d, usb_device_info_t *info)
{ (void)d; info->speed = USB_SPEED_HIGH; return ESP_OK; }
esp_err_t usb_host_client_register(const usb_host_client_config_t *c, usb_host_client_handle_t *h)
{ (void)c; *h = &device_token; return ESP_OK; }
esp_err_t usb_host_client_handle_events(usb_host_client_handle_t c, unsigned w)
{ (void)c; (void)w; return ESP_OK; }
esp_err_t usb_host_lib_handle_events(unsigned w, uint32_t *f)
{ (void)w; *f = 0; return ESP_OK; }
esp_err_t usb_host_device_free_all(void) { return ESP_OK; }
esp_err_t usb_host_install(const usb_host_config_t *c)
{ assert(c->fifo_settings_custom.ptx_fifo_lines >= 256 && c->peripheral_map == 1);
  // 512-byte interrupt IN needs three overhead words (IDFGH-18354).
  assert(c->fifo_settings_custom.rx_fifo_lines >= 512 / 4 + 3); return ESP_OK; }

static void key(uint8_t k, bool pressed, void *ctx)
{ (void)ctx; ++events; last_key = k; last_pressed = pressed; }
static void complete(usb_transfer_t *t, int status)
{
    assert(t && t->test_inflight);
    t->test_inflight = false; t->status = status;
    t->actual_num_bytes = status == USB_TRANSFER_STATUS_COMPLETED ? t->num_bytes : 0;
    t->callback(t);
}
static void press(unsigned k)
{
    memset(s.input->data_buffer, 0, s.input->num_bytes);
    s.input->data_buffer[0] = 1; s.input->data_buffer[1 + k] = 1;
    complete(s.input, USB_TRANSFER_STATUS_COMPLETED);
}
static void removed(void)
{
    usb_host_client_event_msg_t e = {.event = USB_HOST_CLIENT_EVENT_DEV_GONE, .dev_gone.dev_hdl = s.device};
    client_event(&e, NULL);
}
static void reconnect(void)
{
    open_device(1); assert(s.device && s.claimed && allocations == 3);
    service_device(); assert(s.in_busy && s.ctrl_busy);
}

static unsigned provider_calls;
static bool provider(uint8_t key_number, uint8_t *bmp, void *context)
{
    (void)context;
    ++provider_calls;
    if (key_number != 0) return false;
    mini_demo_bmp(5, true, bmp);
    return true;
}

int main(void)
{
    assert(deck_usb_set_image_provider(provider, NULL) == ESP_OK);
    assert(deck_usb_start(key, NULL) == ESP_OK);
    reconnect();
    assert(s.control->data_buffer[0] == 0x21 && s.control->data_buffer[3] == 3);
    assert(s.control->data_buffer[13] == 30);
    complete(s.control, USB_TRANSFER_STATUS_COMPLETED);
    service_device(); assert(s.out_busy && s.reset_stream);
    complete(s.output, USB_TRANSFER_STATUS_COMPLETED);
    service_device(); assert(s.image_key == 0 && s.page == 0);
    // Press while that key's image is in-flight: the snapshot must stay stable,
    // and the dirty bit must request another complete image afterwards.
    uint8_t snapshot[MINI_BMP_BYTES]; memcpy(snapshot, s.bmp, sizeof(snapshot));
    uint8_t expected[MINI_BMP_BYTES]; mini_demo_bmp(5, true, expected);
    assert(provider_calls && !memcmp(snapshot, expected, sizeof(expected)));
    assert(deck_usb_set_image_provider(NULL, NULL) == ESP_ERR_INVALID_STATE);
    press(0); assert(events == 1 && last_key == 0 && last_pressed);
    assert(s.dirty & 1); assert(!memcmp(snapshot, s.bmp, sizeof(snapshot)));
    unsigned pages = 0;
    do {
        if (s.out_busy) { complete(s.output, USB_TRANSFER_STATUS_COMPLETED); ++pages; }
        service_device();
        assert(pages <= 140);
    } while (s.out_busy || s.dirty || s.image_key >= 0);
    assert(pages == 140); // Six original tiles plus the changed first tile.
    removed(); close_removed(); assert(s.device && releases == 0 && allocations == 3);
    complete(s.input, USB_TRANSFER_STATUS_CANCELED);
    close_removed(); assert(!s.device && !allocations && releases == 1 && closes == 1);
    assert(events == 3 && !last_pressed); // Release held key on disconnect.

    // Unplug during control transfer: input completes first, control much later.
    reconnect(); removed(); close_removed();
    complete(s.input, USB_TRANSFER_STATUS_CANCELED); close_removed();
    assert(s.device && allocations == 3 && releases == 1);
    complete(s.control, USB_TRANSFER_STATUS_CANCELED); close_removed();
    assert(!s.device && !allocations && releases == 2);

    // Control timeout keeps memory alive until removal and completion.
    reconnect(); now_us += 3000001; service_device(); assert(s.fault);
    service_device(); assert(s.cancel_started && allocations == 3);
    complete(s.input, USB_TRANSFER_STATUS_CANCELED);
    removed(); close_removed(); assert(s.device);
    complete(s.control, USB_TRANSFER_STATUS_CANCELED); close_removed();
    assert(!s.device && !allocations);

    // OUT removal likewise waits for both callbacks, independent of order.
    reconnect(); complete(s.control, USB_TRANSFER_STATUS_COMPLETED); service_device();
    assert(s.out_busy); removed(); close_removed();
    complete(s.output, USB_TRANSFER_STATUS_CANCELED); close_removed(); assert(s.device);
    complete(s.input, USB_TRANSFER_STATUS_CANCELED); close_removed();
    assert(!s.device && !allocations && releases == 4);
    puts("PASS: concurrent keys/images, hotplug, delayed cancellation, control timeout, reconnect");
    return 0;
}
