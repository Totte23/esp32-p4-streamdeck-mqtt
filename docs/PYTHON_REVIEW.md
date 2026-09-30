# Vergleich mit der Python-Bibliothek

Referenz: StreamController/streamcontroller-python-elgato-streamdeck, Commit
`ca683c4c94143c2227d5b6f2669e2dd0d95ba7cb`, abgerufen am 26.09.2026.

Originaldateien mit Lizenz liegen unter `tests/reference/python-streamdeck/`.
`tests/compare_python.py` führt deren Methoden mit einer simulierten Transportklasse aus und vergleicht die tatsächlichen Bytes der C-Implementierung. Pillow wird dabei als unabhängiger Bildrenderer verwendet, nicht die Browser-Konvertierung des Projekts.

| Bereich | Ergebnis |
|---|---|
| Helligkeit | Alle 101 Prozentwerte: exakt gleiche 17-Byte-Feature-Reports |
| Bildpakete | 6 Tasten × 20 Pakete: exakt gleiche Header, 1008-Byte-Nutzdaten, Endekennung und Nullfüllung |
| Tasten | Alle 64 Kombinationen: gleiche Zuordnung, obere Reihe 0/1/2, untere 3/4/5 |
| Bildorientierung | Alle 19.200 Pixelbytes entsprechen Pillow: 90° gegen Uhrzeigersinn, vertikal spiegeln, BMP bottom-up |
| Transparenz | Alpha-Komposition aller Pixel stimmt mit Pillow überein |
| Stream-Reset | Ein 1024-Byte-Output-Report beginnend mit 0x02; entspricht `_reset_key_stream` |

Abweichungen sind bewusst:

- Python `set_key_image` lässt durch seinen Grenzvergleich auch Index 6 zu. C akzeptiert ausschließlich 0–5.
- Das konstante Python-`BLANK_KEY_IMAGE` ist als 80×80 kommentiert und hat entsprechend viele Pixelbytes, im Header stehen jedoch 72×72 und die dazugehörige Größe. Unser Renderer erstellt konsistente 80×80-BMPs. Die normalen mit Pillow erzeugten Python-Bilder sind davon nicht betroffen.
- Python klemmt ungültige Helligkeiten auf 0–100; unsere öffentlichen APIs weisen ungültige Werte zurück.
- Python wandelt beliebige Eingabebytes in Boolean um. Unser Parser erwartet für die sechs Tasten 0/1, Report-ID 1, mindestens sieben Bytes und akzeptiert zusätzliches Padding.
- Python bietet einen gesonderten Geräte-Reset per Feature-Report `0x0b,0x63` sowie Lesen von Seriennummer und Firmwareversion. Diese Diagnosefunktionen werden bislang nicht benötigt und sind nicht implementiert. Der Stream-Reset beim Verbinden ist kein vollständiger Geräte-Reset.

Übernommen wird das Protokollverhalten. Python/HIDAPI-Threads und Transportcode werden nicht portiert; auf dem ESP übernimmt ESP-IDF den USB-Host mit asynchronen Transfers. Die Python-Bibliothek enthält keine C6-WLAN-Anbindung und ersetzt keine MCU-Treiber.

Textbeschriftung ist Teil der StreamController-Anwendung, nicht einer besonderen Textfunktion des Mini. Deren `DeckController.py` komponiert Hintergrund/Icon und zeichnet Labels mit Pillow. Unser eigener Renderer folgt diesem Prinzip, mit einer kleinen fest eingebauten Bitmap-Schrift. Es wurde kein GPL-Anwendungscode in die Firmware kopiert.

## Tests starten

```sh
./tests/run.sh
python3 -m venv /tmp/streamdeck-reference-test
/tmp/streamdeck-reference-test/bin/pip install Pillow
/tmp/streamdeck-reference-test/bin/python tests/compare_python.py
```

C-Compiler erforderlich; Referenztest erzeugt eine temporäre dynamische Bibliothek unter `build/tests`. Kein USB-Gerät, kein Broker und keine HTTP-Aufrufe Richtung ioBroker werden angesprochen. Der Bytevergleich beweist Protokollgleichheit für diese Fälle, nicht den Betrieb des realen P4-Hostcontrollers oder die Stromversorgung.

Quellen:
- https://github.com/StreamController/streamcontroller-python-elgato-streamdeck/tree/ca683c4c94143c2227d5b6f2669e2dd0d95ba7cb
- https://github.com/StreamController/StreamController/blob/main/src/backend/DeckManagement/DeckController.py
- https://github.com/dhepper/font8x8 (gemeinfreie Schrift)
