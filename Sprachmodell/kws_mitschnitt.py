#!/usr/bin/env python3
# ============================================================
#  Mitschnitte der Waage holen und einsortieren
#
#  Die Waage schneidet auf der Mikrofonseite mit, was sie im Alltag hört
#  (/Waage/Sprache/Mitschnitt: w…/k… = Weckwort-Versuche, b…_<befehl>_PP =
#  Befehle, n… = nicht verstanden). Dieses Skript
#
#    1. holt den Ordner als ZIP von der Waage – nur Dateien, die es noch
#       nicht kennt (gleicher Inhalt = schon da, egal unter welchem Namen) –
#       und gleich auch neue Wort-Aufnahmen (Setup → Ton → Wörter) nach Aufnahmen\<wort>\,
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
import kws_whisper as W
from kws_daten import load, save

MIT = os.path.join(HERE, "Mitschnitt")
DONE = os.path.join(MIT, "erledigt")
KNOWN = os.path.join(HERE, "mitschnitt_bekannt.csv")
SAFE_CMD = 0.60   # Befehl gilt als "sicher erkannt" ab dieser Wahrscheinlichkeit
SAFE_UNK = 0.70   # anderes Wort
MARGIN = 0.25     # sicher nur mit Abstand zum zweitbesten Wort (50/150/250 liegen eng beieinander)


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
def fetch_zip(waage, folder, timeout=600):
    """Ordner der Waage als ZIP holen. None = nicht erreichbar oder kein ZIP."""
    url = "http://%s/zip?dir=%s" % (waage, urllib.request.quote(folder))
    print("Hole", url, "…")
    try:
        with urllib.request.urlopen(url, timeout=timeout) as r:
            data = r.read()
    except Exception as e:
        print("Waage nicht erreichbar (%s). WLAN an? Weboberfläche offen? Sonst --waage <IP-Adresse>." % e)
        return None
    try:
        return zipfile.ZipFile(io.BytesIO(data))
    except zipfile.BadZipFile:
        print("Keine ZIP-Datei bekommen (gibt es den Ordner %s auf der SD-Karte?)" % folder)
        return None


def unique_path(d, name, h):
    """Gleicher Name, anderer Inhalt (Nummern fangen auf der Karte neu an): Name + Prüfsumme."""
    dst = os.path.join(d, name)
    if os.path.exists(dst):
        stem, ext = os.path.splitext(name)
        dst = os.path.join(d, "%s_%s%s" % (stem, h[:6], ext))
    return dst


def holen(waage, aufnahmen):
    z = fetch_zip(waage, "/Waage/Sprache/Mitschnitt")
    if z is None:
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
        with open(unique_path(MIT, os.path.basename(info.filename), h), "wb") as f:
            f.write(b)
        known.add(h)
        new += 1
    print("Mitschnitte: %d neu, %d schon bekannt (übersprungen)" % (new, old))
    return True


def holen_aufnahmen(waage, aufnahmen):
    """Neue Wort-Aufnahmen (Setup → Ton → Wörter) direkt in die Wort-Ordner übernehmen."""
    z = fetch_zip(waage, "/Waage/Sprache/Aufnahmen")
    if z is None:
        return False
    known = set(file_hashes([aufnahmen])) if os.path.isdir(aufnahmen) else set()
    per, old = {}, 0
    for info in z.infolist():
        parts = info.filename.replace("\\", "/").split("/")
        if not info.filename.lower().endswith(".wav") or len(parts) < 3:
            continue  # erwartet Aufnahmen/<wort>/<datei>.wav
        cls = parts[-2]
        b = z.read(info)
        h = sha1(b)
        if h in known:
            old += 1
            continue
        d = os.path.join(aufnahmen, cls)
        os.makedirs(d, exist_ok=True)
        with open(unique_path(d, parts[-1], h), "wb") as f:
            f.write(b)
        known.add(h)
        per[cls] = per.get(cls, 0) + 1
    if per:
        print("Aufnahmen: %d neu (%s), %d schon da"
              % (sum(per.values()), ", ".join("%s %d" % kv for kv in sorted(per.items())), old))
    else:
        print("Aufnahmen: nichts Neues (%d schon da)" % old)
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
    second = max([v for c, v in pk.items() if c != best], default=0.0)
    if p - second < MARGIN:
        return best, p, False
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


def device_word(name):
    """Was die Waage beim Mitschnitt verstanden hatte: b012_ziel_150_63.wav -> ziel_150."""
    parts = os.path.splitext(name)[0].split("_")
    if name[:1].lower() == "b" and len(parts) >= 3:
        return "_".join(parts[1:-1])
    return None


