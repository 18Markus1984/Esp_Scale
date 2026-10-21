# Sprachmodell der Waage

Die Waage versteht das Weckwort „Hallo Waage“ und danach Befehle wie „Tara“, „Wie viel?“,
„Zweihundertfünfzig Gramm“. Das macht ein kleines neuronales Netz, das komplett auf der Waage läuft
(kein Internet, kein ESP-SR, keine eigene Partition). Das Netz liegt als Datei auf der SD-Karte:

    /Waage/Sprache/kws_de.bin

Ein neues Modell ist nur eine neue Datei. Die Firmware muss dafür nicht neu gebaut werden. Am
einfachsten lädst du die Datei in der Weboberfläche unter **Dateien** in den Ordner `/Waage/Sprache`
hoch. Die Waage lädt sie dann beim nächsten Einschalten der Sprachbefehle neu.

## Befehle

| Sagen | Klasse | Was passiert |
|---|---|---|
| „Hallo Waage“ (oder oben zweimal tippen) | `hallo_waage` | „Ja?“, 6 s lang auf einen Befehl warten |
| „Tara“, „Tarieren“, „Null“ | `tara` | Tara (beim Zielwiegen: nächste Portion) |
| „Wie viel?“, „Wie schwer?“ | `wie_viel` | Gewicht ansagen (beim Zielwiegen: Restmenge) |
| „Speichern“ | `speichern` | ins Protokoll |
| „Weiter“ | `weiter` | Weiter-Knopf der Seite drücken (Rezept, Spiele): „Weiter.“ bzw. „Das geht hier gerade nicht.“ |
| „Zurück“ | `zurueck` | zur Wiegeseite („Zurück zur Startseite.“) |
| „Stopp“, „Ende“, „Abbrechen“ | `stopp` | Zielwiegen bzw. Modus beenden („Zielwiegen beendet.“) |
| „50 / 100 / 150 / 200 / 250 / 300 / 500 Gramm“, „ein Kilo“ | `ziel_50` … `ziel_1000` | Zielwiegen mit Parkpiepser („Ziel 250 Gramm. Jetzt einfüllen.“) |
| „Hilfe“, „Was kann ich sagen?“ | `hilfe` | liest die Befehle vor |
| „Nochmal“, „Wiederholen“ | `nochmal` | letzte Ansage (Gewicht, Restmenge) nochmal |
| „Lauter“ / „Leiser“ | `lauter` / `leiser` | Lautstärke ±20 %, „Lautstärke 80 Prozent“ in der neuen Lautstärke |
| „Akku“ | `akku` | „Akku 60 Prozent“ |
| „Uhrzeit“, „Wie spät ist es?“ | `uhrzeit` | „vierzehn Uhr fünf“ (ohne Uhrzeit: „Uhrzeit unbekannt.“) |

Wird nach dem Weckwort etwas gesagt, aber kein Befehl erkannt, antwortet die Waage „Nicht
verstanden. Sag Hilfe.“ Die Rückmeldungen kommen aus dem gewählten Stimmpaket oder, falls es diese
Sätze nicht hat, aus `/Waage/Stimme/_Zusatz` (englisch `_Zusatz-EN`). **Fehlt ein Satz auch dort,
piept die Waage nur.** Welche fehlen, steht in der Weboberfläche unter Einstellungen → Ton (unter
„Stimme“). Abhilfe: den Ordner `SD-Karte/Waage/Stimme/_Zusatz` aus dem ZIP nach `/Waage/Stimme` kopieren.

Weitere Ansagen ohne Sprachbefehl (Ansage „Sprüche“ an): „Waage bereit.“ nach dem Start, „Timer
abgelaufen.“, „Akku fast leer. Bitte laden.“, „Die Waage schaltet sich gleich aus.“

Alle Sätze einer Stimme auflisten (z. B. zum Selbst-Einsprechen für Lena) bzw. nur einzelne neu erzeugen:

    python stimme_erzeugen.py --liste
    python stimme_erzeugen.py --nur bereit,weiter

