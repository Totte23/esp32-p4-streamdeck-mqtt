# Referenzen und Abhängigkeiten

Die Firmware verwendet ESP-IDF 5.5.5 über pioarduino 55.03.312-1. Deren Quelltexte und Werkzeuge werden beim Build heruntergeladen und behalten ihre jeweiligen Lizenzen; sie sind nicht in diesem Repository vendort.

Die Mini-Paketformate und die Bildorientierung wurden mit dem Projekt **Python Stream Deck Library** von **Dean Camera** abgeglichen:

- https://github.com/abcminiuser/python-elgato-streamdeck
- `src/StreamDeck/Devices/StreamDeckMini.py`
- `src/StreamDeck/ImageHelpers/PILHelper.py`

Die C-Implementierung und die kleinen Ziffern-Bitmaps wurden für dieses Repository geschrieben. Zur Dokumentation der Protokollreferenz folgt die Lizenzdatei des Python-Projekts, abgerufen am 26.09.2026. Quelltext-Dateiköpfe des Projekts bezeichnen es als MIT; maßgeblich für diese Referenz ist der folgende tatsächlich im Repository vorhandene Lizenztext.

```
Permission to use, copy, modify, and distribute this software
and its documentation for any purpose is hereby granted without
fee, provided that the above copyright notice appear in all
copies and that both that the copyright notice and this
permission notice and warranty disclaimer appear in supporting
documentation, and that the name of the author not be used in
advertising or publicity pertaining to distribution of the
software without specific, written prior permission.

The author disclaims all warranties with regard to this
software, including all implied warranties of merchantability
and fitness.  In no event shall the author be liable for any
special, indirect or consequential damages or any damages
whatsoever resulting from loss of use, data or profits, whether
in an action of contract, negligence or other tortious action,
arising out of or in connection with the use or performance of
this software.

```

## Komponenten in Firmware 0.2

- `joltwallet/littlefs` 1.22.3: LittleFS-Einbindung; https://github.com/joltwallet/esp_littlefs
- `espressif/esp_hosted` 1.4.7 und `espressif/esp_wifi_remote` 0.14.5: C6-SDIO und WLAN-API; https://github.com/espressif/esp-hosted-mcu und https://github.com/espressif/esp-wifi-remote

Die Komponenten und ihre jeweiligen Lizenzdateien werden vom IDF Component Manager heruntergeladen; Versions- und Prüfsummenbindung siehe `dependencies.lock`. Die Weboberfläche benötigt keine extern geladenen Browserbibliotheken. Playwright/Chromium wird nur als externes Entwicklungs-Testwerkzeug verwendet und nicht auf dem ESP ausgeliefert.

## Ergänzungen in 0.3

- `src/fonts/font8x8_basic.h`, `font8x8_ext_latin.h`: Daniel Hepper, font8x8, Public Domain, https://github.com/dhepper/font8x8. Dateiheader bleiben erhalten; `src/web/font.js` enthält dieselben Glyphen als JavaScript-Daten.
- `tests/reference/python-streamdeck`: unveränderte Referenzdateien aus dem StreamController-Fork, Commit ca683c4c94143c2227d5b6f2669e2dd0d95ba7cb. Originale LICENSE liegt bei. Nur Hosttests, kein Bestandteil der Firmware. Die Quelldateiheader nennen MIT; die beiliegende tatsächliche Lizenz ist maßgeblich.
- ESP-MQTT und cJSON sind Bestandteile der bereits verwendeten ESP-IDF-Installation mit deren jeweiligen Lizenzen.


See [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md) for redistributed assets and license files.
