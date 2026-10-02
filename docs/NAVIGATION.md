# Navigation wie StreamController

Das Hauptmenü belegt alle sechs Tasten: Licht, Raffstores, Rollos oben,
Szenen, Markise, Rollos unten (zeilenweise).

Unterseiten: links oben Home bzw. übergeordnete Übersicht, links unten nächste
Seite derselben Kategorie. Die Lichtseiten schalten direkt; die Rolloräume bilden
je Etage einen Kreis. Die Raffstore-Übersicht verwendet unten links stattdessen
„Alle“, damit alle fünf Ziele auf eine Seite passen. Lamellenseiten bleiben separat
und enthalten keinen Stopp. Szenen enthält Schlafenszeit (Taste „Schlafen“).

Bei Szenen und Markise bleibt die Weiter-Taste leer: Beide Kategorien haben
nur eine Seite. Auf Blättertasten steht oben der aktuelle Raum und unten das
Ziel; Home-/Übersichtstasten tragen keinen Raumtitel.

Links unten 2 Sekunden halten bleibt als zusätzliche Home-Geste erhalten.
Nach 3 Minuten ohne Tastendruck erscheint Home.

## JSON und Editor

`buttons.leftTop` und `buttons.leftBottom` belegen die beiden linken physischen
Tasten. Die bisherigen vier Slots bleiben unverändert. Alle sechs Tasten lassen
sich im Offline-Editor auswählen und bearbeiten. Ihre `onPress.page` bestimmt
das Ziel; `previous` und `next` sind nur der Fallback für alte Menüs ohne explizite
linke Tasten. Ein explizites `null` lässt die jeweilige Taste leer.

Die neue Firmware liest weiterhin alte Menüs. Die neuen linken Slots benötigen
zuerst diese Firmware; danach kann menu.json über die Weboberfläche hochgeladen
werden. Firmware und Editor bilden dieselbe Tastenbelegung ab.
