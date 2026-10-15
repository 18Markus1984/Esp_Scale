#!/usr/bin/env python3
# ============================================================
#  Sprachmodell trainieren und als kws_de.bin speichern (Schritt 2 von 2)
#
#  Nimmt die Sätze aus daten/ (kws_daten.py) und, falls angegeben,
#  deine Aufnahmen von der Waage. Jede Aufnahme wird mehrfach
#  verändert (lauter/leiser, schneller/langsamer, Hall, Rauschen,
#  Küchengeräusche, verschoben im Zeitfenster), damit das Netz nicht
#  auswendig lernt. Eine Piper-Stimme bleibt zur Prüfung draußen.
#
#    python kws_train.py
#    python kws_train.py --aufnahmen E:/Waage/Sprache/Aufnahmen
#    python kws_train.py --epochen 40
#
#  Ergebnis: kws_de.bin -> auf die SD-Karte nach /Waage/Sprache/
#  (oder in der Weboberfläche unter Dateien hochladen).
#
#  Aufnahmen-Ordner: ein Unterordner je Klasse (Namen wie in
#  kws_common.py, z. B. hallo_waage, tara, ziel_250), dazu
#  _hintergrund für Küchengeräusche ohne Sprache. Eigene Aufnahmen
#  zählen beim Training vierfach, weil sie genau das Mikrofon und
#  den Raum der Waage zeigen.
# ============================================================
import argparse, datetime, glob, os, random, sys, time, zlib
import numpy as np
from multiprocessing import Pool

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import kws_common as K
from kws_daten import load

HERE = os.path.dirname(os.path.abspath(__file__))
WIN_S = K.FRAMES * K.HOP  # Abtastwerte im Netzfenster
CLASSES = list(K.CLASSES_DE)
HOLDOUT = "de-ramona-low"  # diese Stimme nur zur Prüfung

# ------------------------------------------------------------
#  Geräusche
# ------------------------------------------------------------
def colored_noise(n, rng, kind):
    w = rng.standard_normal(n).astype(np.float32)
    if kind == "white":
        return w
    f = np.fft.rfft(w)
    k = np.arange(len(f)) + 1.0
    f /= np.sqrt(k) if kind == "pink" else k
    x = np.fft.irfft(f, n).astype(np.float32)
    return x / (x.std() + 1e-9)


def hum(n, rng):
    t = np.arange(n) / K.RATE
    base = rng.choice([50.0, 100.0])
    x = sum(rng.uniform(0.2, 1.0) / (h + 1) * np.sin(2 * np.pi * base * (h + 1) * t + rng.uniform(0, 6)) for h in range(5))
    return (x / (np.std(x) + 1e-9)).astype(np.float32)


def clatter(n, rng):
    """Klappern: kurze, gedämpfte Schläge (Besteck, Schüssel)."""
    x = np.zeros(n, np.float32)
    for _ in range(rng.integers(1, 8)):
        p = rng.integers(0, max(1, n - 4000))
        L = rng.integers(400, 4000)
        f = rng.uniform(800, 5000)
        t = np.arange(L) / K.RATE
        x[p:p + L] += (np.sin(2 * np.pi * f * t) * np.exp(-t * rng.uniform(20, 80)) * rng.uniform(0.3, 1)).astype(np.float32)
    return x / (np.abs(x).max() + 1e-9)


