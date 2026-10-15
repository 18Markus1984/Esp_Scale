#!/usr/bin/env python3
# Baut die Artboards des UI-Design-Canvas: jede Display-Seite (aus dem Simulator)
# und jede Seite der Weboberfläche (aus dem Browser-Rundgang), alles als Formen.
import glob, json, os, re, html, datetime, sys
sys.path.insert(0, "/home/claude/host")
from tohtml import screen

ROOT = sys.argv[1]
PROJ = os.path.join(ROOT, "project")
os.makedirs(PROJ, exist_ok=True)

FIX = [("pruef", "prüf"), ("faellig", "fällig"), ("kueche", "küche"), ("muenzen", "münzen"), ("toepfe", "töpfe"),
       ("zurueck", "zurück"), ("sprueche", "sprüche"), ("aufloesung", "auflösung"), ("giessen", "gießen"),
       ("lautst", "Lautstärke"), ("einst", "Einstellungen"), ("zaehlen", "zählen"), ("ueberlast", "Überlast"),
       ("waehlen", "wählen"), ("uebersicht", "Übersicht"), ("loeffel", "Löffel"), ("mikro", "Mikrofon"),
       ("lz ", "Langzeit "), ("lz_", "Langzeit "), ("fw", "Firmware"), ("spk", "Lautsprecher"), ("cd b", "Countdown")]


def caption(n):
    n = re.sub(r"^(de|en)_\d+_", "", n)
    n = n.replace("_", " ")
    for a, b in FIX:
        n = n.replace(a.replace("_", " "), b)
    return n[:1].upper() + n[1:]


HEAD = """<!doctype html>
<html lang="de">
<head>
<meta charset="utf-8">
<title>{title}</title>
<script src="./support.js"></script>
</head>
<body>
<x-dc>
<helmet>
{links}
<style>
{css}
</style>
</helmet>
{body}
</x-dc>
<script type="text/x-dc" data-dc-script data-props='{{"$preview":{{"width":{w},"height":{h}}}}}'>
class Component extends DCLogic {{
renderVals() {{ return {{}}; }}
}}
</script>
</body>
</html>
"""
FONTS_DISPLAY = ('<link rel="preconnect" href="https://fonts.googleapis.com">\n'
                 '<link href="https://fonts.googleapis.com/css2?family=Space+Grotesk:wght@400;500;600&amp;display=swap" rel="stylesheet">\n'
                 '<link href="https://fonts.googleapis.com/css2?family=Material+Symbols+Rounded:opsz,wght,FILL,GRAD@20..48,100..700,0..1,-50..200&amp;display=block" rel="stylesheet">')

# ---------------- Display ----------------
SECTIONS = [
    ("wiegen", "Display · Wiegen und Hauptmenü",
     r"lagecheck|wiegen|modi$|system$|^setup$|home|spk|ueberlast|topf_cd_b|topf_tariert|topf_mit_inhalt|praezision|negativ|autonull|ml_|nach_wechsel|gruppe"),
    ("setup", "Display · Setup",
     r"^Wiegen$|display|Zeit|^Ton$|ton_|^Waage$|wlan|fw|^web$|uhrzeit|datum|lautst|stimme|mikro_aus|autotara|bluetooth|kalib|msa|libelle_setup|pruef|qr_"),
    ("kueche", "Display · Küche", r"rezept|cocktail|ziel|portion|timer|tassen"),
    ("werkstatt", "Display · Werkstatt", r"spule|zaehlen|porto|mix|lz_|muenzen"),
    ("spiele", "Display · Spiele", r"spiel|trink|blind|halb"),
    ("system", "Display · System und Eingabe", r"protokoll|toepfe|topf_|akku|^mic$|^libelle$|platzhalter|texteingabe"),
    ("weiss", "Display · Farbschema Weiß", r"weiss|farbe"),
]
SKIP = {"topf_cd_a", "topf_cd_c", "home_gedreht", "display_zurueck"}
dumps = sorted(glob.glob("/home/claude/host/dump/de_*.json"), key=lambda p: int(re.search(r"de_(\d+)_", p).group(1)))
used = set()
groups = {k: [] for k, _, _ in SECTIONS}
for p in dumps:
    short = re.sub(r"^de_\d+_", "", os.path.basename(p)[:-5])
    if short in SKIP:
        continue
    for key, _, rx in sorted(SECTIONS, key=lambda x: x[0] != "weiss"):  # Weiß zuerst prüfen
        if re.search(rx, short):
            groups[key].append(p)
            used.add(p)
            break
