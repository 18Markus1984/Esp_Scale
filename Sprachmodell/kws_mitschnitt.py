#!/usr/bin/env python3
# ============================================================
#  Mitschnitte der Waage holen und einsortieren
#
#  Die Waage schneidet auf der Mikrofonseite mit, was sie im Alltag hört
#  (/Waage/Sprache/Mitschnitt: w…/k… = Weckwort-Versuche, b…_<befehl>_PP =
#  Befehle, n… = nicht verstanden). Dieses Skript
#
#    1. holt den Ordner als ZIP von der Waage – nur Dateien, die es noch
#       nicht kennt (gleicher Inhalt = schon da, egal unter welchem Namen),
#    2. lässt alle Modelle (*.bin hier und in modelle\) drüber laufen und
#       sortiert jede Datei, bei der sich die sicheren Modelle einig sind,
#       nach Aufnahmen\<wort>\m_<name>.wav (nur das gemeinte Wort ausgeschnitten),
#    3. fragt nur nach, wenn kein Modell sicher ist oder sie sich
#       widersprechen: Datei wird abgespielt, du tippst das Wort.
#
#    python kws_mitschnitt.py                     holen + sortieren (waage.local)
#    python kws_mitschnitt.py --waage 192.168.178.50
#    python kws_mitschnitt.py holen               nur holen
#    python kws_mitschnitt.py sortieren           nur sortieren
#    python kws_mitschnitt.py sortieren --alles-fragen   jede Datei selbst bestimmen
#
#  Erledigte Dateien wandern nach Mitschnitt\erledigt, jede Entscheidung steht
#  in mitschnitt_bekannt.csv (so wird nichts doppelt geholt oder gefragt).
#  Danach wie gewohnt neu trainieren.
# ============================================================
import argparse, csv, hashlib, io, os, shutil, subprocess, sys, urllib.request, zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import numpy as np
import kws_common as K
import kws_pruefen as P
from kws_daten import load, save

MIT = os.path.join(HERE, "Mitschnitt")
DONE = os.path.join(MIT, "erledigt")
KNOWN = os.path.join(HERE, "mitschnitt_bekannt.csv")
SAFE_CMD = 0.60   # Befehl gilt als "sicher erkannt" ab dieser Wahrscheinlichkeit
SAFE_UNK = 0.70   # anderes Wort


def sha1(b):
    return hashlib.sha1(b).hexdigest()


def read_known():
    known = {}
    if os.path.exists(KNOWN):
        with open(KNOWN, encoding="utf-8") as f:
            for r in csv.reader(f, delimiter=";"):
                if r and len(r[0]) == 40:
                    known[r[0]] = r
    return known


def add_known(h, name, cls, how):
    new = not os.path.exists(KNOWN)
    with open(KNOWN, "a", encoding="utf-8", newline="") as f:
        w = csv.writer(f, delimiter=";")
        if new:
            w.writerow(["sha1", "datei", "wort", "wie"])
        w.writerow([h, name, cls, how])


def file_hashes(folders):
    hs = {}
    for d in folders:
        for root, _, files in os.walk(d):
            for n in files:
                if n.lower().endswith(".wav"):
                    p = os.path.join(root, n)
                    with open(p, "rb") as f:
                        hs[sha1(f.read())] = p
    return hs


# ------------------------------------------------------------
#  1. Holen
# ------------------------------------------------------------
def holen(waage, aufnahmen):
    url = "http://%s/zip?dir=%s" % (waage, urllib.request.quote("/Waage/Sprache/Mitschnitt"))
    print("Hole", url, "…")
    try:
        with urllib.request.urlopen(url, timeout=180) as r:
            data = r.read()
    except Exception as e:
        print("Waage nicht erreichbar (%s). WLAN an? Weboberfläche offen? Sonst --waage <IP-Adresse>." % e)
        return False
    try:
        z = zipfile.ZipFile(io.BytesIO(data))
    except zipfile.BadZipFile:
        print("Keine ZIP-Datei bekommen (gibt es den Ordner Mitschnitt auf der SD-Karte schon?)")
        return False
    known = set(read_known())
    known |= set(file_hashes([MIT] + ([aufnahmen] if aufnahmen and os.path.isdir(aufnahmen) else [])))
    os.makedirs(MIT, exist_ok=True)
    new = old = 0
    for info in z.infolist():
        if not info.filename.lower().endswith(".wav"):
            continue
        b = z.read(info)
        h = sha1(b)
        if h in known:
            old += 1
            continue
        name = os.path.basename(info.filename)
        dst = os.path.join(MIT, name)
        if os.path.exists(dst):  # gleicher Name, anderer Inhalt (Nummern fangen auf der Karte neu an)
            stem, ext = os.path.splitext(name)
            dst = os.path.join(MIT, "%s_%s%s" % (stem, h[:6], ext))
        with open(dst, "wb") as f:
            f.write(b)
        known.add(h)
        new += 1
    print("Mitschnitte: %d neu, %d schon bekannt (übersprungen)" % (new, old))
    return True


