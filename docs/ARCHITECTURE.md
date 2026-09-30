# Architektur und nächste Schritte

## Module

- `mini_protocol.c`: portable C-Funktionen für Eingabe, Helligkeit, 1024-Byte-Bildreports und ein synthetisches 80×80-BMP. Kein ESP-IDF, kein Netzwerk.
- `deck_descriptors.c`: begrenzter Deskriptorparser für ein HID-Interface mit Interrupt-IN und -OUT, Alternate Setting 0. Prüft Längen, Interfacezuordnung und Paketgrößen.
- `deck_usb.c`: ESP-IDF-Host, Gerätelebenszyklus, Transfers und Demo-Zustand. Ein USB-Client-Task besitzt sämtliche veränderlichen Geräte-/Transferdaten. Der separate Host-Task verarbeitet die Bibliotheksereignisse. Öffnen, Claim, Submit, Halt/Flush und Close erfolgen außerhalb der Callbacks.
- `icon_format.c` / `icon_store.c`: BMP-Validierung, LittleFS, atomarer Dateiersatz und dauerhafte Zuordnung. Ein Mutex schützt Speicherzugriffe zwischen HTTP- und USB-Task.
- `web_ui.c` / `src/web/`: HTTP-Endpunkte und eingebettete Oberfläche. Der Browser wandelt SVG/PNG in native Mini-BMPs um; die Firmware prüft Größe und Format.
- `network.c` / `network_credentials.c`: C6-WLAN über ESP-Hosted; getrennte Zugangsdaten-Übersetzungseinheit verhindert das Wegoptimieren der gesamten WLAN-Implementierung bei leerer SSID.
- `main.c`: UART-Konsole und Tasten-Callback. UART0 auf GPIO37/38 entspricht dem CH343 des NANO.

Ein IN-Transfer bleibt während der Ausgabe aktiv. Protokollgültige Zustandsänderungen erzeugen Flankenereignisse. Bildübertragungen bestehen aus 20 Reports; während einer Übertragung ist der BMP-Puffer ein unveränderlicher Schnappschuss. Neue Tastendrücke markieren die Kachel erneut als geändert, damit schnelle Änderungen nicht zu gemischten Bilddaten führen. Es gibt genau einen OUT-Transfer und einen Control-Transfer, die nacheinander verwendet werden.

Feature SET_REPORT setzt die Helligkeit über Endpoint 0 (`bmRequestType=0x21`, `bRequest=9`, `wValue=0x0305`, `wIndex=HID-Interface`). Für das ursprüngliche Mini werden die bewährten 17-Byte-Feature-Reports verwendet. Elgatos aktuelle Familiendokumentation nennt an manchen Stellen andere/paddingabhängige Größen; die Mini-Implementierung der Python-Bibliothek und das ursprüngliche Gerät waren hier maßgeblich. Eingabereports akzeptieren mindestens sieben Bytes und ignorieren Padding.

Das Bildformat übernimmt die in python-elgato-streamdeck verwendete Transformation (90° gegen den Uhrzeigersinn, dann vertikal spiegeln; danach BMP-typische BGR-Zeilen von unten). Diese Orientierung muss am vorhandenen Mini visuell bestätigt werden. Der Encoder ist absichtlich auf genau 80×80, 24-Bit-BMP mit 54-Byte-Header beschränkt.

## PlatformIO-Build

`scripts/chip_info.py` ergänzt das auslesende Custom-Target und die Revisionsgrenzen bei `elf2image`. Die gepinnte pioarduino-Version übernimmt diese Grenzen nicht automatisch aus ESP-IDF. Der Hook setzt sie für Bootloader und Anwendung anhand der generierten SDK-Konfiguration und bricht bei einem falschen Revisionsbereich ab. `tests/check_images.py` prüft die fertigen Images einschließlich Hash und Factory-Offsets.

## USB-Fehler und Grenzen

Das P4-High-Speed-Peripheral nutzt einen eigenen periodischen TX-FIFO von 256 32-Bit-Wörtern, damit ein 1024-Byte-Interrupt-OUT-Paket hineinpasst. RX erhält 512 und nichtperiodisches TX 256 Wörter. Die Vorgabe ist für den P4-HS-Controller gedacht, nicht für ESP32-S3/FS.

