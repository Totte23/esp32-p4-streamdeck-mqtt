# Erfahrungsbericht: WLAN-Ausfälle und C6-Firmware

Stand: 1. Oktober 2026. Getestet mit Waveshare ESP32-P4-NANO, P4-Silizium v1.3 und Stream Deck Mini.

**Nach dem Abgleich der P4- und C6-Firmware funktionieren WLAN, MQTT und HTTP in unseren bisherigen Tests wieder. Eine dauerhaft beseitigte Fehlerursache ist damit noch nicht nachgewiesen.** Dieser Bericht beschreibt unsere Beobachtungen und den gewählten Lösungsweg, keine allgemeine Reparaturgarantie für jedes P4/C6-Board.

## Das beobachtete Problem

Nach einiger Betriebszeit funktionierten Seitenwechsel am Stream Deck weiterhin, aber Gerätezustände erschienen als `?` und Schaltbefehle kamen nicht mehr an. Auch die Weboberfläche war zeitweise nicht erreichbar. Ein Neustart stellte die Verbindung wieder her. Im MQTT-Broker war ein Verbindungsabbruch des Panels sichtbar; der Broker selbst lief weiter.

Die lokale USB-/Menüverarbeitung war somit noch aktiv. Wir vermuteten ein Problem im Netzwerkpfad zwischen P4, C6 und WLAN. Das ist eine Eingrenzung, kein Beweis für einen bestimmten SDIO-Bug. Ein Watchdog, der nur auf WLAN-Disconnect-/IP-Verlust-Ereignisse reagiert, hilft nicht, wenn ein hängender Treiber weiter „verbunden“ meldet.

## Was die Recherche ergeben hat

Im Espressif-Issue-Tracker finden sich ähnliche Berichte:

