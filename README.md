# ESP32-P4 Stream Deck MQTT

Standalone firmware for the **Waveshare ESP32-P4-NANO** and **Elgato Stream Deck Mini**. Use a Stream Deck as a configurable MQTT control panel without a desktop computer or SD card.

Developed with extensive AI assistance (“vibe coding”), with automated tests and practical testing on a P4 v1.3 and Mini 0fd9:0063. Experimental community project, not affiliated with Elgato, Espressif or Waveshare.

## Features

- USB High-Speed host, six button displays and key input.
- Wi-Fi through the on-board ESP32-C6 and ESP-Hosted, with staged recovery: reconnect, C6/SDIO reset, then P4 reboot.
- JSON menus, PNG/SVG upload, text overlays and state-dependent colors.
- Embedded web interface and standalone offline HTML editor.
- MQTT commands and confirmed-state feedback; works with any backend implementing the documented protocol.
- Optional ioBroker example and Home Assistant automation example. **No automatic Home Assistant discovery.**
- Left upper button: next page. Left lower: previous page; hold two seconds for Home. Three minutes idle returns Home.

## Hardware and build

Connect the Mini directly to the board’s USB-A host port. Power/flash through USB-C; a regulated 5 V supply with sufficient reserve for both devices is required. No separate filesystem upload is needed: the web interface is embedded and menu/icons are stored in LittleFS.

**C6 firmware prerequisite:** the tested setup uses ESP-Hosted **2.12.13 on both P4 and C6**. A normal PlatformIO upload updates only the P4. See [Wi-Fi setup and recovery](docs/WIFI.md) before upgrading an older installation.

1. Install PlatformIO in VS Code, open this directory.
2. Copy `include/secrets.example.h` to `include/secrets.h`; fill in 2.4 GHz WLAN and MQTT credentials. This file is ignored by Git.
3. Select `waveshare_p4_nano` for early P4 silicon (including tested v1.3), or `waveshare_p4_nano_rev3` for revision 3. Check silicon with esptool/startup logs; the board’s printed revision is not the chip revision.
4. Build and Upload, then Monitor. Open the IP address printed in the log.
5. Adapt `examples/menu.json` or open `examples/menu-editor.html`. The supplied room names and targets are demonstration data. Upload your menu and any additional icons through the web UI.

```sh
pio run -e waveshare_p4_nano
pio run -e waveshare_p4_nano -t upload
```

The alternate Mini PID 0090 is recognized but has not been physically validated. Home Assistant YAML is an integration example, not tested on a live Home Assistant instance. The web server has no authentication: use a trusted LAN, not public Internet exposure.

## Integrations

- [MQTT protocol](docs/MQTT.md): backend-independent commands, feedback and availability.
- [Home Assistant](docs/HOME_ASSISTANT.md): allowlisted automation and feedback example.
- [ioBroker](docs/MQTT_IOBROKER.md): optional mapping-based bridge.
- [Menu format](docs/MENU.md), [web uploads](docs/WEB_ICONS.md), [Wi-Fi](docs/WIFI.md).

The HTML editor derives entity suggestions from the menu itself. Neither the firmware nor the editor requires ioBroker.

## Development

Host C tests: `./tests/run.sh` (C compiler, sanitizers, ESP-IDF cJSON headers). Bridge tests: `node tests/test_iobroker.cjs`. Browser tests require Playwright; set `PLAYWRIGHT_MODULE` to its installed module path if necessary. Build dependencies are pinned in PlatformIO and `dependencies.lock`.

After editing icons or templates, run `python3 scripts/build_starter_icons.py` (Pillow) and `python3 scripts/build_menu_editor.py`. Rebuild the optional ioBroker example with `python3 scripts/build_iobroker.py`.

## Licenses

Original project code: MIT. Third-party files retain their own licenses; see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). Distributed icons are Twemoji (CC BY 4.0), except the original orange bulb (MIT). Personal installation data and icons of unknown provenance are excluded from this repository and its history.
