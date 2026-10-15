#!/usr/bin/env python3
# ============================================================
#  Sprachbausteine für die Waage erzeugen
#
#  Erzeugt alle WAV-Dateien für /Waage/Stimme auf der SD-Karte.
#  Voraussetzung: espeak-ng installiert (Linux: apt install espeak-ng,
#  Windows: espeak-ng von der Projektseite installieren).
#
#    python3 stimme_erzeugen.py ausgabeordner
#    python3 stimme_erzeugen.py ausgabeordner --lang en   (englisches Paket)
#    python3 stimme_erzeugen.py _Zusatz --zusatz          (nur die Ansagen der Sprachsteuerung)
#    python3 stimme_erzeugen.py _Zusatz --zusatz --piper de-eva_k-x-low.onnx
#                                       (Piper statt espeak-ng, pip install piper-tts)
#    python3 stimme_erzeugen.py Roboter --zusatz --nur bereit,weiter   (nur diese Dateien)
#    python3 stimme_erzeugen.py --liste   (alle Sätze als Text, z. B. zum Einsprechen)
#
#  Der Ordner /Waage/Stimme/_Zusatz (englisch _Zusatz-EN) springt ein, wenn
#  dem gewählten Paket ein Satz der Sprachsteuerung fehlt ("Ja?", Hilfe …).
#
#  Eigene Stimme? Einfach die Dateien im Ordner ersetzen:
#  16 kHz, Mono, 16 Bit PCM, Dateinamen wie unten.
# ============================================================
import os, subprocess, sys, wave, array

WORDS = {
    "null": "null", "eins": "eins", "ein": "ein", "zwei": "zwei", "drei": "drei", "vier": "vier",
    "fuenf": "fünf", "sechs": "sechs", "sieben": "sieben", "acht": "acht", "neun": "neun",
    "zehn": "zehn", "elf": "elf", "zwoelf": "zwölf", "dreizehn": "dreizehn", "vierzehn": "vierzehn",
    "fuenfzehn": "fünfzehn", "sechzehn": "sechzehn", "siebzehn": "siebzehn", "achtzehn": "achtzehn",
    "neunzehn": "neunzehn", "zwanzig": "zwanzig", "dreissig": "dreißig", "vierzig": "vierzig",
    "fuenfzig": "fünfzig", "sechzig": "sechzig", "siebzig": "siebzig", "achtzig": "achtzig",
    "neunzig": "neunzig", "hundert": "hundert", "tausend": "tausend", "und": "und",
    "komma": "komma", "minus": "minus", "gramm": "Gramm", "kilogramm": "Kilogramm",
    "unze": "Unzen", "pfund": "Pfund", "milliliter": "Milliliter", "stueck": "Stück", "meter": "Meter",
    # feste Ansagen
    "tara": "Tara", "gespeichert": "gespeichert", "ziel_erreicht": "Ziel erreicht",
    "ueberlast": "Überlast", "topf_erkannt": "Topf erkannt", "fertig": "fertig",
    "waage_leer": "Waage leer",
    # Zielwiegen: "noch 120 Gramm", "20 Gramm zu viel"
    "noch": "noch", "zu_viel": "zu viel",
}

# Ansagen der Sprachsteuerung (auch einzeln: --zusatz). Dateinamen höchstens 13 Zeichen.
ZUSATZ = {
    "ja": "Ja?",
    "nicht_verst": "Nicht verstanden. Sag Hilfe.",
    "hilfe": "Sag zuerst: Hallo Waage. Oder tippe zweimal oben auf die Waage. Dann zum Beispiel: "
             "Tara. Wie viel? Speichern. Fünfzig Gramm. Stopp. Akku. Uhrzeit. Lauter. Leiser. Nochmal.",
    "ziel": "Ziel",
    "einfuellen": "Jetzt einfüllen.",
    "ziel_beendet": "Zielwiegen beendet.",
    "akku": "Akku",
    "prozent": "Prozent",
    "laedt": "wird geladen",
    "uhr": "Uhr",
    # seit 1.1.21: Rückmeldung für jeden Sprachbefehl und Hinweise für blinde Nutzer
    "bereit": "Waage bereit.",
    "weiter": "Weiter.",
    "zurueck": "Zurück zur Startseite.",
    "geht_nicht": "Das geht hier gerade nicht.",
    "lautstaerke": "Lautstärke",
    "akku_leer": "Akku fast leer. Bitte laden.",
    "timer_ab": "Timer abgelaufen.",
    "zeit_unbek": "Uhrzeit unbekannt.",
    "schaltet_aus": "Die Waage schaltet sich gleich aus.",
}
ZUSATZ_EN = {
    "yes": "Yes?",
    "not_underst": "Not understood. Say help.",
    "help": "First say: Hallo Waage. Or tap the top of the scale twice. Then for example: "
            "tare, how much, save, fifty grams, stop, battery, time, louder, quieter, again.",
    "target": "Target",
    "fill_now": "Fill in now.",
    "target_off": "Target weighing stopped.",
    "battery": "Battery",
    "percent": "percent",
    "charging": "charging",
    "oclock": "o'clock",
    "ready": "Scale ready.",
    "next": "Next.",
    "back": "Back to the start page.",
    "cannot": "That is not possible right now.",
    "volume": "Volume",
    "batt_low": "Battery low. Please charge.",
    "timer_done": "Timer finished.",
    "time_unknown": "Time unknown.",
    "power_off": "The scale will switch off soon.",
}
WORDS.update(ZUSATZ)

