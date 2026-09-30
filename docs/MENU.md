# Menü, Beschriftung und Zustandsfarben (Firmware 0.3)

Die bearbeitbare Vorlage liegt in `examples/menu.json`, das VS-Code-Schema daneben in `examples/menu.schema.json`. Die Firmware enthält denselben Ausgangsstand als Startmenü. Nach einem Web-Upload verwendet sie die Datei auf LittleFS. Normaler Firmware-Upload überschreibt das gespeicherte Menü nicht.

## Menü aus deinem StreamController-Backup

Die öffentliche Vorlage enthält 29 Demoseiten für Licht, Beschattung und Szenen. Passe Räume, Beschriftungen und MQTT-Entities an dein eigenes System an. Lamellenseiten verwenden Schritt +/−; Stopp befindet sich auf den Bewegungsseiten.

Die Vorlage enthält 42 Icon-IDs aus dokumentierten Twemoji-Grafiken und der originalen orangefarbenen Glühbirne. Sie werden beim Start in LittleFS angelegt; vorhandene Dateien werden nicht überschrieben. Eigene PNG/SVG können über die Weboberfläche geladen werden.

Apple-TV-Shellbefehle, Herunterfahren des alten Linux-PCs und dessen Kindersicherungs-Tastensequenz sind nicht auf den ESP übertragen. Es werden keine Shellbefehle aus dem Backup ausgeführt.

## Feste Tastenpositionen

```text
┌────────────┬──────────────┬──────────────┐
│ next       │ topLeft      │ topRight     │
├────────────┼──────────────┼──────────────┤
│ previous   │ bottomLeft   │ bottomRight  │
└────────────┴──────────────┴──────────────┘
```

Die vier Positionsnamen beziehen sich auf das rechte 2×2-Feld. Links oben und links unten sind fest reserviert. `previous`/`next` sind explizite Seiten-IDs, keine Browser-Historie. Dadurch kann eine Unterseite beim Zurückblättern wieder zur Auswahl führen. Beim Booten und Aktivieren einer neuen Konfiguration öffnet sich `startPage`.

## Eine weitere Lichtauswahl hinzufügen

Neue Seite `licht-2` mit bis zu vier Raumknöpfen anlegen. In `licht` `next` auf `licht-2` setzen; auf der neuen Seite `previous` auf `licht` und `next` beispielsweise auf `beschattung`. Die referenzierten Raumseiten müssen ebenfalls existieren. Seitenreihenfolge ergibt sich aus den Links, nicht aus ihrer Position im JSON-Text.

## Tastenfelder

```json
{
  "id": "flur-licht",
  "icon": "lampe.svg",
  "iconSize": 48,
  "fontSize": 16,
  "background": "#18222F",
  "textColor": "#FFFFFF",
  "labels": { "top": "Flur", "center": "", "bottom": "Decke" },
  "onPress": { "entity": "licht.flur", "action": "toggle" },
  "state": "licht.flur",
  "appearance": {
    "unknown": { "background": "#333333", "center": "?" },
    "off": { "background": "#18222F", "center": "AUS" },
    "on": { "background": "#F2B600", "textColor": "#000000", "center": "AN" }
  }
}
```

- `labels`: Beschriftung oben/mittig/unten. Nicht benötigte Texte weglassen oder leer lassen.
- `icon`: gespeicherte Icon-ID, optional mit `.svg`/`.png`. Kein Pfad und keine URL. `lampe.svg` verwendet die hochgeladene ID `lampe`.
- `iconSize`: 1–80 Pixel, Standard 48, zentriert. Farben des Piktogramms bleiben erhalten.
- `iconY`: optionaler Abstand vom oberen Tastenrand in Pixeln; Standard vertikal zentriert. `iconY + iconSize` darf 80 nicht überschreiten. In der Vorlage stehen Status-Icons mit 32 Pixeln bei Y=3 oberhalb des mittigen Statuswerts.
- `fontSize`: 8 oder 16 Pixel, Standard 8. Gemeinfreie Bitmap-Schrift mit ASCII und Latin-1 einschließlich ÄÖÜäöüß. Lange Texte werden zunächst auf 8 Pixel reduziert, dann am Ende mit `~` gekürzt. Kein frei wählbarer TTF-Font, keine Laufschrift und keine Textkontur in Version 1.
- `background`/`textColor`: `#RRGGBB`. Standard dunkel/weiß.
- `onPress`: genau eine Aktion, entweder `{"page":"flur"}` oder `{"entity":"licht.flur","action":"toggle"}`. Für feste Werte etwa `{"entity":"rollo.room_a","action":"position","value":45}`; alte `command`-Einträge bleiben kompatibel. Auslösung einmal beim Drücken, nicht erneut beim Loslassen.
- `state`: logische MQTT-Status-ID. Kein Backend-Datenpunkt.
- `appearance`: Zustand → optionale Überschreibungen von Hintergrund, Textfarbe und mittlerer Beschriftung. `unknown` gilt vor der ersten Statusmeldung und nach Verlust der MQTT-Verbindung. Ohne Zustandsstil gelten die Grundfarben.
- `{value}` wird in Beschriftungen durch den gemeldeten Wert ersetzt. Beispiel `"center": "{value} %"`, Status `{"state":"position","value":45}`.
- Leere Taste: `null`. Kein `onPress`: reine Anzeige.

