# Erste Hardware-Abnahme

Checkliste für neue Aufbauten. Bisheriger Testumfang: siehe VALIDATION.md. Benötigt: P4-NANO, USB-C-Datenkabel, Stream Deck Mini und stabile 5-V-Versorgung.

1. Chiprevision auslesen, passendes PlatformIO-Environment wählen, Build und Upload.
2. Monitor öffnen. Bootmeldung mit Chiprevision und Kommandohilfe prüfen. Ohne Mini darf keine Neustartschleife auftreten.
3. Mini direkt an USB-A anschließen. Erwartet: VID/PID `0fd9:0063` (ursprüngliches Mini) bzw. `0fd9:0090` (2022), HID-Interface mit IN/OUT und möglichst `HIGH (480 Mbps)`.
4. Sechs nummerierte Kacheln: oben 1/2/3, unten 4/5/6. Heller Strich jeweils oben. Falls verdreht/gespiegelt: Foto und USB-Log für die Korrektur festhalten.
5. Jede Taste einzeln, danach mehrere gleichzeitig drücken/loslassen. Genau die passenden DOWN-/UP-Flanken prüfen. Ein Druck schaltet die jeweilige Kachel um, Loslassen schaltet sie nicht zurück.
6. `b 0`, `b 30`, `b 100` senden. Helligkeitsänderung prüfen; keine Resets. `b 101` muss abgewiesen werden.
7. `demo` senden und während der Bilderzeugung rasch mehrere Tasten drücken. Am Ende müssen Bilder und letzter Toggle-Zustand zusammenpassen.
8. Mini bei Leerlauf, während Upload und bei gedrückter Taste jeweils abziehen. Es darf keinen Absturz geben; gehaltene Tasten werden freigegeben. Wieder anstecken: Bilder und Empfang starten erneut.
9. Mindestens zehn Anschlusszyklen und anschließend 30 Minuten Betrieb testen. Resets, Speicherprobleme, USB-Timeouts und falsche Tastenzuordnung protokollieren.
10. Nach bestandenem Rechner-Test eigenständig am 5-V-Netzteil betreiben. Der Funktionstest muss ohne Rechner starten.

Bei Problemen benötigt werden: exakte Boardbezeichnung/Revision, verwendetes Environment, komplette Boot-/USB-Fehlermeldung und Versorgung/Kabelaufbau. Zugangsdaten und Geräteseriennummern sind dafür nicht nötig.

Ein erfolgreicher Build bestätigt die Übersetzbarkeit. Erst diese Abnahme bestätigt das Zusammenspiel von P4, Board-Stromversorgung und Mini.

## WLAN und Web-Icons (0.2)

11. WLAN-Zugangsdaten setzen, neu bauen/hochladen; im Monitor SDIO/C6-Handshake und DHCP-IP prüfen.
12. Im Browser mehrfarbiges SVG und PNG hochladen, Hintergrund wählen und allen Tasten verschiedene Icons zuweisen. Farben, Orientierung und Reihenfolge am Mini prüfen.
13. P4 neustarten und Mini neu anstecken: gespeicherte Icons werden wieder angezeigt.
14. Dasselbe Icon überschreiben: zugewiesene Tasten aktualisieren sich. Zugewiesenes Icon löschen wird abgewiesen; nach Entfernen der Zuordnung ist Löschen möglich.
15. WLAN-Unterbrechung und erneute Verbindung testen. USB bleibt unabhängig bedienbar.
16. Normalen Firmware-Upload wiederholen: Icon-Dateien und Belegung bleiben erhalten. Kein Erase Flash verwenden.

## Ergänzungen für 0.3

- Linke obere/untere Taste blättert; rechts öffnen Menütasten die richtigen Seiten. Abziehen während gehaltener Taste löst keine weitere Aktion aus.
- SVG mit transparentem Hintergrund hochladen, im JSON referenzieren; Umlaute, Textpositionen und Farben prüfen.
- JSON hochladen und Stromversorgung neu starten: gespeichertes Menü wird wiederhergestellt. Fehlende Icons/Seiten werden beim Upload abgewiesen.
- Zunächst mit Testbroker und ungefährlichem Kommando prüfen: genau ein nicht-retained MQTT-Befehl je Druck, kein Befehl bei Vorschau-Klicks, Statusmeldungen ändern Darstellung.
- Broker trennen/wiederverbinden: unbekannter Zustand während Trennung, retained Status wird geladen, alte Tastendrücke werden nicht wiederholt.
- Heapreserve und Stabilität bei gleichzeitigen Uploads, MQTT-Statusänderungen und Seitenwechseln messen.
