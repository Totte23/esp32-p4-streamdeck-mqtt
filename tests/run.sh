#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p build/tests
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -pedantic \
  -fsanitize=address,undefined -fno-omit-frame-pointer -g -Iinclude \
  src/mini_protocol.c src/deck_descriptors.c tests/test_protocol.c \
  -o build/tests/test_protocol
./build/tests/test_protocol
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -pedantic \
  -fsanitize=address,undefined -fno-omit-frame-pointer -g -Itests/fakes -Iinclude \
  src/mini_protocol.c src/deck_descriptors.c tests/test_usb_lifecycle.c \
  -o build/tests/test_usb_lifecycle
./build/tests/test_usb_lifecycle
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -pedantic \
  -D_POSIX_C_SOURCE=200809L '-DICON_STORE_BASE="build/tests/icon-data"' \
  -fsanitize=address,undefined -fno-omit-frame-pointer -g -Itests/fakes -Iinclude \
  src/mini_protocol.c src/icon_format.c src/tile_render.c tests/test_icon_store.c \
  -o build/tests/test_icon_store
./build/tests/test_icon_store
CJSON_DIR="${CJSON_DIR:-$HOME/.platformio/packages/framework-espidf/components/json/cJSON}"
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -pedantic \
  -D_POSIX_C_SOURCE=200809L '-DMENU_STORE_BASE="build/tests/menu-data"' \
  -fsanitize=address,undefined -fno-omit-frame-pointer -g -Itests/fakes -Iinclude -I"$CJSON_DIR" \
  src/mini_protocol.c src/icon_format.c src/tile_render.c src/menu_model.c "$CJSON_DIR/cJSON.c" tests/test_menu.c \
  -o build/tests/test_menu
./build/tests/test_menu
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -pedantic \
  -fsanitize=address,undefined -fno-omit-frame-pointer -g -Itests/fakes -Iinclude -I"$CJSON_DIR" \
  -D_POSIX_C_SOURCE=200809L src/icon_format.c src/menu_model.c "$CJSON_DIR/cJSON.c" tests/test_mqtt.c \
  -o build/tests/test_mqtt
./build/tests/test_mqtt
