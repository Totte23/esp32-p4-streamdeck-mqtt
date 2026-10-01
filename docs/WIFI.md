# WLAN über den C6 des Waveshare P4-NANO

Die Firmware verwendet die ESP-IDF-WLAN-API über `esp_wifi_remote` und ESP-Hosted. Der P4 kommuniziert über SDIO mit dem aufgelöteten C6. Ein LAN-Kabel wird dafür nicht benötigt.

## Festgelegte Kombination

- ESP-IDF **5.5.5** (pioarduino **55.03.312-1**)
- `espressif/esp_hosted` **2.12.13**
- `espressif/esp_wifi_remote` **0.14.5**
- LittleFS **1.22.3**

Auf dem getesteten Board läuft zusätzlich auf dem C6 **ESP-Hosted 2.12.13**, gebaut mit ESP-IDF 5.5.5 im SDIO-Paketmodus. Diese Kombination ersetzt seit 2026-10-01 die frühere P4-Version 1.4.7 mit unbekannter C6-Werksfirmware. Das Testboard bestätigte nach SDIO-OTA und Neustart die C6-Version 2.12.13; andere Boards werden durch einen P4-Upload nicht automatisch aktualisiert. Die Langzeitstabilität muss noch geprüft werden.

Dies weicht von Waveshares konservativer IDF-5.x-Empfehlung (Hosted 1.4.x) ab. Espressifs Komponente 2.12.13 unterstützt laut eigenem Manifest IDF >=5.3; Build und C6-Handshake wurden hier geprüft. `src/idf_component.yml` und `dependencies.lock` halten die Host-Abhängigkeiten fest.

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

`include/secrets.h` bleibt lokal und von Git ausgeschlossen. `WIFI_SSID` darf 1–32 Bytes, `WIFI_PASSWORD` 8–63 Bytes enthalten (leer nur für ein offenes WLAN). Mit leerer SSID bleibt WLAN abgeschaltet. MQTT-Zugangsdaten werden von der MQTT-Bridge verwendet.

Nach Upload meldet der Monitor die IP-Adresse, sobald WLAN und DHCP funktionieren. Verbindungsabbrüche lösen neue Verbindungsversuche aus. Die USB-Tasks laufen unabhängig vom WLAN-Setup. Der vollständige WLAN-Code wird auch bei leerer SSID mitverlinkt, damit ein Build ohne Zugangsdaten tatsächlich diese Funktion mitprüft.

## C6-Update und Wiederherstellung

Der normale PlatformIO-Upload programmiert ausschließlich den P4, nicht den C6. Die getestete Kombination benötigt auch auf dem C6 ESP-Hosted 2.12.13 im SDIO-Paketmodus. Eine ältere Werksfirmware ist damit nicht automatisch verifiziert.

