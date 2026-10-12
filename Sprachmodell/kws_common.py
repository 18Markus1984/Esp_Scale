# ============================================================
#  Gemeinsame Teile des Sprachmodells der Waage
#
#  - Klassen und Sätze (was die Waage verstehen soll)
#  - Merkmale: genau wie kws.cpp in der Firmware (gleiche Zahlen!)
#  - Netz (PyTorch) und Export als kws_de.bin
#
#  Wer Befehle ergänzt: Klasse in CLASSES_DE eintragen. Der Name
#  ist zugleich die Aktion in der Firmware (voice.cpp):
#    hallo_waage  Weckwort
#    tara, wie_viel, speichern, weiter, zurueck, stopp
#    ziel_<Gramm> Zielwiegen mit diesem Gewicht (z. B. ziel_750)
#  Klassen mit "_" vorn sind Hintergrund und lösen nichts aus.
# ============================================================
import struct
import numpy as np

# ---- Merkmale (muss zu kws.cpp passen) ----
RATE = 16000
WIN = 480          # 30 ms
HOP = 320          # 20 ms
NFFT = 512
MELS = 40
FRAMES = 100       # 2 s, so viel sieht das Netz ("zweihundertfünfzig Gramm" passt ganz hinein)
FMIN, FMAX = 20.0, 7600.0
CMN_FRAMES = 150   # gleitender Mittelwert je Band über etwa 3 s
INFER_HOPS = 10    # alle 200 ms auswerten (etwa 4 Mio. Multiplikationen je Auswertung)
THRESHOLD = 0.40        # Befehle (nur nach dem Weckwort, deshalb niedriger)
WAKE_THRESHOLD = 0.65   # Weckwort

# ---- Klassen und Sätze (Deutsch) ----
CLASSES_DE = {
    "_stille": [],     # nur Hintergrund
    "_unbekannt": [],  # andere Wörter (siehe OTHER_DE)
    "hallo_waage": ["Hallo Waage", "Hallo, Waage!", "Hallo Waage."],
    "tara": ["Tara", "Tarieren", "Auf null"],
    "wie_viel": ["Wie viel?", "Wie viel ist das?", "Wie schwer?"],
    "speichern": ["Speichern", "Speichern bitte"],
    "weiter": ["Weiter", "Weiter bitte"],
    "zurueck": ["Zurück", "Zurück bitte"],
    "stopp": ["Stopp", "Ende", "Beenden"],
    "ziel_50": ["fünfzig Gramm", "fünfzig"],
    "ziel_100": ["hundert Gramm", "hundert", "einhundert Gramm"],
    "ziel_150": ["hundertfünfzig Gramm", "hundertfünfzig"],
    "ziel_200": ["zweihundert Gramm", "zweihundert"],
    "ziel_250": ["zweihundertfünfzig Gramm", "zweihundertfünfzig"],
    "ziel_300": ["dreihundert Gramm", "dreihundert"],
    "ziel_500": ["fünfhundert Gramm", "fünfhundert", "ein Pfund"],
    "ziel_1000": ["tausend Gramm", "ein Kilo", "eintausend Gramm"],
}

# Andere Wörter: ähnlich klingende, andere Zahlen, Küchenwörter, Alltagssätze
OTHER_DE = [
    "Hallo", "Waage", "Wagen", "Hallo Wagen", "Hallo Mama", "Hallo zusammen", "Na hallo", "Wage es",
    "Tante", "Tafel", "Karte", "Tasse", "Sahne", "Paar", "Tarif", "Speicher", "Speiche", "Weit",
    "Weiß", "Leiter", "Zucker", "Rücken", "Stoppen", "Topf", "Tropfen", "Ende gut", "Wie bitte",
    "Wie spät ist es", "Wieso", "viel Spaß", "Kilo", "Pfund", "Gramm",
    "eins", "zwei", "drei", "vier", "sechs", "sieben", "acht", "neun", "zehn", "elf", "zwölf",
    "zwanzig", "dreißig", "vierzig", "sechzig", "siebzig", "achtzig", "neunzig",
    "vierhundert", "sechshundert", "siebenhundert", "achthundert", "neunhundert",
    "dreihundertfünfzig", "vierhundertfünfzig", "fünfundzwanzig", "fünfundfünfzig", "einhundertzehn",
    "zweitausend", "zwölfhundert",
    "Mehl", "Butter", "Milch", "Eier", "Salz", "Kuchen", "backen", "rühren", "Ofen", "Teller",
    "Schüssel", "Löffel", "Gabel", "Messer", "Kaffee", "Tee", "Wasser", "Brot", "Nudeln", "Reis",
    "Guten Morgen", "Danke", "Bitte", "Ja", "Nein", "Vielleicht", "Was ist das", "Komm mal her",
    "Hast du Hunger", "Das Wasser kocht", "Mach mal Pause", "Heute ist schönes Wetter",
    "Wo ist der Schlüssel", "Ich gehe einkaufen", "Das schmeckt gut", "Noch ein bisschen",
    "Ein bisschen mehr", "Das reicht", "Zu viel", "Genau richtig", "Moment mal", "Schau mal",
    "Gib mir mal den Zucker", "Wir brauchen noch Eier", "Der Kuchen ist fertig", "Wann kommst du",
    "Ich habe keine Zeit", "Das Telefon klingelt", "Mach das Fenster zu", "Gute Nacht",
]


