idf_component_register(
    SRCS "main.c" "mini_protocol.c" "deck_descriptors.c" "deck_usb.c"
         "icon_format.c" "icon_store.c" "web_ui.c" "network.c" "network_credentials.c"
         "tile_render.c" "menu_model.c" "menu.c" "mqtt_bridge.c"
    INCLUDE_DIRS "." "../include"
    REQUIRES usb esp_timer esp_driver_uart esp_hw_support esp_http_server
             mqtt esp_netif esp_event esp_wifi nvs_flash json joltwallet__littlefs
             espressif__esp_wifi_remote espressif__esp_hosted
    EMBED_FILES "starter_icons.bin" "web/index.html" "web/app.js" "web/icon-codec.js" "web/menu-ui.js" "web/font.js" "../examples/menu.json" "../examples/menu.schema.json")
