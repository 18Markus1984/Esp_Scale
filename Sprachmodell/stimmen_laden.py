#!/usr/bin/env python3
# ============================================================
#  Lädt sechs deutsche Piper-Stimmen (je etwa 60 MB) nach stimmen/
#  Quelle: github.com/rhasspy/piper, Release v0.0.2 (MIT-Lizenz,
#  Stimmen siehe jeweilige MODEL_CARD). Nur einmal nötig.
#
#    python stimmen_laden.py
# ============================================================
import os, tarfile, urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
URL = "https://github.com/rhasspy/piper/releases/download/v0.0.2/voice-de-%s.tar.gz"
VOICES = ["thorsten-low", "eva_k-x-low", "karlsson-low", "kerstin-low", "pavoque-low", "ramona-low"]


def main():
    out = os.path.join(HERE, "stimmen")
    os.makedirs(out, exist_ok=True)
    for v in VOICES:
        onnx = os.path.join(out, "de-%s.onnx" % v)
        if os.path.exists(onnx):
            print("vorhanden:", v)
            continue
        tgz = os.path.join(out, "de-%s.tar.gz" % v)
        print("lade", v, "…", flush=True)
        urllib.request.urlretrieve(URL % v, tgz)
        with tarfile.open(tgz) as t:
            for m in t.getmembers():
                if m.name.endswith(".onnx") or m.name.endswith(".onnx.json"):
                    m.name = os.path.basename(m.name)
                    t.extract(m, out)
        os.remove(tgz)
    print("fertig:", out)


if __name__ == "__main__":
    main()
