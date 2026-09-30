# Third-party notices

The top-level MIT license applies to original project contributions, not as a replacement for third-party terms.

## Python Stream Deck Library — Dean Camera and contributors

Protocol behavior, image orientation and packet construction were compared against https://github.com/abcminiuser/python-elgato-streamdeck and its https://github.com/StreamController/streamcontroller-python-elgato-streamdeck fork.

Reference Python files in `tests/reference/python-streamdeck` originate from fork commit `ca683c4c94143c2227d5b6f2669e2dd0d95ba7cb`. Original headers and `LICENSE` are preserved. Headers describe MIT, while the actual accompanying permission notice has different wording; we preserve that notice verbatim rather than relabel it. These files are used in host tests, not embedded in the firmware. See `docs/PYTHON_REVIEW.md` and `docs/THIRD_PARTY.md`.

## font8x8 — Daniel Hepper and upstream contributors

https://github.com/dhepper/font8x8 — public domain according to original headers. Headers remain in `src/fonts/`. `src/web/font.js` contains corresponding glyph data.

## Twemoji — Twitter, Inc. and other contributors

https://github.com/jdecked/twemoji — graphics licensed under CC BY 4.0. Full license and attribution: `assets/starter-icons/LICENSE-TWEMOJI.txt` and `TWEMOJI-NOTICE.txt`. Per-file upstream URLs and SHA-256 hashes: `assets/starter-icons/manifest.json`. PNGs are reused under local IDs and resized/padded into the firmware RGBA bundle and embedded HTML editor. The original orange bulb SVG and PNG rendering are project artwork under MIT.

## Firmware dependencies

ESP-IDF (including ESP-MQTT), ESP-Hosted, ESP Wi-Fi Remote and their Espressif transport components: upstream license files copied into `licenses/`. LittleFS wrapper and underlying littlefs: likewise retained. cJSON: MIT; license copied into `licenses/`. Dependencies are downloaded by the build, with versions pinned by `platformio.ini` and `dependencies.lock`. When distributing compiled firmware, retain this notice and relevant dependency notices.

Upstream sources:
- https://github.com/espressif/esp-idf
- https://github.com/espressif/esp-hosted-mcu
- https://github.com/espressif/esp-wifi-remote
- https://github.com/joltwallet/esp_littlefs
- https://github.com/littlefs-project/littlefs
- https://github.com/DaveGamble/cJSON

PlatformIO/pioarduino and Playwright are external build/test tools, not bundled application code. StreamController is an inspiration for menu-based control; its desktop application is not bundled. No Teensy implementation is bundled. Elgato and Stream Deck are trademarks of their respective owners; no endorsement is implied.