Der Browser zeigt dieselben Bitmap-Glyphen wie der ESP. In der Vorschau kann zwischen Zuständen gewechselt werden; sie sendet keine MQTT-Befehle. Auf dem Gerät löst ein Tastendruck allein keinen Farbwechsel aus: maßgeblich ist die Statusmeldung des MQTT-Backends.

## Upload und Sicherung

1. WLAN-Zugangsdaten setzen, Firmware normal bauen/flashen, die IP im Monitor öffnen.
2. Benötigte Icons hochladen. „Transparenz erhalten“ eingeschaltet lassen, damit der Tastenhintergrund sichtbar bleibt. Bei älteren, bereits auf einen festen Hintergrund reduzierten Icons die Originaldatei erneut hochladen.
3. `menu.json` importieren oder im Textfeld bearbeiten. „Prüfen & Vorschau“ zeigt Seiten und fehlende Bilddateien.
4. „Menü aktivieren“ validiert noch einmal auf dem ESP, speichert atomar und aktiviert das Menü ohne Neustart.
5. „JSON herunterladen“ speichert den geprüften aktuellen Editorinhalt. Das Schema separat daneben speichern, damit VS Code Vorschläge und Fehler anzeigen kann.

Grenzen: maximal 48 Seiten, vier Inhaltstasten pro Seite, 64 KiB JSON, 64 Icons. IDs bis 63 ASCII-Zeichen, Icon-IDs bis 32. Texte maximal 63 UTF-8-Bytes auf dem ESP; das sichtbare 80×80-Feld ist die praktisch engere Grenze. Keine JSON-Kommentare. Das Schema prüft Struktur; die Firmware prüft zusätzlich Seitenziele, doppelte Felder und verfügbare Bilddateien. Fehlgeschlagene Validierung oder Dateiumbenennung lässt das bisherige Menü bestehen.

Bilder und JSON werden getrennt hochgeladen. Die Aktivierung des JSON ist atomar; ein vorher separat ersetztes Icon ist davon unabhängig bereits geändert. JSON-Export ist keine Sicherung der Original-SVGs/PNGs: diese lokal aufbewahren. In der neuen Firmware steuert das Menü die Tasten; alte sechs statische Zuordnungen bleiben als Altbestand gespeichert und werden nicht als Menü importiert.

## MQTT-Backend

Der neue Vertrag trennt `entity`, `action` und optional `value`. Dieselbe Entity steht im `state`-Feld der Taste. Das allgemeine Protokoll steht in [MQTT.md](MQTT.md). Optionale Backend-Beispiele gibt es für Home Assistant und ioBroker. Die Weboberfläche lädt das Menü auf den ESP; Backend-Zuordnungen bleiben im jeweiligen System. Zugangsdaten stehen weiterhin ausschließlich in der ignorierten lokalen `include/secrets.h`.

## Mitgelieferte Icons

Originalauswahl: `assets/starter-icons/`, Herkunft und Prüfsummen in `manifest.json`. Namen beginnen mit `sd_` oder `color_`. Das Bundle enthält 42 Icon-IDs und belegt etwa 1,03 MiB zusätzlichen Firmware-Flash; die RGBA-Daten benötigen rund 1,03 MiB LittleFS plus Dateisystem-Overhead. Wenn bereits ein eigenes Menü gespeichert ist, bleibt es aktiv: zum Übernehmen der neuen Symbolbelegung `examples/menu.json` über die Weboberfläche importieren. Die Icons sind nach einem normalen Firmware-Upload bereits vorhanden. Gelöschte Starter-Icons werden beim nächsten Boot erneut angelegt; selbst ersetzte gleichnamige Icons bleiben erhalten.

## Rückkehr bei Inaktivität

Nach 180 Sekunden ohne gültigen Tastendruck kehrt das Gerät automatisch zur `startPage` (Vorlage: `hauptmenue`) zurück. Prüfung alle 250 ms. Auch ein Schalt- oder Navigationstastendruck startet die Frist neu; MQTT-Statusmeldungen nicht. Keine Schaltaktion wird dabei ausgelöst. Die linke Spalte bleibt Weiter/Zurück. Dafür Firmware normal neu bauen/hochladen; kein Filesystem-Upload nötig.