class Background:
    def __init__(self, rec_dirs, babble_clips):
        self.recs = []
        for d in rec_dirs:
            for p in K.wav_files(d):
                a = load(p)
                if len(a) > K.RATE // 2:
                    self.recs.append(a / (np.abs(a).max() + 1e-9))
        self.babble = babble_clips

    def get(self, n, rng):
        kinds = ["white", "pink", "brown", "hum", "clatter", "babble", "silence"] + (["rec"] * 4 if self.recs else [])
        kind = kinds[rng.integers(len(kinds))]
        if kind == "silence":
            return colored_noise(n, rng, "pink") * 0.02
        if kind == "hum":
            return hum(n, rng)
        if kind == "clatter":
            return clatter(n, rng) + colored_noise(n, rng, "pink") * 0.05
        if kind == "babble" and self.babble:
            x = np.zeros(n, np.float32)
            for _ in range(rng.integers(1, 4)):
                c = self.babble[rng.integers(len(self.babble))]
                p = rng.integers(0, max(1, n - len(c)))
                seg = c[:n - p] / (np.abs(c).max() + 1e-9)
                x[p:p + len(seg)] += seg * rng.uniform(0.3, 1.0)
            return x + colored_noise(n, rng, "pink") * 0.02
        if kind == "rec":
            r = self.recs[rng.integers(len(self.recs))]
            if len(r) < n:
                r = np.tile(r, n // len(r) + 1)
            p = rng.integers(0, len(r) - n + 1)
            return r[p:p + n].astype(np.float32)
        return colored_noise(n, rng, kind)


def reverb(x, rng):
    rt = rng.uniform(0.08, 0.45)
    L = int(rt * K.RATE)
    t = np.arange(L) / K.RATE
    ir = rng.standard_normal(L).astype(np.float32) * np.exp(-6.9 * t / rt)
    ir[0] = 1.0
    from scipy.signal import fftconvolve
    y = fftconvolve(x, ir)[:len(x)]
    return y / (np.abs(y).max() + 1e-9) * np.abs(x).max()


def speed(x, rng):
    from scipy.signal import resample
    f = rng.uniform(0.88, 1.12)
    return resample(x, int(len(x) / f)).astype(np.float32)


def coloring(x, rng):
    """einfache Mikrofon-Färbung: Hoch- und Tiefpass erster Ordnung"""
    from scipy.signal import lfilter
    a = rng.uniform(0.0, 0.9)
    y = lfilter([1 - a], [1, -a], x) if rng.random() < 0.5 else x
    b = rng.uniform(0.9, 0.995)
    return lfilter([1, -1], [1, -b], y).astype(np.float32) if rng.random() < 0.5 else y.astype(np.float32)


# ------------------------------------------------------------
#  Ein Trainingsbeispiel: Vorlauf + Fenster, Wort am Ende des Fensters
# ------------------------------------------------------------
def make_example(args):
    clip, label, seed, bg = args[:4]
    prefix = args[4] if len(args) > 4 else None
    rng = np.random.default_rng(seed)
    pre = int(rng.uniform(1.0, 3.0) * K.RATE)
    n = pre + WIN_S
    x = np.zeros(n, np.float32)
    if clip is not None:
        c = clip.copy()
        if rng.random() < 0.5:
            c = speed(c, rng)
        if rng.random() < 0.3:
            c = reverb(c, rng)
        c = coloring(c, rng)
        c = c / (np.abs(c).max() + 1e-9)
        # Wortende 0,05 … 0,45 s vor dem Fensterende
        end = n - int(rng.uniform(0.05, 0.45) * K.RATE)
        start = end - len(c)
        ws = n - WIN_S  # Beginn des Fensters, das das Netz sieht
        if start < ws:
            # Wort ragt vorn aus dem Fenster: lieber hinten kürzen ("Gramm") als vorne,
            # sonst lernt das Netz "…hundertfünfzig" als zweihundertfünfzig
            start = ws
            c = c[:end - ws]
        x[start:start + len(c)] = c
        # Davor das Weckwort (so kommt ein Befehl an der Waage immer an): das Netz sieht dann
        # das Ende von "Hallo Waage" und danach den Befehl im selben Fenster
        if prefix is not None:
            pc = coloring(prefix.copy(), rng)
            pc = pc / (np.abs(pc).max() + 1e-9) * rng.uniform(0.5, 1.4)
            pe = start - int(rng.uniform(0.15, 0.8) * K.RATE)
            ps = pe - len(pc)
            if pe > 0:
                if ps < 0:
                    pc, ps = pc[-ps:], 0
                x[ps:pe] += pc[:pe - ps]
        speech_peak = 10 ** (rng.uniform(-45, -3) / 20)  # sehr leise bis laut
        x *= speech_peak
        snr = rng.uniform(8, 65)  # bis fast still (leise Küche, lauter Sprecher)
        noise_rms = speech_peak * 0.3 * 10 ** (-snr / 20)
    else:
        noise_rms = 10 ** (rng.uniform(-70, -15) / 20)
    noise = bg.get(n, rng)
    noise = noise / (noise.std() + 1e-9) * noise_rms
    # Hintergrund vor dem Fenster oft anders laut (Dunstabzug aus, Radio an …):
    # der gleitende Mittelwert der Merkmale muss damit klarkommen
    if rng.random() < 0.5:
        noise[:pre] *= 10 ** (rng.uniform(-25, 10) / 20)
    x += noise
    # Rauschboden des Waagen-Mikrofons: in den Aufnahmen etwa -61 dBFS (rms 29 von 32768),
    # unabhängig davon, wie laut gesprochen wird. Vorher stand hier -85 dB, und das Rauschen
    # war sonst immer relativ zur Sprache: leise (weit entfernte) Sprache war im Training
    # damit viel sauberer als an der echten Waage.
    x += rng.standard_normal(n).astype(np.float32) * 10 ** (rng.uniform(-64, -57) / 20)
    pcm = np.clip(x * 32767, -32768, 32767).astype(np.int16)
    # andere Stimmlagen: Frequenzachse leicht stauchen/dehnen (VTLP)
    warp = float(rng.uniform(0.86, 1.14)) if clip is not None and rng.random() < 0.7 else 1.0
    f = K.features(pcm, warp=warp)
    return f[-K.FRAMES:]


_BG = None


def _init(bg):
    global _BG
    _BG = bg


def _job(a):
    clip, label, seed, prefix = a
    return make_example((clip, label, seed, _BG, prefix))


def build_set(items, copies, bg, procs, seed0):
    """items: Liste (clip oder None, label). copies: Anzahl Varianten je Eintrag."""
    jobs = []
    s = seed0
    wake = CLASSES.index("hallo_waage")
    wakes = [c for c, l, w in items if l == wake and c is not None]
    rng = random.Random(seed0)
    for clip, label, w in items:
        cls = CLASSES[label]
        # Befehle oft mit dem Weckwort davor, "andere Wörter" manchmal (-> "nicht verstanden")
        p_pre = 0.4 if not cls.startswith("_") and label != wake else (0.2 if cls == "_unbekannt" else 0)
        for _ in range(copies * w):
            pre = wakes[rng.randrange(len(wakes))] if wakes and clip is not None and rng.random() < p_pre else None
            jobs.append((clip, label, s, pre))
            s += 1
    with Pool(procs, initializer=_init, initargs=(bg,)) as pool:
        X = pool.map(_job, jobs, chunksize=32)
    y = np.array([j[1] for j in jobs], np.int64)
    return np.stack(X).astype(np.float32), y


# ------------------------------------------------------------
#  Eigene Aufnahmen von der Waage
# ------------------------------------------------------------
REC_EXTRA = ("_hintergrund", "_stille")
MULTI = ("hallo_waage", "wie_viel") + tuple(c for c in CLASSES if c.startswith("ziel_"))
OWN_VAL = []   # eigene Aufnahmen in der Prüfmenge: (audio, klasse) – Prüfung "wie an der Waage"
FRAG = set()   # id() der Weckwort-Anfänge ("Hallo Wa…" als unbekannt): keine fremde Sprache


def find_rec_dir(path):
    """Ordner mit den Wort-Unterordnern finden (auch wenn das ZIP ihn doppelt verschachtelt hat)."""
    if not path:
        return None
    names = set(CLASSES) | set(REC_EXTRA)
    cand = [path] + sorted(glob.glob(os.path.join(path, "*"))) + sorted(glob.glob(os.path.join(path, "*", "*")))
    for c in cand:
        if os.path.isdir(c) and any(os.path.isdir(os.path.join(c, n)) for n in names):
            if os.path.normpath(c) != os.path.normpath(path):
                print("Aufnahmen gefunden in", c)
            return c
    print("WARNUNG: In %s liegen keine Wort-Ordner (hallo_waage, tara …). Ohne eigene Aufnahmen weiter." % path)
    return None


def envelope(a, win=320, hop=160):
    n = max(1, (len(a) - win) // hop + 1)
    return np.array([np.sqrt(np.mean(a[i * hop:i * hop + win] ** 2) + 1e-9) for i in range(n)], np.float32)


def split_utterances(a, min_s=0.2, max_s=2.6, gap_s=0.35, pad_s=0.12, bounds=False):
    """Serienaufnahme (dasselbe Wort mehrmals mit Pausen) in einzelne Wörter zerschneiden.
    bounds=True: [(anfang, ende)] in Abtastwerten statt der Stücke."""
    hop = 160
    env = envelope(a, hop=hop)
    floor = np.percentile(env, 10)
    thr = max(floor * 4.0, env.max() * 0.05)
    act = env > thr
    segs, i, n = [], 0, len(act)
    gap = int(gap_s * K.RATE / hop)
    while i < n:
        if not act[i]:
            i += 1
            continue
        j = i
        quiet = 0
        while j < n and quiet <= gap:
            quiet = 0 if act[j] else quiet + 1
            j += 1
        e = j - quiet
        segs.append((i, e))
        i = j
    out = []
    pad = int(pad_s * K.RATE)
    for b, e in segs:
        d = (e - b) * hop / K.RATE
        if min_s <= d <= max_s:
            r = (max(0, b * hop - pad), min(len(a), e * hop + 160 + pad))
            out.append(r if bounds else a[r[0]:r[1]])
    return out


def cut_off(a):
    """Endet die Aufnahme mitten im Wort (Sprache bis ganz zum Schluss)?"""
    env = envelope(a)
    if len(env) < 20:
        return False
    return env[-4:].mean() > env.max() * 0.25 and env[-4:].mean() > np.percentile(env, 20) * 4


def load_recordings(rec_dir, drop_cut=False):
    """{klasse: [(audio, gewicht)]} und eine Übersicht je Wort."""
    from kws_daten import trim
    out = {}
    cut_files = []
    print("Eigene Aufnahmen:  Dateien  Wörter  abgeschnitten  Länge (Median)")
    total = 0
    for cls in CLASSES:
        files = K.wav_files(os.path.join(rec_dir, cls))
        if not files:
            continue
        clips, cut = [], 0
        for p in files:
            a = load(p)
            if len(a) < 800:
                continue
            if len(a) > 4 * K.RATE:  # Serie: in einzelne Wörter zerschneiden
                # "Hallo … Waage", "Ziel … fünfzig": die Pause im Wort ist oft länger als
                # zwischen einsilbigen Wiederholungen – sonst entstehen halbe Wörter
                gap = 0.6 if cls in MULTI else 0.35
                clips += [(trim(x), 4, p) for x in split_utterances(a, gap_s=gap)]
            elif cut_off(a):
                cut += 1
                cut_files.append(p)
                if not drop_cut:
                    clips.append((trim(a), 2, p))  # unvollständig: zählt weniger
            else:
                clips.append((trim(a), 4, p))
        out[cls] = clips
        total += len(clips)
        dur = np.median([len(c) / K.RATE for c, w, f in clips]) if clips else 0
        print("  %-14s %5d  %6d  %6d  %4.1f s%s" % (cls, len(files), len(clips), cut, dur,
                                          "  <- Aufnahme länger machen" if cut > len(files) // 3 else ""))
    for extra in REC_EXTRA:
        n = len(K.wav_files(os.path.join(rec_dir, extra)))
        if n:
            print("  %-14s %5d  (Hintergrund)" % (extra, n))
    if "_unbekannt" not in out:
        print("  Tipp: unter \"ein anderes Wort\" (_unbekannt) aufnehmen senkt Fehlauslösungen.")
    if not any(os.path.isdir(os.path.join(rec_dir, e)) for e in REC_EXTRA):
        print("  Tipp: unter \"Küchengeräusche\" (_hintergrund) die Küche aufnehmen.")
    print("  zusammen %d eigene Wörter" % total)
    if cut_files:
        liste = os.path.join(HERE, "abgeschnitten.txt")
        with open(liste, "w", encoding="utf-8") as f:
            f.write("\n".join(cut_files) + "\n")
        print("  %d Aufnahmen enden mitten im Wort (%s): Liste in %s" %
              (len(cut_files), "weggelassen" if drop_cut else "zählen halb, --ohne-abgeschnittene lässt sie weg", liste))
    return out


def collect(data_dir, rec_dir, drop_cut=False):
    train, val = [], []
    recs = load_recordings(rec_dir, drop_cut) if rec_dir else {}
    for ci, cls in enumerate(CLASSES):
        for p in glob.glob(os.path.join(data_dir, cls, "*.wav")):
            a = load(p)
            if len(a) < 800:
                continue
            (val if os.path.basename(p).startswith(HOLDOUT) else train).append((a, ci, 1))
        # eigene Aufnahmen: Stille vorn/hinten weg, vierfach gewichtet (abgeschnittene doppelt).
        # Prüfmenge je DATEI (fest nach dem Namen): die Wörter einer Serie landen alle auf
        # derselben Seite, sonst prüft man mit fast denselben Aufnahmen, mit denen trainiert wurde
        for a, w, f in recs.get(cls, []):
            if K.is_holdout(f, cls):
                val.append((a, ci, w))
                OWN_VAL.append((a, ci))
            else:
                train.append((a, ci, w))
    # Weckwort-Anfänge ("Hallo…") als unbekannt, damit es nicht zu früh auslöst
    hw, unk = CLASSES.index("hallo_waage"), CLASSES.index("_unbekannt")
    # das Weckwort ist am wichtigsten: dreifach so viele Varianten; kurze Einzelwörter
    # ("Tara", "Stopp", "Weiter", "Speichern") sind schwerer zu erkennen: doppelt
    # (auch die neuen kurzen Wörter doppelt zu gewichten hat im Test nicht geholfen)
    short = [CLASSES.index(c) for c in ("tara", "stopp", "weiter", "speichern")]
    train = [(a, ci, w * 3 if ci == hw else (w * 2 if ci in short else w)) for a, ci, w in train]
    for a, ci, w in list(train):
        if ci == hw:
            frag = a[:int(len(a) * random.uniform(0.3, 0.5))]
            FRAG.add(id(frag))
            train.append((frag, unk, 1))
    # Hintergrund ohne Sprache
    st = CLASSES.index("_stille")
    n_st = max(150, len(train) // len(CLASSES))
    train += [(None, st, 1)] * n_st
    val += [(None, st, 1)] * (n_st // 5)
    return train, val


def foreign_stream(bg, clips_unk, minutes, rng):
    """Langer Strom aus anderen Sätzen und Geräuschen (Abtastwerte wie 16 Bit)."""
    n = int(minutes * 60 * K.RATE)
    x = bg.get(n, rng) * 0.002
    p = 0
    while p < n - K.RATE:
        c = clips_unk[rng.integers(len(clips_unk))]
        c = c / (np.abs(c).max() + 1e-9) * 10 ** (rng.uniform(-35, -6) / 20)
        L = min(len(c), n - p)
        x[p:p + L] += c[:L]
        p += L + int(rng.uniform(0.3, 2.5) * K.RATE)
    return x * 32767


FA_MAX = 3  # höchstens so viele Fehl-Weckungen in 10 min Dauergerede


def calibrate(path, bg, clips_unk, val, forced=None):
    """Mit der fertigen Modelldatei rechnen wie die Waage (kws_pruefen.py):
    1. eigene Prüf-Aufnahmen – würde "Hallo Waage" wecken, werden Befehle erkannt?
    2. 10 min fremde Sprache – wie oft weckt sie die Waage?
    Daraus die Weckwort-Schwelle wählen und in die Datei schreiben."""
    import kws_pruefen as P
    m = P.read_model(path)
    hw = CLASSES.index("hallo_waage")
    cmd = [i for i, c in enumerate(CLASSES) if not c.startswith("_") and i != hw]
    rng = np.random.default_rng(5)
    t0 = time.time()
    # Fremde Sprache
    ts, sp = P.stream(m, foreign_stream(bg, clips_unk, 10, rng))
    ok = ts >= 2000
    def cross(v, thr):
        return int(((v[1:] >= thr) & (v[:-1] < thr)).sum())
    grid = [round(0.40 + 0.05 * i, 2) for i in range(9)]  # 0,40 … 0,80
    fa = {t: cross(sp[ok, hw], t) for t in grid}
    fa_cmd = cross(sp[ok][:, cmd].max(1), m["thr"])
    # Weckwort: eigene Prüf-Aufnahmen (sonst die Prüfstimme), nah und 20 dB leiser
    wake = [a for a, ci in OWN_VAL if ci == hw]
    src = "eigene Prüf-Aufnahmen"
    if len(wake) < 5:
        wake = [a for a, ci, w in val if ci == hw and a is not None][:40]
        src = "Prüfstimme " + HOLDOUT
    near = [P.wake_peak(m, a, rng, 0, 30) for a in wake]
    far = [P.wake_peak(m, a, rng, -20, 30) for a in wake]
    far2 = [P.wake_peak(m, a, rng, -30, 30) for a in wake]  # etwa 1 bis 2 m weg
    print("\nWie an der Waage gerechnet (geglättet, nach dem Einschwingen), %d x Hallo Waage (%s):" % (len(wake), src))
    print("  Schwelle   weckt (wie aufgenommen)  (20 dB leiser)  (30 dB leiser)  Fehlalarme in 10 min fremder Sprache")
    for t in grid:
        print("    %.2f          %3.0f %%               %3.0f %%          %3.0f %%          %d" %
              (t, np.mean([v >= t for v in near]) * 100, np.mean([v >= t for v in far]) * 100,
               np.mean([v >= t for v in far2]) * 100, fa[t]))
    if forced:
        thr = forced
        why = "vorgegeben (--weckschwelle)"
    else:
        # erlaubt: höchstens FA_MAX Fehlalarme; davon die höchste Schwelle, die (fast) so oft
        # weckt wie die niedrigste – tiefer bringt dann nichts, nur mehr Fehlalarme im Alltag
        cand = [t for t in grid if fa[t] <= FA_MAX and 0.45 <= t <= 0.70] or [0.70]
        det = {t: np.mean([v >= t for v in near + far + far2]) for t in cand}  # auch weiter weg zählt
        top = max(det.values())
        thr = max(t for t in cand if det[t] >= top - 0.03)
        why = "weckt %.0f %%, höchstens %d Fehlalarme" % (det[thr] * 100, FA_MAX)
    P.set_thresholds(path, wake=thr)
    print("  -> Weckwort-Schwelle %.2f (%s), im Modell gespeichert" % (thr, why))
    # Befehle auf eigenen Prüf-Aufnahmen
    own_cmd = [(a, ci) for a, ci in OWN_VAL if ci in cmd]
    if own_cmd:
        right, wrong = 0, {}
        for a, ci in own_cmd:
            tt, s2 = P.stream(m, P.embed(a, rng))
            s2 = s2[tt >= 2000]
            j = int(np.argmax(s2[:, cmd].max(1)))
            best = cmd[int(np.argmax(s2[j, cmd]))]
            if best == ci and s2[j, ci] >= m["thr"]:
                right += 1
            else:
                wrong[CLASSES[ci]] = wrong.get(CLASSES[ci], 0) + 1
        worst = sorted(wrong.items(), key=lambda kv: -kv[1])[:4]
        print("  Befehle (eigene Prüf-Aufnahmen): %d von %d erkannt%s" %
              (right, len(own_cmd), ("; daneben am häufigsten: " + ", ".join("%s %d" % kv for kv in worst)) if worst else ""))
    print("\nFehlalarme in 10 min fremder Sprache: Weckwort %d, Befehle %d  (%.0f s)" %
          (cross(sp[ok, hw], thr), fa_cmd, time.time() - t0))
    return thr


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--daten", default=os.path.join(HERE, "daten"))
    ap.add_argument("--aufnahmen", default=None, help="Ordner Aufnahmen von der SD-Karte")
    ap.add_argument("--ohne-abgeschnittene", action="store_true",
                    help="eigene Aufnahmen, die mitten im Wort enden, ganz weglassen")
    ap.add_argument("--epochen", type=int, default=40)
    ap.add_argument("--varianten", type=int, default=6, help="Varianten je Aufnahme")
    ap.add_argument("--aus", default=os.path.join(HERE, "kws_de.bin"))
    ap.add_argument("--weckschwelle", type=float, help="Weckwort-Schwelle fest vorgeben (sonst automatisch)")
    ap.add_argument("--kein-vergleich", action="store_true", help="am Ende nicht alle Modelle vergleichen")
    ap.add_argument("--prozesse", type=int, default=max(1, os.cpu_count() or 1))
    args = ap.parse_args()

    import torch
    import torch.nn as nn
    torch.manual_seed(3)
    random.seed(3)
    torch.set_num_threads(max(1, os.cpu_count() or 1))

    args.aufnahmen = find_rec_dir(args.aufnahmen)
    train, val = collect(args.daten, args.aufnahmen, args.ohne_abgeschnittene)
    rec_bg = [os.path.join(args.aufnahmen, d) for d in REC_EXTRA] if args.aufnahmen else []
    unk = CLASSES.index("_unbekannt")
    babble = [a for a, ci, w in train if ci == unk and a is not None][:300]
    bg = Background(rec_bg, babble)
    print("Aufnahmen: %d Training, %d Prüfung" % (len(train), len(val)))
    t0 = time.time()
    Xtr, ytr = build_set(train, args.varianten, bg, args.prozesse, 1)
    Xva, yva = build_set(val, 2, bg, args.prozesse, 10 ** 7)
    print("Beispiele: %d Training, %d Prüfung (%.0f s)" % (len(ytr), len(yva), time.time() - t0))

    mean = Xtr.reshape(-1, K.MELS).mean(0)
    istd = 1.0 / (Xtr.reshape(-1, K.MELS).std(0) + 1e-5)
    Xtr = (Xtr - mean) * istd
    Xva = (Xva - mean) * istd

    model = K.build_model(len(CLASSES))
    opt = torch.optim.AdamW(model.parameters(), lr=3e-3, weight_decay=1e-3)
    steps = args.epochen * ((len(ytr) + 63) // 64)
    sched = torch.optim.lr_scheduler.OneCycleLR(opt, max_lr=3e-3, total_steps=steps)
    counts = np.bincount(ytr, minlength=len(CLASSES)).astype(np.float32)
    wts = torch.tensor((counts.sum() / (counts + 1)) ** 0.5, dtype=torch.float32)
    lossf = nn.CrossEntropyLoss(weight=wts / wts.mean(), label_smoothing=0.05)
    Xt, yt = torch.tensor(Xtr), torch.tensor(ytr)
    Xv, yv = torch.tensor(Xva), torch.tensor(yva)
    best, best_state = -1, None
    for ep in range(args.epochen):
        model.train()
        perm = torch.randperm(len(yt))
        tl = 0
        for i in range(0, len(perm), 64):
            b = perm[i:i + 64]
            x = Xt[b].clone()
            # SpecAugment: Zeit- und Frequenzstreifen ausblenden
            for j in range(len(x)):
                if random.random() < 0.6:
                    t = random.randint(0, K.FRAMES - 8)
                    x[j, t:t + random.randint(1, 8)] = 0
                if random.random() < 0.6:
                    f = random.randint(0, K.MELS - 6)
                    x[j, :, f:f + random.randint(1, 6)] = 0
            opt.zero_grad()
            loss = lossf(model(x), yt[b])
            loss.backward()
            opt.step()
            sched.step()
            tl += loss.item() * len(b)
        model.eval()
        with torch.no_grad():
            pv = torch.cat([model(Xv[i:i + 512]) for i in range(0, len(yv), 512)]).argmax(1)
        acc = (pv == yv).float().mean().item()
        print("Epoche %2d  Verlust %.3f  Prüfung %.1f %%" % (ep + 1, tl / len(yt), acc * 100), flush=True)
        if acc >= best:
            best = acc
            best_state = {k: v.clone() for k, v in model.state_dict().items()}
    model.load_state_dict(best_state)
    model.eval()

    # Verwechslungen auf der Prüfstimme
    with torch.no_grad():
        pv = torch.cat([model(Xv[i:i + 512]) for i in range(0, len(yv), 512)]).argmax(1).numpy()
    print("\nTrefferquote je Klasse (Prüfstimme %s):" % HOLDOUT)
    for ci, c in enumerate(CLASSES):
        m = yva == ci
        if m.sum():
            wrong = np.bincount(pv[m], minlength=len(CLASSES))
            wrong[ci] = 0
            top = CLASSES[wrong.argmax()] if wrong.sum() else ""
            print("  %-12s %5.1f %%  %s" % (c, (pv[m] == ci).mean() * 100, ("meist verwechselt mit " + top) if top else ""))
    info = "trainiert %s, %d Beispiele, Prüfung %.0f %%" % (datetime.date.today().isoformat(), len(ytr), best * 100)
    # Das bisherige Modell nicht überschreiben, sondern in modelle\ aufheben (zum Vergleichen)
    if os.path.exists(args.aus):
        import shutil
        keep = os.path.join(HERE, "modelle")
        os.makedirs(keep, exist_ok=True)
        stamp = datetime.datetime.fromtimestamp(os.path.getmtime(args.aus)).strftime("%Y-%m-%d_%H%M")
        dst = os.path.join(keep, "modell_%s.bin" % stamp)
        if not os.path.exists(dst):
            shutil.copy2(args.aus, dst)
            print("\nBisheriges Modell aufgehoben: %s" % dst)
    size = K.export(model, CLASSES, mean, istd, args.aus, info)
    # Fremde Sprache ohne die abgeschnittenen Weckwörter ("Hallo Wa…"), die sind zu nah dran
    unk_clips = [a for a, ci, w in val + train if ci == unk and a is not None and id(a) not in FRAG]
    calibrate(args.aus, bg, unk_clips, val, args.weckschwelle)
    print("\nGespeichert: %s (%d KB) – %s" % (args.aus, size // 1024, info))
    # Zum Schluss alle Modelle mit ALLEN eigenen Wörtern vergleichen (Weckwort, Befehle, anderes
    # Wort), und zwar nur mit den Prüf-Dateien, die das neue Modell nicht gelernt hat, 20 dB leiser
    if not args.kein_vergleich and args.aufnahmen:
        import kws_pruefen as P
        models = [m for m in P.all_models() if os.path.abspath(m) != os.path.abspath(args.aus)] + [args.aus]
        print("\n==== Vergleich aller Modelle: alle Wörter, Prüf-Dateien, 20 dB leiser (wie aus etwa 1 m) ====")
        P.check_all_words(models, args.aufnahmen, level_db=-20, holdout_only=True)


if __name__ == "__main__":
    main()
