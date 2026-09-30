# Firmware 0.3: Menü und transparente Icons

Die aktuelle Bedienung steht in [MENU.md](MENU.md). Die folgende Beschreibung dokumentiert die ältere statische Zuordnung in Version 0.2. In 0.3 steuert das JSON-Menü die Tasten; die alten Zuordnungs-APIs bleiben nur aus Kompatibilitätsgründen bestehen.

# Piktogramme über den Browser

## Einmalige Einrichtung

1. In `include/secrets.h` die Werte für `WIFI_SSID` und `WIFI_PASSWORD` eintragen. Ein 2,4-GHz-WLAN verwenden. Falls die Datei fehlt, `include/secrets.example.h` kopieren.
2. In PlatformIO die zur Chiprevision passende Umgebung **bauen und normal hochladen**. Der normale Upload aktualisiert auch die Partitionstabelle.
3. Den Monitor mit 115200 Baud öffnen. Nach erfolgreicher Verbindung erscheint `Open http://<IP-Adresse>/ in your browser`.
4. Diese Adresse auf einem Rechner oder Handy im gleichen WLAN öffnen. Alternativ lässt sich die IP-Adresse in der Geräteliste des Routers ermitteln; der angeforderte DHCP-Hostname lautet `streamdeck-mini`.

Kein **Build Filesystem**, kein **Upload Filesystem**, keine SD-Karte und kein zusätzlicher Server erforderlich. Die Weboberfläche ist in der Anwendung eingebettet. Die neue 2-MB-LittleFS-Partition `icons` beginnt bei `0x210000`; sie wird beim ersten Start nur dann automatisch eingerichtet, wenn sie vollständig leer/gelöscht ist.

Die C6-Anbindung ist in [WIFI.md](WIFI.md) beschrieben. Bei leeren WLAN-Zugangsdaten bleibt die USB-Funktion verfügbar, aber es startet kein Webzugang. WLAN-Zugangsdaten werden bislang über den Header und einen Firmware-Upload geändert; es gibt noch kein WLAN-Einrichtungsportal.

## Bilder verwenden

1. SVG oder PNG auswählen, bis 2 MB Dateigröße. PNGs maximal 4096×4096 Pixel.
2. Eine kurze Icon-ID vergeben, z. B. `lampe_an`: maximal 32 Zeichen, nur A–Z, a–z, 0–9, `_` und `-`.
3. Hintergrundfarbe wählen und Vorschau prüfen. Der Browser passt das Bild proportional ein, glättet es auf 80×80 Pixel und ersetzt transparente Bereiche durch die Hintergrundfarbe.
4. **Im Stream Deck speichern** anklicken.
5. Bei einer der sechs Tasten das Icon auswählen und **Zuweisen** anklicken.

Mehrere Farben, SVG-Pfade und lokale Farbverläufe werden unterstützt. Die Umwandlung geschieht im Browser, auf dem ESP landet ein geprüftes natives Mini-BMP. Das SVG-Original bleibt auf dem Rechner; es wird nicht als bearbeitbare Vektordatei archiviert. Skripte, Animationen und externe SVG-Ressourcen werden abgewiesen. Selbstständige Piktogramme funktionieren ohne externe Schriftdateien oder Bilder am zuverlässigsten.

Bis zu 64 Piktogramme sind vorgesehen. Ein Icon benötigt 19.254 Bytes zuzüglich Dateisystem-Metadaten. Icons und Tastenbelegung bleiben nach Neustart, Abziehen des Mini und gewöhnlichem Firmware-Upload erhalten. Ein vorhandener Name ersetzt das Bild erst nach vollständigem und geprüftem Upload. Die Oberfläche fragt vor dem Ersetzen nach; alle Tasten mit dieser Icon-ID erhalten das neue Bild.

**Demo** in der Tastenauswahl entfernt nur die Zuordnung und zeigt wieder die nummerierte Kachel. Ein zugewiesenes Icon wird beim Tastendruck nicht automatisch umgefärbt; Tastendrücke werden weiterhin im Monitor ausgegeben. Die spätere MQTT-Logik kann zwischen `lampe_an` und `lampe_aus` umschalten.

Vor dem Löschen eines Icons müssen seine Tastenzuordnungen entfernt werden. Der Helligkeitsregler gilt bis zum Neustart. MQTT ist implementiert; Firmware-OTA ist nicht implementiert.

## Speicher und Updates

Nicht **Erase Flash** verwenden, wenn die hochgeladenen Bilder erhalten bleiben sollen. Ein manuell hochgeladenes Filesystem-Image würde den Bildspeicher ebenfalls ersetzen; diese Funktion ist absichtlich nicht für den normalen Ablauf vorgesehen. Bei einem Fehler beim Einbinden eines bereits beschriebenen LittleFS wird es **nicht** automatisch gelöscht. Das serielle Log zeigt dann den Fehler, und die Oberfläche meldet den Speicher als nicht verfügbar.

Die Weboberfläche wird bei jedem Firmware-Build mitgebaut und beim normalen Upload aktualisiert. Hochgeladene Icons liegen in der getrennten Datenpartition und werden dabei nicht mitgeflasht.

## HTTP-Schnittstelle

Die Oberfläche verwendet diese lokalen Endpunkte. Tasten in URLs sind 1-basiert.

| Methode / Pfad | Inhalt |
| --- | --- |
| `GET /api/state` | JSON: verfügbare Icon-IDs, sechs Zuordnungen, belegter/gesamter Speicher |
| `GET /api/icons/<id>` | Natives 80×80-Mini-BMP, noch gedreht/gespiegelt |
| `PUT /api/icons/<id>` | Exakt 19.254 Bytes validiertes Mini-BMP |
| `POST /api/keys/<1..6>` | Icon-ID als Text; leerer Body setzt Demo |
| `DELETE /api/icons/<id>` | Nicht zugewiesenes Icon löschen |
| `POST /api/brightness` | Textzahl 0–100 |

Schreibende Aufrufe benötigen den Header `X-Deck-Request: 1`. Er schützt vor browserbasierten fremden Ursprüngen, ist aber **kein Passwort**. Die Verwaltung ist für das eigene vertrauenswürdige WLAN vorgesehen und hat derzeit keine Anmeldung oder TLS. Es wird kein Cloud-Dienst angesprochen.

## Verifikation

Der Browserablauf wurde mit Chromium und einer simulierten HTTP-API getestet. Das tatsächliche Speichermodul wurde mit POSIX-Dateien und simuliertem LittleFS-Mount getestet, einschließlich Neustart, Zuweisung, Fehler beim Ersetzen und Schutz vor automatischem Löschen vorhandener Daten. Beide P4-Varianten wurden mit dem vollständigen Netzwerkcode gebaut. Eine Abnahme am echten P4/C6/Mini steht noch aus.
