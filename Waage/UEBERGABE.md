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
Angefasst wurde davon nur **`Touch_SPD2010.cpp`** (seit 14.10.2026, liegt im ZIP unter `Waage/` und
ersetzt die Waveshare-Fassung; Längenprüfungen, siehe Abschnitt 5 „Absturz 14.10.2026“). Die
Testumgebung lässt alle Waveshare-Dateien beim Bauen weg (`WAVESHARE` im Makefile).

## 3. Aufbau des Codes

| Datei | Inhalt |
|---|---|
| `Waage.ino` | Setup und Hauptschleife |
| `config.h` | Schalter und Schwellen: `SIM_WAAGE`, `WAAGE_MAX_G`, `SD_FREQ_KHZ 10000`, `USE_VOICE`, `BAT_WARN_V 3.45`, `BAT_CUTOFF_V 3.30` |
| `hal.h` / `hal_board.cpp` | Hardware-Abstraktion, Akkukurve, `BAT_RUNTIME_H 10.1` |
| `scale.*` | HX711, Glättung, Mehrpunkt-Kalibrierung (bis 3 Stützstellen) |
| `storage.*` | SD-Karte mit Sperre (`storage_lock/unlock`), Ausfallerkennung, Selbstheilung |
| `data.*` | Rezepte, Cocktails, Dichten, Töpfe, Spulen, Spieler, **Bestenliste** |
| `settings.*`, `settings_profile.cpp` | Einstellungen im Flash, Profil-Voreinstellungen |
| `page_wizard.cpp` | Einrichtungs-Assistent (erster Start, Setup -> Waage -> Einrichtung) |
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
/Waage/Pruefung/<Datum>_<HHMM>.csv   (Messmittelprüfungen, msa_store.cpp; ältere <Datum>.txt bleiben liegen)
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

**Ansagen der Stimmpakete (29.09.2026, Fehler behoben):** Die Ereignis-Wörter (tara, gespeichert,
ziel_erreicht, ueberlast, topf_erkannt, fertig, waage_leer) wurden vorher nie abgespielt, nur Zahlen.
Jetzt: `sound_play()` sagt bei eingeschalteter Ansage Tara/Speichern/Topf/Ziel/Fertig an (dasselbe Wort
frühestens nach 3 s wieder); `sound_play_tone()` nur Ton (Timer-Wecker, Halbe-Halbe-Auflösung).
Speichern auf der Wiegeseite: „<Gewicht> … gespeichert“ (`sound_speak_weight_word`), Überlast: ui.cpp,
„Waage leer“: einmal nach dem Abnehmen eines angesagten Gewichts (`speak_check`).

**Prüfgewicht (29.09.2026):** `refcheck.*`, `page_refcheck.cpp`. Setup -> Waage -> Prüfgewicht
(Gewicht 50…2000 g/aus, Erinnern alle 7…180 Tage/nie, Toleranz ±0,1…5 g; Browser: Einstellungen ->
Waage, beliebiger Wert). Prüfung: leeren (Tara automatisch), auflegen, 2 s ruhig mitteln, Ergebnis mit
Abweichung; gespeichert in Flash (`ref_*`), `/Waage/Pruefung/pruefgewicht.csv` und Protokoll. Fällig
(auch nach einer Prüfung außer Toleranz) -> nach dem Start Seite „Prüfung fällig“ (Jetzt prüfen/Später).
Ohne gestellte Uhr keine Erinnerung.

