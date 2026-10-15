#!/usr/bin/env python3
# ============================================================
#  Ein Modell (kws_de.bin) am PC ausprobieren, genau wie die Waage
#
#  Liest eine WAV-Datei (z. B. eine Aufnahme mit dem Handy: „Hallo
#  Waage … zweihundertfünfzig Gramm“) und zeigt, was die Waage
#  verstehen würde: Weckwort, dann Befehl, mit Zeitpunkt.
#
#    python kws_pruefen.py aufnahme.wav
#    python kws_pruefen.py aufnahme.wav --modell kws_de.bin --alles
#
#  Ganze Ordner (z. B. die Aufnahmen von der SD-Karte) und Modelle
#  vergleichen – wie oft würde "Hallo Waage" die Waage wecken?
#
#    python kws_pruefen.py --ordner Aufnahmen\hallo_waage
#    python kws_pruefen.py --ordner Aufnahmen\hallo_waage --modell alt.bin --modell kws_de.bin
#    python kws_pruefen.py --ordner Aufnahmen\hallo_waage --leiser 25   (wie aus 1–2 m Abstand)
#    (ohne --modell: alle Modelle im Ordner und in modelle\ nebeneinander, mit Übersicht)
#
#  Alle Wörter (Weckwort, Befehle, anderes Wort) mit allen Modellen, schlechteste zuerst:
#
#    python kws_pruefen.py --alle-woerter Aufnahmen
#    python kws_pruefen.py --alle-woerter Aufnahmen --nur-pruefdateien --leiser 20
#
#  Weckwort-Schwelle in einer fertigen kws_de.bin ändern (ohne neu zu trainieren):
#
#    python kws_pruefen.py --setze-weckschwelle 0.5 --modell kws_de.bin
#
#  Rechnet ohne PyTorch, nur mit numpy (Netz wie kws.cpp).
# ============================================================
import argparse, os, struct, sys
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import kws_common as K
from kws_daten import load

AWAKE_MS, DECIDE_MS, DECIDE_ZIEL_MS, AFTER_WAKE_MS, AFTER_CMD_MS = 6000, 450, 1000, 500, 1200


def read_model(path):
    b = open(path, "rb").read()
    assert b[:4] == b"KWS1", "keine Modelldatei"
    p = 4
    hdr = struct.unpack_from("<9H", b, p); p += 18
    ver, rate, win, hop, nfft, mels, frames, infer_hops, cmn_frames = hdr
    fmin, fmax, thr, wthr = struct.unpack_from("<4f", b, p); p += 16
    mean = np.frombuffer(b, "<f4", mels, p); p += 4 * mels
    istd = np.frombuffer(b, "<f4", mels, p); p += 4 * mels
    n = b[p]; p += 1
    classes = []
    for _ in range(n):
        L = b[p]; p += 1
        classes.append(b[p:p + L].decode()); p += L
    nl = b[p]; p += 1
    layers, c = [], 1
    for _ in range(nl):
        t = b[p]; p += 1
        if t == 1:
            oc, = struct.unpack_from("<H", b, p); p += 2
            kh, kw, sh, sw, pad, act = b[p:p + 6]; p += 6
            w = np.frombuffer(b, "<f4", oc * kh * kw * c, p).reshape(oc, kh, kw, c); p += 4 * w.size
            bias = np.frombuffer(b, "<f4", oc, p); p += 4 * oc
            layers.append(("conv", w, bias, (sh, sw), act)); c = oc
        elif t == 2:
            kh, kw, sh, sw, pad, act = b[p:p + 6]; p += 6
            w = np.frombuffer(b, "<f4", kh * kw * c, p).reshape(kh, kw, c); p += 4 * w.size
            bias = np.frombuffer(b, "<f4", c, p); p += 4 * c
            layers.append(("dw", w, bias, (sh, sw), act))
        elif t == 3:
            layers.append(("gap",))
        elif t == 4:
            o, = struct.unpack_from("<H", b, p); p += 2
            act = b[p]; p += 1
            w = np.frombuffer(b, "<f4", o * c, p).reshape(o, c); p += 4 * w.size
            bias = np.frombuffer(b, "<f4", o, p); p += 4 * o
            layers.append(("dense", w, bias, act)); c = o
    il, = struct.unpack_from("<H", b, p); p += 2
    info = b[p:p + il].decode(errors="replace")
    return dict(classes=classes, mean=mean, istd=istd, layers=layers, thr=thr, wthr=wthr,
                infer_hops=infer_hops, frames=frames, info=info)


