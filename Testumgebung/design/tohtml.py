#!/usr/bin/env python3
# LVGL-Objektbaum (dump/*.json aus dem Simulator) -> HTML aus gezeichneten Formen
import json, math, html

FONT = {"sg14": 14, "sg18": 18, "sg24": 24, "sg34": 34, "sg80": 80, "sg104": 104}
# LVGL-Symbole (FontAwesome) -> Material-Symbols-Namen
SYM = {"": "wifi", "": "bolt", "": "battery_full", "": "battery_5_bar",
       "": "battery_3_bar", "": "battery_2_bar", "": "battery_0_bar",
       "": "chevron_left", "": "chevron_right", "": "check", "": "close",
       "": "add", "": "remove", "": "volume_up", "": "bluetooth"}


def rgba(c, a=255):
    r, g, b = (c >> 16) & 255, (c >> 8) & 255, c & 255
    return f"#{r:02x}{g:02x}{b:02x}" if a >= 255 else f"rgba({r},{g},{b},{a/255:.3f})"


def esc(t):
    return html.escape(t, quote=False)


def arc_parts(o, ox, oy):
    """Bogen als Ring mit conic-gradient + runde Enden als Kreise"""
    out = []
    pad = o.get("pad", 0)
    x, y = o["x"] - ox + pad, o["y"] - oy + pad
    d = min(o["w"], o["h"]) - 2 * pad
    r = d / 2

    def ring(a0, a1, w, col, opa, rounded):
        if w <= 0 or opa == 0:
            return
        a0 = a0 % 360
        span = (a1 - a0) % 360
        if a1 != a0 and span == 0:
            span = 360
        if span == 0:
            return
        c = rgba(col, opa)
        inner = r - w
        out.append(
            f'<div style="position:absolute;left:{x}px;top:{y}px;width:{d}px;height:{d}px;border-radius:50%;'
            f'background:conic-gradient(from {a0+90}deg,{c} 0deg {span}deg,transparent {span}deg 360deg);'
            f'-webkit-mask:radial-gradient(circle closest-side,transparent {inner-0.5:.1f}px,#000 {inner+0.5:.1f}px);'
            f'mask:radial-gradient(circle closest-side,transparent {inner-0.5:.1f}px,#000 {inner+0.5:.1f}px)"></div>')
        if rounded and span < 360:
            for ang in (a0, a0 + span):
                rad = math.radians(ang)
                cx = x + r + (r - w / 2) * math.cos(rad)
                cy = y + r + (r - w / 2) * math.sin(rad)
                out.append(f'<div style="position:absolute;left:{cx-w/2:.1f}px;top:{cy-w/2:.1f}px;width:{w}px;height:{w}px;border-radius:50%;background:{c}"></div>')

    rot = o["rot"]
    ring(rot + o["bs0"], rot + o["bs1"], o["aw"], o["ac"], o["ao"], o["arr"])
    if o["is0"] != o["is1"]:
        ring(rot + o["is0"], rot + o["is1"], o["iw"], o["ic"], o["io"], o["irr"])
    return "".join(out)


def qr_rects(o):
    w, h, px = o["cw"], o["ch"], o["px"]
    rects = []  # (x, y, w, h)
    open_runs = {}
    for yy in range(h):
        row = px[yy * w:(yy + 1) * w]
        runs = []
        xx = 0
        while xx < w:
            if row[xx] == "1":
                s = xx
                while xx < w and row[xx] == "1":
                    xx += 1
                runs.append((s, xx - s))
            else:
                xx += 1
        new_open = {}
        for run in runs:
            if run in open_runs:
                r = open_runs.pop(run)
                r[3] += 1
                new_open[run] = r
            else:
                new_open[run] = [run[0], yy, run[1], 1]
        rects += open_runs.values()
        open_runs = new_open
    rects += open_runs.values()
    return "".join(f'<div style="position:absolute;left:{a}px;top:{b}px;width:{c}px;height:{d}px;background:#000"></div>'
                   for a, b, c, d in rects)