Eine neue Zielmenge braucht keine Firmware-Änderung: Klasse `ziel_750` mit den Sätzen
„siebenhundertfünfzig Gramm“ in `kws_common.py` eintragen und neu trainieren. Die Waage liest das
Gewicht aus dem Namen.

## Training am PC (Windows, Mac oder Linux)

Python 3.10 oder neuer, dann einmal:

    pip install torch numpy scipy piper-tts

Mehr Stimmen machen das Modell besser für fremde Sprecher. Optional, aber empfohlen:
espeak-ng (Roboterstimmen) und die acht deutschen MBROLA-Stimmen. Linux:
`sudo apt install espeak-ng mbrola mbrola-de1 mbrola-de2 mbrola-de3 mbrola-de4 mbrola-de5 mbrola-de6 mbrola-de7 mbrola-de8`.
Unter Windows geht es ohne; wer mag, nimmt dafür WSL (Ubuntu) mit demselben Befehl.

1. **Stimmen laden** (einmal, etwa 350 MB):

       python stimmen_laden.py

2. **Sätze sprechen lassen**, etwa 5000 kurze WAV-Dateien in `daten/` (etwa 10 min):

       python kws_daten.py

3. **Trainieren** (CPU, etwa 45 bis 60 min; die Merkmale zu berechnen dauert allein etwa 10 min):

       python kws_train.py

   Am Ende steht die Trefferquote je Befehl auf einer Stimme, die beim Training nicht dabei war.
   **Wichtiger** ist der Teil danach „Wie an der Waage gerechnet“: Das Training rechnet mit der fertigen
   `kws_de.bin` genau wie die Waage (alle 200 ms, drei Auswertungen gemittelt) und prüft, wie oft deine
   eigenen Prüf-Aufnahmen „Hallo Waage“ wecken würden – so wie aufgenommen und 20 dB leiser (weiter weg) –
   und wie oft 10 Minuten fremde Sprache die Waage fälschlich wecken. Daraus wählt es die
   **Weckwort-Schwelle** (die niedrigste mit höchstens 3 Fehlalarmen, zwischen 0,45 und 0,70) und
   schreibt sie ins Modell. Fest vorgeben: `--weckschwelle 0.55`. Ergebnis: `kws_de.bin`.

