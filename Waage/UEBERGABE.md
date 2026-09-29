# Übergabe: ESP32-S3 Küchenwaage

Dieses Dokument ist als **erste Nachricht in einem neuen Chat** gedacht, zusammen mit
dem ZIP. Es beschreibt Hardware, Entwicklungsumgebung, Aufbau des Codes, den
aktuellen Stand und die offenen Punkte.

---

## 1. Hardware

| Teil | Angabe |
|---|---|
| Board | Waveshare **ESP32-S3-Touch-LCD-1.46** (rundes Display 412 × 412, SPD2010, QSPI, kapazitiver Touch) |
| Wägezelle | 3 kg mit **HX711**, DOUT = GPIO 44, SCK = GPIO 43, Kalibrierfaktor ca. 631 |
| Lautsprecher | PCM5101 über **I2S0**: BCLK 48, LRC 38, DOUT 47 |
| Mikrofon | MSM261 über **I2S1**: BCK 15, WS 2, DIN 39 (beide Kanäle, Gleichanteil wird abgezogen) |
| SD-Karte | 1-Bit-Modus, Takt auf **10 MHz** gedrosselt (40 MHz stört mit WLAN und Ton) |
| Akku | 1500 mAh LiPo, gemessene Laufzeit **10,1 h** |
| Ein/Aus-Taster | externer Mikrotaster **6 × 6 × 10 mm**, parallel zur PWR-Taste der Platine gelötet (Gehäuse starr verbaut); Firmware unverändert |
| Sonstiges | RTC PCF85063, Lage-Sensor QMI8658 (Libelle), IO-Expander TCA9554 |

## 2. Entwicklungsumgebung

- **Arduino IDE 2.2.1**, Boardpaket **esp32 3.1.1**, Board „Waveshare ESP32-S3-Touch-LCD-1.46“
- **LVGL 8.3.10** (nicht 9.x, die API unterscheidet sich)
- PSRAM aktiv; Partitionsschema normalerweise Standard, für Spracherkennung „ESP SR 16M“
- In `lv_conf.h` nötig: LVGL-Puffer `/20`, für ESP-SR zusätzlich `LV_MEM_CUSTOM_ALLOC = ps_malloc`
- Flashen über USB oder **OTA**: Browser `http://<ip>/update`, oder Arduino-IDE-Netzwerkport „waage“
  (Passwortfeld der IDE beliebig füllen, die Waage verlangt keins)

### Dateien, die NICHT im ZIP sind
Die **Treiber von Waveshare** aus dem Demo-Projekt liegen unverändert im selben
Sketch-Ordner und werden vorausgesetzt:
`Display_SPD2010.*`, `Touch_SPD2010.*`, `LVGL_Driver.*`, `SD_Card.*`, `PWR_Key.*`,
`BAT_Driver.*`, `RTC_PCF85063.*`, `Gyro_QMI8658.*`, `TCA9554PWR.*`, `I2C_Driver.*`.
Angefasst wurde davon nichts; alles Eigene liegt in den unten genannten Dateien.

## 3. Aufbau des Codes

| Datei | Inhalt |
|---|---|
| `Waage.ino` | Setup und Hauptschleife |
| `config.h` | Schalter und Schwellen: `SIM_WAAGE`, `WAAGE_MAX_G`, `SD_FREQ_KHZ 10000`, `USE_VOICE`, `BAT_WARN_V 3.45`, `BAT_CUTOFF_V 3.30` |
| `hal.h` / `hal_board.cpp` | Hardware-Abstraktion, Akkukurve, `BAT_RUNTIME_H 10.1` |
| `scale.*` | HX711, Glättung, Mehrpunkt-Kalibrierung (bis 3 Stützstellen) |
| `storage.*` | SD-Karte mit Sperre (`storage_lock/unlock`), Ausfallerkennung, Selbstheilung |
| `data.*` | Rezepte, Cocktails, Dichten, Töpfe, Spulen, Spieler, **Bestenliste** |
| `settings.*` | Einstellungen im Flash |
| `net.*`, `web.*`, `web_page.h`, `web/page.html`, `web/build.py` | WLAN und Weboberfläche |
| `sound.*`, `voice.*`, `mic.*` | Töne, Sprachausgabe, Mikrofon |
| `batt.*` | Ladezustand, Restzeit, Entladetest |
| `ui*.cpp/h` | Rahmen: Theme, Widgets, Seitenwechsel, Textring, Auto-Aus |
| `i18n.*`, `i18n_en.cpp` | Sprache: `T()` und Übersetzungstabelle Deutsch → English |
| `page_*.cpp` | je eine Datei pro Seite (ziel, rezept, cocktail, spule, zaehlen, porto, spiel, trink, protokoll, toepfe, kalib, msa, akku, mic, setup) |
| `font_*.c` | Space-Grotesk-Schnitte und Symbolfont |

