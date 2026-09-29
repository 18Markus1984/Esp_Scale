#!/usr/bin/env python3
# ============================================================
#  Eine Aufnahme in die einzelnen Sprachbausteine zerschneiden
#
#  Du liest (oder eine KI-Stimme liest) die Wortliste unten der
#  Reihe nach vor, mit deutlicher Pause zwischen den Wörtern.
#  Dieses Skript schneidet daraus die WAV-Dateien für ein Stimmpaket.
#
#    python3 stimme_schneiden.py aufnahme.mp4 Jarvis
#    python3 stimme_schneiden.py aufnahme.mp4 Jarvis-EN --lang en   (englisches Paket)
#    python3 stimme_schneiden.py teil1.mp3 teil2.mp3 Rick-EN --lang en  (mehrere Aufnahmen der Reihe nach)
#    python3 stimme_schneiden.py --liste x x --lang en              (Wortliste zum Vorlesen)
#
#  Ergebnis: Ordner "Jarvis" mit 48 Dateien -> auf die SD-Karte nach
#  /Waage/Stimme/Jarvis/
#
#  Voraussetzung: ffmpeg im Pfad.
#  Passt die Anzahl der gefundenen Stücke nicht, mit --pause und
#  --schwelle nachjustieren (das Skript zeigt an, was es gefunden hat).
# ============================================================
import argparse, array, glob, os, re, subprocess, sys, wave

# Reihenfolge der Wörter in der Aufnahme (gleich wie in stimme_erzeugen.py!)
ORDER = [
    "null", "eins", "ein", "zwei", "drei", "vier", "fuenf", "sechs",
    "sieben", "acht", "neun", "zehn", "elf", "zwoelf", "dreizehn", "vierzehn",
    "fuenfzehn", "sechzehn", "siebzehn", "achtzehn", "neunzehn", "zwanzig", "dreissig", "vierzig",
    "fuenfzig", "sechzig", "siebzig", "achtzig", "neunzig", "hundert", "tausend", "und",
    "komma", "minus", "gramm", "kilogramm", "unze", "pfund", "stueck", "meter",
    "tara", "gespeichert", "ziel_erreicht", "ueberlast", "topf_erkannt", "fertig", "waage_leer",
    "milliliter",
]

# Vorlage zum Kopieren in ein Sprachprogramm (eine Zeile pro Wort):
TEXTS = [
    "null", "eins", "ein", "zwei", "drei", "vier", "fünf", "sechs",
    "sieben", "acht", "neun", "zehn", "elf", "zwölf", "dreizehn", "vierzehn",
    "fünfzehn", "sechzehn", "siebzehn", "achtzehn", "neunzehn", "zwanzig", "dreißig", "vierzig",
    "fünfzig", "sechzig", "siebzig", "achtzig", "neunzig", "hundert", "tausend", "und",
    "komma", "minus", "Gramm", "Kilogramm", "Unzen", "Pfund", "Stück", "Meter",
    "Tara", "gespeichert", "Ziel erreicht", "Überlast", "Topf erkannt", "fertig", "Waage leer",
    "Milliliter",
]
assert len(ORDER) == len(TEXTS) == 48

# Englisches Paket (Einstellung Sprache = English). Die Dateinamen sind die
# englischen Wörter; an grams.wav erkennt die Waage ein englisches Paket.
ORDER_EN = [
    "zero", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine",
    "ten", "eleven", "twelve", "thirteen", "fourteen", "fifteen", "sixteen", "seventeen", "eighteen", "nineteen",
    "twenty", "thirty", "forty", "fifty", "sixty", "seventy", "eighty", "ninety", "hundred", "thousand",
    "point", "minus", "grams", "kilograms", "ounces", "pounds", "pieces", "meters",
    "tare", "saved", "goal_reached", "overload", "pot_detected", "done", "scale_empty",
    "milliliters",
]
TEXTS_EN = [
    "zero", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine",
    "ten", "eleven", "twelve", "thirteen", "fourteen", "fifteen", "sixteen", "seventeen", "eighteen", "nineteen",
    "twenty", "thirty", "forty", "fifty", "sixty", "seventy", "eighty", "ninety", "hundred", "thousand",
    "point", "minus", "grams", "kilograms", "ounces", "pounds", "pieces", "meters",
    "Tare", "saved", "target reached", "overload", "pot detected", "done", "scale empty",
    "milliliters",
]
assert len(ORDER_EN) == len(TEXTS_EN) == 46


def load(path):
    tmp = "_stimme_tmp.wav"
    subprocess.run(["ffmpeg", "-v", "error", "-y", "-i", path, "-ac", "1", "-ar", "16000",
                    "-c:a", "pcm_s16le", tmp], check=True)
    with wave.open(tmp, "rb") as w:
        data = w.readframes(w.getnframes())
    os.remove(tmp)
    a = array.array("h")
    a.frombytes(data)
    return a


