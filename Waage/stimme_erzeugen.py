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
}

# Englisch: Dateinamen = englische Wörter (an grams.wav erkennt die Waage die Sprache)
WORDS_EN = {w: w for w in [
    "zero", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine",
    "ten", "eleven", "twelve", "thirteen", "fourteen", "fifteen", "sixteen", "seventeen", "eighteen", "nineteen",
    "twenty", "thirty", "forty", "fifty", "sixty", "seventy", "eighty", "ninety", "hundred", "thousand",
    "point", "minus", "grams", "kilograms", "ounces", "pounds", "pieces", "meters", "milliliters"]}
WORDS_EN.update({"tare": "Tare", "saved": "saved", "goal_reached": "target reached", "overload": "overload",
                 "pot_detected": "pot detected", "done": "done", "scale_empty": "scale empty"})


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


def main(out_dir, lang="de"):
    os.makedirs(out_dir, exist_ok=True)
    words = WORDS_EN if lang == "en" else WORDS
    for name, text in words.items():
        raw = os.path.join(out_dir, "_tmp.wav")
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
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    lang = "en" if "--lang" in sys.argv and sys.argv[sys.argv.index("--lang") + 1:][:1] == ["en"] else "de"
    args = [a for a in args if a != "en"]
    main(args[0] if args else ("Stimme-EN" if lang == "en" else "Stimme"), lang)