**Weboberfläche:** `web/page.html` ist die Quelle. Nach jeder Änderung
`python3 web/build.py` ausführen, das erzeugt `web_page.h` **gzip-komprimiert**
(aktuell rund 87 KB → 25 KB) und der Server liefert sie mit `Content-Encoding: gzip`.

## 4. Aufbau der SD-Karte

```
/Waage/Rezepte/*.txt      /Waage/Cocktails/*.txt
/Waage/Protokoll/JJJJ-MM-TT.txt
/Waage/Toepfe/toepfe.txt  /Waage/Toepfe/spulen.txt
/Waage/Porto/porto.txt
/Waage/Spiel/spieler.txt  /Waage/Spiel/bestenliste.txt  /Waage/Spiel/spiele.txt
/Waage/Stimme/<Paket>/*.wav   (16 kHz, Mono, 16 Bit; englische Pakete mit englischen Dateinamen)
/Waage/Akku/test_<Datum>.csv
/Waage/Pruefung/<Datum>.txt
```

## 5. Was fertig ist

**Wiegen und Modi:** Ziel mit Parkpiepser, Rezept Schritt für Schritt, Cocktail in
ml über Dichtetabelle, Spule mit Restmeter-Berechnung, Zählen mit Referenz und
Bluetooth-Tastatur, Porto mit Klassen, Schätzspiel, Trinkspiel.

**Verwalten:** Rezept- und Cocktail-Editor, Töpfe und Spulen mit Farben, Protokoll
mit Tagesansicht und Export, Akku mit Verlauf und Entladetest, Dateiverwaltung der
SD-Karte im Browser (herunterladen, hochladen, löschen, Ordner anlegen).

**Technik:** Auto-Aus mit Countdown, Topferkennung, Touch-Filter gegen Doppelklicks,
SD-Selbstheilung, Uhrzeit-Backup im Flash, Mehrpunkt-Kalibrierung,
Messmittelprüfung nach Verfahren 1 (Cg/Cgk) an Gerät und Browser, OTA,
Tiefentladeschutz (Warnung 3,45 V, Abschaltung 3,30 V), Spracherkennung ESP-SR
(optional, rechenintensiv, aktuell nicht priorisiert).

**Akku:** Ladeerkennung nach Messung (über 4,19 V lädt, 4,14–4,19 V mit Kabel =
voll, nach dem Abziehen 4,13 V = Akkubetrieb). Prozentkurve aus dem echten
Entladetest, die Prozentzahl entspricht der **verbleibenden Laufzeit**
(3,95 V = 89 %, 3,80 V = 69 %, 3,70 V = 50 %, 3,60 V = 34 %, 3,50 V = 19 %).

**Spieler und Bestenliste:** Namen werden einmal gepflegt (Gerät: Spiel → Auswählen,
antippen = dabei, halten = umbenennen, „+ Spieler“; Browser: Einstellungen →
Spielernamen). In den Spielen wird nur noch ausgewählt. Ergebnisse beider Spiele
landen in **einer** Datei `/Waage/Spiel/bestenliste.txt`
(`Datum;Spiel;Name;Wert;Einheit`, kleiner ist besser); die Weboberfläche zeigt sie
unter jedem Spiel.

**Weboberfläche:** komplett im dunklen Design des Displays, Farben aus `ui_theme.h`
übernommen. Ringanzeigen, runde Tara-/Speichern-Knöpfe, Strichsymbole, Kopfzeile mit
rundem Zurück-Knopf. 20 Seiten inklusive eigener Unterseiten für Portoklassen,
Spielernamen und Firmware-Update.