left = [p for p in dumps if p not in used and re.sub(r"^de_\d+_", "", os.path.basename(p)[:-5]) not in SKIP]
groups["system"] += left
print("nicht zugeordnet:", [os.path.basename(p) for p in left])

boards, order, pages = {}, [], []
COLS, PER = 4, 8
CELL_W, CELL_H, GAP = 412, 412 + 46, 56
BW = 56 * 2 + COLS * CELL_W + (COLS - 1) * GAP
DCSS = ("body{margin:0;background:#191A17}\n"
        ".cap{font:500 15px 'Space Grotesk',sans-serif;color:#A6A398;text-align:center;width:412px}\n")
first = True
for key, title, _ in SECTIONS:
    items = groups[key]
    pages.append({"id": key, "name": title.replace("Display · ", "Display: ")})
    y = 0
    for bi in range(0, len(items), PER):
        chunk = items[bi:bi + PER]
        rows = (len(chunk) + COLS - 1) // COLS
        bh = 56 * 2 + rows * CELL_H + (rows - 1) * GAP
        cells = "".join(
            f'<div style="display:flex;flex-direction:column;align-items:center;gap:22px">{screen(p)}'
            f'<div class="cap">{html.escape(caption(os.path.basename(p)[:-5]))}</div></div>' for p in chunk)
        body = (f'<div style="width:{BW}px;height:{bh}px;box-sizing:border-box;padding:56px;display:grid;'
                f'grid-template-columns:repeat({COLS},minmax(0,1fr));row-gap:{GAP}px;column-gap:{GAP}px;align-content:start;justify-items:center">{cells}</div>')
        name = "Main.dc.html" if first else f"d_{key}_{bi // PER + 1}.dc.html"
        first = False
        btitle = f"{title} {bi // PER + 1}"
        open(os.path.join(PROJ, name), "w").write(
            HEAD.format(title=html.escape(btitle), links=FONTS_DISPLAY, css=DCSS, body=body, w=BW, h=bh))
        boards[name] = {"x": 0, "y": y, "w": BW, "h": bh, "title": btitle, "page": key}
        order.append(name)
        y += bh + 120

# ---------------- Web ----------------
page_html = open("/home/claude/work/src/Waage/web/page.html").read()
css = "".join(re.findall(r"<style>(.*?)</style>", page_html, re.S))
css = css.replace("html,body{", ".ph{").replace("body{", ".ph{").replace("position:fixed", "position:absolute")
css = re.sub(r"env\([^)]*\)", "0px", css)
css = css.replace("html[data-theme=w]", ".thw")
WCSS = "body{margin:0;background:#191A17}\n" + css + \
    "\n.ph{position:relative;width:390px;overflow:hidden;border-radius:28px;box-shadow:0 0 0 8px #050505,0 0 0 10px #34352F;flex:none}\n" \
    ".wcap{font:500 15px system-ui,sans-serif;color:#A6A398;text-align:center;width:390px}\n"

SVG_ATTR = ["stroke-width", "stroke-linecap", "stroke-linejoin", "fill-rule", "clip-rule", "stroke-dasharray",
            "stroke-dashoffset", "stroke-opacity", "fill-opacity", "text-anchor", "font-size", "font-weight",
            "font-family", "dominant-baseline", "stop-color", "stop-opacity", "stroke-miterlimit"]


