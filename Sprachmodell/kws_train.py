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
import argparse, datetime, glob, os, random, sys, time
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
            for p in glob.glob(os.path.join(d, "*.wav")):
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
    clip, label, seed, bg = args
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
    x += rng.standard_normal(n).astype(np.float32) * 10 ** (-85 / 20)  # Mikrofon-Eigenrauschen
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
    clip, label, seed = a
    return make_example((clip, label, seed, _BG))


def build_set(items, copies, bg, procs, seed0):
    """items: Liste (clip oder None, label). copies: Anzahl Varianten je Eintrag."""
    jobs = []
    s = seed0
    for clip, label, w in items:
        for _ in range(copies * w):
            jobs.append((clip, label, s))
            s += 1
    with Pool(procs, initializer=_init, initargs=(bg,)) as pool:
        X = pool.map(_job, jobs, chunksize=32)
    y = np.array([j[1] for j in jobs], np.int64)
    return np.stack(X).astype(np.float32), y


def collect(data_dir, rec_dir):
    train, val = [], []
    for ci, cls in enumerate(CLASSES):
        for p in glob.glob(os.path.join(data_dir, cls, "*.wav")):
            a = load(p)
            if len(a) < 800:
                continue
            (val if os.path.basename(p).startswith(HOLDOUT) else train).append((a, ci, 1))
        if rec_dir:
            for p in glob.glob(os.path.join(rec_dir, cls, "*.wav")):
                a = load(p)
                if len(a) < 800:
                    continue
                # eigene Aufnahmen: Stille vorn/hinten weg, vierfach gewichtet
                from kws_daten import trim
                a = trim(a)
                (val if random.random() < 0.15 else train).append((a, ci, 4))
    # Weckwort-Anfänge ("Hallo…") als unbekannt, damit es nicht zu früh auslöst
    hw, unk = CLASSES.index("hallo_waage"), CLASSES.index("_unbekannt")
    # das Weckwort ist am wichtigsten: dreifach so viele Varianten; kurze Einzelwörter
    # ("Tara", "Stopp", "Weiter", "Speichern") sind schwerer zu erkennen: doppelt
    short = [CLASSES.index(c) for c in ("tara", "stopp", "weiter", "speichern")]
    train = [(a, ci, w * 3 if ci == hw else (w * 2 if ci in short else w)) for a, ci, w in train]
    for a, ci, w in list(train):
        if ci == hw:
            train.append((a[:int(len(a) * random.uniform(0.3, 0.5))], unk, 1))
    # Hintergrund ohne Sprache
    st = CLASSES.index("_stille")
    n_st = max(150, len(train) // len(CLASSES))
    train += [(None, st, 1)] * n_st
    val += [(None, st, 1)] * (n_st // 5)
    return train, val


def stream_false_alarms(model, mean, istd, bg, clips_unk, minutes, rng):
    """Langer Strom aus anderen Sätzen und Geräuschen: wie oft löst etwas aus?"""
    import torch
    n = int(minutes * 60 * K.RATE)
    x = bg.get(n, rng) * 0.002
    p = 0
    while p < n - K.RATE:
        c = clips_unk[rng.integers(len(clips_unk))]
        c = c / (np.abs(c).max() + 1e-9) * 10 ** (rng.uniform(-35, -6) / 20)
        L = min(len(c), n - p)
        x[p:p + L] += c[:L]
        p += L + int(rng.uniform(0.3, 2.5) * K.RATE)
    pcm = np.clip(x * 32767, -32768, 32767).astype(np.int16)
    f = K.features(pcm, mean, istd)
    idx = list(range(K.FRAMES, len(f), K.INFER_HOPS))
    probs = []
    with torch.no_grad():
        for i in range(0, len(idx), 256):
            b = np.stack([f[j - K.FRAMES:j] for j in idx[i:i + 256]])
            probs.append(torch.softmax(model(torch.tensor(b)), 1).numpy())
    pr = np.concatenate(probs)
    sm = np.stack([pr[max(0, i - 2):i + 1].mean(0) for i in range(len(pr))])  # wie SMOOTH 3
    wake = CLASSES.index("hallo_waage")
    fa_wake = int(((sm[1:, wake] >= K.WAKE_THRESHOLD) & (sm[:-1, wake] < K.WAKE_THRESHOLD)).sum())
    cmd = [i for i, c in enumerate(CLASSES) if not c.startswith("_") and c != "hallo_waage"]
    mx = sm[:, cmd].max(1)
    fa_cmd = int(((mx[1:] >= K.THRESHOLD) & (mx[:-1] < K.THRESHOLD)).sum())
    return fa_wake, fa_cmd


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--daten", default=os.path.join(HERE, "daten"))
    ap.add_argument("--aufnahmen", default=None, help="Ordner Aufnahmen von der SD-Karte")
    ap.add_argument("--epochen", type=int, default=40)
    ap.add_argument("--varianten", type=int, default=6, help="Varianten je Aufnahme")
    ap.add_argument("--aus", default=os.path.join(HERE, "kws_de.bin"))
    ap.add_argument("--prozesse", type=int, default=max(1, os.cpu_count() or 1))
    args = ap.parse_args()

    import torch
    import torch.nn as nn
    torch.manual_seed(3)
    random.seed(3)
    torch.set_num_threads(max(1, os.cpu_count() or 1))

    train, val = collect(args.daten, args.aufnahmen)
    rec_bg = [os.path.join(args.aufnahmen, d) for d in ("_hintergrund", "_stille")] if args.aufnahmen else []
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
    rng = np.random.default_rng(5)
    unk_clips = [a for a, ci, w in val + train if ci == unk and a is not None]
    fw, fc = stream_false_alarms(model, mean, istd, bg, unk_clips, 10, rng)
    print("\nFehlalarme in 10 min fremder Sprache: Weckwort %d, Befehle %d" % (fw, fc))
    info = "trainiert %s, %d Beispiele, Prüfung %.0f %%" % (datetime.date.today().isoformat(), len(ytr), best * 100)
    size = K.export(model, CLASSES, mean, istd, args.aus, info)
    print("\nGespeichert: %s (%d KB) – %s" % (args.aus, size // 1024, info))


if __name__ == "__main__":
    main()