**Geschwindigkeit (wichtig, war ein echter Fehler):** Der `WebServer` bedient nur
eine Verbindung gleichzeitig und wartet auf stumme Verbindungen bis zu 5 s
(`HTTP_MAX_DATA_WAIT`). Browser öffnen Verbindungen auf Vorrat → daher früher 10–30 s
Ladezeit. Gegenmaßnahmen: `drop_idle_client()` trennt stumme Verbindungen nach
350 ms, Seite gzip, Server mehrfach je Durchlauf und alle 5 ms bedient, und die
Oberfläche schickt ihre Anfragen nacheinander statt parallel.

**Sprache Deutsch/English (neu, 26.09.2026):** Einstellung `g_set.lang` (Flash-Schlüssel
`lang`), umschaltbar am Gerät unter Setup → Ton → Sprache und im Browser unter
Einstellungen → Sprache (`/api/lang?l=0|1`). Gilt für Display, Weboberfläche und Ansage.
- Display: Der Code bleibt deutsch. `i18n.h` (über `ui_theme.h` eingebunden) ersetzt
  `lv_label_set_text()` per Makro durch eine Version, die den Text vorher mit `T()` in
  `i18n_en.cpp` nachschlägt (binäre Suche). Feste Texte brauchen deshalb nichts;
  zusammengesetzte Texte haben `T()` am Format: `snprintf(b, n, T("noch %d g"), rest)`.
  Neuer Text = neue Zeile `{ "Deutsch", "English" }` in `i18n_en.cpp` (gleiche
  Platzhalter, nur Zeichen aus der Schrift, also `"` statt „“). Kein Variablenname `T`!
- Beim Umschalten baut `ui_lang_changed()` die Startseite neu auf, setzt die Texte der
  Standby-/Aus-Überlagerungen neu und wählt ein passendes Stimmpaket.
- Web: `page.html` bleibt deutsch; Tabelle `EN` (ganzer Text) und Muster `ENP`
  (Texte mit Zahlen/Namen, `T$1` = übersetzte Gruppe). Ein MutationObserver übersetzt
  alles, was ins Dokument kommt; `translate="no"` schützt z. B. Datei- und Ordnernamen.
  Seite gzip jetzt rund 35 KB.
- Ansage: englische Zahlen („one thousand two hundred thirty four point five grams“).
  Englische Pakete haben englische Dateinamen (`zero.wav … grams.wav`, `tare`, `saved`,
  `goal_reached`, `overload`, `pot_detected`, `done`, `scale_empty`), daran (`grams.wav`)
  erkennt die Waage die Sprache; in der Auswahl stehen nur Pakete der eingestellten
  Sprache. Dateinamen höchstens 13 Zeichen. Vorlage und Wortliste:
  `stimme_erzeugen.py <ordner> --lang en`, `stimme_schneiden.py … --lang en`
  (`--liste` gibt die Vorleseliste aus). Testpaket `Roboter-EN` (espeak-ng) liegt bei.
- Zahlen bleiben auch auf Englisch mit Komma (dieselben Formatierer schreiben auch die Dateien).
- `stimme_erzeugen.py` rechnet jetzt auf 16 kHz um (espeak-ng liefert 22,05 kHz,
  vorher wurden die Dateien nur falsch beschriftet und liefen zu langsam).

**Wiegeseite (Entwurf A, 26.09.2026):** Gewicht in 104 px (`font_sg_104.c`, gleiche Zeichen wie
`font_sg_80`), Einheit in 34 px rechts daneben auf derselben Grundlinie (Flex-Zeile unten bündig,
Einheit per `translate_y` um den Unterschied der `base_line` angehoben). Passt die Zeile nicht
in 372 px (z. B. negative Werte), schaltet `weight_show()` auf 80 px zurück. Zustandschip darunter.

**Einheit ml (27.09.2026):** `UNIT_ML` (Index 4) in `data.*`: Gramm / Dichte der gewählten
Flüssigkeit (`g_set.liquid`, 12 Flüssigkeiten von Wasser bis Brühe). Wählen: Setup → Wiegen →
Einheit (g, kg, oz, lb, ml), Flüssigkeit durch Antippen der Einheit auf der Wiegeseite oder im
Browser. Oben auf der Wiegeseite steht z. B. „Milch · 1,03 g/ml“. Ansage-Wort `milliliter.wav`
(EN `milliliters.wav`) fehlt noch in den vorhandenen Stimmpaketen außer Roboter-EN.