Ein vorhandener, funktionsfähiger C6 kann mit Espressifs [SDIO-OTA-Beispiel](https://github.com/espressif/esp-hosted-mcu/tree/v2.12.13/examples/host_performs_slave_ota) aktualisiert werden. Dieses Projekt führt kein automatisches C6-Update aus und enthält keinen C6-Updater. Vor dem Update passende Zielkonfiguration, Image und Wiederherstellungsweg prüfen; eine P4-Sicherung enthält nicht den C6-Flash. Flash-Sicherungen können Zugangsdaten enthalten und gehören nicht ins Repository.

Falls SDIO nicht mehr startet, erfordert die Wiederherstellung den separaten C6-UART-Anschluss mit 3,3-V-Logik. Waveshare beschreibt C6_IO9 auf Low beim Einschalten sowie den P4 im Downloadmodus in der [FAQ](https://docs.waveshare.com/ESP32-P4-NANO/FAQ). Der normale USB-C-Port erreicht nur den P4.

## Größe des Starter-Pakets

Der neue Treiber braucht mehr Flash-Platz. `scripts/build_starter_icons.py` bettet deshalb nur die 18 vom Demo-Menü referenzierten Icons ein. Die vollständige Sammlung mit Lizenzangaben bleibt in `assets/starter-icons`; zusätzliche Icons lassen sich über die Weboberfläche hochladen. Bestehende LittleFS-Inhalte bleiben erhalten, die Partitionen ändern sich nicht. Nach Änderungen am eingebetteten Menü das Icon-Paket und anschließend den Offline-Editor neu erzeugen.

## Abnahme am Board

- Kaltstart: C6-Handshake, WLAN-Anmeldung, DHCP-Adresse, Browserzugriff.
- SVG/PNG hochladen, Taste zuweisen und Bild auf dem Mini prüfen.
- P4 neustarten: Icons und Zuordnung erhalten, WLAN kommt wieder.
- WLAN kurz abschalten: USB-Tasten bleiben bedienbar; Webzugriff kehrt nach Reconnect zurück.
- Uploads und USB-Bildübertragung gleichzeitig, anschließend mehrere Warm-/Kaltstarts.

## WLAN-Watchdog

Ein unabhängiger Task überwacht alle fünf Sekunden die Netzwerkgesundheit. Der Netzwerktask prüft ungefähr alle zehn Sekunden die Verbindung per C6-RPC (`esp_wifi_sta_get_ap_info`) und per ICMP-Ping zum lokalen Standardgateway. Eine gemeldete IP allein genügt nicht: die letzte erfolgreiche Prüfung darf höchstens 25 Sekunden alt sein. So kann auch ein blockierter RPC-/SDIO-Aufruf zum Neustart führen, ohne dass der Überwachungstask auf diesen Aufruf warten muss.

Ab erkanntem Fehler eskaliert die Wiederherstellung:

1. Nach 30 Sekunden: WLAN trennen und erneut verbinden.
2. Nach 90 Sekunden: Netif stoppen, WLAN und Hosted abbauen, SDIO samt C6-Reset neu initialisieren und WLAN neu konfigurieren.
3. Nach 180 Sekunden: P4 neu starten; dessen Start setzt auch den C6 zurück.

Jede Stufe wird pro Störung einmal angefordert. Nach 60 Sekunden durchgehend gesundem Zustand wird die Eskalation zurückgesetzt. Kurze Zwischenverbindungen löschen die Fehlerhistorie nicht. Nach einem Watchdog-Neustart bleibt ein weiterer Watchdog-Neustart bis 600 Sekunden Laufzeit gesperrt; Reconnect und C6-Reset sind währenddessen möglich. NVS, Menü, Icons und Zugangsdaten werden nicht gelöscht. Ohne WLAN-Konfiguration startet kein Watchdog.

Router, die ICMP sperren, lösen keine dauernde Neustartschleife aus: Der Datenpfad wird erst verpflichtend überwacht, nachdem das aktuelle Gateway mindestens einmal geantwortet hat. Bis dahin gelten die C6-/Link-Prüfungen; das wird im Log kenntlich gemacht. In diesem Fallback kann ein reiner Datenpfadfehler bei funktionierendem RPC unentdeckt bleiben. Ein Router-/WLAN-Ausfall kann dagegen berechtigt alle Stufen auslösen. Internet, MQTT und ioBroker sind keine Prüfziele; ein alleiniger Broker-Ausfall löst keine Wiederherstellung aus.

Alle blockierenden Treiberaufrufe und die Recovery laufen im selben Netzwerktask. Der unabhängige Supervisor ruft keine C6-APIs auf. Hängt eine niedrigere Stufe, kann der Supervisor trotzdem den P4-Neustart auslösen; bereits wartende Reconnect-/Reset-Anforderungen werden nicht parallel ausgeführt.

USB-Konsole zur gezielten Diagnose:

- `wifi-reconnect`: Stufe 1 anfordern.
- `wifi-reset`: Stufe 2 anfordern (keine Neuprogrammierung des C6).
- `menu-default`: gespeichertes Menü ausdrücklich durch die eingebettete Vorlage ersetzen; vorher eigene Änderungen exportieren.

Die Reihenfolge beim Transport-Neustart orientiert sich an Espressifs [Hosted-2.12.13-Beispiel](https://github.com/espressif/esp-hosted-mcu/tree/v2.12.13/examples/host_shuts_down_slave_to_power_save). Der normale PlatformIO-Upload aktualisiert weiterhin nur den P4.