# ------------------------------------------------------------
#  2./3. Sortieren
# ------------------------------------------------------------
def play(path):
    try:
        if os.name == "nt":
            import winsound
            winsound.PlaySound(path, winsound.SND_FILENAME)
            return True
        for cmd in (["aplay", "-q"], ["afplay"], ["ffplay", "-nodisp", "-autoexit", "-loglevel", "quiet"]):
            if shutil.which(cmd[0]):
                subprocess.run(cmd + [path])
                return True
    except Exception:
        pass
    print("   (kann nicht abspielen – Datei selbst öffnen: %s)" % path)
    return False


def candidates(name, classes):
    """Was kann in der Datei stecken? Der Name sagt, wie die Waage sie gespeichert hat."""
    k = name[:1].lower()
    cmd = [c for c in classes if not c.startswith("_") and c != "hallo_waage"]
    if k in ("w", "k"):
        return ["hallo_waage", "_unbekannt"]
    if k in ("b", "n"):
        return cmd + ["_unbekannt"]
    return [c for c in classes if c != "_stille"]


def views(a):
    """Zwei Sichten auf einen Mitschnitt: so wie er ist (der Ring hat den Raumklang schon
    vorn drin) und wie im Training mit 2,6 s Einschwingen davor. Je Wort zählt die bessere."""
    v = [(P.base_features(P.embed(a, np.random.default_rng(1))), 2000, 2600)]
    if len(a) >= K.RATE:
        v.append((P.base_features(np.asarray(a, np.float32)), 0, 0))
    return v


def scan(m, vs):
    """{wort: (höchste Wahrscheinlichkeit, wann in ms ab Dateianfang)} eines Modells."""
    cls = m["classes"]
    pk = {}
    for f0, t0, shift in vs:
        ts, sp = P.probs_from(m, f0, t_from=t0)
        ok = ts >= t0
        if not ok.any():
            continue
        ts, sp = ts[ok], sp[ok]
        for i, c in enumerate(cls):
            j = int(sp[:, i].argmax())
            if float(sp[j, i]) > pk.get(c, (0.0, 0))[0]:
                pk[c] = (float(sp[j, i]), int(ts[j]) - shift)
    return pk


def guess(m, pk, cand):
    """(Wort, Wahrscheinlichkeit, sicher?) eines Modells für eine Datei."""
    pk = {c: pk[c][0] for c in cand if c in pk}
    if not pk:
        return "-", 0.0, False
    best = max(pk, key=pk.get)
    p = pk[best]
    if best == "hallo_waage":
        sure = p >= m["wthr"]
    elif best == "_unbekannt":
        others = [v for c, v in pk.items() if c != "_unbekannt"]
        sure = p >= SAFE_UNK and (not others or max(others) < m["thr"])
    else:
        sure = p >= max(SAFE_CMD, m["thr"])
    return best, p, sure