def node(o, ox, oy):
    if not o:
        return ""
    x, y, w, h = o["x"] - ox, o["y"] - oy, o["w"], o["h"]
    st = [f"position:absolute;left:{x}px;top:{y}px;width:{w}px;height:{h}px;box-sizing:border-box"]
    if o["opa"] < 255:
        st.append(f"opacity:{o['opa']/255:.3f}")
    if o["bgo"] > 0 and o["t"] != "canvas":
        st.append(f"background:{rgba(o['bg'], o['bgo'])}")
    if o["bw"] > 0 and o["bo"] > 0:
        c = rgba(o["bc"], o["bo"])
        bs = o["bs"]
        if bs == 15:
            st.append(f"border:{o['bw']}px solid {c}")
        else:
            for bit, side in ((1, "bottom"), (2, "top"), (4, "left"), (8, "right")):
                if bs & bit:
                    st.append(f"border-{side}:{o['bw']}px solid {c}")
    if o["r"] > 0:
        st.append(f"border-radius:{min(o['r'], min(w, h) / 2):g}px")
    inner = ""
    t = o["t"]
    if t == "label":
        text = o["text"]
        f = o["font"]
        col = rgba(o["tc"], o["to"])
        if f == "icons":
            st.append(f"font-family:'Material Symbols Rounded';font-size:26px;line-height:{o['lh']}px;color:{col};"
                      "display:flex;justify-content:center;font-variation-settings:'FILL' 0,'wght' 400,'opsz' 24")
            inner = esc(text)
        elif f == "mont14":
            name = " ".join(SYM.get(ch, "circle") for ch in text)
            st.append(f"font-family:'Material Symbols Rounded';font-size:16px;line-height:{o['lh']}px;color:{col};display:flex;justify-content:center")
            inner = esc(name)
        else:
            fs = FONT.get(f, 16)
            st.append(f"font-family:'Space Grotesk',sans-serif;font-weight:500;font-size:{fs}px;line-height:{o['lh']}px;color:{col}")
            if o["ls"]:
                st.append(f"letter-spacing:{o['ls']}px")
            st.append(f"padding:{o['pt']}px {o['pr']}px {o['pb']}px {o['pl']}px")
            lines = text.count("\n") + 1
            single = lines == 1 and h - o["pt"] - o["pb"] <= o["lh"] * 1.5
            al = {0: "left", 1: "left", 2: "center", 3: "right"}.get(o["al"], "left")
            if o["wc"] or single:
                st.append("white-space:pre")
                if o["wc"]:
                    # Box passt sich dem Text an: Text mittig halten, darf etwas überstehen
                    st.append("display:flex;flex-direction:column;align-items:center;text-align:" + al)
                else:
                    st.append("text-align:" + al)
                if o["lm"] in (1, 4):
                    st.append("overflow:hidden;text-overflow:ellipsis")
            else:
                st.append("white-space:pre-wrap;overflow-wrap:break-word;text-align:" + al)
            inner = esc(text)
    elif t == "arc":
        inner = arc_parts(o, o["x"], o["y"])
    elif t == "line":
        pts = " ".join(f"{a},{b}" for a, b in o["pts"])
        inner = (f'<svg width="{w}" height="{h}" viewBox="0 0 {w} {h}" style="position:absolute;left:0;top:0;overflow:visible">'
                 f'<polyline points="{pts}" style="fill:none;stroke:{rgba(o["lc"])};stroke-width:{o["lw"]};stroke-linecap:round;stroke-linejoin:round"></polyline></svg>')
    elif t == "canvas":
        st.append("background:#fff")
        inner = qr_rects(o)
    if not o["ov"] and t != "label":
        st.append("overflow:hidden")
    kids = "".join(node(c, o["x"], o["y"]) for c in o["ch_"])
    return f'<div style="{";".join(st)}">{inner}{kids}</div>'


def screen(path):
    d = json.load(open(path))
    scr = d["scr"]
    bg = rgba(scr["bg"], scr["bgo"])
    body = "".join(node(c, 0, 0) for c in scr["ch_"])
    top = "".join(node(c, 0, 0) for c in (d["top"] or {"ch_": []})["ch_"])
    return (f'<div style="position:relative;width:412px;height:412px;border-radius:50%;overflow:hidden;background:{bg};'
            f'flex:none;box-shadow:0 0 0 9px #111210,0 0 0 11px #2e2f2a">{body}{top}</div>')


if __name__ == "__main__":
    import sys
    names = sys.argv[1:]
    parts = "".join(f'<div style="display:flex;flex-direction:column;align-items:center;gap:14px">{screen(n)}'
                    f'<div style="font:14px sans-serif;color:#aaa">{n}</div></div>' for n in names)
    print('<!doctype html><html><head><meta charset="utf-8">'
          '<link href="https://fonts.googleapis.com/css2?family=Space+Grotesk:wght@400;500;600&display=swap" rel="stylesheet">'
          '<link href="https://fonts.googleapis.com/css2?family=Material+Symbols+Rounded:opsz,wght,FILL,GRAD@20..48,100..700,0..1,-50..200" rel="stylesheet">'
          f'</head><body style="margin:0;background:#191a17;display:flex;flex-wrap:wrap;gap:40px;padding:30px">{parts}</body></html>')