**Spiel „Halbe-Halbe“** (`page_halb.cpp`, Spiele): Lebensmittel verdeckt wiegen, abnehmen,
teilen, eine Hälfte auflegen; Auflösung in % mit Trommelwirbel, Endstand nach Abstand zu 50 %,
Bestenliste „Halbieren“ (Einheit %). Symbol `ICON_HALB` (content_cut).

**Neue Modi (27.09.2026):** Küche: Timer, Tassen & Löffel · Werkstatt: 2K mischen, Langzeit,
Münzen · Spiele: Blindgießen. Neue Symbole in `font_icons_26.c` (timer, science, monitoring,
coffee, savings, water_drop).
- `tools.*`: Hintergrunddienste im UI-Takt (`tools_tick()` aus `home_update_cb`).
  **Küchentimer** (3 Stück, `timer_start/stop/left`), klingeln mit Überlagerung auf jeder Seite
  (auch aus dem Standby, `ui_power_wake()`), max. 2 min, Eintrag im Protokoll. Laufender Timer
  steht in der Statuszeile der Wiegeseite. **Langzeitmessung** schreibt `/Waage/Messung/
  JJJJ-MM-TT_HHMM.csv` (Uhrzeit;Minuten;Gewicht_g), Intervall 10 s–15 min, Trend = Regression
  der letzten 30 Punkte. Beides pausiert Auto-Aus (`tools_keep_awake()`).
- `page_timer.cpp`, `page_mix.cpp` (A:B als Teile B je 100 A, Toleranz B 1 %/mind. 0,1 g, geht
  ruhig im Ziel nach 1,5 s von selbst weiter, Topfzeit startet einen Timer „Topfzeit“),
  `page_langzeit.cpp` (Kurve mit `lv_line`, kein lv_chart nötig), `page_tassen.cpp` (US-Tasse,
  EL = 1/16, TL = 1/48 Tasse, Ziel in ¼-Tassen), `page_blind.cpp` (gemeinsame Spielerliste,
  Bestenliste „Blindgiessen“), `page_muenzen.cpp` (EZB-Münzgewichte, eine Sorte je Durchgang,
  „+ Summe“ für den Kassensturz, Warnung wenn das Gewicht nicht zur Sorte passt).
- Weboberfläche: neue Seite **Messungen** (Übersicht → Messungen): Kurve, Dauer, Differenz,
  Trend, CSV-Download.

**Auto-Null (Nullpunkt-Nachführung):** `scale.cpp`, Einstellung `g_set.azt` (Standard an),
Setup → Wiegen → Auto-Null und im Browser. Führt nur nach, wenn die Waage ruhig und fast leer
ist (< 0,5 g), nach echtem Nullstand (< 0,05 g) „scharf“ geschaltet wurde und sich das Brutto
in 1 s um < 0,08 g ändert. Ein aufgelegtes Teil (Sprung) sperrt die Nachführung, bis die Waage
wieder leer ist. Test im Simulator: 0,30 g Drift -> 0,06 g, danach 0,35 g aufgelegt bleibt erhalten.

**Portionieren (Küche, 27.09.2026):** `page_portion.cpp`. Ganzes wiegen, Stückzahl wählen,
Waage leeren (Tara automatisch, sobald brutto < 5 g und ruhig, sonst Tara-Knopf), dann Stück
für Stück: Ring, Abweichung („+4 g“/„passt“, Toleranz 2 %, mind. 1 g) und Parkpiepser.
Übernommen wird beim Abnehmen der letzte ruhige Wert; das Soll jedes Stücks = Rest / restliche
Stücke. Ergebnis mit Ø, kleinstem/größtem Stück, Streuung s; Protokoll „Portionen: 12 × 102,0 g“.
Neues Symbol `ICON_PORTION` (bakery_dining) in `font_icons_26.c`.

**Präzisionsmodus:** `g_set.precise` (Flash `precise`), Setup → Wiegen → Präzision, im Browser
unter Einstellungen, am Gerät Gewicht lange drücken. Nur in Gramm: solange ruhig, mittelt
`scale_precise()` bis 5 s (50 Werte) und zeigt zwei Nachkommastellen, Chip „präzise ±0,05 g“
(= 2·s/√n, ohne Kalibrierfehler). Speichern nimmt den Mittelwert. `scale_tare()` nutzt jetzt
immer den Mittelwert, wenn das Gewicht ≥ 1 s ruhig lag.