def segments(a, pause_ms, level):
    win = 320  # 20 ms
    env = [max(abs(x) for x in a[i:i + win]) for i in range(0, len(a) - win, win)]
    peak = max(env) if env else 0
    thr = peak * level
    gap_max = max(1, pause_ms // 20)
    out, start, gap = [], None, 0
    for i, v in enumerate(env):
        if v > thr:
            if start is None:
                start = i
            gap = 0
        elif start is not None:
            gap += 1
            if gap >= gap_max:
                out.append((start * win, (i - gap) * win))
                start = None
    if start is not None:
        out.append((start * win, len(env) * win))
    return out


def normalize(a, peak=0.85):
    """Alle Wörter auf gleiche Lautstärke bringen."""
    m = max((abs(x) for x in a), default=0)
    if m == 0:
        return a
    f = peak * 32767 / m
    return array.array("h", [max(-32768, min(32767, int(x * f))) for x in a])


def from_folder(folder, paket):
    """Wenn das Sprachprogramm pro Zeile eine Datei ausgibt:
    Dateien alphabetisch sortiert den Wörtern zuordnen."""
    files = sorted(glob.glob(os.path.join(folder, "*")),
                   key=lambda f: [int(t) if t.isdigit() else t.lower()
                                  for t in re.split(r"(\d+)", os.path.basename(f))])
    files = [f for f in files if f.lower().endswith((".wav", ".mp3", ".m4a", ".ogg", ".flac"))]
    print(f"{len(files)} Dateien gefunden, {len(ORDER)} erwartet")
    if len(files) != len(ORDER):
        sys.exit(1)
    os.makedirs(paket, exist_ok=True)
    for name, f in zip(ORDER, files):
        a = normalize(load(f))
        with wave.open(os.path.join(paket, name + ".wav"), "wb") as o:
            o.setnchannels(1)
            o.setsampwidth(2)
            o.setframerate(16000)
            o.writeframes(a.tobytes())
    print("fertig:", paket)


def main():
    p = argparse.ArgumentParser()
    p.add_argument("dateien", nargs="+",
                   help="Aufnahme(n) in Reihenfolge, zuletzt der Paketname; mit --ordner: Verzeichnis und Paket")
    p.add_argument("--ordner", action="store_true",
                   help="Quelle ist ein Ordner mit je einer Datei pro Wort")
    p.add_argument("--pause", type=int, default=300, help="Mindestpause zwischen Wörtern in ms")
    p.add_argument("--schwelle", type=float, default=0.06, help="Anteil des Maximalpegels")
    p.add_argument("--liste", action="store_true", help="nur die Wortliste zum Vorlesen ausgeben")
    p.add_argument("--lang", choices=["de", "en"], default="de", help="Sprache des Pakets (Standard: de)")
    args = p.parse_args()
    if len(args.dateien) < 2:
        p.error("mindestens eine Aufnahme und der Paketname")
    args.paket = args.dateien[-1]
    quellen = args.dateien[:-1]
    global ORDER, TEXTS
    if args.lang == "en":
        ORDER, TEXTS = ORDER_EN, TEXTS_EN

    if args.liste:
        print("\n".join(TEXTS))
        return

    if args.ordner:
        from_folder(quellen[0], args.paket)
        return

    # Mehrere Aufnahmen (z. B. weil die KI nur 500 Zeichen auf einmal liest)
    # werden der Reihe nach zerschnitten und hintereinander zugeordnet.
    a = array.array("h")
    segs = []
    for q in quellen:
        b = load(q)
        found = segments(b, args.pause, args.schwelle)
        print(f"{q}: {len(found)} Stücke")
        off = len(a)
        a.extend(b)
        a.extend(array.array("h", [0] * 8000))  # 0,5 s Stille zwischen den Dateien
        segs += [(x + off, y + off) for x, y in found]
    print(f"{len(segs)} Stücke gefunden, {len(ORDER)} erwartet")
    if len(segs) != len(ORDER):
        print("Bitte --pause oder --schwelle anpassen. Dauer der Stücke (s):")
        print(", ".join(f"{(e - s) / 16000:.2f}" for s, e in segs[:20]))
        sys.exit(1)

    os.makedirs(args.paket, exist_ok=True)
    for name, (s, e) in zip(ORDER, segs):
        s = max(0, s - 1600)   # 100 ms Vorlauf
        e = min(len(a), e + 3200)
        with wave.open(os.path.join(args.paket, name + ".wav"), "wb") as o:
            o.setnchannels(1)
            o.setsampwidth(2)
            o.setframerate(16000)
            o.writeframes(normalize(a[s:e]).tobytes())
    print("fertig:", args.paket)


if __name__ == "__main__":
    main()