## Home per langem Tastendruck

Links unten: kurz drücken und loslassen = `previous`; mindestens zwei Sekunden halten = direkt `startPage`. Home wird schon beim Halten ausgelöst (Prüfung alle 250 ms), beim Loslassen folgt keine Zurück-Aktion. Andere Tasten reagieren weiterhin sofort beim Drücken. USB-Trennung verwirft die laufende Geste. Firmware-Upload genügt; bestehendes Menü-JSON bleibt kompatibel.

## Sichtbare Orientierung

Links oben steht der `title` der aktuellen Seite in Hellblau über dem Weiter-Pfeil, z. B. `Wohnzimmer` oder zweizeilig `Rollo` / `RoomA`. Links unten steht über dem Zurück-Pfeil `2s: Home`. So bleiben alle vier Inhaltstasten frei. Die Webvorschau zeigt dieselbe Beschriftung. Titel lassen sich im Menü-JSON ändern; lange Zeilen werden wie andere Beschriftungen mit `~` gekürzt. Bestehende Menüs brauchen keine neuen Felder.

## Eigenständiger visueller Editor

`examples/menu-editor.html` per Doppelklick im Browser öffnen. Die einzelne HTML enthält die aktuelle Menüvorlage, das Schema, die Bitmap-Schrift und die 42 Starter-Icons. Sie funktioniert ohne Server und ohne Internetverbindung.

1. Bei einem bereits angepassten ESP-Menü zuerst dessen JSON herunterladen und im Editor über **JSON öffnen** importieren.
2. Links die Seite wählen; im Modus **Tasten bearbeiten** eine rechte Taste anklicken. Texte, Icon, Größe, Farben, Zustandsstile und Aktionen im rechten Bereich ändern. Unbearbeitete JSON-Felder bleiben erhalten.
3. **Navigation ausprobieren** aktiviert die Seitenwechsel der Inhaltstasten. MQTT-Aktionen werden nur angezeigt. Zurück/Home funktionieren auch in der Vorschau.
4. Mit **menu.json speichern** herunterladen, anschließend über die vorhandene ESP-Weboberfläche importieren und aktivieren. Es erfolgt kein direkter Upload und keine Geräteschaltung.

Die HTML speichert Änderungen nicht in sich selbst. Vor dem Schließen JSON exportieren. Eigene, nicht eingebettete Icons bleiben als Referenz erhalten und erscheinen als Platzhalter; die Bilddateien separat auf den ESP laden. Der Editor prüft Struktur und Seitenverweise, nicht die tatsächliche Existenz neuer Backend-Entities oder deren freigegebene Aktionen.

Nach Änderungen an Vorlage, Schema oder Renderer: `python3 scripts/build_menu_editor.py`. Der Generator übernimmt Validator, Bitmap-Schrift und Tastenrenderer aus den tatsächlichen Webressourcen. Browserprüfung: `node tests/test_editor.cjs` mit installierter Playwright-Umgebung. Keine Firmwareänderung für die Nutzung dieses Editors erforderlich.

Küche: `Licht → Weiter → Licht 2 → Küche`. Zwei unabhängige Tasten `Licht 1` und `Licht 2`, gelber Hintergrund bei bestätigt AN. Zurück führt zur zweiten Lichtauswahl, langes Halten weiterhin ins Hauptmenü.

### Einfacher Seitenplan

`examples/seitenstruktur.yaml` zeigt alle Seiten als eingerückten Baum und separat die aktuellen Weiter-/Zurück-Ziele. Fortsetzungsseiten sind markiert. Diese Datei dient der Abstimmung und wird noch nicht direkt importiert; Firmware und HTML-Editor verwenden weiterhin `menu.json`. Die Kurzbeschriftungen wurden auf höchstens 59 Pixel Breite geprüft. Home durch zwei Sekunden Halten links unten wurde vom Nutzer auf dem Gerät bestätigt.

Die Standardmenüs verwenden jetzt durchgehend farbige Icons. Untere Beschriftungen sitzen zwei Pixel höher (auch in beiden Web-Vorschauen). Sieben zusätzliche Twemoji-Icons sind unter CC BY 4.0 eingebunden; Herkunft und Lizenz stehen in `assets/starter-icons/TWEMOJI-NOTICE.txt`.

Lichttasten verwenden bei AN Cremegelb (`#FFF3B0`), dunkelbraune Schrift (`#4A2C16`) und eine orange Glühbirne mit dunkler Kontur ohne Hintergrundkasten. AUS bleibt dunkel. Vorschau und Firmware verwenden dieselbe Darstellung.