**QR-Code:** Setup → Zeit & Funk → Weboberfläche → „QR-Code ›“ (nur wenn sie läuft).
Im eigenen WLAN: `WIFI:T:nopass;S:Waage-Setup;;`, Antippen wechselt zur Adresse; im Heimnetz
nur die Adresse. **Braucht `#define LV_USE_QRCODE 1` in `lv_conf.h`**, sonst steht dort ein Hinweis.

**Speicheranzeige:** Dateiseite im Browser zeigt die SD-Karte in GB (dezimal wie auf dem
Etikett, 1 GB = 10⁹ Byte), unter 1 GB in MB: „5,7 MB von 8,03 GB belegt · 8,02 GB frei“.
Protokoll-Tabelle: Uhrzeit und Gewichte brechen nicht mehr um, nur die Notiz.

**Mehrere WLAN-Netze (27.09.2026):** `net.cpp` speichert bis zu `NET_MAX` = 5 Netze im
Flash-Namespace `wlan` (Schlüssel `n`, `s0..s4`, `p0..p4`). Beim ersten Start wird das alte
Einzelnetz aus `g_set.ssid/pass` übernommen; `g_set.ssid` ist jetzt nur noch „zuletzt verbunden“.
Verbinden (`net_connect_start`): asynchroner Scan, gespeicherte Netze in Reichweite nach
Signalstärke, danach die übrigen (versteckte), je 10 s Versuch. Genutzt von Uhrzeit-Abgleich und
Weboberfläche. Gerät: Setup → Zeit & Funk → WLAN → „Netzwerke (n) ›“ → Liste mit „+ Neues
Netzwerk“ (Suche + Passwort wie bisher, neues Netz vorne) und gespeicherten Netzen (antippen →
Entfernen). Ist die Liste voll, fällt das älteste heraus. Browser: Einstellungen → „Gespeicherte
WLAN-Netze“ (`GET /api/wlan` ohne Passwörter, `POST /api/wlan` {ssid,pass} speichert nur, die
laufende Verbindung bleibt; `/api/wlan/del?i=`).

**Weboberfläche für die neuen Modi (27.09.2026):** Startseite im Browser jetzt in Gruppen wie am
Gerät (Küche, Werkstatt, Spiele) mit den neuen Seiten `#/portion`, `#/timer`, `#/tassen`, `#/mix`,
`#/muenzen`, `#/blind`, `#/halb`; „Langzeit“ führt zu `#/messung`, dort Langzeitmessung starten/beenden
(Intervall 10 s … 15 min). Abläufe laufen im Browser (Gewicht aus `/api/state`), Parkpiepser über
`/api/park`, Spiele verstecken das Display über `/api/hide` und schreiben in die Bestenliste
(`Blindgiessen`, `Halbieren`). Neue Endpunkte: `GET /api/tools` (Timer + Langzeitmessung),
`/api/timer?i=&s=&name=` | `&stop=1` | `&add=±s` | `?ringoff=1`, `/api/lt?on=1&iv=` | `?on=0`.
`/api/state` hat zusätzlich `tm` (Restzeit nächster Timer, -1), `ring`, `lt`: die Startseite zeigt
„Timer 4:12“ / „Messung läuft“ und bei Alarm „Timer abgelaufen · Alarm aus“. Neu in `tools.h`:
`timer_ringing()`. Web-Test: `mock.py` kennt `/mock/w?g=` (Gewicht festsetzen) für die Abläufe in `webtour.py`.