# ------------------------------------------------------------
#  Merkmale wie in kws.cpp
# ------------------------------------------------------------
def _hz_to_mel(f):
    return 2595.0 * np.log10(1.0 + f / 700.0)


def _mel_to_hz(m):
    return 700.0 * (10.0 ** (m / 2595.0) - 1.0)


def mel_matrix(warp=1.0):
    """warp != 1: Frequenzachse gestaucht/gedehnt (nur fürs Training, simuliert andere Stimmen)"""
    nb = NFFT // 2 + 1
    pts = _mel_to_hz(np.linspace(_hz_to_mel(FMIN), _hz_to_mel(FMAX), MELS + 2)) * warp
    f = np.arange(nb) * RATE / NFFT
    W = np.zeros((MELS, nb), np.float32)
    for m in range(MELS):
        l, c, r = pts[m], pts[m + 1], pts[m + 2]
        up = (f > l) & (f < c)
        dn = (f >= c) & (f < r)
        W[m, up] = (f[up] - l) / (c - l)
        W[m, dn] = (r - f[dn]) / (r - c)
    return W


_W = mel_matrix()
_HANN = (0.5 - 0.5 * np.cos(2 * np.pi * np.arange(WIN) / WIN)).astype(np.float32)


def highpass(x):
    """y[n] = x[n] - x[n-1] + 0.995 y[n-1], Start mit 0 (wie kws.cpp)."""
    from scipy.signal import lfilter
    return lfilter([1.0, -1.0], [1.0, -0.995], x).astype(np.float32)


_WARPED = {}


def logmel(pcm, warp=1.0):
    """int16-Mono -> (Rahmen, MELS) Log-Mel, vor CMN und Normierung."""
    x = pcm.astype(np.float32) / 32768.0
    y = highpass(x)
    y = np.concatenate([np.zeros(WIN - HOP, np.float32), y])
    n = (len(y) - (WIN - HOP)) // HOP
    if n <= 0:
        return np.zeros((0, MELS), np.float32)
    idx = np.arange(WIN)[None, :] + HOP * np.arange(n)[:, None]
    fr = y[idx] * _HANN
    spec = np.fft.rfft(fr, NFFT)
    pw = (spec.real ** 2 + spec.imag ** 2).astype(np.float32)
    W = _W
    if warp != 1.0:
        key = round(warp, 2)
        if key not in _WARPED:
            _WARPED[key] = mel_matrix(key)
        W = _WARPED[key]
    return np.log(pw @ W.T + 1e-7).astype(np.float32)


def cmn(lm):
    """Gleitender Mittelwert je Band abziehen (wie kws.cpp: y = x - m; m += a (x - m))."""
    from scipy.signal import lfilter
    if len(lm) == 0:
        return lm
    a = 1.0 / CMN_FRAMES
    # m_t (nach dem Rahmen t) = (1-a) m_{t-1} + a x_t, Start m_{-1} = x_0
    zi = ((1 - a) * lm[0])[None, :]
    m_after = lfilter([a], [1.0, -(1 - a)], lm, axis=0, zi=zi)[0]
    m_before = np.vstack([lm[:1], m_after[:-1]])
    return (lm - m_before).astype(np.float32)


def features(pcm, mean=None, istd=None, warp=1.0):
    f = cmn(logmel(pcm, warp))
    if mean is not None:
        f = (f - mean) * istd
    return f.astype(np.float32)


