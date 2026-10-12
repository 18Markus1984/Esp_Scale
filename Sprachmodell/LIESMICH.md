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
| „Hallo Waage“ | `hallo_waage` | Ton, „Ich höre zu …“, 6 s lang auf einen Befehl warten |
| „Tara“, „Tarieren“, „Auf null“ | `tara` | Tara (beim Zielwiegen: nächste Portion) |
| „Wie viel?“, „Wie schwer?“ | `wie_viel` | Gewicht ansagen (beim Zielwiegen: Restmenge) |
| „Speichern“ | `speichern` | ins Protokoll |
| „Weiter“ | `weiter` | Weiter-Knopf der Seite drücken (Rezept, Spiele) |
| „Zurück“ | `zurueck` | zur Wiegeseite |
| „Stopp“, „Ende“ | `stopp` | Zielwiegen bzw. Modus beenden |
| „50 / 100 / 150 / 200 / 250 / 300 / 500 Gramm“, „ein Kilo“ | `ziel_50` … `ziel_1000` | Zielwiegen mit Parkpiepser |

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

   Am Ende steht die Trefferquote je Befehl auf einer Stimme, die beim Training nicht dabei war,
   und wie oft in 10 Minuten fremder Sprache etwas fälschlich ausgelöst hätte. Ergebnis: `kws_de.bin`.

4. **Ausprobieren** ohne Waage, z. B. mit einer Handy-Aufnahme:

       python kws_pruefen.py aufnahme.wav

5. `kws_de.bin` auf die SD-Karte nach `/Waage/Sprache/` kopieren oder in der Weboberfläche hochladen.

## Mit deiner Stimme und deiner Küche besser werden

Das mitgelieferte Modell kennt nur künstliche Stimmen. Am meisten hilft es, mit dem Mikrofon der Waage
selbst aufzunehmen: Waage → **Setup → Ton → Wörter**. Wort wählen, auf den runden Knopf tippen und nach
dem Piepton sprechen. Je Wort 10 bis 20 Aufnahmen, mal lauter, mal leiser, mal näher, mal weiter weg,
gern auch von anderen Personen. Unter **Küchengeräusche** 10 s lang nur die Küche aufnehmen
(Dunstabzug, Wasser, Radio), unter **ein anderes Wort** irgendwelche anderen Wörter.

Die Aufnahmen liegen auf der SD-Karte unter `/Waage/Sprache/Aufnahmen/<klasse>/a001.wav …`.
Ordner auf den PC kopieren, dann:

    python kws_train.py --aufnahmen E:/Waage/Sprache/Aufnahmen

Eigene Aufnahmen zählen beim Training vierfach.

## Wie es funktioniert (für später)

- Merkmale: 16 kHz, Fenster 30 ms, Schritt 20 ms, 40 Mel-Bänder, Logarithmus, gleitender Mittelwert je
  Band über etwa 3 s abgezogen (dadurch egal, wie laut das Mikrofon ist), Normierung aus dem Modell.
  `kws_common.py` und `kws.cpp` rechnen identisch (geprüft: Abweichung unter 0,001).
- Netz: DS-CNN, 9 Faltungsschichten (32/64/96 Kanäle), etwa 4 Mio. Multiplikationen je Auswertung,
  alle 200 ms über die letzten 2 s. Die Waage mittelt drei Auswertungen und schreibt alle 30 s die
  Rechenzeit in den seriellen Monitor („Sprache: Auswertung … ms“).
- Entscheidung: Weckwort ab 65 %, dann 6 s lang Befehle ab 40 % (nur nach dem Weckwort, deshalb
  niedriger). Nach dem ersten Treffer sammelt die Waage noch 0,45 s und nimmt den stärksten Befehl.
  Bei Zielmengen wartet sie 1 s und nimmt die Zahl, die zuletzt über der Schwelle war, damit
  „zweihundertfünfzig“ nicht schon bei „zweihundert“ auslöst. Während die Waage selbst spricht oder
  piept, hört sie weg; nach dem Einschalten wertet sie 2 s lang nichts aus.
- Schwellen ändern ohne neues Training: `python -c "import kws_common as K; K.set_thresholds('kws_de.bin', 0.40, 0.65)"`,
  vorher mit `kws_pruefen.py aufnahme.wav --schwelle 0.6 --weckschwelle 0.75` ausprobieren.
- Dateiformat: siehe Kopf von `kws.cpp` und `export()` in `kws_common.py`.
