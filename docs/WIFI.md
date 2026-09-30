# WLAN über den C6 des Waveshare P4-NANO

Die Firmware verwendet die ESP-IDF-WLAN-API über `esp_wifi_remote` und ESP-Hosted. Der P4 kommuniziert über SDIO mit dem aufgelöteten C6. Ein LAN-Kabel wird dafür nicht benötigt.

## Festgelegte Kombination

- ESP-IDF **5.5.5** (pioarduino **55.03.312-1**)
- `espressif/esp_hosted` **1.4.7**
- `espressif/esp_wifi_remote` **0.14.5**
- LittleFS **1.22.3**

Das entspricht der von [Waveshare beschriebenen ESP-IDF-5.x-Versionslinie](https://github.com/waveshareteam/ESP32-P4-Platform/blob/main/docs/P4_C6_HOSTED_WIFI.md). `src/idf_component.yml` und `dependencies.lock` halten die Versionen fest. Diese Kombination wurde hier kompiliert, aber noch nicht mit der C6-Firmware des konkreten Boards getestet.

## Pinbelegung

Aus dem [NANO-Schaltplan](https://files.waveshare.com/wiki/ESP32-P4-NANO/ESP32-P4-NANO-schematic.pdf), explizit in `sdkconfig.defaults` gesetzt:

| P4-Signal | GPIO |
| --- | --- |
| SDIO CLK | 18 |
| SDIO CMD | 19 |
| SDIO D0 / D1 / D2 / D3 | 14 / 15 / 16 / 17 |
| C6 CHIP_PU / Reset | 54 |

Vier Datenleitungen, zunächst 20 MHz SDIO-Takt. Der Reset hält den C6 normalerweise eingeschaltet und zieht CHIP_PU zum Zurücksetzen kurz auf Low. ESP-Hosted nennt diese Einstellung `RESET_ACTIVE_HIGH` (entsprechend seinem eingeschalteten Ruhepegel). Die Streaming-Optimierung ist deaktiviert, damit sie nicht zusätzlich Unterstützung der C6-Firmware voraussetzt. Der USB-Host benutzt die dedizierten HS-Leitungen unabhängig davon.

## Zugangsdaten und Start

`include/secrets.h` bleibt lokal und von Git ausgeschlossen. `WIFI_SSID` darf 1–32 Bytes, `WIFI_PASSWORD` 8–63 Bytes enthalten (leer nur für ein offenes WLAN). Mit leerer SSID bleibt WLAN abgeschaltet. MQTT-Werte werden noch nicht verwendet.

Nach Upload meldet der Monitor die IP-Adresse, sobald WLAN und DHCP funktionieren. Verbindungsabbrüche lösen neue Verbindungsversuche aus. Die USB-Tasks laufen unabhängig vom WLAN-Setup. Der vollständige WLAN-Code wird auch bei leerer SSID mitverlinkt, damit ein Build ohne Zugangsdaten tatsächlich diese Funktion mitprüft.

## C6-Werksfirmware

Laut [Waveshare-FAQ](https://docs.waveshare.com/ESP32-P4-NANO/FAQ) wird der C6 mit Firmware ausgeliefert. Der normale PlatformIO-Upload dieses Projekts **programmiert ausschließlich den P4**. Die Firmware benutzt zunächst die vorhandene C6-Firmware; sie überschreibt oder aktualisiert sie nicht automatisch.

Wenn der C6 bereits eine passende ESP-Hosted-SDIO-Firmware ausführt, ist kein zweiter Upload nötig. Falls im Monitor SDIO-/RPC-Fehler, Protokoll-Inkompatibilität oder keine C6-Antwort erscheinen, müssen wir dessen Version am echten Board abgleichen. Ein erfolgreicher P4-Build allein kann das nicht nachweisen.

Für eine eventuell erforderliche passende C6-Firmware liegt der ursprüngliche Slave-Quellcode nach dem ersten Build unter `managed_components/espressif__esp_hosted/slave/`. Die Komponente verweist auf Espressif-Commit `d24d1dc3f709965fa61e3a1304c0896a9f3d497c` (1.4.7). Eine separate Kopie dieses vollständigen Upstream-Repositories an diesem Commit enthält alle gemeinsamen Quellen. Espressif beschreibt Konfiguration und Aufbau in seiner [SDIO-Anleitung für diesen Stand](https://github.com/espressif/esp-hosted-mcu/blob/d24d1dc3f709965fa61e3a1304c0896a9f3d497c/docs/sdio.md). Ziel ist `esp32c6`, Transport SDIO; das ist ein eigenes Projekt, nicht eines der zwei P4-Environments.

Zum direkten Programmieren des C6 beschreibt Waveshare den separaten C6-UART-Anschluss, C6_IO9 auf Low beim Einschalten sowie den P4 im Downloadmodus. Dafür ist ein geeigneter 3,3-V-UART-Adapter erforderlich. Erst Version und Boardzustand prüfen; dieses Projekt nimmt diesen Hardwareeingriff nicht vor. Es wurde kein C6-Flash oder C6-OTA ausgeführt.

## Abnahme am Board

- Kaltstart: C6-Handshake, WLAN-Anmeldung, DHCP-Adresse, Browserzugriff.
- SVG/PNG hochladen, Taste zuweisen und Bild auf dem Mini prüfen.
- P4 neustarten: Icons und Zuordnung erhalten, WLAN kommt wieder.
- WLAN kurz abschalten: USB-Tasten bleiben bedienbar; Webzugriff kehrt nach Reconnect zurück.
- Uploads und USB-Bildübertragung gleichzeitig, anschließend mehrere Warm-/Kaltstarts.