# ------------------------------------------------------------
#  Netz: kleines DS-CNN, Ränder wie TensorFlow "same"
# ------------------------------------------------------------
def build_model(n_classes):
    import torch
    import torch.nn as nn
    import torch.nn.functional as F

    def same_pad(x, k, s):
        # x: (N, C, H, W); Rand wie TF: gesamt = max((ceil(in/s)-1)*s + k - in, 0), vorn die Hälfte
        h, w = x.shape[2], x.shape[3]
        oh, ow = -(-h // s[0]), -(-w // s[1])
        ph = max((oh - 1) * s[0] + k[0] - h, 0)
        pw = max((ow - 1) * s[1] + k[1] - w, 0)
        return F.pad(x, (pw // 2, pw - pw // 2, ph // 2, ph - ph // 2))

    class ConvBN(nn.Module):
        def __init__(self, cin, cout, k, s, dw=False):
            super().__init__()
            self.k, self.s, self.dw = k, s, dw
            self.conv = nn.Conv2d(cin, cout, k, s, groups=cin if dw else 1, bias=False)
            self.bn = nn.BatchNorm2d(cout)

        def forward(self, x):
            return F.relu(self.bn(self.conv(same_pad(x, self.k, self.s))))

    class DSCNN(nn.Module):
        # (Typ, Kanäle, Kern, Schritt) – Reihenfolge = Schichten in kws_de.bin
        PLAN = [("conv", 32, (8, 4), (2, 2)),
                ("dw", 32, (3, 3), (2, 2)), ("pw", 64, (1, 1), (1, 1)),
                ("dw", 64, (3, 3), (1, 1)), ("pw", 64, (1, 1), (1, 1)),
                ("dw", 64, (3, 3), (2, 2)), ("pw", 96, (1, 1), (1, 1)),
                ("dw", 96, (3, 3), (1, 1)), ("pw", 96, (1, 1), (1, 1))]

        def __init__(self, n):
            super().__init__()
            layers, c = [], 1
            for t, oc, k, s in self.PLAN:
                if t == "dw":
                    layers.append(ConvBN(c, c, k, s, dw=True))
                else:
                    layers.append(ConvBN(c, oc, k, s))
                    c = oc
            self.layers = nn.ModuleList(layers)
            self.drop = nn.Dropout(0.2)
            self.fc = nn.Linear(c, n)

        def forward(self, x):  # x: (N, FRAMES, MELS)
            x = x.unsqueeze(1)
            for l in self.layers:
                x = l(x)
            x = x.mean(dim=(2, 3))
            return self.fc(self.drop(x))

    return DSCNN(n_classes)


def set_thresholds(path, threshold, wake_threshold):
    """Schwellen in einer fertigen kws_de.bin ändern (ohne neu zu trainieren)."""
    b = bytearray(open(path, "rb").read())
    struct.pack_into("<2f", b, 4 + 18 + 8, threshold, wake_threshold)
    open(path, "wb").write(bytes(b))


def export(model, classes, mean, istd, path, info=""):
    """Schreibt kws_de.bin (Format siehe kws.cpp). BatchNorm wird in die Faltung gerechnet."""
    import torch
    model.eval()
    out = bytearray(b"KWS1")
    out += struct.pack("<9H", 1, RATE, WIN, HOP, NFFT, MELS, FRAMES, INFER_HOPS, CMN_FRAMES)
    out += struct.pack("<4f", FMIN, FMAX, THRESHOLD, WAKE_THRESHOLD)
    out += np.asarray(mean, "<f4").tobytes() + np.asarray(istd, "<f4").tobytes()
    out += struct.pack("<B", len(classes))
    for c in classes:
        b = c.encode()
        out += struct.pack("<B", len(b)) + b
    blocks = list(model.layers)
    out += struct.pack("<B", len(blocks) + 2)  # + GAP + DENSE
    with torch.no_grad():
        for blk in blocks:
            conv, bn = blk.conv, blk.bn
            scale = (bn.weight / torch.sqrt(bn.running_var + bn.eps)).numpy()
            bias = (bn.bias - bn.running_mean * bn.weight / torch.sqrt(bn.running_var + bn.eps)).numpy()
            w = conv.weight.numpy()  # (oc, ic/groups, kh, kw)
            kh, kw = blk.k
            sh, sw = blk.s
            if blk.dw:
                # Firmware: w[kh][kw][c]
                wf = (w[:, 0] * scale[:, None, None]).transpose(1, 2, 0)
                out += struct.pack("<B6B", 2, kh, kw, sh, sw, 1, 1)
            else:
                # Firmware: w[oc][kh][kw][ic]
                wf = (w * scale[:, None, None, None]).transpose(0, 2, 3, 1)
                out += struct.pack("<BH6B", 1, w.shape[0], kh, kw, sh, sw, 1, 1)
            out += np.ascontiguousarray(wf, "<f4").tobytes() + np.asarray(bias, "<f4").tobytes()
        out += struct.pack("<B", 3)
        fw = model.fc.weight.numpy()
        out += struct.pack("<BHB", 4, fw.shape[0], 0)
        out += np.ascontiguousarray(fw, "<f4").tobytes() + model.fc.bias.numpy().astype("<f4").tobytes()
    ib = info.encode()[:200]
    out += struct.pack("<H", len(ib)) + ib
    with open(path, "wb") as f:
        f.write(out)
    return len(out)
