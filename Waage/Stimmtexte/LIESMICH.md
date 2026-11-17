# Stimmtexte

Eine Textdatei pro Stimmpaket (erzeugt mit `python3 stimme_texte.py`, Quelle der Texte: `stimme_texte.py`).
`_ALLE_STIMMEN.txt` enthält alle Pakete hintereinander.

**Jede Zeile = eine WAV-Datei.** Jede Zeile endet mit einem Punkt (nur die „Ja?“-Zeile mit Fragezeichen),
innerhalb einer Zeile stehen nur Kommas. So setzt das Sprachprogramm nach jeder Zeile eine Pause und
`stimme_schneiden.py` findet die Stücke. Die `#`-Zeilen nicht mitlesen lassen, Abschnitte einzeln kopieren.

| Abschnitt | Schneiden mit |
|---|---|
| Statusansagen (7) | `python stimme_schneiden.py status.mp3 "Paket" --status` (EN zusätzlich `--lang en`) |
| Sprachsteuerung (22) | `python stimme_schneiden.py ansagen.mp3 "Paket" --ansagen` (EN zusätzlich `--lang en`) |
| Wortliste (50, nur neue Pakete wie Bernd das Brot) | `python stimme_schneiden.py aufnahme.mp3 "Paket"` |

Gibt das Sprachprogramm eine Datei pro Zeile aus: Ordner statt Aufnahme angeben und `--ordner` dazu
(Dateien werden alphabetisch zugeordnet). Lange Sätze mit Kommas lieber so, dann schneidet nichts falsch.
Die Bausteine am Anfang der Sprachsteuerung (Ziel, Akku, Prozent, Uhr, noch, zu viel …) werden mit Zahlen
zusammengesetzt und sollten kurz und neutral klingen. Roboter und Roboter-EN erzeugt
`stimme_erzeugen.py … --charakter Roboter` direkt mit espeak-ng.