4. **Ausprobieren** ohne Waage, z. B. mit einer Handy-Aufnahme:

       python kws_pruefen.py aufnahme.wav

   **Alte Modelle bleiben erhalten:** Vor dem Speichern legt das Training das bisherige `kws_de.bin`
   als `modelle\modell_<Datum>_<Uhrzeit>.bin` ab und vergleicht am Ende alle Modelle mit deinen
   Prüf-Aufnahmen **aller Wörter** (nur die 15 %, die das Training nicht gesehen hat), 20 dB leiser.
   Ohne: `--kein-vergleich`.

   **Welche Wörter sind am schwächsten?** Alle Modelle, alle Wörter, schlechteste zuerst:

       python kws_pruefen.py --alle-woerter Aufnahmen
       python kws_pruefen.py --alle-woerter Aufnahmen --nur-pruefdateien --leiser 20

   Je Wort: Anzahl, Trefferquote je Modell (* = bestes) und was das neueste Modell stattdessen
   verstanden hat (z. B. „leiser“ statt „lauter“). Befehle werden wie an der Waage nach dem Weckwort
   entschieden. Darunter stehen die schwächsten Wörter – davon neue Aufnahmen machen
   (verschiedene Abstände und Betonungen). `--nur-pruefdateien` ist ehrlicher, weil das Modell
   Trainingsaufnahmen schon kennt.

   Ganze Ordner prüfen und Modelle vergleichen. Ohne `--modell` nimmt es alle `*.bin` im Ordner und
   in `modelle\`; `--nur-uebersicht` zeigt nur die Zusammenfassung. Für jede Aufnahme steht
   da, wie sicher die Waage „Hallo Waage“ erkennt und ob sie aufwachen würde:

       python kws_pruefen.py --ordner Aufnahmen\hallo_waage --modell alt.bin --modell kws_de.bin

   Mit `--leiser 25` werden die Aufnahmen so leise abgespielt, als hättest du aus 1 bis 2 m Abstand
   gesprochen. Die Spalte „Pegel“ zeigt die lauteste Stelle jeder Aufnahme in dBFS (der Rauschboden
   der Waage liegt bei etwa −61 dBFS); Aufnahmen, die schon leise sind, mit --leiser noch leiser zu
   machen, ergibt sehr weite Entfernungen. Achtung: Aufnahmen, mit denen trainiert wurde, kennt das Modell – sie schneiden besser
   ab als neue. Am aussagekräftigsten sind frische Aufnahmen (eigener Ordner, nicht im Training).

   Weckt sie zu selten, die Schwelle in der fertigen Datei senken (ohne neues Training) und die Datei
   wieder auf die SD-Karte kopieren:

       python kws_pruefen.py --setze-weckschwelle 0.5 --modell kws_de.bin

5. `kws_de.bin` auf die SD-Karte nach `/Waage/Sprache/` kopieren oder in der Weboberfläche hochladen.

## Mit deiner Stimme und deiner Küche besser werden

Das mitgelieferte Modell kennt nur künstliche Stimmen. Am meisten hilft es, mit dem Mikrofon der Waage
selbst aufzunehmen: Waage → **Setup → Ton → Wörter**. Wort wählen, auf den runden Knopf tippen und nach
dem Piepton sprechen (2,5 s, beim Weckwort und den Zielmengen 3 s). Schneller geht **Serie 30 s**:
30 s lang dasselbe Wort immer wieder sagen, mit kurzen Pausen dazwischen (der Knopf beendet die Serie
vorzeitig). Das Training zerschneidet die Serie selbst in einzelne Wörter. Lass zwischen den
Wiederholungen etwa eine Sekunde Pause: Bei „Hallo Waage“ und den Zielmengen trennt das Training erst
ab 0,6 s Stille, damit „Hallo“ und „Waage“ nicht als zwei Wörter gezählt werden. Die Spalte „Länge“
am Anfang des Trainings zeigt, wie lang die Stücke im Mittel sind (Hallo Waage etwa 0,8 bis 1,2 s).

Je Wort 10 bis 20 Aufnahmen, mal lauter, mal leiser, mal näher, mal weiter weg, gern auch von anderen
Personen (Frauen, Männer, Kinder, ältere Stimmen). Wichtig sind auch:
- **Küchengeräusche** (`_hintergrund`): 10 s lang nur die Küche (Dunstabzug, Wasser, Radio), 5 bis 10 Mal.
- **ein anderes Wort** (`_unbekannt`): irgendwelche anderen Wörter und Sätze, gern ähnlich klingende
  („Tante“, „Hallo Mama“). Das senkt Fehlauslösungen.

Die Aufnahmen liegen auf der SD-Karte unter `/Waage/Sprache/Aufnahmen/<klasse>/a001.wav …`.
Am einfachsten holst du sie ohne Kartenleser: Weboberfläche → Dateien → Sprache → bei „Aufnahmen“ auf
„↓ ZIP“ tippen. Die ZIP-Datei entpacken, dann:

    python kws_train.py --aufnahmen E:/Waage/Sprache/Aufnahmen

Eigene Aufnahmen zählen beim Training vierfach. Am Anfang zeigt das Training, was es gefunden hat
(Dateien, Wörter, abgeschnittene Aufnahmen) und warnt, wenn der Ordner leer ist. Liegt der Ordner nach
dem Entpacken doppelt (`Aufnahmen\Aufnahmen`), findet es den inneren selbst. Aufnahmen, die mitten im
Wort enden, zählen nur halb; ihre Namen stehen danach in `abgeschnitten.txt`. Mit
`--ohne-abgeschnittene` lässt das Training sie ganz weg. Ältere Aufnahmen (1,6 s) musst du also nicht
löschen: die vollständigen sind weiter gut, die abgeschnittenen findet das Training selbst.

Die Meldungen „Missing phoneme from id map“ von Piper sind harmlos (ein Zeichen ohne Laut fällt weg);
`kws_daten.py` blendet sie jetzt aus. Unter Windows fehlen espeak-ng und MBROLA meist: dann trainierst
du mit 6 statt etwa 20 Stimmen. Für deine eigene Stimme spielt das kaum eine Rolle.

**Neue Befehle trainieren, ohne alles neu zu sprechen:** `python kws_daten.py --nur hilfe,akku` spricht nur
diese Klassen neu, der Rest von `daten/` bleibt.

## Was die Waage im Alltag hört (Mitschnitt)

Solange die **Mikrofonseite** offen ist (Setup → Ton → Mikrofon), zeigt die Waage live
„Hallo Waage 58 % · weckt ab 50 %“ (grün = hätte geweckt) und speichert nach
`/Waage/Sprache/Mitschnitt/` je 3 s Ton (Zahl am Ende = höchste Wahrscheinlichkeit):

| Datei | bedeutet |
|---|---|
| `w001_93.wav` | geweckt |
| `k002_41.wav` | knapp daneben (Weckwort ab 25 %, aber nicht geweckt) |
| `b003_tara_71.wav` | Befehl nach dem Weckwort erkannt (hier Tara) |
| `n004.wav` | nach dem Weckwort etwas gesagt, aber nicht verstanden |

Höchstens 60 Dateien je Einschalten. **Holen und einsortieren** (Waage im WLAN, am PC):

    python kws_mitschnitt.py
    python kws_mitschnitt.py --waage 192.168.178.50     (falls waage.local nicht geht)

Das Skript holt den Ordner als ZIP und übernimmt nur Dateien, die es noch nicht kennt (gleicher
Inhalt = schon da). **Gleichzeitig holt es neue Wort-Aufnahmen** (Setup → Ton → Wörter) von der Waage
und legt sie direkt in `Aufnahmen\<wort>\` ab; was dort schon liegt, wird übersprungen (ohne:
`--ohne-aufnahmen`). Dann rechnen **alle Modelle** jede Datei durch. Sind sich die sicheren Modelle
einig (Weckwort über der Weckschwelle, Befehl ab 60 %, anderes Wort ab 70 %) und hat die Mehrheit
aller Modelle dasselbe Wort vorn (und, bei `b…`-Dateien, dasselbe Wort, das die Waage verstanden hatte),
wird automatisch einsortiert: nur das gemeinte Wort wird ausgeschnitten und als `Aufnahmen\<wort>\m_<name>.wav`
gespeichert. Ist **kein Modell sicher**, spielt das Skript die Datei ab und fragt:

    Enter = Vorschlag   h = hallo_waage   u = anderes Wort   Nummer = Wort aus der Liste
    w = nochmal hören   x = verwerfen (Geräusch)   s = später   q = beenden

`x` nur für Geräusche oder kaputte Dateien. War es Sprache, aber nicht „Hallo Waage“ (oder kein
Befehl), lieber `u` drücken: solche Beispiele (`_unbekannt`) braucht das Modell, um nicht falsch zu wecken.

**Whisper (optional, nur als Hilfe):** Mit `pip install faster-whisper` und `--whisper` hört ein großes
Spracherkennungsmodell mit. Es kennt aber das leise, verrauschte Mikrofon der Waage nicht und liegt
dort oft daneben. Deshalb entscheidet es nie allein: Es sortiert nur, wenn die eigenen Modelle unsicher
sind, aber mehrheitlich zum selben Wort tendieren. Sonst steht beim Nachfragen nur ein Hinweis da.

    python kws_mitschnitt.py --whisper              (small)  |  --whisper medium

**Wichtig (Firmware ab 16.10.2026): Mitschnitte vorher waren abgehackt.** Der Sprach-Task hat früher
selbst vom Mikrofon gelesen; während das Netz rechnete (alle 200 ms) lief der kleine Mikrofon-Puffer
über und es fehlten Stücke. „Hallo Waage“ war in Mitschnitten im Schnitt 0,85 s lang statt 1,30 s wie in
deinen Aufnahmen, mit Knacksern genau im 200-ms-Takt. Die Erkennung an der Waage hat dasselbe abgehackte
Signal bekommen. Jetzt liest ein eigener Task ohne Lücken in einen 1-s-Puffer.

**Sauberer Neustart danach:** Alle Mitschnitte kommen aus `Aufnahmen\` heraus; die alten (abgehackten)
landen in `Mitschnitt\alt_abgehackt` und werden nicht mehr trainiert. Ein neues Modell lernt nur aus
deinen eigenen Wort-Aufnahmen. Neue Mitschnitte (mit der neuen Firmware) sortiert nur dieses Modell,
und zwar strenger: mindestens 25 Prozentpunkte Abstand zum zweitbesten Wort, bei Zahlen muss das Modell
dasselbe sagen wie die Waage (sonst wird gefragt).

    python kws_mitschnitt.py neustart
    python kws_train.py --aufnahmen Aufnahmen --ohne-abgeschnittene
    (neue Firmware + neues Modell auf die Waage, ein paar Tage benutzen)
    python kws_mitschnitt.py --nur-neuestes
    python kws_train.py --aufnahmen Aufnahmen --ohne-abgeschnittene

Falsch einsortiert? Zurücknehmen und neu sortieren (löscht nur die ausgeschnittenen Kopien, die
Originale kommen aus `Mitschnitt\erledigt` zurück):

    python kws_mitschnitt.py zuruecknehmen               alles, was Whisper entschieden hat
    python kws_mitschnitt.py zuruecknehmen --wie alle    alle Mitschnitte

Erledigte Dateien wandern nach `Mitschnitt\erledigt`, jede Entscheidung steht in
`mitschnitt_bekannt.csv`. `python kws_mitschnitt.py sortieren --alles-fragen` fragt auch die
sicheren Fälle. Danach neu trainieren: `python kws_train.py --aufnahmen Aufnahmen --ohne-abgeschnittene`.

## Mikrofon im Gehäuse

Eine Bohrung direkt über der Mikrofonöffnung der Platine (1 bis 1,5 mm), möglichst kurzer Kanal, und ein
Ring aus Moosgummi zwischen Platine und Gehäuse, damit der Schall durch die Bohrung kommt und nicht im
Hohlraum dahinter hallt. Möglichst weit weg vom Lautsprecher.

## Wie es funktioniert (für später)

- Merkmale: 16 kHz, Fenster 30 ms, Schritt 20 ms, 40 Mel-Bänder, Logarithmus, gleitender Mittelwert je
  Band über etwa 3 s abgezogen (dadurch egal, wie laut das Mikrofon ist), Normierung aus dem Modell.
  `kws_common.py` und `kws.cpp` rechnen identisch (geprüft: Abweichung unter 0,001).
- Netz: DS-CNN, 9 Faltungsschichten (32/64/96 Kanäle), etwa 4 Mio. Multiplikationen je Auswertung,
  alle 200 ms über die letzten 2 s. Die Waage mittelt drei Auswertungen und schreibt alle 30 s die
  Rechenzeit in den seriellen Monitor („Sprache: Auswertung … ms“).
- Entscheidung: Weckwort ab der Schwelle im Modell (Training wählt sie, früher fest 65 %), dann 6 s lang Befehle ab 40 % (nur nach dem Weckwort, deshalb
  niedriger). Nach dem ersten Treffer sammelt die Waage noch 0,45 s und nimmt den stärksten Befehl.
  Bei Zielmengen wartet sie 1 s und nimmt die Zahl, die zuletzt über der Schwelle war, damit
  „zweihundertfünfzig“ nicht schon bei „zweihundert“ auslöst. Während die Waage selbst spricht oder
  piept, rechnet sie gar nicht (spart Rechenzeit, die Ansage läuft auf demselben Kern, und sie hört sich nicht
  selbst) und wartet danach 0,25 s; nach dem Einschalten wertet sie 2 s lang nichts aus.
- Schwellen ändern ohne neues Training: `python -c "import kws_common as K; K.set_thresholds('kws_de.bin', 0.40, 0.65)"`,
  vorher mit `kws_pruefen.py aufnahme.wav --schwelle 0.6 --weckschwelle 0.75` ausprobieren.
- Dateiformat: siehe Kopf von `kws.cpp` und `export()` in `kws_common.py`.
