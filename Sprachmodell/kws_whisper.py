#!/usr/bin/env python3
# ============================================================
#  Whisper als "Schiedsrichter" für Mitschnitte
#
#  Die kleinen Modelle der Waage sind auf wenig Rechenleistung getrimmt.
#  Am PC kann ein großes Spracherkennungsmodell (Whisper, läuft lokal, nichts
#  geht ins Internet außer dem einmaligen Download des Modells) den
#  Mitschnitt in Text umwandeln. Dieser Text wird dann einem Wort der Waage
#  zugeordnet.
#
#  Einmalig installieren:   pip install faster-whisper
#  Modell (beim ersten Lauf geladen, danach offline):
#      small  (~0,5 GB, schnell, Standard)
#      medium (~1,5 GB, genauer bei leisen Aufnahmen)
#      large-v3-turbo (~1,6 GB, am genauesten, langsamer)
#
#  Erfahrung (15.10.): Auf dem leisen, verrauschten Mikrofon der Waage liegt Whisper oft daneben,
#  die eigenen Modelle sind dort besser. kws_mitschnitt nutzt Whisper daher nur auf Wunsch
#  (--whisper) und nur als Bestätigung, wenn die eigenen Modelle unsicher in dieselbe Richtung tendieren.
#
#  Selbsttest der Zuordnung (ohne Whisper):  python kws_whisper.py --test
# ============================================================
import re, sys, unicodedata
from difflib import SequenceMatcher

import numpy as np

_model = None
_model_name = None


def available():
    try:
        import faster_whisper  # noqa: F401
        return True
    except Exception:
        return False


def load(name="small"):
    global _model, _model_name
    if _model is None or _model_name != name:
        from faster_whisper import WhisperModel
        print("Whisper-Modell \"%s\" wird geladen (beim ersten Mal Download) …" % name)
        _model = WhisperModel(name, device="cpu", compute_type="int8")
        _model_name = name
    return _model


# Typische Whisper-Erfindungen bei Stille/Rauschen (deutsche Untertitel im Trainingsmaterial)
HALLU = ("untertitel", "amara", "copyright", "vielen dank fürs zuschauen", "danke fürs zuschauen",
         "swr", "zdf", "ard", "abonnier", "bis zum nächsten mal")


def transcribe(a, name="small"):
    """a: Audio (16 kHz, int16-Skala). -> (Text, [(start_s, ende_s, wort)])"""
    x = np.asarray(a, np.float32) / 32768.0
    pk = float(np.abs(x).max()) if len(x) else 0.0
    if pk > 1e-4:  # leise Mitschnitte anheben (höchstens +30 dB)
        x = x * min(0.7 / pk, 31.6)
    m = load(name)
    segs, _ = m.transcribe(x, language="de", beam_size=5, word_timestamps=True, vad_filter=False,
                           condition_on_previous_text=False, temperature=0.0)
    text, words = [], []
    for s in segs:
        if s.no_speech_prob > 0.6 and s.avg_logprob < -0.8:
            continue  # vermutlich nichts gesagt
        t = s.text.strip()
        if any(h in t.lower() for h in HALLU):
            continue
        text.append(t)
        for w in (s.words or []):
            words.append((float(w.start), float(w.end), w.word.strip()))
    return " ".join(text).strip(), words


# ------------------------------------------------------------
#  Text -> Wort der Waage
# ------------------------------------------------------------
def norm(s):
    s = s.lower().replace("ß", "ss")
    s = unicodedata.normalize("NFKD", s)
    s = "".join(c for c in s if not unicodedata.combining(c))  # ä -> a usw.
    s = s.replace(".000", "000").replace("1.000", "1000")
    s = re.sub(r"(\d)[.,](\d{3})\b", r"\1\2", s)               # 1.000 -> 1000
    s = re.sub(r"[^a-z0-9 ]+", " ", s)
    return re.sub(r"\s+", " ", s).strip()


NUM_WORDS = {
    "50": ["funfzig"], "100": ["hundert", "einhundert"], "150": ["hundertfunfzig", "einhundertfunfzig"],
    "200": ["zweihundert"], "250": ["zweihundertfunfzig"], "300": ["dreihundert"],
    "500": ["funfhundert", "ein pfund", "pfund"], "1000": ["tausend", "eintausend", "ein kilo", "1 kilo", "kilo"],
}
# je Wort der Waage: Schreibweisen, wie Whisper sie liefert (nach norm())
PHRASES = {
    "tara": ["tara", "tarieren", "tariere", "auf null", "null", "0"],
    "wie_viel": ["wie viel", "wieviel", "wie schwer", "wievielt"],
    "speichern": ["speichern", "speicher"],
    "weiter": ["weiter"],
    "zurueck": ["zuruck"],
    "stopp": ["stopp", "stop", "ende", "beenden", "abbrechen"],
    "hilfe": ["hilfe", "was kann ich sagen"],
    "nochmal": ["nochmal", "noch mal", "noch einmal", "wiederholen"],
    "lauter": ["lauter"],
    "leiser": ["leiser"],
    "akku": ["akku", "akkustand", "wie voll ist der akku"],
    "uhrzeit": ["uhrzeit", "wie spat ist es", "wie viel uhr", "wieviel uhr"],
}
for n, ws in NUM_WORDS.items():
    PHRASES["ziel_" + n] = [n + " gramm", n + " g", n] + [w + " gramm" for w in ws] + ws