def same_pad(x, k, s):
    h, w = x.shape[:2]
    oh, ow = -(-h // s[0]), -(-w // s[1])
    ph, pw = max((oh - 1) * s[0] + k[0] - h, 0), max((ow - 1) * s[1] + k[1] - w, 0)
    return np.pad(x, ((ph // 2, ph - ph // 2), (pw // 2, pw - pw // 2), (0, 0))), oh, ow


def run(m, feat):
    x = feat[:, :, None].astype(np.float32)
    for L in m["layers"]:
        if L[0] in ("conv", "dw"):
            w, bias, s, act = L[1], L[2], L[3], L[4]
            kh, kw = (w.shape[1], w.shape[2]) if L[0] == "conv" else (w.shape[0], w.shape[1])
            xp, oh, ow = same_pad(x, (kh, kw), s)
            if L[0] == "conv":
                y = np.zeros((oh, ow, w.shape[0]), np.float32)
                for ky in range(kh):
                    for kx in range(kw):
                        patch = xp[ky:ky + oh * s[0]:s[0], kx:kx + ow * s[1]:s[1], :]
                        y += patch @ w[:, ky, kx, :].T
            else:
                y = np.zeros((oh, ow, x.shape[2]), np.float32)
                for ky in range(kh):
                    for kx in range(kw):
                        y += xp[ky:ky + oh * s[0]:s[0], kx:kx + ow * s[1]:s[1], :] * w[ky, kx]
            y += bias
            x = np.maximum(y, 0) if act else y
        elif L[0] == "gap":
            x = x.mean(axis=(0, 1))
        else:
            x = L[1] @ x + L[2]
            if L[3]:
                x = np.maximum(x, 0)
    e = np.exp(x - x.max())
    return e / e.sum()


def stream(m, pcm):
    """Wie die Waage: Merkmale, alle infer_hops ein Netzdurchlauf, Mittel der letzten 3.
    Liefert (Zeiten in ms, geglättete Wahrscheinlichkeiten)."""
    f = K.features(np.clip(pcm, -32768, 32767).astype(np.int16), m["mean"], m["istd"])
    ts, sps, hist = [], [], []
    for i in range(m["frames"], len(f) + 1, m["infer_hops"]):
        hist = (hist + [softmax_run(m, f[i - m["frames"]:i])])[-3:]
        ts.append(i * K.HOP * 1000 // K.RATE)
        sps.append(np.mean(hist, axis=0))
    return np.array(ts), np.array(sps)


def softmax_run(m, feat):
    return run(m, feat)


def embed(a, rng, level_db=0.0, noise_rms=30.0, pre_s=2.6, post_s=1.2):
    """Aufnahme in ein Stück Hintergrund setzen (vorn 2,6 s: so lange schwingt die Waage ein).
    level_db verschiebt den Pegel der Aufnahme (-20 = weiter weg / leiser gesprochen)."""
    a = np.asarray(a, np.float32) * 10 ** (level_db / 20)
    x = np.concatenate([np.zeros(int(pre_s * K.RATE), np.float32), a, np.zeros(int(post_s * K.RATE), np.float32)])
    return x + rng.standard_normal(len(x)).astype(np.float32) * noise_rms


def wake_peak(m, a, rng, level_db=0.0, noise_rms=30.0):
    """Höchste (geglättete) Weckwort-Wahrscheinlichkeit nach dem Einschwingen."""
    ts, sp = stream(m, embed(a, rng, level_db, noise_rms))
    w = m["classes"].index("hallo_waage")
    ok = ts >= 2000
    return float(sp[ok, w].max()) if ok.any() else 0.0


def set_thresholds(path, wake=None, cmd=None):
    """Schwellen in einer fertigen Modelldatei ändern (Kopf: KWS1, 9 x u16, fmin, fmax, thr, wthr)."""
    b = bytearray(open(path, "rb").read())
    assert b[:4] == b"KWS1", "keine Modelldatei"
    if cmd is not None:
        struct.pack_into("<f", b, 4 + 18 + 8, float(cmd))
    if wake is not None:
        struct.pack_into("<f", b, 4 + 18 + 12, float(wake))
    open(path, "wb").write(bytes(b))


def all_models(here=None):
    """Alle Modelle zum Vergleichen: *.bin im Sprachmodell-Ordner und in modelle/
    (dort legt das Training das jeweils alte Modell ab), älteste zuerst, kws_de.bin zuletzt."""
    import glob
    here = here or os.path.dirname(os.path.abspath(__file__))
    fs = [f for f in glob.glob(os.path.join(here, "*.bin")) + glob.glob(os.path.join(here, "modelle", "*.bin"))]
    uniq = {}
    for f in fs:
        uniq[os.path.normcase(os.path.abspath(f))] = f
    fs = sorted(uniq.values(), key=lambda f: (os.path.basename(f).lower() == "kws_de.bin", os.path.getmtime(f)))
    return fs


def short_name(p):
    n = os.path.basename(p)
    if n.lower().endswith(".bin"):
        n = n[:-4]
    if n.lower().startswith("modell_"):
        n = n[7:]
    return n[:15]


def check_folders(models, folders, rng_seed=1, level_db=0.0, quiet=False):
    """Jede WAV in den Ordnern: würde sie die Waage wecken? Einzelaufnahmen: höchste
    Weckwort-Wahrscheinlichkeit; Serien (> 4 s): wie oft das Weckwort auslöst.
    Am Ende eine Übersicht je Modell. quiet = nur die Übersicht."""
    files = []
    for d in folders:
        files += K.wav_files(d)
    if not files:
        print("Keine WAV-Dateien in", ", ".join(folders))
        return
    W = 16
    names = [short_name(p) for p in models]
    ms = [read_model(p) for p in models]
    for n, p, m in zip(names, models, ms):
        print("%-15s %s · %s · Weckschwelle %.2f" % (n, os.path.basename(p), m["info"], m["wthr"]))
    if level_db:
        print("Aufnahmen %d dB leiser abgespielt (wie weiter weg gesprochen)" % -level_db)
    print()
    if not quiet:
        print("%-28s%7s" % ("Datei", "Pegel") + "".join("%*s" % (W, n) for n in names))
    hits = [0] * len(ms)
    total = [0] * len(ms)
    psum = [0.0] * len(ms)
    ser = [0] * len(ms)
    for f in files:
        a = load(f)
        row = "%-28s" % os.path.join(os.path.basename(os.path.dirname(f)), os.path.basename(f))[-28:]
        # Spitze der Aufnahme in dBFS (nach --leiser): -1 = sehr laut/nah, unter -35 = leise/weit
        pk_db = 20 * np.log10(max(1.0, float(np.abs(a).max())) * 10 ** (level_db / 20) / 32768)
        row += "%7.0f" % pk_db
        for k, m in enumerate(ms):
            rng = np.random.default_rng(rng_seed)
            if len(a) > 4 * K.RATE:  # Serie: Auslösungen zählen
                ts, sp = stream(m, embed(a, rng, level_db))
                w = m["classes"].index("hallo_waage")
                n = 0
                for t, v in zip(ts, sp[:, w]):
                    if t >= 2000 and v >= m["wthr"] and (n == 0 or t - last > 1500):
                        n, last = n + 1, t
                ser[k] += n
                row += "%*s" % (W, "Serie: %d x" % n)
            else:
                pk = wake_peak(m, a, rng, level_db)
                ok = pk >= m["wthr"]
                hits[k] += ok
                total[k] += 1
                psum[k] += pk
                row += "%*s" % (W, "%3.0f %% %s" % (pk * 100, "weckt" if ok else "-"))
        if not quiet:
            print(row)
    # Übersicht: alle Modelle nebeneinander, bestes markiert
    print("\nÜbersicht%s:" % (" (%d dB leiser)" % -level_db if level_db else ""))
    print("%-35s" % "" + "".join("%*s" % (W, n) for n in names))
    if total[0]:
        best = max(hits)
        print("%-35s" % "Einzelaufnahmen, die wecken" + "".join(
            "%*s" % (W, ("%d von %d" % (h, t)) + (" *" if h == best else "  ")) for h, t in zip(hits, total)))
        print("%-35s" % "mittlere Sicherheit" + "".join("%*s" % (W, "%.0f %%  " % (100 * s / t)) for s, t in zip(psum, total)))
    if any(ser):
        print("%-35s" % "Weckungen in Serien" + "".join("%*s" % (W, "%d  " % n) for n in ser))
    print("%-35s" % "Weckschwelle" + "".join("%*s" % (W, "%.2f  " % m["wthr"]) for m in ms))
    print("(* = die meisten Treffer. Prozent = höchste Weckwort-Wahrscheinlichkeit, wie auf der Waage")
    print(" geglättet; Pegel = lauteste Stelle in dBFS, Rauschboden der Waage etwa -61 dBFS)")


# ------------------------------------------------------------
#  Alle Wörter prüfen (nicht nur das Weckwort)
# ------------------------------------------------------------
def base_features(x):
    """Merkmale ohne Normierung (einmal je Aufnahme, die Modelle normieren selbst)."""
    return K.features(np.clip(x, -32768, 32767).astype(np.int16))


def probs_from(m, f0, t_from=1600):
    """Geglättete Wahrscheinlichkeiten wie die Waage, erst ab t_from ms gerechnet (spart Zeit)."""
    f = ((f0 - m["mean"]) * m["istd"]).astype(np.float32)
    ts, sps, hist = [], [], []
    for i in range(m["frames"], len(f) + 1, m["infer_hops"]):
        t = i * K.HOP * 1000 // K.RATE
        if t < t_from:
            continue
        hist = (hist + [run(m, f[i - m["frames"]:i])])[-3:]
        ts.append(t)
        sps.append(np.mean(hist, axis=0))
    return np.array(ts), np.array(sps)


def decide_command(m, ts, sp):
    """Befehl wie voice.cpp entscheiden (Schwelle, Sammelfenster, Zielmengen = zuletzt über
    der Schwelle). Gibt (Klasse oder None, Spitze) zurück."""
    cls = m["classes"]
    wake = cls.index("hallo_waage") if "hallo_waage" in cls else -1
    cmd = [k for k, c in enumerate(cls) if not c.startswith("_") and k != wake]
    thr = m["thr"]
    dec_end, peak, peak_t, dz = None, None, None, False
    for t, p in zip(ts, sp):
        if t < 2000:
            continue
        if dec_end is None:
            if max(p[k] for k in cmd) < thr:
                continue
            first = max(cmd, key=lambda k: p[k])
            dz = cls[first].startswith("ziel_")
            dec_end = t + (DECIDE_ZIEL_MS if dz else DECIDE_MS)
            peak, peak_t = np.zeros(len(cls)), np.zeros(len(cls))
        for k in cmd:
            if p[k] > peak[k]:
                peak[k], peak_t[k] = p[k], t
        if t >= dec_end:
            break
    if dec_end is None:
        return None, 0.0
    best = max(cmd, key=lambda k: peak[k])
    if dz:
        z = [k for k in cmd if cls[k].startswith("ziel_") and peak[k] >= thr]
        if z:
            best = max(z, key=lambda k: (peak_t[k], peak[k]))
    return cls[best], float(peak[best])


def judge(m, cls_name, ts, sp):
    """(richtig?, was die Waage verstanden hätte) für eine Aufnahme der Klasse cls_name."""
    cls = m["classes"]
    w = cls.index("hallo_waage")
    ok_t = ts >= 2000
    wake_pk = float(sp[ok_t, w].max()) if ok_t.any() else 0.0
    if cls_name == "hallo_waage":
        return wake_pk >= m["wthr"], ("hallo_waage" if wake_pk >= m["wthr"] else "-")
    got, _ = decide_command(m, ts, sp)
    if cls_name.startswith("_"):  # anderes Wort / Stille: darf weder wecken noch als Befehl gelten
        if wake_pk >= m["wthr"]:
            return False, "hallo_waage"
        return got is None, (got or "-")
    return got == cls_name, (got or "-")


def utterances(path, cls_name):
    """Einzelaufnahme -> [Audio]; Serie (> 4 s) -> einzelne Wörter (wie im Training)."""
    a = load(path)
    if len(a) <= 4 * K.RATE:
        return [a]
    try:
        import kws_train as T
        gap = 0.6 if cls_name in T.MULTI else 0.35
        return T.split_utterances(a, gap_s=gap) or [a]
    except Exception:
        return [a]


def check_all_words(models, rec_dir, level_db=0.0, holdout_only=False, rng_seed=1, worst_n=5):
    """Jede Klasse unter rec_dir (Aufnahmen/<klasse>/*.wav) mit allen Modellen prüfen:
    Weckwort weckt? Befehl richtig verstanden? Anderes Wort ignoriert? Schlechteste zuerst."""
    import time
    names = [short_name(p) for p in models]
    ms = [read_model(p) for p in models]
    classes = [c for c in ms[-1]["classes"] if c != "_stille"]
    W = 16
    for n, p, m in zip(names, models, ms):
        print("%-15s %s · %s · Weckschwelle %.2f, Befehle %.2f" % (n, os.path.basename(p), m["info"], m["wthr"], m["thr"]))
    print("Prüfung %s%s (wie an der Waage gerechnet; Befehle ohne vorheriges Weckwort)" % (
        "nur Prüf-Dateien (nicht trainiert)" if holdout_only else "alle eigenen Aufnahmen",
        ", %d dB leiser" % -level_db if level_db else ""))
    t0 = time.time()
    rows = []
    wrong = {}
    for c in classes:
        files = K.wav_files(os.path.join(rec_dir, c))
        if holdout_only:
            files = [f for f in files if K.is_holdout(f, c)]
        utts = []
        for f in files:
            utts += utterances(f, c)
        if not utts:
            continue
        hits = [0] * len(ms)
        for a in utts:
            rng = np.random.default_rng(rng_seed)
            f0 = base_features(embed(a, rng, level_db))
            for k, m in enumerate(ms):
                ts, sp = probs_from(m, f0)
                ok, got = judge(m, c, ts, sp)
                hits[k] += ok
                if not ok and k == len(ms) - 1:
                    wrong.setdefault(c, {}).setdefault(got, 0)
                    wrong[c][got] += 1
        rows.append((c, len(utts), hits))
        print("  %-12s %4d Wörter geprüft (%.0f s)" % (c, len(utts), time.time() - t0), end="\r", flush=True)
    print(" " * 60, end="\r")
    if not rows:
        print("Keine Aufnahmen in", rec_dir)
        return
    rows.sort(key=lambda r: (r[2][-1] / r[1], r[0]))  # schlechteste (neuestes Modell) zuerst
    print("\n%-14s%6s" % ("Wort", "Anzahl") + "".join("%*s" % (W, n) for n in names) + "   neuestes verstand stattdessen")
    tot_n, tot_h = 0, [0] * len(ms)
    for c, n, hits in rows:
        tot_n += n
        tot_h = [a + b for a, b in zip(tot_h, hits)]
        best = max(hits)
        cells = "".join("%*s" % (W, "%3.0f %%%s" % (100 * h / n, " *" if h == best and len(ms) > 1 else "  ")) for h in hits)
        wr = wrong.get(c, {})
        top = ", ".join("%s %d" % kv for kv in sorted(wr.items(), key=lambda kv: -kv[1])[:2])
        print("%-14s%6d%s   %s" % (c, n, cells, top))
    print("%-14s%6d" % ("gesamt", tot_n) + "".join("%*s" % (W, "%3.0f %%  " % (100 * h / tot_n)) for h in tot_h))
    weak = [r for r in rows if r[2][-1] / r[1] < 0.9][:worst_n]
    if weak:
        print("\nAm schwächsten (%s): %s" % (names[-1], ", ".join("%s %.0f %%" % (c, 100 * h[-1] / n) for c, n, h in weak)))
        print("→ Davon mehr aufnehmen (Setup → Ton → Wörter, gern als Serie und aus verschiedenen Abständen).")
    print("(* = bestes Modell je Wort. „-“ = nichts verstanden. Anderes Wort (_unbekannt) gilt als richtig,")
    print(" wenn es weder weckt noch als Befehl gilt. Aufnahmen, mit denen trainiert wurde, kennt das Modell –")
    print(" ehrlicher ist --nur-pruefdateien.)  Dauer %.0f s" % (time.time() - t0))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("wav", nargs="?")
    ap.add_argument("--modell", action="append", help="Modelldatei (mehrfach: Modelle vergleichen). Mit --ordner ohne --modell: alle *.bin im Ordner und in modelle/")
    ap.add_argument("--nur-uebersicht", action="store_true", help="mit --ordner: nur die Übersicht, keine Zeile je Datei")
    ap.add_argument("--alle-woerter", metavar="AUFNAHMEN", help="alle Wörter prüfen: Ordner mit den Klassen-Unterordnern")
    ap.add_argument("--nur-pruefdateien", action="store_true", help="mit --alle-woerter: nur Dateien, die das Training nicht benutzt")
    ap.add_argument("--ordner", nargs="+", help="alle WAV-Dateien dieser Ordner prüfen")
    ap.add_argument("--leiser", type=float, default=0, help="mit --ordner: Aufnahmen so viele dB leiser (z. B. 20 = weiter weg)")
    ap.add_argument("--setze-weckschwelle", type=float, help="Weckwort-Schwelle in die Modelldatei schreiben")
    ap.add_argument("--setze-schwelle", type=float, help="Befehls-Schwelle in die Modelldatei schreiben")
    ap.add_argument("--alles", action="store_true", help="jede Auswertung ausgeben")
    ap.add_argument("--schwelle", type=float, help="Befehls-Schwelle ausprobieren (sonst aus dem Modell)")
    ap.add_argument("--weckschwelle", type=float, help="Weckwort-Schwelle ausprobieren")
    a = ap.parse_args()
    models = a.modell or ([] if not (a.ordner or a.alle_woerter) else all_models()) or [os.path.join(os.path.dirname(os.path.abspath(__file__)), "kws_de.bin")]
    if a.setze_weckschwelle is not None or a.setze_schwelle is not None:
        for p in models:
            set_thresholds(p, a.setze_weckschwelle, a.setze_schwelle)
            m = read_model(p)
            print("%s: Weckschwelle %.2f, Befehlsschwelle %.2f – Datei wieder auf die SD-Karte "
                  "(/Waage/Sprache/kws_de.bin) kopieren" % (p, m["wthr"], m["thr"]))
        return
    if a.alle_woerter:
        check_all_words(models, a.alle_woerter, level_db=-abs(a.leiser), holdout_only=a.nur_pruefdateien)
        return
    if a.ordner:
        if a.weckschwelle or a.schwelle:
            print("Hinweis: --weckschwelle/--schwelle gelten hier nicht, dafür --setze-weckschwelle")
        check_folders(models, a.ordner, level_db=-abs(a.leiser), quiet=a.nur_uebersicht)
        return
    if not a.wav:
        ap.error("WAV-Datei oder --ordner angeben")
    m = read_model(models[0])
    if a.schwelle: m["thr"] = a.schwelle
    if a.weckschwelle: m["wthr"] = a.weckschwelle
    print("Modell:", m["info"])
    x = load(a.wav)
    if len(x) < 4 * K.RATE:  # kurze Aufnahme: wie an der Waage Vorlauf zum Einschwingen davor
        x = embed(x, np.random.default_rng(1))
        print("(kurze Aufnahme: 2,6 s Vorlauf ergänzt)")
    pcm = np.clip(x, -32768, 32767).astype(np.int16)
    f = K.features(pcm, m["mean"], m["istd"])
    cls = m["classes"]
    wake = cls.index("hallo_waage") if "hallo_waage" in cls else -1
    hist, awake, awake_until, mute, dec, dec_end, peak = [], False, 0, 0, False, 0, None
    for i in range(m["frames"], len(f) + 1, m["infer_hops"]):
        t = i * K.HOP * 1000 // K.RATE
        p = run(m, f[i - m["frames"]:i])
        hist = (hist + [p])[-3:]
        sp = np.mean(hist, axis=0)
        if a.alles:
            top = np.argsort(sp)[::-1][:2]
            print("%6.2f s  %s %.2f  %s %.2f" % (t / 1000, cls[top[0]], sp[top[0]], cls[top[1]], sp[top[1]]))
        if t < 2000 or t < mute:  # wie die Waage: erst nach 2 s Einschwingen
            dec = False
            continue
        if wake >= 0 and sp[wake] >= m["wthr"] and not dec:
            if not awake:
                print("%6.2f s  Weckwort (%.0f %%)" % (t / 1000, sp[wake] * 100))
            awake, awake_until, mute = True, t + AWAKE_MS, t + AFTER_WAKE_MS
            continue
        if not awake:
            continue
        if not dec and t > awake_until:
            awake = False
            continue
        cmd = [k for k, c in enumerate(cls) if not c.startswith("_") and k != wake]
        if not dec:
            if max(sp[k] for k in cmd) < m["thr"]:
                continue
            first = max(cmd, key=lambda k: sp[k])
            dz = cls[first].startswith("ziel_")
            dec, dec_end, peak, peak_t = True, t + (DECIDE_ZIEL_MS if dz else DECIDE_MS), np.zeros(len(cls)), np.zeros(len(cls))
        for k in cmd:
            if sp[k] > peak[k]:
                peak[k], peak_t[k] = sp[k], t
        if t >= dec_end:
            best = int(np.argmax(peak))
            if dz:  # Zielmengen: die zuletzt über der Schwelle war (wie voice.cpp)
                z = [k for k in cmd if cls[k].startswith("ziel_") and peak[k] >= m["thr"]]
                if z:
                    best = max(z, key=lambda k: (peak_t[k], peak[k]))
            print("%6.2f s  Befehl: %s (%.0f %%)" % (t / 1000, cls[best], peak[best] * 100))
            dec, awake, mute = False, False, t + AFTER_CMD_MS


if __name__ == "__main__":
    main()
