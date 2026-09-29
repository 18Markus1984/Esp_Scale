# Testumgebung (nur für die Entwicklung, nicht für die Waage)

Diese Dateien laufen auf dem Rechner, nicht auf dem ESP32. Damit lässt sich die
Oberfläche ohne Hardware prüfen.

- `main2.cpp`, `stubs_host.cpp`: Gerüst, um die Geräteoberfläche (LVGL) auf dem PC
  zu bauen. Gewicht, SD-Karte und Ton werden simuliert (`-DSIM_WAAGE=1`).
  Screenshots landen als .ppm-Dateien.
- `mock.py`: kleiner Testserver, der die Endpunkte der Waage nachbildet und
  `page.html` ausliefert. **Wichtig:** Er wird zwischen Aufrufen beendet, also immer
  im selben Befehl starten und testen.
- `shot.py`: rendert Seiten der Weboberfläche über WebKit zu PNG.
- `jstest.py`: führt JavaScript in der geladenen Seite aus und liefert das Ergebnis
  über `document.title` zurück. Damit wurden Abläufe wie die Messmittelprüfung und
  das Auto-Weiter geprüft.

Typische Prüfungen, die sich bewährt haben:
1. Alle `href="#/..."` gegen die vorhandenen Seiten in `P` abgleichen.
2. Alle Hilfsfunktionen gegen ihre Aufrufe abgleichen.
3. Rundgang über alle Seiten mit `window.onerror`, Ergebnis muss „OK ohne Fehler“ sein.

## Neu: Host-Build und Sprach-Prüfung (Stand 26.09.2026)

- `Makefile`, `hal_host.cpp`, `lv_conf.h`, `main_tour.cpp`: Geräteoberfläche auf dem PC bauen.
  Einmalig LVGL 8.3.10 hierher holen:
  `git clone --depth 1 -b v8.3.10 https://github.com/lvgl/lvgl.git lvgl`, dann `make`.
- `./sim de` bzw. `./sim en`: Rundgang über alle Seiten (Screenshots in `shots2/`).
  Am Ende stehen unter `MISS:` Texte, die auf Englisch keinen Eintrag in
  `i18n_en.cpp` gefunden haben (Zahlen und Namen darunter sind normal).
  `shown_en.txt` enthält alle angezeigten Texte des Rundgangs.
- `webtour.py de|en`: Rundgang über die Weboberfläche mit Playwright (statt WebKit),
  sammelt alle Texte und JavaScript-Fehler in `webtour_<lang>/result.json`.
  Vorher `WAAGE_LANG=1 python3 mock.py &` (1 = English) starten.
- Bewährt: deutschen Rundgang vorher/nachher per Pixelvergleich prüfen – so fällt auf,
  wenn eine Änderung die deutsche Oberfläche unbeabsichtigt verändert.
