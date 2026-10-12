#!/usr/bin/env python3
# ============================================================
#  Trainingsdaten für das Sprachmodell erzeugen (Schritt 1 von 2)
#
#  Spricht alle Sätze aus kws_common.py mit mehreren Stimmen,
#  unterschiedlich schnell und betont, und legt sie als WAV ab:
#    daten/<klasse>/<stimme>_<nr>.wav   (16 kHz, Mono)
#
#  Stimmen:
#   - Piper (natürlich klingend): stimmen_laden.py lädt sechs
#     deutsche Stimmen nach stimmen/
#   - espeak-ng (roboterhaft, aber viele Varianten), falls installiert
#   - Stimmpakete der Waage (SD-Karte/Waage/Stimme), aus Einzelwörtern
#     zusammengesetzt (Zahlen, "Tara")
#
#  Deine eigenen Aufnahmen von der Waage (Setup -> Ton -> Wörter
#  aufnehmen, liegen auf der SD-Karte unter /Waage/Sprache/Aufnahmen)
#  nimmt kws_train.py direkt mit: --aufnahmen <Pfad>.
#
#    python kws_daten.py                 (alles)
#    python kws_daten.py --schnell       (weniger Varianten, zum Ausprobieren)
# ============================================================
import argparse, glob, os, random, shutil, subprocess, sys, tempfile, wave
import numpy as np
from multiprocessing import Pool

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import kws_common as K

HERE = os.path.dirname(os.path.abspath(__file__))


def save(path, pcm):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(K.RATE)
        w.writeframes(pcm.astype(np.int16).tobytes())