def _sim(a, b):
    return SequenceMatcher(None, a, b).ratio()


def has_wake(t):
    """'hallo waage' auch als 'hallo wage', 'hallo waag', 'halo waage' … (nicht 'hallo wagen')"""
    toks = t.split()
    for i in range(len(toks) - 1):
        if _sim(toks[i], "hallo") >= 0.75 and _sim(toks[i + 1], "waage") >= 0.85:
            return True
    return "hallowaage" in t.replace(" ", "")


def _find(t, phrase, fuzzy=True):
    """Position (Wortindex des Endes) des letzten Vorkommens, ganze Wörter. -1 = nicht da."""
    toks, ph = t.split(), phrase.split()
    best = -1
    for i in range(len(toks) - len(ph) + 1):
        if toks[i:i + len(ph)] == ph:
            best = i + len(ph) - 1
        elif fuzzy and len(ph) == 1 and len(ph[0]) >= 5 and not ph[0].isdigit() and _sim(toks[i], ph[0]) >= 0.85:
            best = i  # kleine Schreibvarianten bei längeren Wörtern (speichan, zuruck)
    return best


def classify(text, cand):
    """Text -> (wort, index_des_letzten_tokens) oder (None, -1), wenn nichts Brauchbares gesagt wurde.
    cand: mögliche Wörter (nach Dateiname). Andere Sprache -> '_unbekannt'."""
    t = norm(text)
    if not t:
        return None, -1
    toks = t.split()
    if "hallo_waage" in cand and not any(c for c in cand if c not in ("hallo_waage", "_unbekannt")):
        if has_wake(t):
            idx = max(i for i in range(len(toks)) if _sim(toks[i], "waage") >= 0.85) if any(
                _sim(x, "waage") >= 0.85 for x in toks) else len(toks) - 1
            return "hallo_waage", idx
        return "_unbekannt", len(toks) - 1
    # Befehle: das zuletzt gesagte zählt (davor steht oft noch "Hallo Waage")
    found = []  # (Ende, Länge, Wort): das zuletzt gesagte, bei gleicher Stelle das längere (250 vor 50)
    for c, phs in PHRASES.items():
        if c not in cand:
            continue
        for ph in phs:
            p = _find(t, ph, fuzzy=not c.startswith("ziel_"))  # Zahlen nur exakt
            if p >= 0:
                found.append((p, len(ph), c))
    hit, pos = (max(found)[2], max(found)[0]) if found else (None, -1)
    if hit:
        return hit, pos
    rest = [x for x in toks if not (_sim(x, "hallo") >= 0.75 or _sim(x, "waage") >= 0.85)]
    if not rest:
        return (("hallo_waage", len(toks) - 1) if "hallo_waage" in cand else (None, -1))
    return "_unbekannt", len(toks) - 1


def word_end_ms(words, idx):
    """Ende des idx-ten Worts (Zählung wie norm().split()) in ms, sonst None."""
    flat = []
    for s, e, w in words:
        for _ in norm(w).split():
            flat.append(e)
    if 0 <= idx < len(flat):
        return int(flat[idx] * 1000)
    return None


def _selftest():
    cmds = list(PHRASES) + ["_unbekannt"]
    wake = ["hallo_waage", "_unbekannt"]
    cases = [
        ("Hallo Waage!", wake, "hallo_waage"), ("Hallo, Wage.", wake, "hallo_waage"),
        ("Halo Wage", wake, "hallo_waage"), ("Hallo Wagen", wake, "_unbekannt"), ("Hallo Mama.", wake, "_unbekannt"), ("", wake, None),
        ("Hallo Waage, Tara.", cmds, "tara"), ("Tarieren", cmds, "tara"), ("Wie viel?", cmds, "wie_viel"),
        ("250 Gramm", cmds, "ziel_250"), ("Zweihundertfünfzig Gramm.", cmds, "ziel_250"),
        ("50 Gramm", cmds, "ziel_50"), ("Ein Kilo", cmds, "ziel_1000"), ("1.000 Gramm", cmds, "ziel_1000"),
        ("Hundertfünfzig", cmds, "ziel_150"), ("Noch mal.", cmds, "nochmal"), ("Zurück!", cmds, "zurueck"),
        ("Wie spät ist es?", cmds, "uhrzeit"), ("Akku", cmds, "akku"), ("Leiser.", cmds, "leiser"),
        ("Das Wasser kocht.", cmds, "_unbekannt"), ("Hallo Waage.", cmds, None), ("Stopp!", cmds, "stopp"),
        ("Speichern bitte.", cmds, "speichern"), ("Hilfe.", cmds, "hilfe"),
        ("Wie viel Uhr ist es?", cmds, "uhrzeit"), ("Hallo Waage, stopp.", cmds, "stopp"),
        ("Hallo Waage, 500 Gramm.", cmds, "ziel_500"), ("Hallo Waage. Lauter!", cmds, "lauter"),
    ]
    bad = 0
    for text, cand, want in cases:
        got = classify(text, cand)[0]
        if got != want:
            bad += 1
            print("FALSCH: %-28r -> %s (erwartet %s)" % (text, got, want))
    print("Selbsttest: %d von %d richtig" % (len(cases) - bad, len(cases)))
    return bad == 0


if __name__ == "__main__":
    if "--test" in sys.argv:
        sys.exit(0 if _selftest() else 1)
    print("faster-whisper installiert:", available())