- [Issue #167](https://github.com/espressif/esp-hosted-mcu/issues/167): nicht wiederherstellbarer SDIO-Zustand bei P4/C6, unter anderem auf P4 v1.3; Ausfälle nach kurzer oder längerer Laufzeit, auch bei 20 MHz.
- [Issue #184](https://github.com/espressif/esp-hosted-mcu/issues/184): eingehende TCP-Übertragung bleibt bei einem anderen P4/C6-Aufbau hängen.
- [Issue #210](https://github.com/espressif/esp-hosted-mcu/issues/210): unterschiedliche Fehler in Streaming- und Paketmodus, darunter nicht mehr antwortender Slave und RPC-Timeouts.

Diese Berichte betreffen teilweise andere Boards, Softwarestände und Lastprofile. Sie waren Anlass, den C6 und die Transportversionen genauer zu prüfen. Sie belegen weder, dass unser Fehler identisch war, noch dass Version 2.12.13 alle dort beschriebenen Fehler behebt.

[Waveshares Hinweise zu Hosted Wi-Fi](https://github.com/waveshareteam/ESP32-P4-Platform/blob/main/docs/P4_C6_HOSTED_WIFI.md) behandeln P4-Host und C6-Firmware als gemeinsam zu prüfende Kombination. Ein erfolgreicher P4-Build allein bestätigt nicht die Kompatibilität der vorhandenen C6-Firmware.

## Ausgangslage und gewählter Lösungsweg

Auf dem P4 war zunächst ESP-Hosted 1.4.7 eingebunden. Die vorhandene C6-Firmware beantwortete die Versionsabfrage nicht. **Ihre Version blieb unbekannt**; eine fehlende Versionsantwort beweist für sich genommen weder einen Defekt noch eine bestimmte Versionsnummer.

Wir haben anschließend folgende Kombination aufgebaut und geprüft:

| Bestandteil | Getesteter Stand |
| --- | --- |
| ESP-IDF | 5.5.5 |
| PlatformIO-Plattform | pioarduino 55.03.312-1 |
| ESP-Hosted auf dem P4 | 2.12.13 |
| ESP-Hosted-Anwendung auf dem C6 | 2.12.13 |
| esp_wifi_remote | 0.14.5 |
| SDIO | 4 Bit, 20 MHz, Paketmodus ohne Streaming |
| P4-Build für das Testboard | `waveshare_p4_nano`, nicht `waveshare_p4_nano_rev3` |

Diese Kombination weicht von Waveshares konservativer IDF-5.x-Versionslinie mit Hosted 1.4.x ab. Das [Komponentenmanifest von Hosted 2.12.13](https://github.com/espressif/esp-hosted-mcu/blob/v2.12.13/idf_component.yml) erlaubt IDF ab 5.3. Entscheidend waren zusätzlich unsere Builds und die Prüfung am realen Board; daraus folgt keine Freigabe beliebiger Versionsmischungen.

Der tatsächliche Ablauf:

1. Den benutzten P4-Flashbereich einschließlich Einstellungen, Menü und Bildern lokal sichern. Diese Sicherung enthält möglicherweise Zugangsdaten und wurde nicht veröffentlicht.
2. Die C6-Anwendung aus [ESP-Hosted v2.12.13](https://github.com/espressif/esp-hosted-mcu/tree/v2.12.13), Commit `dd0176e5fc959d79b306f6677a2f878123c58e9a`, für ESP32-C6 im SDIO-Paketmodus bauen und das Image prüfen.
3. Vorübergehend einen separaten Updater auf dem P4 starten. Er überträgt die C6-Anwendung über die interne SDIO-Verbindung. Grundlage ist [Espressifs Beispiel „host_performs_slave_ota“](https://github.com/espressif/esp-hosted-mcu/tree/v2.12.13/examples/host_performs_slave_ota). Der Updatebefehl wurde bewusst ausgelöst, nicht automatisch beim Booten.
4. Den erfolgreichen Abschluss der Übertragung prüfen und neu starten. Danach antwortete die C6-Versionsabfrage mit `2.12.13`.
5. Die Stream-Deck-Anwendung mit passendem Hosted-Stand wieder auf den P4 schreiben. Partitionen und gespeicherte Menü-/Bilddaten blieben erhalten.

Ein normaler PlatformIO-Upload dieses Projekts aktualisiert **nur den P4**, nicht den C6. Dieser Erfahrungsbericht veröffentlicht keine Firmware-Binärdateien oder Updater; die Upstream-Links beschreiben den separaten Updateweg.

Ohne direkten C6-UART-Zugang konnten wir die ursprüngliche C6-Firmware nicht vollständig sichern. Scheitert die SDIO-Kommunikation, kann dieser Updateweg nicht als Wiederherstellung vorausgesetzt werden. Die [Waveshare-FAQ](https://docs.waveshare.com/ESP32-P4-NANO/FAQ) beschreibt dafür C6-UART und Downloadmodus; der normale USB-C-Anschluss ist an den P4 angebunden. Vor einem eigenen C6-Update deshalb Quelle, Boardkonfiguration und Wiederherstellungsweg klären.

## Zusätzliche Wiederherstellung im Projekt

Ergänzend überwacht die Firmware jetzt frische C6-Antworten und den lokalen Router, unabhängig von MQTT. Nach erkanntem Fehler versucht sie nach 30 Sekunden einen WLAN-Reconnect, nach 90 Sekunden eine C6-/SDIO-Neuinitialisierung und nach 180 Sekunden einen P4-Neustart. Der Supervisor wartet selbst nicht auf C6-Aufrufe und kann deshalb auch bei blockiertem Netzwerktask reagieren.

Die Transport-Neuinitialisierung orientiert sich an [Espressifs Beispiel „host_shuts_down_slave_to_power_save“](https://github.com/espressif/esp-hosted-mcu/tree/v2.12.13/examples/host_shuts_down_slave_to_power_save). Details zu Router-Ping-Fallback, Neustartsperre und Diagnosekommandos stehen in [WIFI.md](WIFI.md). Ein alleiniger Ausfall von MQTT oder dessen Backend löst keine dieser Stufen aus.

## Was bestätigt ist – und was offen bleibt

Bestätigt sind die neue C6-Versionsantwort, WLAN/IP, MQTT-Anmeldung und HTTP-Zugriff. Wiederholte Webabfragen über zwei Minuten blieben erfolgreich. Reconnect und C6-/SDIO-Reset wurden am Board ausgeführt; danach kam die Verbindung zurück. Die gemeinsam verwendete Netzwerkimplementierung wurde mit der privaten Installation am Board geprüft; das öffentliche Demo-Menü wurde dort nicht geflasht. Beide Buildvarianten und die Hosttests bestehen.

Offen bleiben Langzeitstabilität und ein kontrollierter Nachweis der ursprünglichen Fehlerursache. Die Reboot-Eskalation wurde als Logik getestet, aber nicht durch einen absichtlich blockierten Hardwaretreiber provoziert. Das Firmware-Update und die Recovery sind daher ein bisher erfolgreich getesteter Lösungsansatz, kein Nachweis, dass jeder WLAN-Ausfall behoben ist.