def cut_word(a, cls, t_ms):
    """Aus dem 3-s-Ring nur das gemeinte Wort ausschneiden (dort steht oft noch „Hallo Waage“
    vor dem Befehl oder der Befehl hinter dem Weckwort). Das Modell meldet ein Wort kurz nach
    seinem Ende, also das Stück, das zuletzt vor diesem Zeitpunkt endet."""
    import kws_train as T
    segs = T.split_utterances(a, gap_s=0.6 if cls in T.MULTI else 0.35, bounds=True)
    if not segs:
        return a
    t = t_ms * K.RATE // 1000
    before = [s for s in segs if s[1] <= t + K.RATE // 5]
    b, e = max(before, key=lambda s: s[1]) if before else min(segs, key=lambda s: abs(s[1] - t))
    return a[b:e]


def save_to(path, cls, aufnahmen, a, t_ms):
    d = os.path.join(aufnahmen, cls)
    os.makedirs(d, exist_ok=True)
    name = "m_" + os.path.basename(path)
    dst = os.path.join(d, name)
    k = 2
    while os.path.exists(dst):
        stem, ext = os.path.splitext(name)
        dst = os.path.join(d, "%s_%d%s" % (stem, k, ext))
        k += 1
    save(dst, np.clip(cut_word(a, cls, t_ms), -32768, 32767))
    return dst


def finish(path, h, cls, how):
    os.makedirs(DONE, exist_ok=True)
    dst = os.path.join(DONE, os.path.basename(path))
    if os.path.exists(dst):
        stem, ext = os.path.splitext(os.path.basename(path))
        dst = os.path.join(DONE, "%s_%s%s" % (stem, h[:6], ext))
    shutil.move(path, dst)
    add_known(h, os.path.basename(path), cls, how)


def sortieren(aufnahmen, alles_fragen=False):
    files = [f for f in K.wav_files(MIT)]
    if not files:
        print("Keine neuen Mitschnitte in", MIT)
        return
    models = P.all_models(HERE)
    if not models:
        print("Kein Modell gefunden (kws_de.bin oder modelle\\*.bin)")
        return
    ms = [P.read_model(p) for p in models]
    names = [P.short_name(p) for p in models]
    classes = [c for c in ms[-1]["classes"] if c != "_stille"]
    known = read_known()
    interactive = sys.stdin.isatty()
    print("Modelle: " + ", ".join(names))
    print("%d Mitschnitte. Sicher = Weckwort über der Weckschwelle, Befehl ab %.0f %%, anderes Wort ab %.0f %%.\n"
          % (len(files), SAFE_CMD * 100, SAFE_UNK * 100))
    auto = manual = skipped = 0
    shown_list = False
    for f in files:
        with open(f, "rb") as fh:
            h = sha1(fh.read())
        if h in known:  # schon entschieden (Datei lag noch da)
            finish(f, h, known[h][2], "doppelt")
            continue
        a = load(f)
        vs = views(a)
        cand = candidates(os.path.basename(f), classes)
        scans = [scan(m, vs) for m in ms]
        gs = [guess(m, pk, cand) for m, pk in zip(ms, scans)]

        def when(c):  # Zeitpunkt des Worts: vom sichersten Modell
            ts = [pk[c] for pk in scans if c in pk]
            return max(ts)[1] if ts else len(a) * 1000 // K.RATE
        sure = sorted({g[0] for g in gs if g[2]})
        line = "  ".join("%s: %s %.0f%%%s" % (n, g[0], g[1] * 100, " ✓" if g[2] else "") for n, g in zip(names, gs))
        print("%-26s %s" % (os.path.basename(f)[:26], line))
        if len(sure) == 1 and not alles_fragen:
            dst = save_to(f, sure[0], aufnahmen, a, when(sure[0]))
            finish(f, h, sure[0], "auto")
            print("   → %s (alle sicheren Modelle einig)" % os.path.relpath(dst, HERE))
            auto += 1
            continue
        if not interactive:
            print("   → offen (unsicher; zum Nachfragen im Terminal starten)")
            skipped += 1
            continue
        # nachfragen
        if not shown_list:
            print("\n   Wörter: " + "  ".join("%d=%s" % (i, c) for i, c in enumerate(classes)))
            print("   Enter = Vorschlag übernehmen, h = hallo_waage, u = _unbekannt (anderes Wort),")
            print("   Nummer = Wort, w = nochmal hören, x = verwerfen (Geräusch/kaputt), s = später, q = beenden\n")
            shown_list = True
        votes = {}
        for g in gs:
            votes[g[0]] = votes.get(g[0], 0) + g[1]
        sugg = max(votes, key=votes.get) if votes else "-"
        play(f)
        while True:
            ans = input("   Was war das? [Enter = %s] " % sugg).strip().lower()
            if ans == "w":
                play(f)
                continue
            if ans == "q":
                print("Beendet. %d automatisch, %d von Hand einsortiert." % (auto, manual))
                return
            if ans == "s":
                skipped += 1
                break
            if ans == "x":
                finish(f, h, "-", "verworfen")
                break
            cls = {"": sugg, "h": "hallo_waage", "u": "_unbekannt"}.get(ans)
            if cls is None and ans.isdigit() and int(ans) < len(classes):
                cls = classes[int(ans)]
            if cls is None or cls == "-":
                print("   ? Nummer, h, u, w, x, s oder q")
                continue
            dst = save_to(f, cls, aufnahmen, a, when(cls))
            finish(f, h, cls, "hand")
            print("   → %s" % os.path.relpath(dst, HERE))
            manual += 1
            break
    print("\nFertig: %d automatisch, %d von Hand einsortiert, %d offen." % (auto, manual, skipped))
    if auto or manual:
        print("Jetzt neu trainieren: python kws_train.py")


def main():
    ap = argparse.ArgumentParser(description="Mitschnitte der Waage holen und einsortieren")
    ap.add_argument("was", nargs="?", choices=["holen", "sortieren", "beides"], default="beides")
    ap.add_argument("--waage", default="waage.local", help="Adresse der Waage (Standard waage.local)")
    ap.add_argument("--aufnahmen", default=os.path.join(HERE, "Aufnahmen"), help="Ordner mit den Wort-Unterordnern")
    ap.add_argument("--alles-fragen", action="store_true", help="auch sichere Fälle nachfragen")
    a = ap.parse_args()
    if a.was in ("holen", "beides"):
        if not holen(a.waage, a.aufnahmen) and a.was == "holen":
            sys.exit(1)
    if a.was in ("sortieren", "beides"):
        sortieren(a.aufnahmen, a.alles_fragen)


if __name__ == "__main__":
    main()