def sortieren(aufnahmen, alles_fragen=False, whisper=None, nur_neuestes=False):
    files = [f for f in K.wav_files(MIT)]
    if not files:
        print("Keine neuen Mitschnitte in", MIT)
        return
    models = P.all_models(HERE)
    if nur_neuestes and models:
        models = models[-1:]  # nur kws_de.bin (z. B. nach dem Neustart: das saubere Modell)
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
    if whisper:
        print("Whisper (%s) hört nur als Hilfe mit: entscheidet nie allein, nur wenn die eigenen Modelle"
              " unsicher sind und in dieselbe Richtung tendieren." % whisper)
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
        # automatisch nur, wenn die sicheren Modelle einig sind UND die Mehrheit aller
        # Modelle dasselbe Wort vorne hat (ein einzelnes sicheres Modell reicht nicht gegen den Rest)
        top = sum(1 for g in gs if g[0] == (sure[0] if sure else None))
        mc = sure[0] if len(sure) == 1 and top * 2 > len(gs) else None  # Urteil der eigenen Modelle
        dw = device_word(os.path.basename(f))
        if mc and dw and mc.startswith("ziel_") and dw.startswith("ziel_") and mc != dw:
            print("   Waage hatte %s verstanden, Modell sagt %s – lieber selbst hören" % (dw, mc))
            mc = None  # Zahlen verwechselt: nicht automatisch
        wcls, wt = None, None
        if whisper:
            try:
                text, words = W.transcribe(a, whisper)
                wcls, idx = W.classify(text, cand)
                wt = W.word_end_ms(words, idx) if wcls else None
                print("   Whisper: \"%s\" → %s" % (text[:60], wcls or "nichts verstanden"))
            except Exception as e:
                print("   Whisper geht nicht (%s) – weiter nur mit den eigenen Modellen" % e)
                whisper = None
        # Mehrheit aller Modelle (auch wenn unsicher): wohin tendieren sie?
        tops = {}
        for g in gs:
            tops[g[0]] = tops.get(g[0], 0) + 1
        lean = max(tops, key=tops.get) if tops else None
        if lean and tops[lean] * 2 <= len(gs):
            lean = None
        if not alles_fragen:
            # Die eigenen Modelle kennen das Mikrofon der Waage: ihr sicheres Urteil gilt zuerst
            if mc:
                dst = save_to(f, mc, aufnahmen, a, when(mc))
                finish(f, h, mc, "auto")
                print("   → %s (sichere Modelle einig, Mehrheit gleich)" % os.path.relpath(dst, HERE))
                auto += 1
                continue
            # Modelle unsicher, aber Mehrheit tendiert zum selben Wort wie Whisper
            if wcls and wcls == lean:
                dst = save_to(f, wcls, aufnahmen, a, wt if wt is not None else when(wcls))
                finish(f, h, wcls, "whisper")
                print("   → %s (Modelle tendieren dazu, Whisper bestätigt)" % os.path.relpath(dst, HERE))
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
        if wcls and wcls != sugg:
            print("   (Whisper meint %s – nur ein Hinweis)" % wcls)
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
            dst = save_to(f, cls, aufnahmen, a, wt if (cls == wcls and wt is not None) else when(cls))
            finish(f, h, cls, "hand")
            print("   → %s" % os.path.relpath(dst, HERE))
            manual += 1
            break
    print("\nFertig: %d automatisch, %d von Hand einsortiert, %d offen." % (auto, manual, skipped))
    if auto or manual:
        print("Jetzt neu trainieren: python kws_train.py --aufnahmen Aufnahmen --ohne-abgeschnittene")


def zuruecknehmen(aufnahmen, wie="whisper", quiet=False):
    """Einsortierte Mitschnitte wieder zurück nach Mitschnitt\\ (zum neu Sortieren),
    z. B. alles, was Whisper entschieden hat."""
    if not os.path.exists(KNOWN):
        print("Noch nichts einsortiert.")
        return
    with open(KNOWN, encoding="utf-8") as f:
        rows = list(csv.reader(f, delimiter=";"))
    keep, back = rows[:1], 0
    for r in rows[1:]:
        if len(r) < 4 or not (wie == "alle" or r[3] == wie) or r[2] in ("-", ""):
            keep.append(r)
            continue
        name, cls = r[1], r[2]
        stem, ext = os.path.splitext("m_" + name)
        d = os.path.join(aufnahmen, cls)
        for g in [stem + ext] + ["%s_%d%s" % (stem, k, ext) for k in range(2, 10)]:
            p = os.path.join(d, g)
            if os.path.exists(p):
                os.remove(p)  # nur die ausgeschnittene Kopie; das Original liegt in erledigt
                break
        src = os.path.join(DONE, name)
        if not os.path.exists(src):
            alt = os.path.join(DONE, "%s_%s%s" % (os.path.splitext(name)[0], r[0][:6], os.path.splitext(name)[1]))
            src = alt if os.path.exists(alt) else None
        if src:
            os.makedirs(MIT, exist_ok=True)
            shutil.move(src, unique_path(MIT, name, r[0]))
        back += 1
    with open(KNOWN, "w", encoding="utf-8", newline="") as f:
        csv.writer(f, delimiter=";").writerows(keep)
    if not quiet:
        print("%d Mitschnitte (%s) zurück nach Mitschnitt – jetzt neu sortieren: python kws_mitschnitt.py sortieren"
              % (back, wie))
    return back