def clean(h):
    # SVG-Darstellungsattribute in style verschieben (sicher für React)
    def fix_tag(m):
        tag = m.group(0)
        extra = []
        for a in SVG_ATTR:
            mm = re.search(r'\s' + a + r'="([^"]*)"', tag)
            if mm:
                extra.append(f"{a}:{mm.group(1)}")
                tag = tag.replace(mm.group(0), "")
        if extra:
            if ' style="' in tag:
                tag = tag.replace(' style="', ' style="' + ";".join(extra) + ";", 1)
            else:
                tag = tag.replace(">", f' style="{";".join(extra)}">', 1) if not tag.endswith("/>") else tag[:-2] + f' style="{";".join(extra)}"/>'
        return tag
    h = re.sub(r"<(?:svg|path|circle|rect|line|polyline|polygon|g|ellipse|text|tspan)\b[^>]*>", fix_tag, h)
    # Auswahl in <select>: value am select statt selected an der option
    def fix_select(m):
        s = m.group(0)
        sel = re.search(r'<option([^>]*)\sselected(="")?([^>]*)>', s)
        val = None
        if sel:
            v = re.search(r'value="([^"]*)"', sel.group(1) + sel.group(3))
            val = v.group(1) if v else None
            if val is None:
                t = re.search(r'<option[^>]*\sselected(?:="")?[^>]*>([^<]*)<', s)
                val = t.group(1) if t else None
        s = re.sub(r'\sselected(="")?', "", s)
        if val is not None:
            s = s.replace("<select", f'<select value="{val}"', 1)
        return s
    h = re.sub(r"<select\b.*?</select>", fix_select, h, flags=re.S)
    h = re.sub(r'\schecked=""', ' checked=""', h)
    h = h.replace("{{", "{ {").replace("}}", "} }")
    # selbstschließende Nicht-Void-Elemente vermeiden (kommt aus innerHTML nicht vor, sicherheitshalber)
    return h


PERW, WG = 4, 56


def web_boards(wfiles, prefix, page, title, cls=""):
    y = 0
    for bi in range(0, len(wfiles), PERW):
        chunk = wfiles[bi:bi + PERW]
        phs = []
        maxh = 0
        for p in chunk:
            d = json.load(open(p))
            h = min(max(d["h"], 700), 4200)
            maxh = max(maxh, h)
            cap = caption("de_0_" + "_".join(os.path.basename(p)[:-5].split("_")[1:]))
            phs.append(f'<div style="display:flex;flex-direction:column;align-items:center;gap:22px">'
                       f'<div class="ph {cls} {html.escape(d.get("bodyClass", ""))}" style="height:{h}px">{clean(d["html"])}</div>'
                       f'<div class="wcap">{html.escape(cap)}</div></div>')
        bw = 56 * 2 + PERW * 390 + (PERW - 1) * WG
        bh = 56 * 2 + maxh + 50
        body = (f'<div style="width:{bw}px;height:{bh}px;box-sizing:border-box;padding:56px;display:flex;gap:{WG}px;align-items:flex-start">'
                + "".join(phs) + "</div>")
        name = f"{prefix}_{bi // PERW + 1:02d}.dc.html"
        t = f"{title} {bi // PERW + 1}"
        open(os.path.join(PROJ, name), "w").write(HEAD.format(title=t, links="", css=WCSS, body=body, w=bw, h=bh))
        boards[name] = {"x": 0, "y": y, "w": bw, "h": bh, "title": t, "page": page}
        order.append(name)
        y += bh + 120


def sort_web(d):
    return sorted(glob.glob(d + "/*.json"), key=lambda p: int(os.path.basename(p).split("_")[0]))


wfiles = sort_web("/home/claude/webtest/de_dom")
pages.append({"id": "web", "name": "Weboberfläche"})
web_boards(wfiles, "w", "web", "Weboberfläche")
wwhite = sort_web("/home/claude/webtest/de_white_dom")
if wwhite:
    pages.append({"id": "webw", "name": "Weboberfläche: Farbschema Weiß"})
    web_boards(wwhite, "ww", "webw", "Weboberfläche Weiß", "thw")

canvas = {"v": 3, "createdOnFiles": {"v": 1, "at": "2026-09-29T21:40:00Z"}, "title": "Waage UI Design",
          "launch": {"view": "canvas", "page": "wiegen"}, "pages": pages, "boards": boards, "order": order,
          "notes": {}, "designSystems": []}
json.dump(canvas, open(os.path.join(PROJ, "canvas.json"), "w"), ensure_ascii=False)
print(len(order), "Artboards;", sum(len(v) for v in groups.values()), "Display-Seiten;", len(wfiles), "Web-Zustände")