# Englisch: Dateinamen = englische Wörter (an grams.wav erkennt die Waage die Sprache)
WORDS_EN = {w: w for w in [
    "zero", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine",
    "ten", "eleven", "twelve", "thirteen", "fourteen", "fifteen", "sixteen", "seventeen", "eighteen", "nineteen",
    "twenty", "thirty", "forty", "fifty", "sixty", "seventy", "eighty", "ninety", "hundred", "thousand",
    "point", "minus", "grams", "kilograms", "ounces", "pounds", "pieces", "meters", "milliliters"]}
WORDS_EN.update({"tare": "Tare", "saved": "saved", "goal_reached": "target reached", "overload": "overload",
                 "pot_detected": "pot detected", "done": "done", "scale_empty": "scale empty",
                 "to_go": "to go", "too_much": "too much"})
WORDS_EN.update(ZUSATZ_EN)


def trim(data, threshold=300, lead=400, tail=800):
    a = array.array("h")
    a.frombytes(data)
    s, e = 0, len(a) - 1
    while s < len(a) and abs(a[s]) < threshold:
        s += 1
    while e > s and abs(a[e]) < threshold:
        e -= 1
    return a[max(0, s - lead):min(len(a) - 1, e + tail)].tobytes()


def to_16k_mono(data, rate, channels):
    """Auf 16 kHz Mono umrechnen (lineare Interpolation genügt für Sprache)."""
    a = array.array("h")
    a.frombytes(data)
    if channels > 1:
        a = array.array("h", a[::channels])
    if rate == 16000:
        return a.tobytes()
    n = int(len(a) * 16000 / rate)
    out = array.array("h", bytes(2 * n))
    step = rate / 16000
    for i in range(n):
        x = i * step
        k = int(x)
        f = x - k
        y = a[k] * (1 - f) + (a[k + 1] if k + 1 < len(a) else a[k]) * f
        out[i] = int(y)
    return out.tobytes()


def speak_piper(onnx, text, raw):
    from piper import PiperVoice, SynthesisConfig
    if onnx not in _piper:
        _piper[onnx] = PiperVoice.load(onnx)
    with wave.open(raw, "wb") as w:  # etwas langsamer: Ansagen für ältere und blinde Nutzer
        _piper[onnx].synthesize_wav(text, w, syn_config=SynthesisConfig(length_scale=1.1))


_piper = {}


def main(out_dir, lang="de", zusatz=False, piper=None, only=None):
    os.makedirs(out_dir, exist_ok=True)
    if zusatz:
        words = ZUSATZ_EN if lang == "en" else ZUSATZ
    else:
        words = WORDS_EN if lang == "en" else WORDS
    if only:
        words = {k: v for k, v in words.items() if k in only}
    for name, text in words.items():
        raw = os.path.join(out_dir, "_tmp.wav")
        if piper:
            speak_piper(piper, text, raw)
        else:
            subprocess.run(["espeak-ng", "-v", "en" if lang == "en" else "de", "-s", "160", "-p", "35", "-a", "170",
                            "-w", raw, text], check=True)
        with wave.open(raw, "rb") as w:
            data = w.readframes(w.getnframes())
            rate, width, channels = w.getframerate(), w.getsampwidth(), w.getnchannels()
        if width != 2:
            print(f"Achtung: {name} hat {8 * width} Bit – übersprungen")
            continue
        data = to_16k_mono(data, rate, channels)  # espeak-ng liefert 22050 Hz
        with wave.open(os.path.join(out_dir, name + ".wav"), "wb") as o:
            o.setnchannels(1)
            o.setsampwidth(2)
            o.setframerate(16000)
            o.writeframes(trim(data))
        os.remove(raw)
    print(len(words), "Dateien in", out_dir)


if __name__ == "__main__":
    argv = sys.argv[1:]
    piper = None
    if "--piper" in argv:
        i = argv.index("--piper")
        piper = argv[i + 1]
        del argv[i:i + 2]
    only = None
    if "--nur" in argv:
        i = argv.index("--nur")
        only = set(argv[i + 1].split(","))
        del argv[i:i + 2]
    lang = "en" if "--lang" in argv and argv[argv.index("--lang") + 1:][:1] == ["en"] else "de"
    if "--liste" in argv:  # Text zum Einsprechen (stimme_schneiden.py --ansagen)
        for k, v in (ZUSATZ_EN if lang == "en" else ZUSATZ).items():
            print("%-14s %s" % (k, v))
        sys.exit(0)
    args = [a for a in argv if not a.startswith("--") and a != "en"]
    main(args[0] if args else ("Stimme-EN" if lang == "en" else "Stimme"), lang, "--zusatz" in argv, piper, only)