**Ansagen ohne gegenseitiges Unterbrechen (29.09.2026):** sound.cpp hat zwei Warteschlangen:
Töne (Länge 1, nur der neueste wartet) und Ansagen (FIFO, 4 Plätze). Eine Ansage wird immer ganz
gesprochen; Töne warten bis danach, Klicks werden während einer Ansage verworfen. Dieselbe Ansage
innerhalb von 2 s nur einmal. Speichern nach einer schon angesagten Auflage sagt nur noch
„gespeichert“ (`speak_saved` in ui.cpp). „Waage leer“ nicht mehr, wenn nur tariert wurde (Tara > 20 g).
Behobene Blockaden: Speichern brach die Gewichtsansage ab; ein Klick während einer wartenden Ansage
warf sie weg; der Speicherton wurde von der Ansage verschluckt; Tara/Topf brachen Ansagen ab.
**Ansage-Modus:** `g_set.speak` (Zahlen) + `g_set.speak_ev` (Sprüche, Flash-Schlüssel `speak_ev`),
`sound_speak_mode()`/`_set()`: Zahlen + Sprüche / nur Zahlen / nur Sprüche / aus. Setup -> Ton -> Ansage,
Browser: Auswahl. Wiegeseite: Lautsprecher-Knopf unter Tara/Speichern (nur mit Stimmpaket):
spricht gerade -> `sound_speak_stop()`, sonst Modus weiterschalten (Symbole volume_up, numbers,
record_voice_over, volume_off; Schrift font_icons_26 neu erzeugt).
**QR-Code:** Adresse immer `http://waage.local`, darunter die IP als Ersatz.
**Menü-Umbau (29.09.2026, mit Markus abgestimmt):** Mikrofon-Pegeltest aus System entfernt, jetzt
Setup -> Ton -> Mikrofon (Zeile 26, direkt unter Sprachbefehle, mit Zurück-Knopf). Portoklassen aus
Setup -> Waage entfernt; im Porto-Modus Knopf „Klassen“ (Fertig führt zurück in den Porto-Modus), im Web
oben rechts „Klassen“ -> #/portoklassen. Setup-Zeilen haben feste Kennungen (`IDS[4][8]` in
page_setup.cpp), 34 ist frei. Web-Einstellungen: Sprache und Sprachbefehle (voice_on, nur wenn
`voice_ok`) im Abschnitt Ton wie am Gerät. Bewusst nicht umgesetzt: Töpfe bei Auto-Tara,
Libelle/Wasserwaage zusammenlegen, Auto-Aus zum Akku, Kategorie Waage aufteilen.
**Farbschemata (04.10.2026):** Setup -> Waage -> Farbe: **Grün** (Standard, das ursprüngliche
Design) oder **Weiß** (alles, was sonst grün ist, in #F4F4F1: Ring, „stabil“, Hauptknöpfe, Auswahl,
Toasts, Schalter, Balken; Warnknöpfe nur gelb umrandet). Gespeichert als `theme` (0/1).
Umsetzung: `g_scheme` in ui_theme.cpp, `C_ACCENT`/`C_PRIMARY`/`C_ICON`/`C_VALUE`/`C_SUB_SEL` sind
Laufzeitfarben; `ui_theme_changed()` baut die Startseite neu auf (wie beim Sprachwechsel).
Hintergrund #090909 und neutrale Grautöne gelten für beide. Die Weboberfläche übernimmt das Schema
über `th` in /api/state (`html[data-theme=w]`, zusätzlich im Browser gemerkt gegen Aufblitzen).
**Display drehen (04.10.2026):** `disp_rot.*`. Setup -> Waage -> Display: Bild in 0,1°-Schritten um
bis zu ±5° drehen (Fadenkreuz und Kreis als Hilfe, gedrückt halten zählt weiter), gespeichert als
`disp_rot` (Zehntelgrad). Nur bei Wert != 0 wird beim Start umgebaut: LVGL zeichnet dann im
direct_mode in ein eigenes Vollbild (PSRAM, 2 × 340 KB), der geänderte Bereich wird beim Ausgeben
bilinear gedreht und an den unveränderten Waveshare-Flush übergeben; der Touch-Lesecallback wird
passend zurückgedreht. Ausgabebereich auf 4er-Raster (SPD2010). Bei Drehung kosten große
Bildwechsel etwas Bildrate.
Überarbeitet (04.10.2026): LVGL meldet im direct_mode immer das ganze Bild, der echte Bereich steht in
`draw_ctx->clip_area`; jetzt wird nur der wirklich geänderte Bereich gedreht, jeder für sich (bis 8 pro
Bild, sich berührende zusammengelegt). Bereiche ab ca. halbem Bild (Seitenwechsel, Wischen) schnell
ohne Glättung, 250 ms nach der letzten großen Änderung einmal geglättet nachgezeichnet. Bei 0,0°
wird der Originaltreiber wieder eingesetzt. Während des Online-Updates wird die Drehung ausgesetzt
(`disp_rot_suspend`, gibt 680 KB PSRAM frei), bei einem Fehler wieder eingeschaltet. Update.begin
unterscheidet jetzt „zu wenig Platz“ (Partition) und „zu wenig Arbeitsspeicher“; freier interner
Speicher wird beim Update seriell ausgegeben.
Nochmals überarbeitet (1.0.8): Schnellpfad ohne Glättung wieder entfernt (sah verschoben aus). Bilinear
jetzt mit „aufgespreiztem“ RGB565 (eine Multiplikation pro Farbpaar, 5-Bit-Gewichte, Zeilen ohne
Randprüfung, wenn sie ganz innen liegen) – immer geglättet, deutlich schneller.
**Update-Suche ohne GitHub-API:** statt api.github.com (60 Anfragen/Stunde, große JSON-Antwort) wird
nur `https://github.com/<Repo>/releases/latest` abgefragt (ohne Weiterleitung folgen); die Version steht
im Location-Header (`/releases/tag/vX.Y.Z`), die Datei ist
`https://github.com/<Repo>/releases/download/<tag>/Waage.ino.bin`. Bei Verbindungsfehler ein zweiter
Versuch; Fehlercode und freier Speicher gehen auf die serielle Ausgabe.
**1.0.9:** Gedrehte Ausgabe nicht mehr über einen eigenen 340-KB-Puffer im PSRAM, sondern in Streifen
über die Original-Puffer des Treibers (interner DMA-Speicher). Vermutete Ursache der Update-Probleme:
Aus einem PSRAM-Puffer legt der SPI-Treiber für jede Übertragung Hilfspuffer im internen RAM an, der
dann WLAN/TLS fehlt (Abstürze, „GitHub nicht erreichbar“). Streifen-Höhe = Puffergröße / Breite (4er-Raster),
mit zwei Puffern wird der nächste Streifen gerechnet, während der vorige überträgt. Jeder Streifen
wird geteilt: obere Hälfte rechnet die Anzeige (Kern 1), untere ein Helfer-Task auf Kern 0 („rot“).
Die Drehung wird jetzt schon bei der Update-Suche ausgesetzt (synchron in start(), Aufrufer laufen im
LVGL-Kontext) und danach wieder eingeschaltet.
**Design:** Canvas-Artefakt „Waage UI Design“ (claude.ai) mit jeder Seite, gezeichnet aus Formen:
Display = Objektbaum des Simulators (Testumgebung/design/dump.inc schreibt bei jedem S() dump/*.json,
tohtml.py baut daraus Divs/Bögen/QR-Rechtecke), Web = DOM aus dem Browser-Rundgang (tour.py -> de_dom/),
build_canvas.py erzeugt alle Artboards. Nach UI-Änderungen: Sim + Rundgang laufen lassen, build_canvas.py.

**Logo und Startbild:** Logo = Variante „F1“ aus dem Logo-Canvas „Waage Logos“ (claude.ai): eure
Gehäuseform von oben, ganz in Grün #3DDC97, Platte und Display als Öffnungen; Schriftzug „esp scale“ in
Space Grotesk 600. Startbild in `ui_splash.cpp`: Die Hintergrundbeleuchtung bleibt ab `Driver_Init()`
aus (vorher 60 % – dadurch waren beim Einschalten Testbild/Bildreste des Controllers zu sehen) und
fährt erst hoch, wenn das erste Bild gezeichnet ist. Ablauf (≈ 2,9 s): Licht 0 → BRIGHT_ON, das Leuchten
um das Logo blüht auf und klingt ab, Logo erscheint, „esp scale“ gleitet ein, alles blendet zurück ins
Schwarz, dann Lagecheck. Bilder in `img_splash.c` (nur Deckkraft, LV_IMG_CF_ALPHA_8BIT, ≈ 100 KB Flash),
die Farbe kommt aus dem Farbschema (Grün bzw. Weiß). Überblendet wird nur über `img_opa` der Bilder,
nicht über die Bildschirm-Überblendung (die erzeugt helle Kästen). `BRIGHT_ON` steht jetzt in
`ui_power.h`; `ui_power_init()` schaltet das Licht nicht mehr selbst ein.
**Web-Symbol:** Logo F1 als SVG-Favicon und 180-px-Touch-Icon (schwarzer Grund) direkt in `page.html`
eingebettet (Data-URI, kein zusätzlicher Pfad am Server). README zeigt oben das Logo mit Schriftzug
(`docs/images/logo*.svg`, hell/dunkel per `<picture>`).

**Farben, Auto-Tara negativ, Einschalten, README-Fotos (05.10.2026):**
- **Farbe:** Setup -> Waage -> Farbe öffnet jetzt eine Auswahlliste mit Farbpunkten: Grün (Standard),
  Weiß, Blau, Violett, Pink, Orange, Gelb, Rot (Index = `g_set.theme`, Reihenfolge nicht ändern).
  Tabelle `SCHEMES` in ui_theme.cpp (Akzent, Schrift darauf, Setup-Wert, Untertitel gewählt,
  Warnknöpfe gefüllt/umrandet; bei Weiß, Gelb und Orange nur umrandet, sonst mit Gelb verwechselbar).
  Startbild/Logo (Bilder nur Deckkraft, Farbe = C_ACCENT), Ring, Knöpfe, Auswahl folgen automatisch;
  das Browser-Symbol (Favicon) färbt `setTheme()` in page.html ebenfalls um. Web: `THEMES` in page.html setzt die
  CSS-Variablen (--acc, --on-acc, --acc2, --accbg, --accbd, --bar, --accsh), Weiß bleibt
  `html[data-theme=w]`. Rot liegt nah an der Fehlerfarbe (#FF6B5E) – bewusst trotzdem angeboten.
- **Auto-Tara bei negativem Wert** (`neg_tare_check()` in ui.cpp, nur Wiegeseite): Netto < −1 g,
  Brutto < 3 g (Waage wirklich leer), ruhig, kein Topf/Countdown/Schätzspiel -> nach 3 s Tara mit
  Ton und „Tara gesetzt“. Liegt noch etwas auf (z. B. Glas, aus dem entnommen wird), bleibt der
  Minuswert stehen. Getestet im Simulator (Schüssel ab -> 0,0; 800 g tariert, 200 g entnommen -> −200).
- **Einschalten:** `early_power_hold()` in hal_board.cpp (Konstruktor, Priorität 101) setzt GPIO7
  schon vor `initArduino()` (dort PSRAM-Init mit Speichertest) auf HIGH. Vorher erst in `setup()`.
  Der ROM-/2nd-Stage-Bootloader davor bleibt; am Gerät noch zu messen, wie kurz der Druck jetzt sein darf.
- **README:** neues Hero-Bild aus dem echten Foto (docs/images/hero.jpg, Logo F1 + Schriftzug links),
  drei Fotos aus der Küche unter der Einleitung, Innenleben unter „Hardware“. Fotos zugeschnitten,
  leicht entsättigt/kontrastiert und mit Vignette; Markenprodukte im Hintergrund weggeschnitten.

**Zählen (06.10.2026):** Jede neue Stückzahl, die 1,5 s ruhig liegt, wird einmal ins Protokoll
geschrieben und angesagt (vorher nur einmal pro Auflegen über `track_update()`, beim Nachlegen kam
nichts mehr). Gleiche Zahl = keine Wiederholung; nach dem Leeren wird wieder angesagt. Simulator:
`SPKDBG=1 ./sim de` zeigt die Ansagen (10 -> 15 -> 20 -> leer -> 5).
GitHub-Workflow läuft fest auf `ubuntu-24.04` statt `ubuntu-latest`.

**Weboberfläche schneller erreichbar (07.10.2026):** erster Eintrag der Systemliste (dritte Seite der
Startseite, Symbol Smartphone U+E7BA neu in font_icons_26). `page_web_quick_create()` startet die
Weboberfläche sofort; „Fertig“ führt dann zur Wiegeseite (`wb_from_system`). Die Zeile in Setup ->
Zeit & Funk ist entfallen (ID 13 bleibt ungenutzt), „Dateien ›“ unter Waage führt weiter dorthin.
**Ansage auf der Wiegeseite:** nicht mehr nur einmal pro Auflegen. Ändert sich das ruhige Gewicht seit
der letzten Ansage um mehr als max(1 g, 0,5 %), wird es neu angesagt (`speak_change_g()`), z. B. beim
Nachfüllen in einen per Auto-Tara erkannten Topf. Simulator: `SPKDBG=1` (Topf -> +200 -> +200).

**Ein-/Ausschalt-Ton (07.10.2026):** `SND_POWER_ON` startet in `ui_splash_start()` direkt vor `s_t0`,
also gleichzeitig mit der Startanimation (Markus' Test: aus `setup()` heraus kam er zu früh, weil `ui_init()`
mit SD, Stimme, WLAN und Wägezelle einige Sekunden braucht). `sound_begin()` ist mehrfach aufrufbar.
Längeres Halten der Taste beim Einschalten schadet nicht (`s_ignore_press`). Ausschalten spielt
`SND_POWER_OFF` statt des Überlast-Tons. Beide auch im Schema „Minimal“, bei Lautstärke 0 still.
Klang (Markus' Wahl: Variante A „Aufblühen“): keine Tonfolge, sondern ein Akkord aus Sinus-Teiltönen
mit eigener Hüllkurve, `sound_swell.h` (plattformunabhängig, Werte + Normierung dort). Ein: G4–D5–G5–H5
schwellen gestaffelt an (Spitze nach ≈0,8 s, passend zum Leuchten), Funke G6 bei 0,8 s, Länge 2,4 s.
Aus „Verglühen“: derselbe Akkord, oben zuerst verklingend, 1,2 s; deshalb gibt `ui_power.cpp` den Akku
erst nach `OFF_RELEASE_MS` = 1300 ms frei (vorher 400). `play_swell()` in `sound.cpp`, CAT_LONG (wird
nicht von Klicks abgebrochen). Schema „Sanft“ nur leiser, „Retro“ bleibt Sinus. Die Normierung wurde mit
einem Host-Test gemessen (C-Ausgabe = Hörprobe, max. Abweichung < 0,1 %).

**Libelle und Akku (08.10.2026):** Libelle: X und Y des Lagesensors waren gegenüber dem Display
vertauscht (Markus' Test am Gerät). `config.h`: `LEVEL_SWAP_XY 1`, dazu `LEVEL_FLIP_X`/`LEVEL_FLIP_Y`, falls
eine Achse danach noch falsch herum läuft; angewendet in `ui_read_tilt_raw()` vor `DISPLAY_MOUNT`. Der
gespeicherte Nullpunkt liegt jetzt unter `lvl_x2`/`lvl_y2`; ein alter Nullpunkt (`lvl_x`/`lvl_y`) wird
beim ersten Laden getauscht übernommen. Akku: aus der Systemliste entfernt, jetzt Setup -> Zeit & Funk ->
Akku (Zeilen-ID 15, Wert „72 % ›“, Untertitel der Kachel „Uhr, WLAN, Akku“). Neu `ui_set_back(fn)` in
ui.cpp: Wischen nach rechts auf der Akku-Seite führt zurück zu Zeit & Funk statt zur Wiegeseite.

**Rezepte mit Eiern (08.10.2026, mit Markus abgestimmt):** Enthält ein Rezept die Zutat „Ei“/„Eier“
(auch „Egg(s)“, `is_egg()` in page_rezept.cpp bzw. `isEgg` im Web), zählt der Minus/Plus-Knopf ganze
Eier statt Portionen. Eier im Grundrezept = round(Gramm / `EGG_G`), `EGG_G` = 55 g in config.h (Web:
Konstante `EGG_G` in page.html, gleich halten). Abwiegen: Eier immer zuerst (`s_order`), Anzeige
„3 Eier aufschlagen“, Ring grün ab 80 %, nie „zu viel“, kein Parkpiepser, kein Auto-Weiter. Bei
„Weiter“ wird der Faktor aus dem echten Eigewicht berechnet (`s_factor` = gewogen / Eier-Gramm im
Grundrezept, unter 10 g bleibt der Plan); folgende Schritte zeigen „an 178 g Eier angepasst“. Zurück zu
den Eiern setzt den Faktor wieder auf den Plan. Rezepte ohne Eier unverändert. Eigelb/Eiweiß zählen
bewusst nicht als Eier.

**Minus nach Abnehmen (08.10.2026):** Ansage: Ein Minuswert bei leerer Waage (brutto < `EMPTY_G`,
also nur Tara-Rest) wird nicht mehr vorgelesen (`ui_neg_tare_pending()` in `speak_check()`); Minus mit
Ware auf der Waage (etwas aus der tarierten Schüssel entnommen) wird weiter angesagt. Auto-Tara bei
Minus jetzt als `ui_neg_tare_due(&since, allowed)` in ui.cpp, genutzt von der Wiegeseite und neu vom
Ziel-Modus (page_ziel.cpp: Tara + Ton + Toast, `za_logged` zurück, damit der nächste Durchgang wieder
ins Protokoll kommt). Im Simulator dauert es Beruhigung (~2,5 s) + 3 s; `SPKDBG=1` testet Schüssel und Ziel.

**Libelle Richtung (09.10.2026):** Kurz beide Achsen umgedreht (`LEVEL_FLIP_X/Y 1`), nach Markus' Test
wieder zurück: Richtung war schon richtig, `LEVEL_FLIP_X/Y` = 0. Nullpunkt jetzt unter `lvl_x4`/`lvl_y4`;
Fallback-Kette in settings.cpp: `-lvl_x3` (aus der umgedrehten Version) -> `lvl_x2` -> altes `lvl_y`.

**Rezepte mit Anweisungen (10.10.2026, mit Markus abgestimmt):** Rezept = Folge von Schritten,
`ingredient_t` hat jetzt `kind` (STEP_WEIGH/STEP_NOTE), `icon` (SI_*), `secs`, `text[56]`;
`recipe_t.weigh` = Anzahl Zutaten (Listen zeigen „N Zutaten“). `RECIPE_MAX_ING` 24. Datei:
`>icon;Text;Minuten` (Schlüssel in data.cpp `SI_KEYS`, unbekannt -> hinweis; Cocktails ignorieren
`>`-Zeilen). Gerät: page_rezept.cpp `page_note_create()` – Icon in `font_icons_80` (neu, 15
Material-Symbols-Zeichen, `SI_GLYPH` in gleicher Reihenfolge) in `C_ACCENT`, Text 24 bzw. 18 px,
Knopf „Timer m:ss“ startet `timer_start(timer_free(), …)`, Ring = Fortschritt; Tara nur vor
Zutaten-Schritten, kein Parkpiepser. Eier weiterhin zuerst, Rest in Dateireihenfolge. Am Gerät
legt „Neues Rezept“ weiter nur Zutaten an; Anweisungen im Web-Editor (Typ, Icon-Auswahl,
Minuten, ▲▼ verschieben; `{ } [ ] ;` werden ersetzt, Text auf 55 Byte gekürzt). Web-Icons sind
SVG-Pfade derselben Glyphen (`SI` in page.html), Farbe `var(--acc)`. API: Anweisung als
`{"k":1,"icon":"ruehren","text":"…","sec":120}`. Beispielrezepte auf der SD-Karte und das
eingebaute Pfannkuchen-Beispiel haben jetzt Anweisungen. Simulator: `NOTETEST=1`, `SAVETEST=1`.

**Profile, Etappe 1 (11.10.2026, Konzept „Waage für Oma“ im Claude-Doc):** Neue Einstellungen
`profile` (0 Standard, 1 Küche, 2 Werkstatt, 3 Einfach), `cats` (Bit 0 Küche, 1 Werkstatt, 2 Spiele)
und `modes_off` (Bit = Modus über alle Gruppen: Küche 0–5, Werkstatt 6–11, Spiele 12–15), im Flash
als `profile`/`cats`/`modes_off`. `settings_profile_apply()` steht in der eigenen Datei
`settings_profile.cpp` (ohne Flash, damit der Simulator sie mitnutzt) und setzt nur beim Profilwechsel
die Voreinstellungen (Einfach: Farbe 6 Gelb, Ansage + Sprüche an, Lautstärke ≥ 80, g, Auto-Tara/
Präzision/Auto-Speichern aus, Auto-Aus ≥ 60 min). ui.cpp: `profile_lists()` baut vor jedem Aufbau
die sichtbaren Gruppen/Systemeinträge (Küche ohne Wasserwaage, Werkstatt ohne Töpfe) mit Index-
Abbildung (`s_modi_map`, `s_mode_map`, `s_sys_map`); `h_tiles` = 1–3 Seiten, Punkte passend.
`ui_profile_changed()` = Farben + Startseite neu + speichern, bei Einfach offene Seite schließen und
Topf/Countdown beenden. **Einfach (Variante A):** `build_simple_layout()` blendet Ring, Statuszeile,
Chips, Knöpfe, Lautsprecher aus (Wieder-Einblenden in `set_pot_mode`, `speaker_update`,
`home_update_cb` abgefangen), ganze Gramm, keine Topferkennung, kein Auto-Speichern; obere Fläche
(0–268 px) sagt mit `sound_speak_weight_now()` an (neu in sound.cpp: Art 2 = immer, ohne 2-s-Sperre,
bricht laufende Ansage ab), untere Fläche = Tara (Akzent gemischt mit Hintergrund, Rand oben),
Meldungen oben; Wischen > 30 px löst nichts aus. PWR unverändert. Setup -> Waage -> Profil (Kennung
38, erste Zeile; `ROWS/IDS[4][9]`, `row_val[10]`) mit Liste und Rückfrage für Einfach. Web: `/api/profile?p=`,
POST `/api/modes {cats, modes_off}`, Karte „Profil“ oben in den Einstellungen (Chips, Schalter je
Gruppe, Modi als Chips; Namen in page.html `GROUPS`, Reihenfolge wie `GROUP_*` in ui.cpp halten).
Simulator: `PROFTEST=1`. Nächste Etappen laut Konzept: Einrichtung beim ersten Start, Updates
(automatisch nach dem Start, Schalter „Auto-Update“), Sprachsteuerung mit Weckwort „Hallo Waage“
(eigenes deutsches KWS-Netz), Zielwiegen mit Parkpiepser per Stimme im Profil Einfach.

**Profile, Nachbesserung (11.10.2026):** Ist nur eine Gruppe sichtbar (Profil Küche/Werkstatt oder
alles andere ausgeblendet), zeigt die Modi-Seite gleich deren Modi (Titel = Gruppenname,
`s_tile_items`/`tile_mode_cb`, Zurück führt auf diese Seite). Einfach: obere Fläche **5 s gedrückt
halten** öffnet `page_web_quick_create()` (startet die Weboberfläche, `SIMPLE_WEB_HOLD_MS`, über
`LV_EVENT_PRESSING`, Bewegung > 30 px bricht ab), Hinweis in der Rückfrage am Gerät und in der Web-Karte.
Akkustand ganz oben („Akku 72 %“, „lädt“/„voll“, ab 20 % orange, ohne Akku ausgeblendet).
Rezept-Anweisung mit Timer: Timer-Knopf in eigener Zeile über Zurück/Weiter, Icon und Text rücken hoch.

**Etappe 2 und Korrekturen (11.10.2026):** Profil Werkstatt schaltet die Präzision nicht mehr ein,
Einfach lässt die Farbe unverändert (vorher Gelb). Einfach sagt nur ganze Gramm an (`speak_check`
rundet). „Überlast“ ohne Gewicht kam von einzelnen falsch gelesenen HX711-Werten: scale.cpp filtert
jetzt mit dem Median der letzten drei Rohwerte (`median3`) und meldet Überlast erst nach drei
Messungen in Folge (`OVER_SAMPLES`). **Einrichtungs-Assistent** (page_wizard.cpp): Sprache, Profil,
Kategorien (nicht bei Einfach), Farbe, Ton (Lautstärke ±10, Ansage), WLAN; jeder Schritt mit
„‹ Zurück“, „Überspringen ›“, Wischen nach rechts = Zurück. Startet nach dem Startbild, solange
`setup_done` (Flash-Schlüssel `setup_done`) falsch ist; bereits eingerichtete Waagen (Schlüssel
`unit` vorhanden) gelten als erledigt. Danach Lagecheck. Wiederholen: Setup -> Waage -> Einrichtung
(Kennung 39, `ROWS/IDS[4][10]`). Während er läuft, leitet `ui_set_home_override()` alle
„zur Wiegeseite“-Wege (Fertig, Wischen) zum aktuellen Schritt um, `page_setup_back_override()` die
WLAN-Seite. Schritt 1 bietet „Mit dem Handy einrichten“ (startet die Weboberfläche mit QR-Code).
Web: Seite `#/einrichtung` (öffnet sich von selbst, solange `setup_done` false ist; Link in den
Einstellungen unter Waage), POST `/api/setup {profile,cats,theme,vol,speak}` beendet auch den
Assistenten an der Waage (`wizard_finish_external`). Der Schritt „Updates“ aus dem Konzept kommt mit
Etappe 3 (Auto-Update). Simulator: `FIRSTRUN=1` (Ablauf erster Start + Wiederholen mit Einfach),
Mock: `MOCK_FIRSTRUN=1`. **Prüfung vor jeder Lieferung:** zusätzlich zum Simulator ein echter Build
mit arduino-cli, ESP32-Paket 3.1.1, Waveshare-Treibern und LVGL aus dem Waveshare-Paket, FQBN wie im
GitHub-Workflow (`PartitionScheme=app3M_fat9M_16MB`, `USE_VOICE 0`).

**Assistent neu (11.10.2026, Markus: Ring aus Variante D, Erklärung aus Variante B):** Statt
„Zurück/Überspringen“ zwei runde Pfeil-Knöpfe unten links und rechts (45° unter der Mitte,
`NAV_R 150`, damit sie den Fortschrittsbogen am Rand nicht berühren; Bogen = `ui_ring` mit 6 px).
Der rechte Pfeil übernimmt in Listen den mittleren Eintrag (`list_center`), im letzten Schritt ist er
ein Haken. Listen sitzen `LIST_SHIFT` (-30 px) höher, unten verdeckt eine Fläche die Einträge hinter
den Knöpfen. Nach dem WLAN folgt „Probier es aus“ (`WZ_TUT1..3`): links wischen (animierte Hand),
auf der nachgestellten Modi-Seite rechts wischen, Zahl lange drücken; im Profil Einfach oben tippen
(Ansage) und unten auf TARA tippen. Der rechte Pfeil bleibt gedämpft, bis die Übung geklappt hat,
überspringt sie aber auch so. Übungsseiten werden mit `lv_scr_load_anim` ohne die allgemeine
Wisch-Geste geladen und werten Wischen selbst aus; nach einem Wechsel per Geste `lv_indev_reset`,
sonst kommt beim nächsten Drücken kein LONG_PRESSED. Seiten-Zeiger werden beim Löschen nur
vergessen, wenn sie zur gelöschten Seite gehören (`forget`), weil die alte Seite erst nach dem Aufbau
der neuen gelöscht wird. Neue Symbole: `ICON_ZURUECK/WEITER/HAKEN` (font_icons_26),
`ICON80_HAND/TIPPEN` (font_icons_80). Web: nur noch „← Zurück“ und „Weiter →“ bzw. „Fertig ✓“.

**Kleinigkeiten (11.10.2026):** Übung „Zahl halten“ zeigt das echte Gewicht (`th_show`, 200 ms) und
schaltet beim langen Drücken zwischen einer und zwei Nachkommastellen um, mit Chip „präzise“ (nur
Übung, `g_set.precise` bleibt). Ist Auto-Speichern an, ist der Speichern-Knopf auf der Wiegeseite
ausgeblendet (Tara steht allein; wird in `home_update_cb` nachgeführt). Topf im Countdown abgelehnt
(`s_pot_rejected`): die Wägung wird jetzt trotzdem automatisch gespeichert, bis die Waage wieder leer
ist. Simulator: `ASTEST=1`.

**Etappe 3: Auto-Update (11.10.2026):** Neue Einstellung `upd_auto` (Flash `upd_auto`, Standard an).
Beim Lagecheck sucht `boot_update_start()` (ui.cpp) still nach einer neuen Version
(`upd_check_quiet()`, ohne Töne); nicht bei lokal gebauter Firmware (`FW_LOCAL`, sonst würde die
IDE-Version sofort überschrieben) und nicht ohne gespeichertes WLAN. `boot_update_tick()` wartet,
bis die Startseite offen ist (nie in einem Modus oder Countdown), dann: Auto-Update an und sicher
(Waage leer, Akku ≥ 40 % oder am Kabel, höchstens 60 s nach dem Einschalten,
`BOOT_UPD_AUTO_MS`) -> `page_update_auto_create()` installiert sofort; sonst
`page_update_offer_create()` mit „Später“ (fragt beim nächsten Start wieder) und „Jetzt“. Nach 3 min
ohne Ergebnis gibt sie auf. **OTA-Rollback:** `verifyRollbackLater()` liefert true (update_online.cpp),
`upd_mark_valid()` markiert die laufende Firmware erst nach 15 s Betrieb (`MARK_VALID_MS`) als gültig;
startet eine neue Firmware nicht sauber, nimmt der Bootloader beim nächsten Start die alte (gilt auch
für Updates per Browser und Arduino-IDE übers Netz). Vor dem Neustart merkt sich die Waage die
Zielversion (`upd_to`); nach dem Start meldet `upd_boot_notice()` „Aktualisiert auf Version …“ oder
„Update fehlgeschlagen, alte Version aktiv“. Firmware-Seite (Setup -> Waage -> Firmware) hat jetzt
die Zeile „Auto-Update an/aus“, die Weboberfläche einen Schalter unter Einstellungen -> Waage, der
Assistent einen Schritt „Updates“ (nur mit WLAN, Gerät und Web). Simulator: `UPDTEST=1` bzw. `0`
mit `SIM_UPD=1`.

**Etappe 4: Zielwiegen mit Parkpiepser im Profil Einfach (11.10.2026):** `ui_target_start(g)` /
`ui_target_stop()` / `ui_target_goal()` (ui.h) – derselbe Einstieg für Weboberfläche und später die
Sprachsteuerung. Profil Einfach: tariert, zeigt auf der Wiegeseite oben „Ziel 250 g“, darunter die
Restmenge groß („noch 120 g“, „Ziel erreicht“, „25 g zu viel“ in Rot, Gewicht dann auch rot), der Ring
(`h_ring`, im Einfach-Layout sonst aus, jetzt über der Tara-Fläche) füllt sich bis zum Ziel. Parkpiepser
wie im Modus Ziel (`sound_parking`, Toleranz ±2 g). Ansage: oben antippen bzw. automatisch nach 1,5 s
Ruhe „noch 120 Gramm“ / „25 Gramm zu viel“ (`sound_speak_rest()`), im Ziel „Ziel erreicht“ vom Piepser.
Ergebnis kommt still ins Protokoll („Ziel 250 g“). Tara = nächste Portion mit demselben Ziel. Ende:
nach dem Erreichen abgenommen (3 s unter 10 % des Ziels, `TGT_END_MS`) -> „fertig“, die Waage stellt sich
wieder auf 0 (Einfach hat kein Auto-Tara); sonst nach 15 min ohne Gewichtsänderung oder über die
Weboberfläche. Andere Profile: öffnet den Modus Ziel mit diesem Gewicht (`page_ziel_goal_create`,
`page_ziel_running`). Web: GET `/api/target?g=250` (0 = beenden, 409 wenn Start/Assistent läuft),
`/api/state` hat `tgt`; Seite Ziel hat „An der Waage anzeigen“ und die Zeile „An der Waage läuft … Beenden“.
**Neue Wörter im Stimmpaket:** `noch.wav`, `zu_viel.wav` (englisch `to_go.wav`, `too_much.wav`).
Fehlen sie, sagt die Waage statt der Restmenge das Gewicht an. Roboter/Roboter-EN haben sie (espeak-ng);
eigene Pakete: `stimme_schneiden.py aufnahme.mp3 <Paket> --nachtrag` (Vorleseliste:
`--liste x x --nachtrag`), alte Aufnahmen mit 48 Wörtern lassen sich weiter schneiden.
Simulator: `TGTTEST=1` (Einfach mit Schale, Restmenge, zu viel, Abnehmen -> fertig, Tara-Runde,
Standard-Profil öffnet den Modus Ziel). Mock: `/api/target`, `tgt`.
Übersetzungen: `/home/claude/i18n/insert_web.py page.html` setzt den Sprachblock ein (vorher lag
`setLang` durch das Einsetzen achtmal in page.html, bereinigt).

**Zielwiegen Einfach, Variante A (11.10.2026, Markus: Start übers Handy war schlecht, Ansicht auch;
vier Varianten gezeigt, A gewählt mit der Auflage, dass nichts über den Rand ragt):** Die untere Fläche
der Wiegeseite ist geteilt: links TARA, rechts ZIEL (je Symbol über der Schrift). Beide Hälften liegen in
einem Kreis `SIMPLE_ZONE_D` 368 px mit `clip_corner`, also innerhalb des Rings (Ring 396 px, innen
188 px Radius); Beschriftung `SIMPLE_LAB_DX` 66 px neben der Mitte, weil der Kreis am Rand zu schmal ist.
ZIEL öffnet `page_ziel_simple_create()` (page_ziel.cpp): „Wie viel?“ mit vier Kacheln 100/200/250/500 g,
zuletzt benutztes Ziel hervorgehoben, Antippen startet sofort (`ui_target_start`). Beim Zielwiegen:
Akku/Lautsprecher/Hinweis weg, oben „Ziel 250 g“, Zahl höher (`CENTER -54`), darunter das Feld
`h_simple_pill` (gedämpft „noch 70 g“, gefüllt mit Haken „Ziel erreicht“, rot „25 g zu viel“, nach dem
Abnehmen „Fertig“), Ring am Rand, Flächen ab `SIMPLE_ZONE_Y_TGT` 286: TARA (nächste Portion, ohne
Meldung) und ENDE (grau, beendet). Neue Symbole: `ICON_TARA` (adjust, font_icons_26), `ICON36_HAKEN`
(neue Schrift `font_icons_36`, nur der Haken). Einrichtungs-Übung Einfach zeigt jetzt TARA | ZIEL.
Simulator: `TGTTEST=1` startet über die Fläche ZIEL und prüft mit `edge_check()` (main.cpp), dass keine
Beschriftung über den Display-Rand bzw. den beschnittenen Kreis ragt (6 px Luft); `EDGEALL=1` prüft
das bei jedem Bildschirmfoto (meldet auch kleine Seitentitel ganz oben, die sichtbar noch passen).
Der Web-Knopf „An der Waage anzeigen“ und `/api/target` bleiben als zusätzlicher Weg.
**Etappe 5: Sprachsteuerung „Hallo Waage“ (12.10.2026):** Eigene Schlüsselwort-Erkennung ohne
Bibliothek und ohne ESP-SR (`USE_VOICE 0` bleibt, Partitionsschema unverändert). `kws.cpp/.h`:
Merkmale (16 kHz, 30/20 ms, FFT 512, 40 Mel 20–7600 Hz, log, gleitender Mittelwert je Band über
`cmn_frames` 150 abgezogen, Normierung aus dem Modell) und Netz (CONV, DW, GAP, DENSE, „same“-Ränder wie
TensorFlow), Modell `/Waage/Sprache/kws_de.bin` (Format im Kopf von kws.cpp), Gewichte und Puffer im PSRAM,
FFT-Puffer per malloc (statisch hätte der interne RAM nicht gereicht: `dram0_0_seg` lief über).
Python und C rechnen identisch (Paritätstest: Merkmale < 0,001, Wahrscheinlichkeiten < 1e-8).
`voice.cpp` (Zweig ohne ESP-SR): Task „Voice“ auf Kern 0 liest das Mikrofon (beide Kanäle addiert),
`process()` → `kws_feed()` alle 10 Rahmen (200 ms, Fenster 2 s = 100 Rahmen, Netz 32/64/96 Kanäle, etwa 4 Mio. MAC; Rechenzeit steht
alle 30 s im seriellen Monitor), drei Auswertungen gemittelt. Ablauf: Weckwort ≥ 65 % → Ton (SND_TEST)
und „Ich höre zu …“ (Einfach: Zeile oben + Mikrofonsymbol in Akzentfarbe, sonst Meldung), 6 s lang
Befehle ≥ 40 % (Schwellen stehen im Modell); nach dem ersten Treffer 0,45 s sammeln und die höchste
Spitze nehmen, bei Zielmengen 1 s sammeln und die Zahl nehmen, die zuletzt über der Schwelle war
(„zweihundertfünfzig“ erreicht seine Spitze nach „zweihundert“). Während Ton/Ansage (`sound_busy()`),
2 s nach dem Einschalten (Mittelwert schwingt ein), 0,5 s nach dem Weckwort und 1,2 s nach einem Befehl
wird nichts ausgewertet. Nach einem Befehl schläft die Waage wieder. Befehle nach Klassennamen:
`tara` (`ui_cmd_tara`), `wie_viel` (`ui_cmd_speak`, beim Zielwiegen Restmenge), `speichern`, `weiter`,
`zurueck`, `stopp`, `ziel_<g>` (`ui_target_start`). Neue Zielmengen also nur im Modell.
**Wörter aufnehmen:** Setup -> Ton -> Wörter (Kennung 27, `page_voice_rec.cpp`): Liste aller Wörter mit
Anzahl, je Wort runder Knopf, Piepton, nach 0,35 s 1,6–1,8 s Aufnahme (Küchengeräusche 10 s), Ring zeigt
den Fortschritt, Pegelbalken. Dateien `/Waage/Sprache/Aufnahmen/<klasse>/a001.wav` (16 kHz Mono).
`voice_rec_session()` zählt offene Seiten. Mikrofonseite zeigt „hört: tara 62 %“. Setup-Zeile
Sprachbefehle: „kein Modell“, ohne Modell lässt sie sich nicht einschalten. Web: Einstellungen zeigen
Modellinfo bzw. Fehler; Hochladen einer Datei `kws_*.bin` lädt das Modell neu (`voice_model_changed`).
**Ordner Sprachmodell** (neu, neben Waage): `kws_common.py` (Klassen, Sätze, Merkmale, Netz, Export),
`stimmen_laden.py` (sechs Piper-Stimmen von GitHub, Release v0.0.2), `kws_daten.py` (Piper, espeak-ng,
acht MBROLA-Stimmen mb-de1…8 falls installiert, Stimmpakete), `kws_train.py` (Augmentierung:
Lautstärke −45…−3 dB, Tempo, Stimmlage per VTLP, Hall, Färbung, Rauschen bei 8–65 dB Abstand,
Hintergrund vor dem Fenster anders laut, Brummen, Klappern, fremde Sprache; Weckwort dreifach, kurze
Befehle doppelt; Prüfstimme ramona bleibt draußen; eigene Aufnahmen vierfach), `set_thresholds()`,
`kws_pruefen.py` (WAV wie die Waage auswerten, nur numpy), LIESMICH.md. Mitgeliefertes Modell
`SD-Karte/Waage/Sprache/kws_de.bin` ist nur mit künstlichen Stimmen trainiert; mit Aufnahmen vom Gerät
wird es deutlich besser. Simulator: `KWSTEST=1` (Testsätze der Prüfstimme aus /home/claude/kws/test,
Aufnahme-Seite); Testumgebung kompiliert voice.cpp jetzt mit (`voice_sim_feed`).

**Ordner als ZIP (12.10.2026):** Weboberfläche -> Dateien: je Ordnerzeile „↓ ZIP“, unter der Liste
„↓ <Ordner> als ZIP herunterladen“ (mit allen Unterordnern, bis 4 Ebenen, höchstens 3000 Dateien).
`/zip?dir=…` in web.cpp (`h_zip`, `zip_walk`), ZIP-Bausteine in `zipw.cpp/.h` (gespeichert ohne
Kompression, Datenbeschreibung hinter den Daten, CRC-32 mit 16er-Tabelle, Chunked-Übertragung). Die
SD-Karte ist nur während jedes 4-KB-Lesevorgangs gesperrt. Die Anzeige steht während des Downloads (wie
bei großen Einzeldateien, die Weboberfläche läuft im Haupttakt). Geprüft: ZIP aus zipw-Bausteinen mit
Python `zipfile` und `unzip -t` fehlerfrei; Mock liefert für /zip Testdaten.

**Stabilität und Kleinigkeiten (12.10.2026, nach Rückmeldung aus dem Alltag):**
- **Ringalphabet:** Die Lupe (drei Buchstaben in der Mitte) zeigt die Nachbarn jetzt so, wie sie auf
  dem Ring liegen: unten (etwa 4 bis 8 Uhr, `cos < -0,5`) läuft das Alphabet von rechts nach links,
  dort steht der nächste Buchstabe links. Oben und an den Seiten bleibt es alphabetisch
  (`ui_text.cpp`, `show_press`). Simulator: `TXTTEST=1`.
- **Profil Standard:** `settings_profile_apply` schaltet Auto-Speichern ein (vorher blieb es nach
  Einfach aus).
- **Assistent:** Schritt „Updates“ immer (Gerät und Web), ohne WLAN mit dem Hinweis „Sucht erst, wenn
  WLAN eingerichtet ist“. Simulator: `NOWLAN=1 FIRSTRUN=1`, Mock: `MOCK_NOWLAN=1`.
- **Akku-Prozent:** `batt_filter.*`: Median aus 5 Messungen, dann gleitender Mittelwert (schnell
  ≈ 3 s für Lade-/Entladeerkennung, langsam ≈ 30 s für Prozent und Volt), Prozent mit Hysterese
  (Entladen nur abwärts, Laden nur aufwärts, Gegenrichtung erst ab 3 Punkten). `hal_board.cpp`
  füttert den Filter mit jedem neuen ADC-Wert (alle 500 ms aus `Driver_Loop`).
- **Online-Update robuster** (`update_online.cpp`):
  - Download wird bei Abbruch bis zu 5× mit `Range: bytes=n-` fortgesetzt (Header bleibt über die
    GitHub-Weiterleitung erhalten; kann der Server nicht fortsetzen, von vorn).
  - Nach jedem Stück `vTaskDelay(1)`: im Arduino-Paket ist `CONFIG_ESP_TASK_WDT_PANIC=y` mit 5 s
    und Leerlauf-Task Kern 0 überwacht, eine Update-Schleife ohne Pause kann sonst einen
    Watchdog-Neustart auslösen.
  - Spracherkennung ruht während Suche und Download (`voice_hold`), auch beim Browser-Update.
  - WLAN bleibt nach „neue Version gefunden“ 3 min an (`upd_wants_wifi`, gilt auch in `wifi_off`
    und `web_stop`), statt zwischen Suche und Installation aus- und wieder einzuschalten.
  - Neustart nach dem Update über `upd_restart()`: beim Router abmelden (`WiFi.disconnect(true)`),
    SD aushängen (`storage_end`), Display in den Reset (`hal_restart`). Vermutete Ursache für
    „nach einem Neustart durch die Firmware klappt das Update schlecht, nach Aus/Ein schon“: der
    Router hält die alte Anmeldung noch, und die Peripherie (Display, SD) behält ihren Zustand.
  - Beim Start keine Update-Suche, wenn die Firmware noch in der Probezeit ist (eben aktualisiert,
    `upd_pending_verify`) oder der letzte Neustart ein Absturz war; sonst erst 3 s nach dem
    Lagecheck (`BOOT_UPD_DELAY_MS`).
- **Absturzbericht** (`crash.*`): beim Start Grund (`esp_reset_reason`) und, nach einer Panik, die
  Core-Dump-Zusammenfassung (Task, PC, Ursache, 8 Rücksprungadressen) lesen, nach
  `/Waage/Absturz.txt` schreiben und den Core-Dump löschen. `crash_phase()` merkt sich in
  RTC_NOINIT, was gerade lief („Beim Laden des Updates“, 37 %). Weboberfläche: Firmware-Karte zeigt
  „Letzter Neustart: Absturz“ mit Phase. Die Action legt `Waage.ino.elf` ins Release
  (addr2line, siehe GITHUB.md). Simulator: `CRASH=1`, Mock: `CRASH=1`.

**Etappe 6: Sprachsteuerung für blinde Nutzer (13.10.2026, nach erstem Test am Gerät):** Rückmeldung
von Markus: Weckwort 1 von 7, „50 Gramm“ 2 von 3, Aufnahmen zu kurz, Sprachausgabe ruckelt.
- **Ruckeln der Ansage:** Sound-Task lief auf Kern 0 mit Priorität 2, die Spracherkennung (KWS, 50–70 ms
  je Auswertung) mit 3 → I2S-Puffer lief leer. Sound-Task jetzt Priorität 4; während `sound_busy()`
  wird das Netz gar nicht gefüttert (`process()` in voice.cpp), danach `AFTER_SOUND_MS` 250 ms Pause.
- **Rückmeldungen** (`sound_say*` in sound.cpp, immer gesprochen, unterbrechen laufende Ansagen):
  „Ja?“ nach dem Weckwort (ohne Stimme: Ton), „Nicht verstanden. Sag Hilfe.“ (wach, es wurde etwas
  gesagt – Befehlsklasse ≥ 0,25 oder `_unbekannt` ≥ 0,6 nach 1,5 s –, aber kein Befehl), „Ziel 250 Gramm.
  Jetzt einfüllen.“ (`ui_target_start`, auch beim Start über die Kacheln), „Zielwiegen beendet.“ (Stopp,
  ENDE), Tara/Gespeichert auch ohne Sprüche. Fehlt ein Satz im Paket, kommt er aus
  `/Waage/Stimme/_Zusatz` (Piper de-eva_k-x-low) bzw. `_Zusatz-EN` (espeak-ng); diese Ordner haben kein
  gramm.wav und erscheinen nicht in der Paketauswahl. Roboter/Roboter-EN haben die Sätze selbst.
  Erzeugen: `stimme_erzeugen.py _Zusatz --zusatz --piper de-eva_k-x-low.onnx`; eigene Pakete:
  `stimme_schneiden.py ansagen/ <Paket> --ansagen --ordner`.
- **Neue Befehle** (Klassen in kws_common.py, WORDS in voice.cpp): `hilfe`, `nochmal` (`sound_repeat`,
  letzte Gewichts-/Restansage; Rückmeldungen zählen nicht), `lauter`/`leiser` (±20, 20…100 %, Ton),
  `akku`, `uhrzeit`; „Null“ = Tara, „Abbrechen“ = Stopp. „Wie spät ist es“ aus OTHER_DE entfernt.
  Modell v7 (`kws_de.bin`, 22 Klassen). Neu im Training: Befehle zu 40 % mit dem Weckwort davor
  (`prefix` in `make_example`, Abstand 0,15–0,8 s), `_unbekannt` zu 20 % – vorher (v6) scheiterten kurze
  Befehle direkt nach dem Weckwort, weil dessen Rest noch im Fenster stand. Prüfsatz (Stimme ramona,
  60 Dateien „Hallo Waage“ + Befehl, /home/claude/kws/eval_suite.sh): v7 40/60, alte Befehle 32/42
  (v5: 31/42), neue 8/18, Weckwort verpasst 2/60 (v5: 4). v8 (neue kurze Wörter doppelt gewichtet,
  50 Epochen) war schlechter (37/60, Weckwort 8× verpasst) und wurde verworfen. Schwach bleiben
  „Akku“, „Hilfe“, „Stopp“; eigene Aufnahmen helfen hier am meisten.
- **Doppeltipp** oben auf der Einfach-Startseite (< 450 ms) = Weckwort (`voice_wake()`).
- **Aufnahmen** 2,5 s (Weckwort, Zielmengen 3 s) statt 1,6/1,8 s; **Serie 30 s** (`voice_rec_start(w, true)`,
  Datei `s001.wav`, Knopf beendet vorzeitig, `voice_rec_stop`). Training (`kws_train.py`):
  `find_rec_dir` findet doppelt verschachtelte Ordner, Übersicht je Wort, Serien werden per Energie in
  Wörter zerschnitten (`split_utterances`), abgeschnittene Aufnahmen (Sprache bis zum Ende) zählen
  halb, Tipps bei fehlendem `_unbekannt`/`_hintergrund`. `kws_daten.py --nur <klassen>` / `--andere`
  ergänzt nur, Piper-Warnungen „Missing phoneme“ ausgeblendet.
- Simulator `KWSTEST=1`: zusätzlich akku, uhrzeit, hilfe, lauter, nochmal, „nicht verstanden“,
  Doppeltipp + Tara, Serienaufnahme. Testsätze: /home/claude/kws/mk_test.py (Prüfstimme ramona).

**Nachtrag 13.10.2026:** Aufnahmezahlen nach dem Neustart standen auf 0, solange kein Modell geladen war
(`s_counts` war statisch 0 und wurde nur in `rebuild_extra()` auf -1 gesetzt) – jetzt `counts_reset()` beim
ersten Zugriff, ohne SD-Karte wird nichts gemerkt. Simulator: `RECCOUNT=1`. Training:
`--ohne-abgeschnittene`, Liste `abgeschnitten.txt`.

**Absturzbericht überarbeitet (14.10.2026)** nach Markus' `/Waage/Absturz.txt` (1.1.17/1.1.18):
- Auswertung (addr2line, Bibliotheksadressen stimmen mit dem lokalen Build überein): Zeilen 1–5
  „assert failed: udp_new_ip_type“ im Task `update` bei der Update-Suche, Rücksprung
  `udp_new ← dns_enqueue/dns_alloc_random_port ← dns_gethostbyname ← sntp_send_request ←
  sntp_try_next_server`. SNTP (von `configTzTime`) lief dauerhaft weiter und fragte mitten in der
  DNS-Anfrage des Updates erneut, die lwIP-Sperrprüfung schlug an. **Abhilfe:** `net_sntp_stop()`
  (net.cpp, unter `LOCK_TCPIP_CORE`): nach erfolgreichem Abgleich oder Zeitüberschreitung, vor
  `wifi_off`, in `web_stop` und am Anfang jedes Online-Updates (`start()` in update_online.cpp).
- Zeilen 6–9 (1.1.18, ohne Tätigkeit): Ausnahme im Task-Wechsel (`vTaskSwitchContext` /
  `prvSelectHighestPriorityTaskSMP`), einmal Task Driver mit LoadProhibited, zweimal IDLE0 mit Ursache 65
  (Sonderfall, im Panik-Handler), einmal ganz zerstört. Das spricht für eine zerstörte Task-Liste
  (Stapelüberlauf oder Speicher überschrieben); Ursache noch offen. Gefunden und behoben: Wettlauf in
  `voice_rec_stop()` (UI-Task kürzte `s_rec_len`, während der Audio-Task schrieb → möglich negatives
  `memcpy`; jetzt kürzt nur die Länge, der Audio-Task prüft `k > 0` und setzt die Länge passend).
- **Neues Format** `/Waage/Absturz.txt` (Spalten mit `;`, erste Zeile Überschrift): Zeitpunkt; Version;
  Laufzeit; Neustartgrund; Tätigkeit; Task; Fehler (Ursache übersetzt, bei 28/29 mit Adresse, bei
  assert „Abbruch“); Meldung (bis 200 Zeichen, vorher 64 und abgeschnitten); Rücksprungadressen (bis 12);
  Speicher intern frei/kleinster Stand/größter Block; Stapel frei der 6 knappsten Tasks.
  **Zeitpunkt** kommt aus einem Lebenszeichen (`crash_tick()` im UI-Takt, RTC_NOINIT `s_hb`): jede Sekunde
  Uhrzeit, Laufzeit und Speicher, alle 10 s die Stapelreserve aller Tasks (`uxTaskGetSystemState`).
  Vorher stand „?“, weil `crash_begin()` vor dem ersten RTC-Lesen lief. Fehlt das Lebenszeichen, wird
  die Uhrzeit beim Neustart mit „(Neustart)“ eingetragen; geschrieben wird erst, wenn Uhrzeit und SD
  da sind (spätestens nach 15 s). Alte Datei ohne Überschrift wird zu `Absturz_alt.txt`.
  Web: Firmware-Karte zeigt Tätigkeit, Zeitpunkt (`crash_time`) und den Pfad. Host-Test des Formats:
  scratchpad `ct/` (Fake-Header für ESP-IDF).

### Stand 14.10.2026 (abends): Touch-Absturz, Ausreißer, Kommastellen, Update-Suche
- **Absturz 14.10.2026 23:39 (1.1.19, Langzeitmessung, 1 h 12 min):** `assert failed: xQueueGenericSend`
  im loopTask. Rücksprungadressen (addr2line mit derselben Quelle) → `i2c_master_cmd_begin` ←
  `TwoWire::requestFrom` ← `I2C_Read_Touch` (Touch_SPD2010.cpp) ← `read_tp_hdp` ← `tp_read_data`.
  Der Waveshare-Touchtreiber vertraut den Längen vom Chip: `I2C_Read_Touch` kopierte alles aus dem
  I2C-Puffer (bis 128 Byte) in 8/32/64-Byte-Stapelpuffer, `read_len` bis 65535 ergab bis zu 169
  `rpt[]`-Einträge (Feld hat 10), `next_packet_len` ungeprüft, die `goto hdp_done_check`-Schleife ohne
  Ende, bei I2C-Fehlern uninitialisierte Puffer. Ein einziger Störimpuls überschreibt so den Stapel →
  zerstörte Semaphore (genau dieser assert) und vermutlich auch die rätselhaften Task-Wechsel-Abstürze
  von 1.1.18. **Gepatcht** in `Touch_SPD2010.cpp`: höchstens `Length` Byte, Rest verwerfen, Puffer
  nullen, Längen auf die Puffer begrenzen, < 4 Byte = keine Berührung, Restdaten höchstens 8-mal.
  `/home/claude/ardu/drivers/Touch_SPD2010.cpp` (für check.sh) und `src/Waage/` sind identisch.
- **Ausreißer 122,14 g statt 239,6 g** (CSV der Langzeitmessung 23:32:11, ein einzelner Wert) und
  die Ansage dazu: `median3` bekam in `scale_update()` denselben alten HX711-Wert mehrfach (UI-Takt
  schneller als 10 Hz), dadurch rutschte ein einzelner Fehlwert durch. Jetzt nur neue Werte in den
  Median (`s_fresh`). Zusätzlich `sp_changed` in ui.cpp: schlägt das Gewicht nur kurz aus und landet
  wieder beim angesagten Wert (innerhalb `speak_change_g`), wird nicht noch einmal angesagt.
  Sim-Test `SPIKETEST` (ohne die Änderung: zweite Ansage 239,0 g).
- **Kommastellen reihum** (Gewicht lange drücken, Einstellungen „Kommastellen“, Web-Auswahl,
  Tutorial-Übung 3): 0,0 g → 0,00 g (präzise) → 0 g (ganze Gramm) → … Neue Einstellung
  `g_set.whole` (NVS `whole`, schließt `precise` aus), `ui_digits_mode()/set()/next()` in ui.cpp.
  Ganze Gramm gilt für g und ml, andere Einheiten bleiben wie gehabt; Ansage dann auch gerundet.
  Profil Einfach setzt `whole=false` (zeigt ohnehin ganze Gramm). Web-JSON: `whole`.
  Sim-Tests: `LPTEST` (3 Stufen), `DIGITSHOT` (Einstellungszeile).
- **Update-Suche „Repository nicht gefunden“:** heißt HTTP 404 auf
  `github.com/<repo>/releases/latest`. Jetzt zweiter Weg über
  `api.github.com/repos/<repo>/releases?per_page=1` (findet auch Vorabversionen); Meldungen
  „Repository nicht gefunden (privat?)“ (API auch 404 → Repository privat oder Name in config.h falsch)
  bzw. „Noch kein Release veröffentlicht“. Die Online-Suche braucht ein **öffentliches** Repository.

### Stand 15.10.2026: Weckwort weckt nicht (Modell vom 15.10.)
- Markus: mit dem neu trainierten Modell (73326 Beispiele, Prüfung 84 %, hallo_waage 96,7 % auf der
  Prüfstimme, 6 Fehl-Weckungen/10 min) ließ sich die Waage **kein einziges Mal** mit „Hallo Waage“ wecken
  (mit v7 1 von 7). Befehle nach Doppeltipp gingen vorher 2 von 3. Firmware-Erkennung ist unverändert
  (geprüft: decide(), Schwellen aus der Modelldatei, sound_busy, voice_hold).
- Die Prozentwerte des Trainings stammen von einer **künstlichen Prüfstimme** und sagen über seine
  Stimme am Waagen-Mikrofon wenig. Mit v7 und seinen zwei alten Aufnahmen (scratchpad `rec/`):
  geglättet 0,93 bzw. 0,66 (Schwelle 0,65) – knapp.
- **Neu in kws_train.py:** nach dem Speichern rechnet `calibrate()` mit der fertigen Datei genau wie die
  Waage (kws_pruefen.stream: alle 200 ms, 3 gemittelt, 2 s Einschwingen) auf den **eigenen
  Prüf-Aufnahmen** (OWN_VAL, 15 %), nah und 20 dB leiser, dazu 10 min fremde Sprache (ohne die
  abgeschnittenen „Hallo Wa…“-Stücke, FRAG). Tabelle Schwelle/weckt/Fehlalarme; gewählt wird die
  höchste Schwelle zwischen 0,45 und 0,70 mit ≤ 3 Fehlalarmen, die fast so oft weckt wie die niedrigste;
  in die Datei geschrieben (`--weckschwelle` gibt sie fest vor). Befehle auf eigenen Prüf-Aufnahmen
  werden auch gezählt. `stream_false_alarms` (torch) entfernt.
- Serien: bei Mehrwort-Klassen (MULTI: hallo_waage, wie_viel, ziel_*) trennt `split_utterances` erst ab
  0,6 s Pause (vorher 0,35 s → „Hallo“ und „Waage“ konnten als zwei Weckwörter zählen). Übersicht zeigt
  die mittlere Länge je Klasse.
- **kws_pruefen.py:** `--ordner` (alle WAVs, Einzelaufnahmen: Spitzenwert + „weckt“, Serien: Anzahl
  Weckungen), `--modell` mehrfach (Modelle vergleichen), `--setze-weckschwelle/--setze-schwelle`
  (Schwelle in fertiger Datei ändern). Kurze WAVs bekommen automatisch 2,6 s Vorlauf.
- **Firmware:** Mikrofonseite zeigt beim Weckwort „hört: Hallo Waage 92 % (weckt ab 65 %)“, grün wenn
  es geweckt hätte; Spitzenwert 2 s gehalten (voice.cpp `s_live_at`). Sim-Test `MICTEST=<wav>`.
- Nächster Schritt: Markus' neue `kws_de.bin` und Aufnahmen-Ordner mit `kws_pruefen.py --ordner`
  prüfen (alt gegen neu) bzw. neu trainieren und die neue Tabelle ansehen.

### Stand 15.10.2026 (vormittags): Messmittelprüfungen speichern und zeichnen, 4 Kalibrierpunkte
- Markus' Prüfungen (Browser): 155 g/T 3 g: Bias −0,34 g, s 0,058 g, Cg 1,73, Cgk −0,23;
  1352 g/T 10 g: Bias +0,21 g, Cg 5,56, Cgk 4,38. Werte nur in 0,1-g-Schritten (Browser nahm `S.g`).
- **msa_store.cpp/.h (neu):** `msa_calc()` (Cg, Cgk, T_min, Spanne) und `msa_save()` →
  `/Waage/Pruefung/JJJJ-MM-TT_HHMM.csv` (bei gleicher Minute `_2` …): Kopfzeilen `Schlüssel;Wert`
  (Datum, Quelle Waage/Browser, Firmware, Sollwert, Toleranz T, n, Mittelwert, Bias, s, Cg, Cgk,
  Ergebnis, T_min, Kalibriert, Kalibrierpunkte), Leerzeile, `Nr;Messwert_g`. Komma als Dezimalzeichen.
- page_msa.cpp nutzt msa_calc/msa_save (einmal, `m_saved`), Ergebnisseite mit Kommas, „≥“ (fehlt im
  Font) → „ab 1,33“, neuer Knopf **Verlauf** → Seite Werteverlauf (lv_line, Band Soll ± 0,1·T, Soll,
  Mittel gestrichelt, Kalibrierdatum und -punkte, Dateiname). Sim-Test `MSATEST` (kalibriert 4 Punkte,
  25 Messungen, Ergebnis, Verlauf, CSV-Inhalt).
- web.cpp: `POST /api/msa {nom,tol,vals}` → msa_save (Quelle Browser); Status hat `gp` (Netto mit 2
  Nachkommastellen, die Browser-Prüfung nimmt jetzt diesen Wert); Einstellungen haben `cal_date`, `cal_pts`.
- page.html P.msa: Startseite mit Karten Kalibrierung (Datum, Alter, Punkte), „Abweichung über dem
  Sollwert“ (Bias je Prüfung über Soll, Balken ± 0,1·T, Dreiecke = Kalibrierpunkte, hohl = nicht fähig)
  und Liste (max. 24 neueste); Prüfbericht mit Kennwerten, Kalibrierung zur Prüfung, Werteverlauf
  (Band, Soll, Mittel, ± 3s, hohle gelbe Punkte außerhalb), Histogramm mit Normalverteilung, Einzelwerte.
  `msaParse` liest auch das alte Browser-CSV (Punkt als Dezimalzeichen). mock.py: `MSA_FILES`, `/api/msa`.
- **scale.cpp:** `CAL_MAX` 3 → 4. Kalibrierdatum `cal_date` (NVS, `scale_cal_date()`).
  **Fehler behoben:** Bei Mehrpunkt-Kalibrierung wurden die gespeicherten Rohwerte absolut mit dem
  beim Einschalten neu gemessenen Nullpunkt verrechnet; hatte sich der Nullpunkt verschoben, lagen die
  oberen Abschnitte um diese Verschiebung daneben. Jetzt relativ zu `cal_zero` (wurde schon immer
  gespeichert, war aber ungenutzt) – alte Kalibrierungen gelten weiter.
- Langzeitmessung 15.10. mit ~2,99 kg: in 28 min stetig von 2991 auf 2884 g (−3,6 %, abklingend).
  Bei 240 g (14.10.) über 23 min stabil auf ±0,1 g. Verdacht: mechanisch (Überlastanschlag/Kontakt
  oder Kriechen gedruckter Teile unter Last nahe Nennlast), nicht Auto-Null (wirkt nur nahe 0).

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
5. **Online-Update auf dem Gerät prüfen** (Stand 12.10.2026 nur im Simulator und per Build geprüft).
   Stürzt es weiter ab: Zeilen aus `/Waage/Absturz.txt` und den seriellen Monitor auswerten
   (Adressen mit `Waage.ino.elf` des Releases, siehe GITHUB.md).

## 7. Hinweise zur Zusammenarbeit

- Kommentare und Oberfläche sind auf Deutsch, Variablennamen englisch.
- Vor dem Ausliefern: `python3 web/build.py`, und die Weboberfläche gegen alle
  Verlinkungen und Hilfsfunktionen prüfen (ein Rundgang über alle Seiten mit
  `window.onerror` hat in der Vergangenheit mehrere eigene Fehler gefunden).
- Beim Ersetzen großer Codeblöcke aufpassen: Es sind schon zweimal ganze Seiten
  versehentlich mitgelöscht worden (Dateiseite, Cocktailkarte).

### Stand 15.10.2026 (mittags): Training mit neuem Skript, Doppelzählung behoben
- Markus' Lauf: eigene Prüf-Aufnahmen weckten 100 % (nah) / 81–88 % (20 dB leiser), Schwelle 0,50,
  2 Fehlalarme; kws_pruefen: Modell vom 14./15.10. und neues Modell wecken beide 66/66 eigene
  Aufnahmen (82–98 %). Das alte Modell hat live aber nie geweckt → Ursache eher Abstand/Pegel im
  Alltag als das Netz selbst. Nächster Schritt: neue Firmware + Modell, Mikrofonseite (Live-%) aus
  normalem Abstand prüfen; Aufnahmen aus 1 m Abstand ergänzen.
- **Fehler behoben:** `glob("*.wav") + glob("*.WAV")` fand unter Windows jede Datei doppelt (Training
  zählte alle eigenen Aufnahmen doppelt, kws_pruefen listete doppelt). Jetzt `K.wav_files()`.
- Prüfmenge der eigenen Aufnahmen jetzt je Datei (crc32 von Name+Klasse < 15 %), nicht je Wort:
  vorher landeten Teile derselben Serie (und durch die Doppelzählung dieselbe Datei) in Training UND
  Prüfung → zu optimistische Werte.
- Kalibrier-Tabelle zusätzlich „30 dB leiser“; kws_pruefen `--leiser <dB>`.
- Markus: Aufnahmen schon aus verschiedenen Abständen/Lautstärken gemacht – meine Abstands-Erklärung
  passt also nicht. Beide Modelle erkennen alle eigenen Aufnahmen (≥ 82 %), live weckte das alte nie.
  Firmware-Pfad geprüft: Aufnahme und Erkennung bekommen dasselbe `pcm`, `voice_rec_session` wird
  beim Verlassen korrekt zurückgezählt (Sim `SESSTEST`). Ursache weiter offen → Daten aus dem Alltag:
- **Mitschnitt (voice.cpp):** solange die Mikrofonseite offen ist (`voice_capture(true/false)`), Ringpuffer
  3 s (PSRAM, `s_cap`), „Versuch“ ab 25 % Weckwort bis < 12 %; gespeichert im Audio-Task
  (`cap_save`) als `/Waage/Sprache/Mitschnitt/wNNN_PP.wav` (geweckt, sofort beim Wecken) bzw.
  `kNNN_PP.wav` (knapp), max. 60 je Einschalten. Mikrofonseite: „Hallo Waage 93 % · weckt ab 65 %“,
  darunter „Mitschnitte: n · letzte Datei“. Sim `MICTEST=<wav>` (`MICMODEL=` anderes Modell): kws.cpp
  94 % ↔ kws_pruefen 93 % auf derselben Datei – PC und Waage rechnen gleich.
- Markus' Lauf ohne Doppelzählung (36 Dateien, 60 Wörter hallo_waage): Prüfung 81 %, eigene Prüf-Aufnahmen
  100 % / 89 % (20 dB) / 44–67 % (30 dB). `kws_pruefen --leiser 25`: bei allen drei Modellen fallen
  a023, a027, a029–a031 und Serien s002/s003 fast auf 0 % → leise Sprache ist die Schwachstelle (passt
  zu „weckt im Alltag nicht“). Rauschboden der Waagen-Aufnahmen gemessen: rms ≈ 29 (−61 dBFS).
- **kws_train.py:** Training legte Rauschen immer relativ zur Sprache an, fester Boden nur −85 dB →
  leise Sprache war im Training viel sauberer als an der Waage. Jetzt fester Rauschboden −64…−57 dBFS
  in jedem Beispiel. Schwellenwahl berücksichtigt auch „30 dB leiser“.
- kws_pruefen `--ordner`: Spalte „Pegel“ (lauteste Stelle in dBFS nach --leiser).

### Stand 15.10.2026 (abends): Online-Update „GitHub nicht erreichbar“
- Markus (1.1.19 und 1.1.20, WLAN da, Repo public): manuelle Suche meldet „GitHub nicht erreichbar“, einmal
  „Download fehlgeschlagen“; die automatische Suche direkt nach dem Start klappt. Updates zuletzt per
  Browser (/update). „GitHub nicht erreichbar“ = `http.GET()` < 0, also TLS/Verbindung.
- Ursache (sehr wahrscheinlich): Arduino-Core baut mbedTLS mit `CONFIG_MBEDTLS_INTERNAL_MEM_ALLOC`;
  die Datensatzpuffer brauchen je ~16,7 KB am Stück im internen RAM. Nach einer Weile Betrieb ist der
  größte freie interne Block ~15 KB (Absturz.txt „37/10/15 KB“) → Handshake scheitert. Nach dem Start
  ist genug da.
- **Fix:** update_online.cpp `tls_use_psram()` (beim ersten `start()`): `mbedtls_platform_set_calloc_free`
  mit eigenem Allocator – ab 2 KB aus dem PSRAM, kleinere intern, sonst Ausweichen. Symbol im Core
  vorhanden (`mbedtls_calloc` ruft über `mbedtls_calloc_func`).
- Fehlergrund sichtbar: `upd_detail()` „HTTP <Code> · Block <KB>“ (Firmware-Seite statt Repo-Zeile,
  Web-Firmware-Karte hinter dem Repo, JSON `detail`).
- Lauf mit festem Rauschboden (15.10. abends): Prüfung 76 %, Schwelle 0,60; bei −25 dB weckt das neue
  Modell 29/33 (vorher 27–28), leise a030/a031 deutlich besser; a023 (−60 dBFS = Rauschboden) unmöglich.
- kws_train legt das bisherige kws_de.bin vor dem Überschreiben als `modelle/modell_<mtime>.bin` ab und
  vergleicht am Ende alle Modelle (`kws_pruefen.all_models()`: *.bin + modelle/*.bin) auf
  Aufnahmen/hallo_waage (+ Mitschnitt/) bei −25 dB, nur Übersicht; `--kein-vergleich`.
  kws_pruefen `--ordner` ohne `--modell` = alle Modelle; Übersicht (Treffer, mittlere Sicherheit,
  Serien-Weckungen, Schwelle, * = bestes); `--nur-uebersicht`.


### Stand 15.10.2026 (spät): Ansagen für alle Sprachfunktionen, alle Wörter prüfen, Mitschnitte sortieren
- Waage „stumm“ bei ja/akku/uhr: `sound_say` spricht nur, wenn ALLE Wörter im Paket oder in
  `/Waage/Stimme/_Zusatz` (EN `_Zusatz-EN`) liegen, sonst Piep. Wahrscheinlich fehlte _Zusatz auf der Karte.
  Neu: `sound_missing()` → Settings-JSON `voice_missing` → Hinweis-Kasten unter „Stimme“ (Web).
- Neue Sätze (stimme_erzeugen.py ZUSATZ/ZUSATZ_EN, WORD_EN in sound.cpp): bereit, weiter, zurueck,
  geht_nicht, lautstaerke (+Zahl+prozent, `sound_say_volume`), akku_leer, timer_ab, zeit_unbek,
  schaltet_aus. Verwendet: voice.cpp (weiter/zurück/stopp ohne Ziel/speichern+nochmal fehlgeschlagen/
  lauter/leiser/Uhrzeit unbekannt), ui.cpp start_finish „bereit“, tools.cpp Timer, ui_power.cpp Akku leer
  (jede 5. Warnung) und Auto-Aus-Warnung. Alle nur mit `speak_ev`. Generiert: _Zusatz (piper eva_k),
  Roboter, _Zusatz-EN, Roboter-EN (espeak). Lena/KI-Packs nehmen _Zusatz. `stimme_erzeugen.py --liste / --nur`.
- Mitschnitt jetzt auch Befehle: `bNNN_<cls>_PP.wav` und nicht verstanden `nNNN.wav` (voice.cpp s_cap_req 3/4).
- kws_pruefen `--alle-woerter AUFNAHMEN [--nur-pruefdateien] [--leiser N]`: `check_all_words()` je Klasse
  Treffer je Modell (Gerätelogik `decide_command`, Befehle ohne Weckwort davor), schlechteste zuerst,
  „verstand stattdessen“. Trainingsende ruft es mit −20 dB, nur Holdout (statt nur hallo_waage).
- Neu `kws_mitschnitt.py`: holen (`/zip?dir=/Waage/Sprache/Mitschnitt`, Dedupe per SHA1 gegen Mitschnitt/,
  erledigt/, Aufnahmen/ und mitschnitt_bekannt.csv) + sortieren (alle Modelle, zwei Sichten: roh und mit
  2,6 s Vorlauf; Kandidaten nach Präfix; sicher = Weckschwelle / Befehl ≥0,6 / _unbekannt ≥0,7; einig →
  auto; sonst abspielen + fragen). Speichert nur das gemeinte Wort (`cut_word`, kws_train
  `split_utterances(bounds=True)`, Segment, das vor dem Modell-Peak endet) als Aufnahmen/<cls>/m_<name>.wav.

### Stand 15.10.2026 (nachts): Ansagen-Texte überarbeitet, Restmenge aus _Zusatz, längeres Befehlsfenster
- `_Zusatz` hat jetzt 22 Dateien: zusätzlich `noch`, `zu_viel`, `milliliter` (EN `to_go`, `too_much`, `milliliters`),
  Texte freundlicher ("Das habe ich nicht verstanden. Sag Hilfe.", Hilfe nennt alle Befehle). Neu erzeugt:
  _Zusatz (piper eva_k), _Zusatz-EN, Roboter, Roboter-EN (espeak).
- sound.cpp: `sound_speak_rest` nutzt `have_word` (Paket oder _Zusatz) statt nur Paket → Restmenge auch mit
  Paketen ohne noch/zu_viel. SAY_WORDS/WORD_EN um noch/zu_viel/milliliter ergänzt (sound_missing zeigt sie).
- voice.cpp: nach dem Ende jeder Ansage im Wachzustand mindestens `AWAKE_AFTER_SAY_MS` 4,5 s Befehlsfenster;
  Wachzustand läuft nicht ab, solange die Waage spricht → längere Charakter-"Ja?" möglich.
- Markus' Stimmenpakete (16, u. a. Rick DE, Drachenlord, KI-AnimeGirl/Männlich/Weiblich): Liste der 22 neuen
  Dateien mit neutralen Texten, Rick-Sprüchen und KI-AnimeGirl-Vorschlägen in seiner Stimmenpakete_Uebersicht.md.
- kws_mitschnitt: automatisch nur, wenn sichere Modelle einig UND Mehrheit aller Modelle dasselbe Wort vorn
  (vorher reichte ein einzelnes sicheres Modell, z. B. w002_53 → _unbekannt gegen zwei hallo_waage).
  Training 15.10. abends (Mitschnitte drin): neues kws_de bestes Modell für hallo_waage (86 % bei −20 dB),
  gesamt 82 % = gleichauf mit 15-10V1; schwach: nochmal (→ stopp), ziel_250/300, akku leise ("-"), _unbekannt zu wenige (12).
- kws_mitschnitt `holen` holt jetzt auch `/zip?dir=/Waage/Sprache/Aufnahmen` und legt neue Dateien (SHA1-Dedupe
  gegen Aufnahmen/) nach Aufnahmen/<wort>/ (Namenskonflikt → _<sha6>); `--ohne-aufnahmen` schaltet es ab.

### Stand 15.10.2026 (spät nachts): Waage hängt nach ZIP-Download (kws_mitschnitt)
- Markus: nach dem Holen per Skript dreht die UI, Ausschalten geht nicht, SD nicht erreichbar, Mic/Waage 5 s
  verzögert; half nur Neu-Flashen (= Neustart). Ursache nicht bewiesen (kein Log). Vermutung: SD fällt unter
  WLAN-Last aus (interner RAM knapp), storage_loop versucht alle 3 s neu einzuhängen und blockiert dabei den
  100-ms-UI-Timer → Anzeige ruckelt im Sekundentakt; PWR-Taste zählte Takte (+100 je Aufruf) statt Zeit →
  3 s Halten erreichte nie OFF_HOLD_MS.
- Fixes: ui_power zählt echte Zeit (PWRTEST im Sim: 3 Takte à 2 s schalten ab); hal_board Task "pwr"
  (Kern 1, Prio 10): PWR 10 s halten → Akku-Halt aus, sonst esp_restart (unabhängig von Anzeige/SD);
  storage: Neu-Einhängen mit Backoff 3/6/12/…/60 s + Log mit freiem internem RAM; h_zip: Puffer im PSRAM,
  wartet bei < 16 KB internem RAM, Log "ZIP fertig … intern frei min."; kws_mitschnitt 5 s Pause zwischen den ZIPs.
- Offen: serielles Log beim nächsten Mal ("SD: …", "ZIP fertig …").

### Stand 15.10.2026 (nachts): Whisper als Schiedsrichter für Mitschnitte
- Neu `Sprachmodell/kws_whisper.py`: faster-whisper (CPU, int8, language=de, word_timestamps), Pegel auf
  Spitze 0,7 angehoben (max +30 dB), Halluzinationsfilter (Untertitel/ZDF …, no_speech>0,6 & logprob<−0,8).
  `classify(text, cand)`: norm() (Umlaute, 1.000→1000), Weckwort fuzzy (hallo≥0,75, waage≥0,85, nicht „Wagen“),
  Befehle: letzte Fundstelle zählt, längere Phrase bei gleicher Stelle (250 vor 50), Zahlen nur exakt,
  sonst Sprache → _unbekannt, leer → None. Selbsttest `python kws_whisper.py --test` (29/29).
- kws_mitschnitt: Whisper standardmäßig an, wenn installiert (`--whisper small|medium|large-v3-turbo`,
  `--ohne-whisper`). Auto: Whisper-Ergebnis, solange das Modell-Urteil (einig+Mehrheit) nicht widerspricht;
  Whisper leer → altes Modell-Urteil; Widerspruch → nachfragen (Vorschlag Whisper). Schnitt mit Whisper-Wortende.
  Mit echtem Whisper-Modell hier NICHT getestet (HuggingFace gesperrt); Logik mit gefälschtem transcribe getestet.
- Markus: Whisper auf den Waagen-Mitschnitten schlechter als die eigenen Modelle (Mikrofon leise/verrauscht).
  → Whisper jetzt aus (opt-in `--whisper [modell]`); entscheidet nie allein: eigene Modelle (einig+Mehrheit) zuerst,
  Whisper sortiert nur, wenn es mit der Mehrheits-Tendenz der (unsicheren) Modelle übereinstimmt; sonst nur Hinweis.
  Neu `kws_mitschnitt.py zuruecknehmen [--wie whisper|auto|hand|alle]`: löscht m_-Kopien, holt Originale aus
  erledigt/ zurück nach Mitschnitt/, entfernt Zeilen aus mitschnitt_bekannt.csv.

### Stand 15.10.2026 (nachts): alle Sprach-Ansagen geprüft (Sim VOICEALL)
- voice.cpp: Befehlsausführung in `run_cmd(c)` ausgelagert; Sim-Hook `voice_sim_cmd(cls)` ("!" = geweckt,
  "?" = nicht verstanden). Host-Test VOICEALL (Baseline host/t_VOICEALL.txt): jeder Befehl + Ereignisse
  (bereit, ja, nicht_verst, timer_ab, akku_leer, schaltet_aus, Rest/zu viel, laedt, zeit_unbek).
- Gefunden und behoben:
  1. „Akku … wird geladen“ kam nie: hal_battery_state erkannte Laden nur ab 4,14 V. Neu: Sprung > +0,06 V in 10 s
     (Kabel rein) oder 10-min-Fenster in beiden Hälften > +6 mV und letzte 2,5 min > +2 mV (Erholung nach Last
     zählt nicht); Ende bei Sprung < −0,05 V oder fallend. Offline mit 7 Spannungsverläufen geprüft, NICHT am Gerät.
  2. Uhrzeit: „Uhrzeit unbekannt“ kam nie (hal_time immer gültig) → prüft jetzt hal_now (Jahr ≥ 2024).
  3. Profil ≠ Einfach: Sprachbefehl „250 Gramm“ öffnete Ziel-Seite ohne Ansage → sagt jetzt „Ziel … einfüllen“;
     „Wie viel?“ auf der Ziel-Seite sagt jetzt die Restmenge („noch …“ / „… zu viel“) statt des Gewichts.
- Hinweis: bereit, timer_ab, akku_leer, schaltet_aus nur mit Ansage „Sprüche“ (speak_ev); Antworten auf
  Sprachbefehle immer. TGTTEST-Baseline um die neue Ziel-Ansage ergänzt.

### Stand 16.10.2026: sauberer Neustart der Mitschnitt-Zuordnung
- Markus traut den Mitschnitt-Zuordnungen nicht (Zahlen 50/150/250 verwechselt, hallo_waage: 97 von 134 Dateien
  sind Mitschnitte). Neu in kws_mitschnitt: `neustart` (zuruecknehmen alle + übrige m_*-Dateien aus Aufnahmen
  zurück nach Mitschnitt/, Original aus erledigt/ bevorzugt), `sortieren --nur-neuestes` (nur kws_de.bin),
  strengere Sicherheit: Abstand ≥ 0,25 zum zweitbesten Wort (MARGIN); bei b…_ziel_X-Dateien muss das Modell
  dieselbe Zahl sagen wie die Waage, sonst nachfragen.
- Geprüft (Cloud, eigene Aufnahmen ziel_50/150/250, kws_de.bin vom 15.10.): 0 dB 95 %, −20 dB 84 %
  (250 oft „nichts“). Alternative Ziel-Entscheidungsregel (längere enthaltene Zahl bevorzugen) brachte dort
  nichts → Firmware-Regel unverändert. Beobachtet: Aufnahmen/ziel_150/m_b001_ziel_250_47.wav (Waage: 250).
- **Ursache der schlechten Mitschnitte gefunden (16.10.):** voice_task las selbst per readBytes vom Mikrofon und
  rechnete dazwischen (Merkmale + DS-CNN alle 200 ms, cap_save); der I2S-DMA-Puffer (ESP_I2S fest 6×240 Frames
  = 90 ms) lief über → fehlende Stücke. Messung an Markus' Dateien: Knackser (LPC-Residuum) in Mitschnitten
  genau auf 200-ms-Raster (Position mod 3200 = 0), in Aufnahmen keine; „Hallo Waage“ Median 0,85 s
  (Mitschnitt) vs. 1,30 s (Aufnahmen). Pegel, Rauschboden, Spektrum sonst gleich.
  Fix voice.cpp: Lese-Task "MicRd" (Prio 6, Kern 0) → StreamBuffer 1 s im PSRAM (nur ganze Stücke, Zähler
  s_rd_lost), voice_task liest daraus (xStreamBufferReceive 30 ms). Log alle 60 s „Mikrofon: Puffer max … ms,
  verworfen … ms“ (sollte 0 sein). Am Gerät NICHT getestet.
  kws_mitschnitt `neustart` schiebt alte Mitschnitte nach Mitschnitt/alt_abgehackt (nicht mehr sortieren;
  `--alte-sortieren` für das alte Verhalten).

### Stand 16.10.2026 (vormittags): 1.1.27 startet ständig neu (Watchdog IDLE0, Task Voice)
- Markus' Absturz.txt: 1.1.27 alle 15–25 s „Task watchdog … IDLE0 (CPU 0)“, Task Voice. addr2line mit dem lokalen
  Build passt: voice_task → kws_feed → kws_run (bzw. push_samples). Ursache: Mit dem Lese-Task geht kein Ton mehr
  verloren, das Netz muss jetzt wirklich alle 200 ms Ton rechnen. Es brauchte aber etwa 200 ms je Auswertung
  (aus den Mitschnitten geschätzt: ~1/3 Ton fehlte bei 90 ms DMA-Puffer → Rechenzeit ≈ 200 ms) → Voice (Prio 3,
  Kern 0) blockierte nie mehr, IDLE0 kam nicht dran. Früher bremste der Tonverlust die Last von selbst.
- Warum so langsam: punktweise Faltungen liefen je Position einmal über alle Gewichte (bis 36 KB, Daten-Cache
  32 KB) → jede Position holte die Gewichte neu aus dem PSRAM.
- **Fix kws.cpp:** Rechenkern `tile_mul` (8 Positionen gleichzeitig, Eingänge verschränkt in `s_tile`, 4 KB intern,
  Gewichtszeile einmal je Kachel gelesen, 8 Summen) für punktweise Schichten und die erste Schicht; Tiefen-
  Faltung summiert im Stapel (`DW_MAX` 128) statt im PSRAM. Ergebnis bitgleich zum alten Code (Host-Vergleich
  30 Zufallseingaben + kws_feed, KWSTEST identisch), Host 2,6× schneller; am Gerät erwartet ~40–60 ms.
  Ohne s_tile (kein interner RAM) alter Weg.
- **Sicherung voice.cpp:** Lastabwurf: Rückstand im Lese-Puffer > 400 ms → `kws_skip(true)` (Merkmale laufen
  weiter, Netz setzt aus) bis < 100 ms. Voice gibt den Kern spätestens alle 250 ms für 2 Ticks frei.
  Log alle 60 s: „Mikrofon: Puffer max …, verworfen …; Netz X ms (max Y), N Auswertungen, K ausgelassen“.
  `kws_stats()`. Weboberfläche: „Sprachmodell: … · 45 ms / 200 ms“ (letzte Auswertung, nur wenn aktiv).
- Ältere Einträge (1.1.20–1.1.26: BTU/BTC_TASK `vQueueDelete` assert, loopTask LoadProhibited in LVGL) lassen
  sich mit dem lokalen Build nicht auflösen (andere Adressen) → ELF der jeweiligen Release nötig.

### Stand 16.10.2026 (mittags): Rückmeldung zu 1.1.27/1.1.28
- Absturz.txt: 159 Einträge 1.1.27 (Watchdog IDLE0, Task Voice, alle ~11 s nach 9 s Laufzeit). Ausschalten der
  Sprachsteuerung ging nicht: Weboberfläche und Setup ignorierten den Schalter, solange `voice_starting()` – und
  das war bis zum Neustart immer so. Markus musste per Arduino-IDE flashen. 1.1.28 (Netz-Beschleunigung) läuft,
  UI leidet nur leicht.
- **Sicherer Start** (crash.cpp, `crash_safe_mode()`): RTC-Zähler `s_quick` für Abstürze < 120 s nach dem Start.
  Letzter Absturz im Task Voice/MicRd < 120 s nach Start oder 3 schnelle Abstürze in Folge → `g_set.voice_on =
  false` (nur im RAM; gespeichert erst mit der nächsten Einstellungsänderung), Setup-Zeile „aus (Absturz)“,
  crash_text mit „– Sprachsteuerung vorerst aus“. Sim: `SAFEMODE=1` (Bild der Ton-Seite).
- **Ausschalten geht immer** (page_setup Fall 25, web.cpp voice_on): nur das Einschalten wartet auf `voice_starting()`.
- **Update „Zu wenig Speicher“** (Foto, 1.1.28 lokal gebaut, Online suchen): kam von `xTaskCreatePinnedToCore`
  (12 KB Stapel intern) direkt nach dem WLAN-Start. Jetzt Task vor `net_connect_start`, scheitert er, versucht
  `upd_loop` es 8 s lang alle 250 ms (`try_create`, Log mit freiem/größtem Block).
- **Voice- und MicRd-Stapel im PSRAM** (`task_psram`, `xTaskCreatePinnedToCoreWithCaps`, CONFIG_SPIRAM_ALLOW_STACK_
  EXTERNAL_MEMORY=1): spart ~11 KB internen RAM. Beide Tasks schreiben nie in den Flash (nur SD/I2S). Fallback intern.
- **Bluetooth-Absturz** (BTU/BTC_TASK `vQueueDelete`, laut Markus immer beim Antippen von Bluetooth): Bluedroid
  bekommt seinen internen RAM nicht. `ble_kbd_begin()` prüft jetzt vorher (frei ≥ 90 KB, Block ≥ 40 KB, Schätzwerte;
  in allen drei Abstürzen 68–73 KB / 30 KB) und lehnt ab: Chip „Bluetooth: zu wenig Speicher“ (Zählen, BT-Seite,
  dort neuer Versuch alle 500 ms). Ob Bluetooth mit WLAN + Sprache überhaupt noch passt, ist offen.
- **Download hängt** (1.1.28, Absturz.txt laden → Anzeige steht, Ausschalter reagiert nicht): `h_file_get` nutzte
  `streamFile` (SD die ganze Zeit gesperrt, kein Blick auf den internen RAM – dieselbe Ursache wie beim ZIP am
  15.10.). Jetzt Teil für Teil (4 KB PSRAM-Puffer, SD nur je Teil gesperrt, unter 16 KB frei kurz warten,
  Abbruch bei Schreibfehler/Verbindung weg/120 s).
- Training mit sauberen Daten (16.10.): 74 % Prüfung, eigene Prüfdateien 87 % (20 dB leiser), schwach ziel_300,
  nochmal, lauter, akku. Nächster Schritt: mit 1.1.29 viel sprechen, Mitschnitte sammeln (ohne Knackser seit dem
  Lese-Task), dann sortieren und neu trainieren.

### Stand 20.10.2026: Party-Rückmeldung (Trinkspiel), Bluetooth-Abstürze, UI-Wächter
- **Build-Umgebung repariert:** /root/.arduino15 war nach der Sitzungspause leer (Index, builtin-Discovery fehlten,
  Download vom Proxy verboten). Abhilfe: `package_esp32_index.json` = /home/claude/ardu/idx.json, leere
  `package_index.json`/`library_index.json`, in /root/Arduino/hardware/espressif/esp32/platform.txt die
  `pluggable_discovery.required.*` auskommentiert. check.sh läuft wieder (> 2 min: im Hintergrund starten).
- **Absturz.txt mit 0.0.0** (Markus' IDE-Builds, anderer ESP32-Core: ipc-Stapel 1108 statt 140 frei → Bibliotheks-
  adressen passen nicht, Sketch-Adressen ungefähr): Stand 16.10. 12:02 nachgebaut (check_z.sh → outz/). Alle sechs
  Einträge landen bei Bluetooth: `ble_kbd_begin` (Schreiben an 0x11c = `server->setCallbacks` mit server NULL),
  `page_bluetooth_create`/`bt_done_cb`. → ble_kbd: `BLEDevice::getInitialized()` und NULL-Prüfung, bei Fehlschlag
  `s_failed` (kein neuer Versuch), Chip wie „zu wenig Speicher“.
- **Einfrieren auf der Party** (Trinkspiel, 3. Runde, Taste half nicht, kein Eintrag): Ursache unbekannt.
  **UI-Wächter** (Waage.ino): `esp_task_wdt_reconfigure` 15 s (IDLE0 wie bisher), `enableLoopWDT()`; hängt loop()
  > 15 s → Neustart mit Bericht (Task loopTask + Adressen). Lange Web-Antworten füttern über `hit()` (web.cpp,
  jetzt mit `feedLoopWDT()`; CSV-Export, Hochladen ergänzt). Notabschaltung: PWR 10 s halten.
- **Langzeitmessung:** während `lt_running()` keine Topferkennung und keine Minus-Tara auf der Startseite (ui.cpp).
- **Trinkspiel-Bestenliste:** vorher Summe der Abweichungen (KO-Aus in Runde 1 mit 16 ml stand vorn). Jetzt
  Ø Abweichung je Schluck, erst ab 3 gewerteten Schlucken (`t_nsip`, `SCORE_MIN_SIPS`), Spielname `Trinkspiel2`
  (alte Summen bleiben in der Datei, zählen nicht). Web gleich (`nsip`, Hinweis „Ø je Schluck (ab 3)“, `PL.board(game,titel,hint)`).
- **Spieler:** `PLAYER_MAX` 8 → 32. Liste voll: Ton (Gerät) bzw. Toast „Spielerliste voll (höchstens 32)“ (Web
  prüft jetzt die Antwort von /api/player_add, vorher landeten Namen nur lokal und verschwanden).
- **Ansagen flüssiger:** `play_wav` lädt das Wort (≤ 512 KB) in den PSRAM und schneidet Stille ab (Schwelle 3 %
  der Spitze, mind. 150), es bleiben 15 ms davor/45 ms danach (Pakete hatten ~100/130–180 ms). Größere Dateien
  wie früher gestreamt.
- **„ein Uhr vierzig“** statt „eins Uhr“ (`sound_say_time`, nur Deutsch).
- **QR-Code:** `#warning` in page_setup.cpp, wenn `LV_USE_QRCODE` 0 ist (IDE zeigt es beim Kompilieren).
- README: neues Party-Foto `docs/images/real-party.jpg` (Markus’ Wahl IMG20261017202609 mit Bechern und Stern, ganzes Hochformat
  auf 1000 px Höhe mittig, Seiten mit unscharfer, abgedunkelter Vergrößerung desselben Bilds aufgefüllt).

### Stand 20.10.2026 (abends): Training mit Party-Mitschnitten
- Markus' Lauf: 61 Mitschnitte (27 automatisch, 11 von Hand), Modell 2026-10-20: Prüfung 74 %, eigene Prüfdateien
  (20 dB leiser) 81 % vs. 83 % für 2026-10-16_1111 – Unterschied 2 von 110 Dateien, im Rauschen. Weckwort-Prüfung
  jetzt 13–14 Dateien (neue, schwerere Mitschnitte); alle Modelle verlieren darauf. Fehlalarme Befehle 79 → 63.
- Fehler gefunden: `b001_nochmal_57.wav` wurde automatisch nach `wie_viel` sortiert (Modell 76 %), obwohl die Waage
  „nochmal“ verstanden hatte. Die Prüfung Waage ↔ Modell galt nur für ziel_*. Jetzt für alle `b…`-Dateien:
  uneinig → nachfragen (kws_mitschnitt.py, LIESMICH).

### Stand 22.10.2026: Auto-Speichern beim Nachlegen, Ansage 179,96 g
- **Auto-Speichern** (`autosave_check`, ui.cpp): speicherte nur einmal pro Auflegen (`as_done` bis die Waage leer
  war), angesagt wurde dagegen jede Änderung. Jetzt wie die Ansage: ändert sich das Gewicht um mehr als
  `speak_change_g` (1 g bzw. 0,5 %) gegenüber dem zuletzt gespeicherten (`as_saved_g`), wird nach 2 s Ruhe wieder
  gespeichert; nur kurzer Ausschlag zurück zum alten Wert = nicht doppelt (`as_changed`). Sim `AS2TEST`
  (200 → 350 → Stoß 360/350 → 120 g: 199,7 / 349,8 / 119,8 gespeichert), Baseline t_AS2TEST.txt.
- **Ansage „179 komma eins null“ statt 180** (`build_weight`, sound.cpp): Ganzzahl wurde abgeschnitten und nur die
  Nachkommastelle gerundet (179,96 → 179 + lroundf(9,6) = 10). Jetzt erst auf die Stellen runden, dann teilen.

### Stand 26.10.2026: Sprachsteuerung verständlicher (was kann man sagen?)
- **Arbeitsumgebung neu aufgebaut** (Sandbox war leer): Quellen aus /mnt/user-data/outputs/Waage.zip, Waveshare-Treiber
  und lv_conf.h aus `git clone https://github.com/18Markus1984/Esp_Scale` (/home/claude/ardu/repo → drivers/),
  Core 3.1.1 + Libs + Toolchain + esptool von GitHub (setup.sh), LVGL 8.3.10 (git) + bogde/HX711 nach
  /root/Arduino/libraries, `platform.local.txt` mit runtime.tools.*-Pfaden, Discovery in platform.txt auskommentiert,
  universal-ctags (apt) + Prototypen-Einfügen in check.sh. Host-Sim: Testumgebung/ (make, `VHELP=1 ./sim de|en`).
- **Neu `voice_help.cpp/.h`:**
  - Nach dem Weckwort (auch Doppeltipp) deckender Hinweis auf `lv_layer_top()` (nicht anklickbar): Mikrofon,
    „Ich höre zu …“, „Sag zum Beispiel:“ + 3 Beispiele je nach Lage (Ziel läuft: Wie viel?/Stopp/Tara; Knopf aus
    `VOICE_NEXT_LABELS` auf der Seite: Weiter/Wie viel?/Zurück; sonst Tara/Wie viel?/250 Gramm), „„Hilfe“ zeigt alle
    Befehle“. Nach einem Befehl 1,3 s Haken + verstandener Befehl. Ersetzt den Toast „Ich höre zu …“.
    Auf der Mikrofonseite aus (`voice_help_block`). Takt: `voice_help_tick()` in home_update_cb nach voice_loop.
  - Befehl „Hilfe“: Liste aller Befehle über der aktuellen Seite (bleibt erhalten), „Schließen“ oder nach 45 s zu.
  - Setup → Ton → **Befehle** (Kennung 28, „Liste ›“): dieselbe Liste als Seite (`page_voice_cmds_create`).
  - `ui_has_button()` (ui.cpp); „Weiter“-Knopfliste jetzt `VOICE_NEXT_LABELS` (voice_help.cpp).
  - Sim: `voice_sim_cmd("!")` setzt jetzt auch `s_awake`, Befehle setzen `s_last` (Bestätigung sichtbar).
- **Weboberfläche:** Einstellungen → Ton: Knopf „Alle Sprachbefehle“ klappt dieselbe Liste auf (`VCMDS`), EN-Texte im
  EN-Wörterbuch. Englische Anführungszeichen am Gerät wie bisher “…“ (Schrift hat kein ”).
