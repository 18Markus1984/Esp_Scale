#!/usr/bin/env python3
# Weboberfläche einbetten: web/page.html -> web_page.h
# Die Seite wird gzip-komprimiert abgelegt und so auch ausgeliefert
# (Content-Encoding: gzip). Das spart rund drei Viertel der Übertragung.
# Nach Änderungen an page.html einmal ausführen:  python3 web/build.py
import gzip, os
here = os.path.dirname(os.path.abspath(__file__))
src = open(os.path.join(here, "page.html"), encoding="utf-8").read()
raw = src.encode("utf-8")
gz = gzip.compress(raw, 9)
lines = []
for i in range(0, len(gz), 16):
    lines.append("  " + ", ".join("0x%02X" % b for b in gz[i:i + 16]) + ",")
with open(os.path.join(here, "..", "web_page.h"), "w", encoding="utf-8") as f:
    f.write("#pragma once\n"
            "// Automatisch erzeugt aus web/page.html (Weboberfläche der Waage)\n"
            "// gzip-komprimiert; der Server schickt sie mit Content-Encoding: gzip.\n"
            "#include <Arduino.h>\n\n"
            "#define WEB_PAGE_LEN %d\n\n"
            "static const uint8_t WEB_PAGE_GZ[] PROGMEM = {\n%s\n};\n" % (len(gz), "\n".join(lines)))
print("%d Zeichen -> %d Bytes gzip (%d %%)" % (len(raw), len(gz), 100 * len(gz) // len(raw)))