ALT = os.path.join(MIT, "alt_abgehackt")


def neustart(aufnahmen, alte_sortieren=False):
    """Sauberer Neuanfang: alle Mitschnitte (m_…) aus den Aufnahmen nehmen. Eigene Aufnahmen
    bleiben unangetastet. Die bisherigen Mitschnitte sind abgehackt (Firmware vor dem
    Mikrofon-Puffer: Stücke fehlten) und passen nicht mehr zu dem, was die Waage jetzt hört –
    sie kommen nach Mitschnitt\\alt_abgehackt und werden nicht wieder einsortiert."""
    rest = zuruecknehmen(aufnahmen, "alle", quiet=True) or 0
    for root, _, files in os.walk(aufnahmen):
        for n in files:
            if not (n.startswith("m_") and n.lower().endswith(".wav")):
                continue
            p = os.path.join(root, n)
            orig = n[2:]
            src = os.path.join(DONE, orig)
            os.makedirs(MIT, exist_ok=True)
            if os.path.exists(src):  # Original (ganzer 3-s-Mitschnitt) ist noch da
                shutil.move(src, unique_path(MIT, orig, sha1(open(src, "rb").read())))
                os.remove(p)
            else:
                with open(p, "rb") as fh:
                    h = sha1(fh.read())
                shutil.move(p, unique_path(MIT, orig, h))
            rest += 1
    old = 0
    if not alte_sortieren:
        os.makedirs(ALT, exist_ok=True)
        for f in K.wav_files(MIT):
            shutil.move(f, os.path.join(ALT, os.path.basename(f)))
            old += 1
    print("Neustart: Aufnahmen enthalten jetzt nur noch eigene Wörter (%d weitere Mitschnitte zurück)." % rest)
    if old:
        print("%d alte (abgehackte) Mitschnitte liegen in Mitschnitt\\alt_abgehackt und werden nicht trainiert." % old)
    print("Weiter:  python kws_train.py --aufnahmen Aufnahmen --ohne-abgeschnittene")
    print("Neue Mitschnitte (mit der neuen Firmware) dann: python kws_mitschnitt.py --nur-neuestes")


def main():
    ap = argparse.ArgumentParser(description="Mitschnitte der Waage holen und einsortieren")
    ap.add_argument("was", nargs="?", choices=["holen", "sortieren", "beides", "zuruecknehmen", "neustart"], default="beides")
    ap.add_argument("--waage", default="waage.local", help="Adresse der Waage (Standard waage.local)")
    ap.add_argument("--aufnahmen", default=os.path.join(HERE, "Aufnahmen"), help="Ordner mit den Wort-Unterordnern")
    ap.add_argument("--alles-fragen", action="store_true", help="auch sichere Fälle nachfragen")
    ap.add_argument("--alte-sortieren", action="store_true", help="neustart: alte Mitschnitte trotzdem neu sortieren")
    ap.add_argument("--nur-neuestes", action="store_true", help="sortieren nur mit kws_de.bin (nicht mit alten Modellen)")
    ap.add_argument("--wie", default="whisper", help="zuruecknehmen: welche Entscheidungen (whisper, auto, hand, alle)")
    ap.add_argument("--ohne-aufnahmen", action="store_true", help="nur Mitschnitte holen, nicht die Wort-Aufnahmen")
    ap.add_argument("--whisper", nargs="?", const="small", default=None, metavar="MODELL",
                    help="Whisper als Hilfe mithören lassen (small, medium, large-v3-turbo); Standard: aus")
    a = ap.parse_args()
    if a.was == "neustart":
        neustart(a.aufnahmen, a.alte_sortieren)
        return
    if a.was == "zuruecknehmen":
        zuruecknehmen(a.aufnahmen, a.wie)
        return
    if a.was in ("holen", "beides"):
        ok = holen(a.waage, a.aufnahmen)
        if ok and not a.ohne_aufnahmen:
            import time
            time.sleep(5)  # der Waage Zeit lassen (WLAN-Puffer freigeben), bevor das nächste ZIP kommt
            holen_aufnahmen(a.waage, a.aufnahmen)
        if not ok and a.was == "holen":
            sys.exit(1)
    if a.was in ("sortieren", "beides"):
        wh = a.whisper
        if wh and not W.available():
            print("Whisper ist nicht installiert (pip install faster-whisper) – weiter ohne.")
            wh = None
        sortieren(a.aufnahmen, a.alles_fragen, wh, a.nur_neuestes)


if __name__ == "__main__":
    main()