Bei Trennung werden IN/OUT angehalten und gespült; Transferpuffer bleiben bis zum jeweiligen Abschluss-Callback gültig. Endpoint-0-Abbrüche verwaltet die Hostbibliothek. Erst danach werden Interface, Gerät und Puffer freigegeben. Gehaltene Tasten erzeugen beim Trennen Release-Ereignisse.

Das SDK unterstützt `usb_transfer_t.timeout_ms` nicht allgemein. Deshalb überwacht der Client selbst OUT/Control mit drei Sekunden. Ein hängendes Gerät wird als Fehler behandelt und muss neu eingesteckt werden. Bei einem noch ausstehenden Control-Transfer wird der Puffer bis zur Entfernung behalten. IN erhält keine Leerlauf-Zeitgrenze: ohne Tastendruck sind NAKs normal.

Diese Version unterstützt ein direkt angeschlossenes Mini und keine garantierte Hub-/Mehrgeräteverwaltung, keine beliebigen HID-Geräte, keine animierten Bilder, kein OTA. Bilder und Zuordnungen sind persistent; Helligkeit und Demo-Toggle-Zustände nicht. Automatisierte Tests ersetzen keinen Test von USB-DMA, Stromversorgung, Geräterevisionen oder Hotplug auf realer Hardware.

## MQTT/ioBroker

Der kurze Tasten-Callback legt Ereignisse in die Menü-Queue. Erst der Menü-Task sendet validierte `entity`/`action`-Nachrichten. Konfigurations- und Verbindungszähler verwerfen veraltete Tastendrücke. MQTT-Verbindungsaufbau, Reconnect, Last Will und Statuscache sind implementiert. Das lokale ioBroker-Skript löst logische Entities über eine feste Aliaszuordnung auf; Details in [MQTT_IOBROKER.md](MQTT_IOBROKER.md).

Der ursprüngliche Aufbau wurde mit Mini/P4 v1.3 praktisch geprüft; die öffentliche Beispielkonfiguration muss für das eigene MQTT-Backend angepasst werden. Header-Secrets konfigurieren WLAN und MQTT; eine leere MQTT-URI deaktiviert die Verbindung. LittleFS und Weboberfläche benötigen kein separates Filesystem-Image.

## Persistente Bilder

Der USB-Client ruft einen vor dem Start registrierten Bildanbieter auf, wenn ein neuer Bild-Schnappschuss beginnt. Dieser liest die gespeicherte Tastenzuordnung und das BMP unter dem Store-Mutex. Der Mutex schützt keine ganze USB-Übertragung: Nach der Kopie bleibt der Upload-Puffer unabhängig. Änderungen markieren alle Tasten über die bestehende Queue als geändert. Bei ungültiger/fehlender Datei fällt die Taste auf die Demo zurück.

Uploads werden vollständig in einem begrenzten RAM-Puffer empfangen und validiert, danach über temporäre Datei, fsync/close und LittleFS-rename ersetzt. Dateinamen sind auf ASCII-IDs beschränkt. Die Oberfläche ist als Firmware-Ressource verfügbar, unabhängig von der Datenpartition. Kein automatisches Formatieren eines nichtleeren, beschädigten Dateisystems.

## Menü-Erweiterung 0.3

`menu_model.c` validiert die deklarative JSON-Struktur. `menu.c` verwaltet atomare Speicherung, aktuelle Seite und Statuscache unter einem Mutex; Tastenereignisse gehen über eine Queue an den Menü-Task. `tile_render.c` komponiert Hintergrund, RGBA-Icon und Text direkt in die native Mini-BMP-Pufferorientierung. Der USB-Task behält weiterhin seinen eigenen unveränderlichen Schnappschuss pro Übertragung. `mqtt_bridge.c` läuft über ESP-MQTT, empfängt fragmentierte Statusmeldungen und sendet Kommandos ohne Retain/Offline-Queue.

Die Bilddateien behalten intern die Endung `.bmp`: entweder altes 19.254-Byte-Mini-BMP oder neues 25.600-Byte-RGBA (logisch oben links, zeilenweise). Die exakte Länge unterscheidet beide Formate. Ein atomarer Dateiersatz hält Farbe und Alpha gemeinsam konsistent. Die Gesamtgrenze bleibt 64 Icons/2 MiB LittleFS.
