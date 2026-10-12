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


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("wav")
    ap.add_argument("--modell", default=os.path.join(os.path.dirname(os.path.abspath(__file__)), "kws_de.bin"))
    ap.add_argument("--alles", action="store_true", help="jede Auswertung ausgeben")
    ap.add_argument("--schwelle", type=float, help="Befehls-Schwelle ausprobieren (sonst aus dem Modell)")
    ap.add_argument("--weckschwelle", type=float, help="Weckwort-Schwelle ausprobieren")
    a = ap.parse_args()
    m = read_model(a.modell)
    if a.schwelle: m["thr"] = a.schwelle
    if a.weckschwelle: m["wthr"] = a.weckschwelle
    print("Modell:", m["info"])
    pcm = np.clip(load(a.wav), -32768, 32767).astype(np.int16)
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