**Online-Update von GitHub (27.09.2026):** `update_online.*`. Version kommt aus `version.h`
(schreibt die GitHub Action aus dem Tag, `v1.2.3` -> `"1.2.3"`), sonst `FW_VERSION "0.0.0"` +
`FW_LOCAL` („lokal gebaut“). `OTA_REPO` in `config.h` (Platzhalter `18Markus1984/Esp_Scale`).
Ablauf in eigener Task (Kern 0, 12 KB Stack): ggf. WLAN über `net_connect_start()` (bis 45 s),
`GET https://api.github.com/repos/<repo>/releases/latest` (tag_name, browser_download_url von
`Waage.ino.bin`), Versionsvergleich, auf Wunsch Download mit Redirects -> `Update` -> Neustart
3 s nach „fertig“ (`upd_loop()` aus dem UI-Takt). TLS ohne Zertifikatsprüfung (`setInsecure`).
Während `upd_busy()`: kein Auto-Aus, `wifi_off()`/`net_sync_now()`/`web_stop()` lassen das WLAN an.
Gerät: Setup -> Waage -> Firmware (Zeile 36). Browser: Firmware-Update -> Karte „Online-Update“,
`/api/ota` (+ `?check=1`, `?install=1`). GitHub-Vorlage im ZIP-Ordner `GitHub/`
(`.github/workflows/firmware.yml`, `.gitignore`, `GITHUB.md` mit Schritt-für-Schritt-Anleitung).
FQBN: `esp32:esp32:waveshare_esp32_s3_touch_lcd_146:PSRAM=enabled,PartitionScheme=app3M_fat9M_16MB`.
Nicht auf echter Hardware getestet (kein Compiler hier), nur Syntaxprüfung mit Ersatz-Headern.

**GitHub (29.09.2026):** Repository `18Markus1984/Esp_Scale`, Online-Update läuft. Für den Build
mussten in den Waveshare-Treiber-Headern die Includes der LVGL-Demoprojekte auskommentiert werden.
Englische README mit Bildern (`README.md`, `docs/images/`: hero.png, screens.png, web-ui.png aus
Simulator/Web-Tour, wiring.svg, architecture.svg), Skripte zum Neuerzeugen in `docs/tools/`.

**Stimmenauswahl (29.09.2026):** bis zu `VOICE_PACKS_MAX` = 32 Pakete (vorher nur die ersten 8
Unterordner, beide Sprachen zusammen gezählt), alphabetisch sortiert, nur Pakete der eingestellten
Sprache. Gerät: Setup -> Ton -> Stimme öffnet eine Auswahlliste (`page_voice_pick_create`),
`sound_voice_set()` setzt und prüft das Paket; Browser: alle Pakete als Chips.

## 6. Offene Punkte

1. **Miau-Modus.** Markus erzeugt die Sounds selbst. Geplant: Ordner
   `/Waage/Sounds/Miau/*.wav` mit mehreren Miaus, bei jedem Ton wird zufällig eins
   gewählt; die Sprachausgabe läuft über ein Stimmpaket, das nur aus Miaus besteht.
   Noch nicht implementiert.
2. **Katzen-Video.** Markus legt einen eigenen Ordner an.
   - Im Browser direkt als MP4 abspielbar (`/Waage/Video/web/`).
   - Auf dem Display als **MJPEG mit paralleler WAV-Tonspur**: machbar mit JPEGDEC,
     240 × 240 zentriert, realistisch **12–15 fps mit Ton**. Vorgeschlagenes
     gepacktes Format `/Waage/Video/katze.mjv` (Kopf, Bildtabelle, JPEGs, PCM), dazu
     ein Python-Packer und zwei ffmpeg-Aufrufe als Vorarbeit.
   - **Einbauorte noch zu bestätigen.** Vorschläge: Siegerehrung Schätz- und
     Trinkspiel, „Fertig“ bei Rezept und Cocktail, Easter Egg bei genau 42,0 g;
     weitere Ideen: Überlast, Aktivierung des Miau-Modus, Bildschirmschoner im
     Browser, Trostvideo für den Letzten.
   - Ein YouTube-Song soll nicht mitgeliefert oder eingebettet werden, nur ein Link
     in der Weboberfläche.
3. **Englisches Stimmpaket** erstellt Markus selbst (Wortliste siehe oben).
4. **Porto-Tabelle ab 01.01.2027** aktualisieren, sobald die neuen Preise feststehen.

## 7. Hinweise zur Zusammenarbeit

- Kommentare und Oberfläche sind auf Deutsch, Variablennamen englisch.
- Vor dem Ausliefern: `python3 web/build.py`, und die Weboberfläche gegen alle
  Verlinkungen und Hilfsfunktionen prüfen (ein Rundgang über alle Seiten mit
  `window.onerror` hat in der Vergangenheit mehrere eigene Fehler gefunden).
- Beim Ersetzen großer Codeblöcke aufpassen: Es sind schon zweimal ganze Seiten
  versehentlich mitgelöscht worden (Dateiseite, Cocktailkarte).