def load(path):
    with wave.open(path) as w:
        sr, ch = w.getframerate(), w.getnchannels()
        a = np.frombuffer(w.readframes(w.getnframes()), np.int16).astype(np.float32)
    if ch > 1:
        a = a.reshape(-1, ch).mean(1)
    if sr != K.RATE:
        from scipy.signal import resample_poly
        from math import gcd
        g = gcd(sr, K.RATE)
        a = resample_poly(a, K.RATE // g, sr // g)
    return a.astype(np.float32)


def trim(a, rel=0.04, keep=0.06):
    """Stille vorn und hinten abschneiden (Schwelle relativ zur Spitze), etwas Rand lassen."""
    if len(a) == 0:
        return a
    env = np.convolve(np.abs(a), np.ones(160) / 160, mode="same")
    idx = np.where(env > env.max() * rel)[0]
    if len(idx) == 0:
        return a
    k = int(keep * K.RATE)
    return a[max(0, idx[0] - k):min(len(a), idx[-1] + k)]


def norm(a, peak=0.9):
    m = np.abs(a).max()
    return a if m == 0 else a * (peak * 32767 / m)


# ---- Piper ----
_voice = {}


def piper_job(job):
    onnx, text, out, ls, ns, nw = job
    from piper import PiperVoice, SynthesisConfig
    if onnx not in _voice:
        _voice[onnx] = PiperVoice.load(onnx)
    v = _voice[onnx]
    cfg = SynthesisConfig(length_scale=ls, noise_scale=ns, noise_w_scale=nw)
    with tempfile.NamedTemporaryFile(suffix=".wav", delete=False) as t:
        tmp = t.name
    try:
        with wave.open(tmp, "wb") as w:
            v.synthesize_wav(text, w, syn_config=cfg)
        save(out, norm(trim(load(tmp))))
    finally:
        os.remove(tmp)
    return out


# ---- espeak-ng ----
ESPEAK_VOICES = ["de", "de+m1", "de+m2", "de+m3", "de+m4", "de+m7", "de+f1", "de+f2", "de+f3", "de+f4", "de+klatt3"]
# MBROLA: acht deutsche Diphon-Stimmen (4 Frauen, 4 Männer), klingen deutlich natürlicher als espeak.
# Linux: apt install mbrola mbrola-de1 … mbrola-de8; Windows: siehe LIESMICH.md (optional)
MBROLA_VOICES = ["mb-de1", "mb-de2", "mb-de3", "mb-de4", "mb-de5", "mb-de6", "mb-de7", "mb-de8"]


def mbrola_ok(v):
    try:
        r = subprocess.run(["espeak-ng", "-v", v, "-w", os.devnull, "test"], capture_output=True, timeout=20)
        return r.returncode == 0 and b"not found" not in r.stderr.lower()
    except Exception:
        return False


def espeak_job(job):
    voice, text, out, speed, pitch = job
    with tempfile.NamedTemporaryFile(suffix=".wav", delete=False) as t:
        tmp = t.name
    try:
        subprocess.run(["espeak-ng", "-v", voice, "-s", str(speed), "-p", str(pitch), "-w", tmp, text],
                       check=True, capture_output=True)
        save(out, norm(trim(load(tmp))))
    finally:
        os.remove(tmp)
    return out


# ---- Stimmpakete der Waage: Zahlen aus Einzelwörtern ----
PACK_WORDS = {
    "ziel_50": [["fuenfzig"], ["fuenfzig", "gramm"]],
    "ziel_100": [["hundert"], ["hundert", "gramm"], ["ein", "hundert", "gramm"]],
    "ziel_150": [["hundert", "fuenfzig"], ["hundert", "fuenfzig", "gramm"]],
    "ziel_200": [["zwei", "hundert"], ["zwei", "hundert", "gramm"]],
    "ziel_250": [["zwei", "hundert", "fuenfzig"], ["zwei", "hundert", "fuenfzig", "gramm"]],
    "ziel_300": [["drei", "hundert"], ["drei", "hundert", "gramm"]],
    "ziel_500": [["fuenf", "hundert"], ["fuenf", "hundert", "gramm"]],
    "ziel_1000": [["tausend", "gramm"], ["ein", "tausend", "gramm"]],
    "tara": [["tara"]],
    "_unbekannt": [["vier", "hundert"], ["sechs", "hundert"], ["drei", "hundert", "fuenfzig"], ["sieben", "hundert"],
                   ["achtzig"], ["vierzig"], ["dreissig"], ["zwanzig"], ["gespeichert"], ["fertig"], ["waage_leer"],
                   ["ueberlast"], ["topf_erkannt"], ["ziel_erreicht"], ["kilogramm"], ["minus"], ["komma"]],
}


def pack_clips(pack_root, out_root):
    n = 0
    for pack in sorted(glob.glob(os.path.join(pack_root, "*"))):
        if not os.path.exists(os.path.join(pack, "gramm.wav")):
            continue  # nur deutsche Pakete
        name = os.path.basename(pack)
        for cls, combos in PACK_WORDS.items():
            for i, words in enumerate(combos):
                parts = []
                ok = True
                for wd in words:
                    p = os.path.join(pack, wd + ".wav")
                    if not os.path.exists(p):
                        ok = False
                        break
                    parts.append(trim(load(p), keep=0.02))
                if not ok:
                    continue
                gap = np.zeros(int(0.03 * K.RATE), np.float32)
                a = np.concatenate([x for p in parts for x in (p, gap)])
                save(os.path.join(out_root, cls, "paket-%s_%d.wav" % (name, i)), norm(a))
                n += 1
    return n


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--stimmen", default=os.path.join(HERE, "stimmen"), help="Ordner mit Piper-Stimmen (*.onnx)")
    ap.add_argument("--ziel", default=os.path.join(HERE, "daten"))
    ap.add_argument("--pakete", default=os.path.join(HERE, "..", "SD-Karte", "Waage", "Stimme"))
    ap.add_argument("--schnell", action="store_true", help="weniger Varianten")
    ap.add_argument("--prozesse", type=int, default=max(1, os.cpu_count() or 1))
    args = ap.parse_args()

    random.seed(7)
    if os.path.exists(args.ziel):
        shutil.rmtree(args.ziel)
    voices = sorted(glob.glob(os.path.join(args.stimmen, "*.onnx")))
    nvar = 3 if args.schnell else 8      # Varianten je Satz und Piper-Stimme
    nvar_other = 1 if args.schnell else 2
    jobs_p, jobs_e = [], []
    mbrola = [v for v in MBROLA_VOICES if shutil.which("espeak-ng") and mbrola_ok(v)]
    print("MBROLA-Stimmen:", ", ".join(mbrola) if mbrola else "keine (optional)")
    for cls, phrases in K.CLASSES_DE.items():
        texts = K.OTHER_DE if cls == "_unbekannt" else phrases
        for ti, text in enumerate(texts):
            for v in voices:
                vn = os.path.basename(v).replace(".onnx", "")
                for k in range(nvar_other if cls == "_unbekannt" else nvar):
                    out = os.path.join(args.ziel, cls, "%s_%d_%d.wav" % (vn, ti, k))
                    jobs_p.append((v, text, out, random.uniform(0.8, 1.35), random.uniform(0.3, 0.9),
                                   random.uniform(0.4, 1.0)))
            if not args.schnell or cls != "_unbekannt":
                for ev in ESPEAK_VOICES:
                    out = os.path.join(args.ziel, cls, "espeak-%s_%d.wav" % (ev.replace("+", "-"), ti))
                    jobs_e.append((ev, text, out, random.randint(130, 185), random.randint(25, 75)))
                for mv in mbrola:
                    for k in range(1 if cls == "_unbekannt" else (2 if args.schnell else 4)):
                        out = os.path.join(args.ziel, cls, "mbrola-%s_%d_%d.wav" % (mv, ti, k))
                        jobs_e.append((mv, text, out, random.randint(120, 190), random.randint(30, 70)))
    print("Piper: %d Stimmen, %d Aufnahmen" % (len(voices), len(jobs_p)))
    if not voices:
        print("Keine Piper-Stimmen gefunden: erst python stimmen_laden.py ausführen")
    with Pool(args.prozesse) as pool:
        for i, _ in enumerate(pool.imap_unordered(piper_job, jobs_p, chunksize=8)):
            if i % 200 == 0:
                print("  Piper %d / %d" % (i, len(jobs_p)), flush=True)
    if shutil.which("espeak-ng"):
        print("espeak-ng: %d Aufnahmen" % len(jobs_e))
        with Pool(args.prozesse) as pool:
            list(pool.imap_unordered(espeak_job, jobs_e, chunksize=8))
    else:
        print("espeak-ng nicht gefunden, übersprungen")
    if os.path.isdir(args.pakete):
        print("Stimmpakete: %d Aufnahmen" % pack_clips(args.pakete, args.ziel))
    for cls in K.CLASSES_DE:
        n = len(glob.glob(os.path.join(args.ziel, cls, "*.wav")))
        print("  %-12s %5d" % (cls, n))


if __name__ == "__main__":
    main()
