# Waage – Schritt 1: Oberfläche

Getestete Umgebung: Arduino IDE 2.2.1, esp32 by Espressif 3.1.1, LVGL 8.3.10 (aus dem Waveshare-Paket).

## Wichtig vor dem ersten Kompilieren: LVGL-Speicher

Die Waveshare-Konfiguration gibt LVGL nur 48 KB Arbeitsspeicher, das reicht für
die vielen Seiten nicht. In `libraries/lvgl/src/lv_conf.h` diese Zeile ändern:

    #define LV_MEM_CUSTOM 1

Dann nutzt LVGL den normalen Heap des ESP32. Ohne die Änderung bricht das
Kompilieren mit einem Hinweis ab.

Für den QR-Code (System → Weboberfläche) in derselben Datei außerdem:

    #define LV_USE_QRCODE 1


## Einrichten

1. Diesen Ordner `Waage` in deinen Sketchbook-Ordner legen (z. B. `C:\Waage\Waage\`).
2. Aus der Waveshare-Demo `LVGL_Arduino` diese Dateien in denselben Ordner kopieren
   (jeweils `.h` und `.cpp` bzw. `.c`):
   - BAT_Driver
   - Display_SPD2010
   - esp_lcd_spd2010
   - Gyro_QMI8658
   - I2C_Driver
   - LVGL_Driver
   - PWR_Key
   - RTC_PCF85063
   - SD_Card
   - TCA9554PWR
   - Touch_SPD2010 (nur die `.h` – die `Touch_SPD2010.cpp` liegt schon hier, mit
     Längenprüfungen gegen Abstürze; nicht mit der Waveshare-Fassung überschreiben)
3. Nicht kopieren: `LVGL_Arduino.ino`, `LVGL_Example.*`, `LVGL_Music.*`,
   `MIC_MSM.*`, `Audio_PCM5101.*`, `Wireless.*`
4. `Waage.ino` öffnen, Board-Einstellungen wie bei der Demo, hochladen.

## Dateien

| Datei | Inhalt |
|---|---|
| `Waage.ino` | Start, Treiber-Task für IMU, Uhr, Akku |
| `config.h` | Simulation an/aus, Messbereich, Lagecheck-Grenzen |
| `hal.h` / `hal_board.cpp` | Zugriff auf Sensoren (IMU, RTC, Akku, BOOT-Taste) |
| `scale.h` / `scale.cpp` | Waagen-Logik: Tara, Stabilität, Überlast, Simulation |
| `ui.h` / `ui.cpp` | Lagecheck, Wiegeseite, Launcher, Navigation |
| `ui_pages.*` | Seitenliste, Wasserwaage, Platzhalter |
| `page_ziel.cpp` | Modus Ziel (Parkpiepser) – Vorlage für neue Seiten |
| `page_portion.cpp` | Modus Portionieren: Teig in gleiche Stücke teilen |
| `page_timer.cpp` | Küchentimer (Übersicht und Einstellen) |
| `page_tassen.cpp` | Tassen & Löffel (US-Rezepte) |
| `page_mix.cpp` | 2K mischen mit Topfzeit |
| `page_langzeit.cpp` | Langzeitmessung mit Kurve |
| `page_muenzen.cpp` | Münzzähler und Kassensturz |
| `page_blind.cpp` | Spiel Blindgießen |
| `page_halb.cpp` | Spiel Halbe-Halbe (in der Mitte teilen) |
| `tools.*` | Hintergrunddienste: Küchentimer, Langzeitmessung |
| `page_zaehlen.cpp` | Modus Zählen: Referenz lernen, zählen, per Bluetooth senden |
| `page_porto.cpp` | Modus Porto: Briefklasse und Preis (Stand 2026) |
| `ble_kbd.h` / `ble_kbd.cpp` | Bluetooth-Tastatur (tippt Zahl + Enter am PC) |
| `page_toepfe.cpp` | Töpfe: Liste, Anlegen, Konflikt, Bearbeiten |
| `page_rezept.cpp` | Rezepte: Auswahl, Vorschau, Schritte, Fertig |
| `page_protokoll.cpp` | Protokoll: heute und ältere Tage |
| `page_setup.cpp` | Setup: Auto-Tara, Uhrzeit, Datum, WLAN |
| `page_kalib.cpp` | Kalibrierung in 3 Schritten |
| `sound.*` | Töne über den Lautsprecher, Parkpiepser |
| `ui_power.*` | PWR-Taste: Standby, Ausschalten |
| `page_spiel.cpp` | Schätzspiel |
| `page_spule.cpp` | Filament-Restmenge |
| `page_trink.cpp` | Trinkspiel (Schluckgröße schätzen) |
| `page_cocktail.cpp` | Cocktails in ml mixen |
| `page_mic.cpp` | Mikrofon-Pegeltest |
| `mic.*` | Mikrofon (I2S-Port 1) |
| `voice.*`, `kws.*` | Sprachbefehle „Hallo Waage“ (eigenes Netz, Modell von der SD-Karte) |
| `ui_text.*` | Texteingabe über den Buchstabenring |
| `storage.*` | SD-Karte (Ordner, Lesen, Schreiben) |
| `data.*` | Dateiformate für Töpfe, Protokoll, Rezepte |
| `settings.*` | Einstellungen im internen Flash |
| `net.*` | WLAN und Uhrzeit per NTP |
| `web.*` | Webserver und Endpunkte |
| `web_page.h` | eingebettete Webseite (erzeugt aus `web/page.html`) |
| `ui_widgets.*` | Kurvenliste, Libelle |
| `ui_theme.*` | Farben, Schriften, Buttons, Chips, Ring |
| `i18n.*`, `i18n_en.cpp` | Sprache Deutsch/English: `T()` und Übersetzungstabelle |
| `font_sg_*.c` | Space Grotesk mit Umlauten (14/18/24/34 px, 80 px nur Ziffern) |

## Bedienung

- Start: Lagecheck. Gerade → automatisch weiter, schief → „Trotzdem weiter“.
- Wischen links/rechts: Wiegen ↔ Modi ↔ System.
- Kurvenliste: hoch/runter scrollen, anderen Eintrag antippen = in die Mitte holen,
  markierten Eintrag antippen = öffnen. Nach rechts wischen = zurück.
- BOOT-Taste: Tara.

## Neue Seite hinzufügen

1. Neue Datei `page_xxx.cpp` anlegen (Vorlage: `page_ziel.cpp`).
2. In `ui_pages.h` die Funktion `lv_obj_t *page_xxx_create();` eintragen.
3. In `ui.cpp` in der Tabelle `MODI_PAGES` bzw. `SYSTEM_PAGES` das passende
   `NULL` durch `page_xxx_create` ersetzen. `NULL` zeigt den Platzhalter.

Regeln:
- Zustand der Seite als `static`-Variablen in ihrer eigenen Datei.
- Timer immer im `LV_EVENT_DELETE` des Screens löschen.
- Mehrere Schritte in einem Modus: `ui_switch_page(page_schritt2_create())`.
- Nach rechts wischen führt immer zurück zur Startseite.
- Keine waagerecht scrollbaren Inhalte auf Seiten (würde das Zurückwischen blockieren).

## SD-Karte

Karte mit FAT32 formatieren. Beim Start legt die Waage diese Ordner an:

    /Waage/Protokoll/   2026-09-18.txt   eine Datei pro Tag
    /Waage/Toepfe/      toepfe.txt       alle Töpfe
    /Waage/Rezepte/     *.txt            ein Rezept pro Datei
    /Waage/Porto/                        (später)
    /Waage/Sounds/                       (später)
    /Waage/Stimme/      *.wav            Sprachbausteine für die Ansage
    /Waage/Spulen/      spulen.txt       Leerspulen (Name;Gewicht)
    /Waage/Spiel/       spiele.txt       Ergebnisse des Schätzspiels
    /Waage/Messung/     *.csv            Langzeitmessungen (Uhrzeit;Minuten;Gewicht_g)

Alle Dateien sind Text mit `;` als Trennzeichen und Komma als Dezimalzeichen,
lassen sich also mit Excel öffnen und am PC bearbeiten.

Protokoll-Zeile:  `14:32:05;1234,5;g;Großer Topf`
Topf-Zeile:       `Großer Topf;1840,2;0;2026-09-12`  (Name;Gewicht;Farbe 0-3;angelegt)

Rezept (z. B. `pfannkuchen.txt`, ohne Umlaute im Dateinamen):

    name=Pfannkuchen
    portionen=2
    Mehl;250
    Milch;500
    >ruehren;Mit dem Schneebesen klumpenfrei verrühren
    >ruhen;Teig quellen lassen;10

Zeilen mit `>` sind **Anweisungen**: `>Icon;Text;Minuten`. Statt der Gewichtsanzeige
zeigt die Waage ein großes Icon und den Text; mit Minuten (auch `0,5`) gibt es einen
Knopf „Timer“, der einen Küchentimer startet (klingelt auch, wenn du schon weiter bist).
Icons: `ruehren`, `mixen`, `kneten`, `braten`, `kochen`, `backen`, `grillen`, `schneiden`,
`kuehlen`, `ruhen`, `mikrowelle`, `giessen`, `erhitzen`, `servieren`, `hinweis`.
Text höchstens 55 Zeichen (Umlaute zählen doppelt), ohne `;`. Am bequemsten legst du
Anweisungen im Browser an (Rezepte -> Bearbeiten -> „+ Anweisung“, mit ▲▼ verschieben).

Ist der Rezeptordner leer, legt die Waage dieses Beispiel selbst an.

**Rezepte mit Eiern** (Zutat „Ei“ oder „Eier“) skalieren nicht nach Portionen, sondern nach
ganzen Eiern: Aus `Eier;110` werden 2 Eier im Grundrezept (55 g je Ei, `EGG_G` in `config.h`).
In der Vorschau wählst du die Anzahl Eier. Die Eier wiegst du als Erstes ab, danach richten sich
alle anderen Zutaten nach ihrem tatsächlichen Gewicht – kleine oder große Eier sind also egal.
Bei den Eiern gibt es keinen Parkpiepser und kein Auto-Weiter.
Einstellungen (Auto-Tara, WLAN) liegen im internen Flash und bleiben auch ohne Karte erhalten.

## Topferkennung (Auto-Tara)

Leeren Topf auf die leere Waage stellen -> wird ein gespeicherter Topf erkannt,
läuft ein Countdown, danach wird die Tara gesetzt und der Topfname angezeigt.
Topf abnehmen -> Tara springt automatisch zurück. Einstellbar unter
System -> Setup -> Auto-Tara. In der Topfliste: Antippen zieht das gespeicherte
Gewicht ab (gefüllter Topf), gedrückt halten öffnet „Bearbeiten“.

Steht die leere Waage nach dem Abnehmen einer tarierten Schüssel im Minus, setzt sie
nach 3 s ruhig selbst wieder auf 0 („Tara gesetzt“) – auf der Wiegeseite und im Ziel-Modus.
Dieser Minuswert wird nicht angesagt.

## WLAN und Uhrzeit

System -> Setup -> WLAN -> Netzwerke -> + Neues Netzwerk: Netz aus der Liste wählen,
Passwort über den Buchstabenring eingeben. Bis zu 5 Netze werden gespeichert (z. B.
Zuhause, Werkstatt, Ferienhaus); die Waage nimmt das stärkste davon in Reichweite.
Gespeicherte Netze antippen zum Entfernen. Im Browser: Einstellungen -> Gespeicherte
WLAN-Netze (hinzufügen und entfernen). Die Waage verbindet sich nur kurz, holt die
Uhrzeit (inkl. Sommerzeit) und schaltet WLAN wieder ab. Abgleich beim Start und
danach einmal täglich. Ohne WLAN: Uhrzeit und Datum im Setup von Hand stellen.

## Texteingabe (Buchstabenring)

Finger am Rand entlangführen, die Lupe zeigt den Buchstaben, Loslassen übernimmt.
Finger vor dem Loslassen in die Mitte ziehen = Zeichen verwerfen.
`abc` / `123` / `ABC` schaltet um, `←` löscht, `Leer` = Leerzeichen.
Nach dem ersten Großbuchstaben geht es automatisch klein weiter.

## SD-Karte

Die Karte läuft mit 10 MHz statt der 40 MHz des Treibers (`SD_FREQ_KHZ` in `config.h`),
weil der schnelle Takt im 1-Bit-Modus mit Lautsprecher und WLAN störanfällig ist.
Alle 20 s prüft die Waage, ob die Karte noch antwortet; fällt sie aus, wird sie alle 3 s
neu eingehängt – dabei wird die D3-Leitung über den I/O-Expander kurz auf Low gezogen,
was eine hängende Karte zurückholt (Meldungen im seriellen Monitor). Ton-Task, Webserver und Oberfläche
greifen nie gleichzeitig auf die Karte zu. In der Statuszeile steht „keine SD“,
solange die Karte fehlt.

## Ein- und Ausschalten (PWR-Taste)

Im Gehäuse ist ein externer Mikrotaster (6 × 6 × 10 mm) parallel zur PWR-Taste der Platine
gelötet. Er verhält sich genau wie die Taste auf der Platine.

Der Akku bleibt dauerhaft angeschlossen, geschaltet wird über die PWR-Taste:
- **Einschalten:** PWR-Taste drücken, bis das Logo aufleuchtet und der Einschaltklang beginnt.
  Die Software hält die Versorgung danach selbst (GPIO7).
- **Kurz drücken:** Standby mit gedimmter Uhr. Aufwecken per Taste, Antippen oder
  Gewicht auflegen.
- **3 s halten:** Ausschalten (Ring zeigt den Fortschritt, Loslassen bricht ab).
- **10 s halten:** Notabschaltung bzw. Neustart am USB – funktioniert auch, wenn die Anzeige hängt
  (eigener kleiner Task, unabhängig von Anzeige und SD-Karte).
- **Auto-Aus:** Nach der eingestellten Zeit ohne Bedienung (Berühren, Taste oder
  Gewichtsänderung) schaltet sich die Waage selbst aus – auch aus dem Standby.
  15 s vorher erscheint ein Countdown mit Warnton, Antippen bricht ab.
  Einstellbar unter Setup -> Wiegen -> Auto-Aus (nie/10/15/30/60 min, Standard 30).
  Eine offene Weboberfläche hält die Waage an.
  Hängt die Waage am USB, bleibt sie versorgt und zeigt „Aus“ – Taste drücken
  startet sie neu.

`PWR_Init()`/`PWR_Loop()` aus der Waveshare-Demo werden nicht mehr benutzt.

## Weboberfläche: Modi

Alle Modi der Waage gibt es auch im Browser, gruppiert wie am Gerät: Küche (Rezept,
Cocktail, Ziel, Portionieren, Timer, Tassen & Löffel), Werkstatt (Spule, Zählen, Porto,
2K mischen, Langzeit, Münzen) und Spiele (Schätzen, Trinken, Blindgießen, Halbe-Halbe).
Timer und Langzeitmessung laufen auf der Waage weiter, auch wenn der Browser zu ist; die
Startseite zeigt laufende Timer, eine laufende Messung und einen abgelaufenen Timer mit
„Alarm aus“.

## Online-Update von GitHub

Setup -> Waage -> Firmware -> „Online suchen“ (oder im Browser unter Firmware-Update) sucht die
neueste Version in den GitHub-Releases und installiert sie auf Wunsch.

**Nach dem Einschalten** sucht die Waage selbst (nur mit WLAN und nicht bei Firmware aus der
Arduino-IDE). Mit **Auto-Update** (Standard, Schalter auf der Firmware-Seite, im Assistenten und
in der Weboberfläche) installiert sie die neue Version gleich, aber nur, wenn die Waage leer ist,
genug Akku hat (≥ 40 % oder am Kabel) und das Einschalten weniger als eine Minute her ist. Sonst
fragt sie auf der Startseite „Später“ oder „Jetzt“; nie mitten in einem Modus. Startet eine neue
Firmware nicht sauber, kehrt die Waage beim nächsten Einschalten zur alten zurück (OTA-Rollback).
Nach dem Neustart meldet sie „Aktualisiert auf Version …“. Einrichtung des
Repositorys und der automatischen Builds: siehe `GITHUB.md`. Vorher in `config.h` bei
`OTA_REPO` den eigenen Repository-Namen eintragen.

## Firmware aktualisieren (OTA)

Beide Partitionsschemata („16M Flash 3MB APP/9.9MB FATFS“ und „ESP SR 16M“) haben zwei
App-Partitionen, OTA funktioniert also ohne Umstellung.

- **Über den Browser:** Weboberfläche starten, dann `http://waage.local/update` bzw.
  die angezeigte IP mit `/update` aufrufen. Dort die Datei `Waage.ino.bin` hochladen
  (Arduino-IDE: Sketch -> Kompilierte Binärdatei exportieren). Die Waage piept beim Start
  der Übertragung, spielt am Ende die Fertig-Melodie und startet neu.
- **Über die Arduino-IDE:** Nur im **Heimnetz** (nicht im eigenen WLAN der Waage).
  Weboberfläche starten, dann erscheint unter Werkzeuge -> Port ein Netzwerk-Port
  „waage at 192.168.x.x“ (kann 10-30 s dauern). Kein Passwort. Voraussetzung: PC im
  selben Netz, Firewall erlaubt die Arduino-IDE.
  Während eines Updates wird die 10-Minuten-Abschaltung laufend zurückgesetzt.

Das WLAN schaltet sich nach 10 Minuten ohne Zugriff ab; ein laufendes Update hält es an.

## Kalibrierung und Messmittelprüfung

**Kalibrierung (Setup -> Waage):** leeren, Referenz auflegen, Kontrolle. Über „Punkt +“
lassen sich bis zu **vier Stützpunkte** aufnehmen (z. B. 200 g, 500 g, 1000 g, 2000 g).
Zwischen den Punkten rechnet die Waage abschnittsweise linear, außerhalb mit der
Steigung des äußersten Abschnitts. Das fängt die Nichtlinearität der Wägezelle ab,
die mit einem einzigen Punkt am Rand des Bereichs sichtbar wird. Die Punkte gelten
relativ zum Nullpunkt (beim Einschalten neu gemessen); Datum und Uhrzeit der
Kalibrierung speichert die Waage mit (Weboberfläche → Messmittelprüfung).
Mehr Punkte helfen nur mit genauen Gewichten: jeder Fehler eines Referenzgewichts
wird an seinem Punkt zur Abweichung der Waage.

**Messmittelprüfung (Setup -> Waage -> Kalibrierung -> „Prüfen“, oder im Browser
unter Einstellungen -> Messmittelprüfung):** Messmittelfähigkeit nach Verfahren 1.
Sollgewicht des Normals und Toleranz T einstellen, 25 oder 50 Messungen. Ablauf je
Messung: auflegen, stabilisieren (0,8 s ruhig), Wert wird gespeichert, abnehmen.
Danach rechnet die Waage:

    x̄   Mittelwert       Bi = x̄ − Soll        s = Standardabweichung
    Cg  = 0,2·T / (6·s)          Cgk = (0,1·T − |Bi|) / (3·s)

Fähig ab 1,33 (Bosch). Zusätzlich wird die kleinste prüfbare Toleranz
T_min = 40·s + 10·|Bi| angezeigt. Ergebnis und alle Einzelwerte landen als CSV in
/Waage/Pruefung/<Datum>_<Uhrzeit>.csv (Kopf: Kennwerte, Quelle, Firmware und die
Kalibrierung, die zur Prüfung galt; darunter Nr;Messwert_g), egal ob an der Waage oder
im Browser geprüft. An der Waage zeigt „Verlauf“ die Einzelwerte mit dem Band
Soll ± 0,1·T. Die Weboberfläche (Messmittelprüfung) zeigt die letzte Kalibrierung, eine
Übersicht „Abweichung über dem Sollwert“ aller Prüfungen und je Prüfung einen
Prüfbericht mit Werteverlauf und Histogramm. Im Browser rechnet die Prüfung mit zwei
Nachkommastellen (nicht mit der gerundeten Anzeige).

## Akku

**Tiefentladeschutz:** Unter 3,45 V warnt die Waage jede Minute hörbar und zeigt
„fast leer – laden!“. Bleibt sie 20 s unter 3,30 V, beendet sie einen laufenden Test
und schaltet sich ab. LiPo-Zellen nehmen unter etwa 3,2 V Schaden.

**Entladetest:** Auf der Akku-Seite (Waage oder Browser) startbar. Solange er läuft,
ist das automatische Ausschalten pausiert, sonst würde die Waage mitten im Test aus gehen. Er schreibt jede
Minute Spannung und Ladestand nach /Waage/Akku/test_<Datum>_<Zeit>.csv, von voll bis
zur Abschaltung. Damit bekommst du die echte Laufzeit und die tatsächliche
Entladekurve, statt sie zu schätzen.

Die Umrechnung Spannung -> Prozent stammt aus einem echten Entladetest
(23./24.09.2026: **10,1 h** von 4,11 V auf 3,02 V). Die Prozentzahl entspricht dem
Anteil der **verbleibenden Laufzeit**, nicht der Spannung. Stützstellen u. a.:
3,95 V = 89 %, 3,80 V = 69 %, 3,70 V = 50 %, 3,60 V = 34 %, 3,50 V = 19 %,
3,40 V = 9 %, 3,30 V = 3 %. Gegenprobe an den Messdaten: die Restzeit stimmt über
den ganzen Bereich auf etwa 5 Minuten genau.

Solange der eigene Verbrauch noch nicht gemessen ist, rechnet die Restzeitanzeige
mit diesen 10,1 h (`BAT_RUNTIME_H` in `hal.h`).

System -> Setup -> Zeit & Funk -> Akku zeigt Ladestand, Zustand, Verlauf der letzten Stunden als Balken,
Verbrauch in Prozent pro Stunde und die geschätzte Restlaufzeit. Alle 5 Minuten kommt
ein Messpunkt dazu, zusätzlich wandert er tageweise auf die SD-Karte
(/Waage/Akku/<Datum>.txt). Dieselben Daten gibt es in der Weboberfläche unter „Akku“.

**Am USB** hängt die gemessene Spannung am Ladegerät und liegt über der echten
Akkuspannung. Deshalb zeigt die Waage dort **„lädt“** bzw. **„voll“** statt eines
falschen Prozentwerts, und in der Statuszeile erscheint ein Blitz- bzw. Akkusymbol.
Erkannt wird das an der Höhe und der Vorgeschichte, abgeglichen mit Messungen am Gerät:
über 4,19 V lädt das Ladegerät aktiv, 4,14 bis 4,19 V mit Kabel heißt „voll“, nach dem
Abziehen fällt die Spannung auf etwa 4,13 V und die Waage zeigt wieder Prozent.
Die Ladekurve endet bei 4,15 V = 100 %.

## Dateien auf der SD-Karte (Browser)

Weboberfläche -> „Dateien“: Ordner durchklicken, Dateien herunterladen (antippen), ganze Ordner
mit Unterordnern als ZIP herunterladen („↓ ZIP“, z. B. die Sprachaufnahmen),
löschen, neue Ordner anlegen und beliebig viele Dateien hochladen (mit Fortschritt).
Damit lassen sich Rezepte, Cocktails, Sprachpakete und Protokolle pflegen, ohne die
Karte auszubauen. Alles ist auf /Waage begrenzt, darüber hinaus geht nichts.
An der Waage steht der Verweis unter Setup -> Waage -> Dateien.

## Weboberfläche

**Geschwindigkeit:** Der WebServer des Boardpakets bedient nur eine Verbindung
gleichzeitig und wartet auf stumme Verbindungen bis zu 5 s (HTTP_MAX_DATA_WAIT).
Browser öffnen aber Verbindungen auf Vorrat - das war der Grund für 10-30 s Ladezeit.
Gegenmaßnahmen: stumme Verbindungen werden nach 350 ms selbst getrennt
(`drop_idle_client` in web.cpp), die Seite wird **gzip** ausgeliefert (76 KB -> 22 KB,
siehe `web/build.py`), der Server wird mehrfach je Durchlauf und alle 5 ms bedient,
und die Oberfläche schickt ihre Anfragen nacheinander statt parallel.

Seit dem Umbau nutzt die Weboberfläche **dasselbe dunkle Design wie das Display**:
identische Farben (übernommen aus `ui_theme.h`), Ringanzeige für Gewicht, Ziel, Spule
und Akku, runde Tara-/Speichern-Knöpfe, Kopfzeile mit rundem Zurück-Knopf und mittigem
Titel. Die Übersicht zeigt Ring plus Aktionen, darunter die acht Modi als Symbolraster
und die Verwaltung als Chips. Eigene Unterseiten gibt es für Portoklassen, Spielernamen
und Firmware-Update (inklusive Schalter für den Update-Modus).

Die Messmittelprüfung zeigt an Waage und Browser das Live-Gewicht und übernimmt den
Wert automatisch nach 0,8 s Ruhe oder von Hand über „Übernehmen“ – so hängt sie nicht
an der Stabilitätserkennung fest.

Startseite nach links wischen bis „System“ -> „Weboberfläche“ antippen (startet sofort). Die Waage verbindet sich mit
dem gespeicherten WLAN (`http://waage.local`) oder macht ohne Heimnetz ein eigenes
WLAN „Waage-Setup“ auf (Adresse steht dann auf dem Display). Nach **10 Minuten ohne
Zugriff** schaltet sich der Server samt WLAN selbst ab; solange die Seite offen ist,
bleibt er an.

Aufbau wie im Design-Sheet: Übersicht mit Live-Gewicht, Auslastungsbalken, Tara
und Speichern, darunter Kacheln für alle Modi und die Verwaltung.

- **Ziel:** Zielgewicht, Balken bis zum Ziel, die Waage piept als Parkpiepser mit.
  „An der Waage anzeigen“ zeigt Ziel und Restmenge groß auf dem Display (Profil Einfach).
- **Rezept:** Rezept wählen, Portionen (bei Eiern: Anzahl Eier), Schritt für Schritt mit Auto-Tara,
  Anweisungen mit Icon und Timer.
- **Spule:** Material, Leerspule, Durchmesser, Restmeter live; neue Leerspule anlegen.
- **Zählen:** Referenz übernehmen, Stückzahl live, Zählliste mit Bezeichnungen
  (als CSV herunterladbar), Senden per Bluetooth.
- **Porto:** passende Klasse und Preis live, alle Klassen im Überblick.
- **Spiel:** Spielernamen eintragen, Tipps aller Spieler auf einer Seite, Auflösung
  mit Trommelwirbel an der Waage, Bestenliste. Während des Spiels zeigt auch das
  Display der Waage nur „? ? ?“.
- **Rezepte, Töpfe und Spulen:** anlegen, umbenennen, Farben, löschen. Neue Töpfe
  und Spulen mit dem gerade aufgelegten Gewicht.
- **Protokoll:** Tage durchblättern, Tabelle, Diagramm „Wägungen pro Tag“ der letzten
  7 Tage, Tag als TXT und alles als CSV herunterladen.
- **Einstellungen:** Wiegen, Auto-Tara, Ton (inkl. Stimme und Hörprobe), Zeit und
  WLAN, Waage (Libelle zurücksetzen), Portoklassen.

Kalibrieren geht bewusst nur an der Waage, weil dabei Gewichte aufgelegt werden.

**Seite ändern:** Quelle ist `web/page.html`. Danach `python3 web/build.py`
ausführen, das erzeugt `web_page.h` neu.

## Protokoll

Neben Wägungen landen auch die Ergebnisse der Modi im Protokoll, immer erst wenn der
Wert 1,5 s ruhig liegt und nur einmal pro Auflegen: Ziel erreicht, Spule (Restmeter),
Zählen (Stückzahl), Porto (Klasse und Preis), Rezept fertig und die Sieger der Spiele.
Die Notiz steht in der Tagesansicht zwischen Uhrzeit und Gewicht. Die Liste lässt
sich wischen.

## Cocktail

Modi -> Cocktail. Rezepte stehen in **Millilitern** in /Waage/Cocktails/*.txt
(gleiches Format wie Rezepte). Gewogen wird in Gramm, die Umrechnung läuft über
eine Dichtetabelle nach Zutatennamen: Sirup 1,28 g/ml, Spirituosen 0,94, Saft 1,05,
unbekannt 1,0. Auf der mitgelieferten SD-Karten-Vorlage liegen **15 Cocktails** (Gin Tonic, Cuba Libre,
Caipirinha, Aperol Spritz, Mojito, Moscow Mule, Margarita, Daiquiri, Negroni,
Espresso Martini, Whisky Sour, Pina Colada, Sex on the Beach, Tequila Sunrise, Hugo)
und **7 Rezepte**. Ist der Ordner leer, legt die Waage vier Beispiele selbst an.

**Eigene Cocktails:** In der Weboberfläche unter „Cocktailkarte“ mit Glas-Vorschau
(farbige Schichten je Zutat), Zutatenvorschlägen und „Menge abwiegen“, das den
aktuellen Waagenwert über die Dichte in ml übernimmt. An der Waage selbst über
„+ Neuer Cocktail“: Name und Zutaten über den Buchstabenring, Menge einstellen
oder mit „Abmessen“ direkt eingießen.

Ablauf: Cocktail wählen, Anzahl Gläser, Glas auf die Waage, dann Zutat für Zutat.
Nach jedem Schritt wird automatisch tariert, der Ring und der Parkpiepser führen
zur Zielmenge, die Anzeige läuft in ml. In der Weboberfläche füllt sich dabei ein
Cocktailglas, das den Füllstand des ganzen Drinks zeigt.

## Mikrofon und Sprache

**System -> Mikrofon** zeigt ohne Zahlen, ob etwas gehört wird: Der Ring schlägt mit
dem Schall aus (grün = hört etwas, rot = kein Signal), darunter steht der Zustand und,
bei eingeschalteter Spracherkennung, ob das Weckwort erkannt wurde und welcher Befehl
zuletzt verstanden wurde. Kommen vom Mikrofon länger keine Daten, startet die Waage
den I2S-Empfang selbst neu.

**Sprachbefehle „Hallo Waage“** schaltest du unter Setup -> Ton -> Sprachbefehle ein und aus
(Weboberfläche: Einstellungen -> Ton). Die Erkennung läuft komplett auf der Waage mit einem kleinen
eigenen Netz (`kws.cpp`), ohne Internet und ohne ESP-SR, mit dem normalen Partitionsschema. Das Modell
ist eine Datei auf der SD-Karte, `/Waage/Sprache/kws_de.bin`; fehlt sie, steht in der Zeile
„kein Modell“. Ablauf: „Hallo Waage“ sagen (im Profil Einfach geht auch: oben zweimal tippen), die Waage
sagt „Ja?“ und zeigt „Ich höre zu …“, dann innerhalb von 6 s einen Befehl. Wird nichts erkannt, sagt sie
„Nicht verstanden. Sag Hilfe.“:

| Sagen | Wirkung |
|---|---|
| „Tara“ | Tara (beim Zielwiegen: nächste Portion) |
| „Wie viel?“ | Gewicht ansagen, beim Zielwiegen die Restmenge |
| „Speichern“ | ins Protokoll |
| „Weiter“ | Weiter-Knopf der Seite (Rezept, Cocktail, Spiele) |
| „Zurück“ | zur Wiegeseite |
| „Stopp“ | Zielwiegen bzw. Modus beenden |
| „50, 100, 150, 200, 250, 300, 500 Gramm“, „ein Kilo“ | Zielwiegen mit Parkpiepser („Ziel 250 Gramm. Jetzt einfüllen.“) |
| „Hilfe“ | liest die Befehle vor |
| „Nochmal“ | letzte Ansage wiederholen |
| „Lauter“ / „Leiser“ | Lautstärke ändern |
| „Akku“ / „Uhrzeit“ | Akkustand bzw. Uhrzeit ansagen |

Jeder Befehl wird gesprochen bestätigt („Weiter.“, „Zurück zur Startseite.“, „Lautstärke 80 Prozent“,
„Akku 60 Prozent, wird geladen.“, „Das geht hier gerade nicht.“, „Uhrzeit unbekannt.“ …). Mit Ansage
„Sprüche“ sagt sie außerdem „Waage bereit.“, „Timer abgelaufen.“, „Akku fast leer.“ und warnt vor dem
Auto-Aus. Fehlen einem Stimmpaket Sätze, kommen sie aus `/Waage/Stimme/_Zusatz`; die Weboberfläche
zeigt unter Einstellungen → Ton, was fehlt. Nach jeder Antwort bleiben mindestens 4,5 s für den Befehl.

Das Mikrofon liest ein eigener Task lückenlos in einen 1-s-Puffer (PSRAM); die Erkennung arbeitet ihn
ab. (Bis 15.10.2026 las die Erkennung selbst und verlor während des Rechnens Stücke – Mitschnitte aus
dieser Zeit sind abgehackt.) Das serielle Log meldet jede Minute „Mikrofon: Puffer max … ms,
verworfen … ms“.

**Mitschnitt:** Solange die Mikrofonseite offen ist, speichert die Waage jeden Weckversuch und Befehl als
3-s-WAV nach `/Waage/Sprache/Mitschnitt` (`w…` geweckt, `k…` knapp daneben, `b…_<befehl>` Befehl,
`n…` nicht verstanden). Am PC holt und sortiert `kws_mitschnitt.py` sie (siehe `Sprachmodell/LIESMICH.md`).

Während die Waage selbst spricht oder piept, hört sie weg. Unter **Setup -> Ton -> Wörter** nimmst du
mit dem Mikrofon der Waage Beispiele auf (Wort wählen, runden Knopf tippen, nach dem Piepton sprechen).
Damit trainierst du am PC ein besseres Modell, siehe Ordner `Sprachmodell` (LIESMICH.md). Das
mitgelieferte Modell kennt nur künstliche Stimmen. Die alte ESP-SR-Variante (englisch, „Hi ESP“) gibt
es weiterhin mit `USE_VOICE 1` und dem Partitionsschema „ESP SR 16M“.

## Anzeige und Modi-Menü

Die Wiegeanzeige ist geglättet: Große Änderungen werden sofort übernommen, kleine
Schwankungen weggemittelt, und sobald der Wert ruhig liegt, wird der Mittelwert des
Messfensters angezeigt – die Zahl steht dann still. Die Logik (Stabilität, Tara,
Kalibrierung) arbeitet weiter mit den ungefilterten Werten.

Das Modi-Menü ist in drei Gruppen geteilt: **Küche** (Rezept, Cocktail, Ziel),
**Werkstatt** (Spule, Zählen, Porto) und **Spiele** (Schätzspiel, Trinkspiel).
Jeder Eintrag hat ein Symbol (Material Symbols, als `font_icons_26.c` eingebettet;
Zeichen siehe `ICON_...` in `ui_theme.h`). Wischen nach rechts führt aus einem Modus
zurück in seine Gruppe und von dort auf die Wiegeseite.

## Einrichtung beim ersten Start

Beim allerersten Einschalten führt nach dem Startbild ein Assistent durch Sprache, Profil,
Kategorien (nicht bei Einfach), Farbe, Ton und WLAN. Unten links und rechts sitzen zwei
Pfeil-Knöpfe (zurück, weiter), der Fortschritt läuft als Bogen um den Rand; Wischen nach rechts
geht ebenfalls einen Schritt zurück. Danach wird die Bedienung zum Ausprobieren erklärt: nach
links wischen, nach rechts zurück, Zahl lange drücken (im Profil Einfach: oben tippen, unten auf
TARA tippen). Danach folgt der gewohnte Lagecheck. Bereits eingerichtete Waagen überspringen den Assistenten nach dem Update.
Wiederholen: Setup -> Waage -> Einrichtung. Alternativ im ersten Schritt „Mit dem Handy
einrichten“: Die Waage startet die Weboberfläche, dort öffnet sich die Einrichtung von selbst
(`#/einrichtung`, auch unter Einstellungen -> Waage -> Einrichtung).

## Profile (Setup -> Waage -> Profil)

Ein Profil legt fest, was die Startseite zeigt, und setzt passende Voreinstellungen:

| Profil | Startseite | Voreinstellungen |
|---|---|---|
| Standard | Wiegen, Modi (alle Gruppen), System | – |
| Küche | nur Gruppe Küche, System ohne Wasserwaage | Auto-Tara an |
| Werkstatt | nur Gruppe Werkstatt, System ohne Töpfe | – |
| Einfach | nur die Wiegeseite, kein Wischen | Farbe bleibt, Ansage an, Lautstärke mind. 80 %, Gramm, Auto-Tara/Präzision/Auto-Speichern aus, Auto-Aus mind. 60 min |

Zusätzlich lassen sich in der Weboberfläche (Einstellungen -> Profil) ganze Gruppen und
einzelne Modi ausblenden. Gruppen ohne sichtbare Modi verschwinden, gibt es keine Gruppe,
fehlt die Modi-Seite, und die Punkte unten zeigen nur die vorhandenen Seiten.

**Einfach** ist für schlechte Augen gedacht (Bildschirm-Variante A aus dem Konzept): ganze
Gramm in großen Ziffern, keine Statuszeile und keine Knöpfe. Oben antippen sagt das Gewicht
an (auch wenn die automatische Ansage aus ist), die farbig hinterlegte Fläche unten ist Tara.
Wischen über die Flächen löst nichts aus. Ganz oben steht der Akkustand. Die PWR-Taste verhält
sich wie immer. Es gibt am Gerät keinen Weg zurück ins Setup: Einfach wird nur in der
Weboberfläche verlassen. Zum Starten der Weboberfläche die obere Fläche **5 Sekunden gedrückt
halten**, dann erscheinen Adresse und QR-Code.

**Zielwiegen im Profil Einfach:** Unten ist die Fläche geteilt: links TARA, rechts ZIEL. ZIEL zeigt
„Wie viel?“ mit vier großen Kacheln (100, 200, 250, 500 g); Antippen tariert und startet. Oben steht
„Ziel 250 g“, unter dem Gewicht die Restmenge („noch 70 g“, „Ziel erreicht“ mit Haken, „25 g zu viel“
in Rot), der Ring am Rand füllt sich und der Parkpiepser piept schneller, je näher das Ziel kommt.
Oben antippen sagt „noch 70 Gramm“ an. Unten links TARA startet die nächste Portion mit demselben
Ziel, rechts ENDE beendet. Wird nach dem Erreichen abgenommen, sagt die Waage „fertig“ und steht
wieder auf 0. Auch aus der Weboberfläche startbar (Ziel → „An der Waage anzeigen“); in den anderen
Profilen öffnet das den Modus Ziel. Für die Ansage braucht das Stimmpaket `noch.wav` und
`zu_viel.wav` (englisch `to_go.wav`, `too_much.wav`), nachzurüsten mit
`stimme_schneiden.py aufnahme.mp3 <Paket> --nachtrag`.

Ist nur eine Gruppe sichtbar (z. B. Profil Küche), zeigt die Modi-Seite gleich deren Modi.

## Spieler und Bestenliste

Spielernamen werden **einmal** gepflegt: an der Waage unter Spiel -> „Spieler“, im
Browser unter Einstellungen -> Spielernamen. In den Spielen wird nur noch ausgewählt,
wer mitspielt (antippen = dabei, halten = umbenennen, „+ Spieler“ legt einen neuen an).

Die Ergebnisse beider Spiele landen in **einer** Datei: `/Waage/Spiel/bestenliste.txt`
im Format `Datum;Spiel;Name;Wert;Einheit`. Kleiner ist besser (Summe der Abweichungen).
Die Weboberfläche zeigt daraus unter jedem Spiel eine Bestenliste mit dem besten Wert
je Person. Kommt später ein Spiel dazu, schreibt es einfach in dieselbe Datei.

## Trinkspiel

Prinzip wie beim SipMaster: Die Waage misst die Schluckgröße (1 g ≈ 1 ml).
Pro Zug: Getränk abstellen, trinken, zurückstellen. Wer nichts getrunken hat und
nur neu abstellt, bekommt „Nichts getrunken? Nochmal“.
- **Zielschluck:** jede Runde ein neues Ziel (10–80 ml), Summe der Abweichungen zählt.
- **KO:** Toleranz startet bei ±15 ml und wird jede Runde um 3 ml kleiner; wer
  darüber liegt, ist raus (außer alle wären raus). Gewinner ist, wer übrig bleibt.
Endstand mit Statistik „getrunken“ pro Spieler, Ergebnis in /Waage/Spiel/spiele.txt.

## Schätzspiel

Modi -> Spiel: Spieler (2-8) und Runden (3/5/10) wählen, „Namen ›“ zum Eintragen
der Namen über den Buchstabenring, Waage leer -> „Los“. Die Namen gelten für
Schätz- und Trinkspiel und die Weboberfläche gemeinsam (/Waage/Spiel/spieler.txt).
Pro Runde: Gegenstand verdeckt auflegen, „Tippen“, jeder gibt reihum seinen Tipp ab.
Auflösung mit Trommelwirbel. Gewertet wird die Summe der Abweichungen, die
Bestenliste wird in /Waage/Spiel/spiele.txt gespeichert.

## Spule (Filament)

Modi -> Spule: Material wählen (PLA, PETG, ABS, ASA, TPU, PC, Nylon), dann Leerspule
(„Ohne Leerspule“, eine gespeicherte oder „+ Leerspule“ neu abwiegen). Anzeige der
Restmeter live, Durchmesser 1,75/2,85 mm umschaltbar, „Speichern“ schreibt ins Protokoll.

## Einstellungen (System -> Setup)

Vier Gruppen als Kacheln:
- **Wiegen:** Einheit (g, kg, oz, lb – gilt für Wiegeseite und Protokoll),
  Auto-Speichern (einmal pro Auflegen, wenn das Gewicht 2 s ruhig liegt, ab 5 g; der
  Speichern-Knopf ist dann ausgeblendet, auch ein abgelehnter Topf wird gespeichert),
  Auto-Weiter (Rezept und Cocktail schalten selbst weiter, wenn die Menge
  erreicht ist und ruhig liegt), Zur Wiegeseite, Auto-Tara, Auto-Aus.
- **Zeit & Funk:** Uhrzeit, Datum, WLAN, Bluetooth, Akku (Ladestand, Verlauf, Restzeit; Wischen nach rechts führt
  zurück). Die Weboberfläche steht jetzt ganz oben in der Systemliste.
- **Ton:** Lautstärke, Tonschema, Ansage, Stimme, Sprache, Sprachbefehle, Mikrofon, Wörter (Aufnahmen fürs Sprachmodell).
- **Waage:** Profil, Kalibrierung, Prüfgewicht, Messmittelprüfung, Libelle, Display, Farbe, Dateien, Firmware.

Einfache Werte werden direkt durch Antippen umgeschaltet, Zeilen mit „›“ öffnen
eine Unterseite. Als Bedienung für „Zur Wiegeseite“ zählt Berühren und auch eine
Gewichtsänderung – ein laufendes Rezept wird beim Abwiegen also nicht geschlossen.

## Töne und Ansage

Tonschemas unter Setup -> Ton & Bluetooth -> Lautstärke: **Klassisch**, **Sanft**
(tiefer und weicher), **Retro** (Rechteckton) und **Minimal** (nur Parkpiepser,
Warnungen, Überlast). Lautstärke 0 % schaltet alles ab.

Die Engine bricht laufende kurze Töne ab, wenn ein neuer kommt, und unterdrückt
denselben Ton innerhalb von 60 ms. Klicks sind leiser als Signaltöne. Beim
Buchstabenring kommt ein Klick, wenn ein Buchstabe übernommen wird.

**Sprachausgabe:** Ordner `Stimme` aus `SD-Karte/Waage/` auf die Karte kopieren
(/Waage/Stimme/<Paket>/*.wav, mitgeliefert: Paket „Roboter“). Dann lassen sich in
Setup -> Ton & Bluetooth die Zeilen „Ansage“ (an/aus) und „Stimme“ (Paket wechseln) nutzen:
Die Waage sagt stabile Werte einmal pro Auflegen an, außerdem beim Speichern und
die Stückzahl beim Senden im Zählmodus. Die Zahlen werden aus Bausteinen
zusammengesetzt („ein-und-zwanzig“), deshalb reichen 47 kleine Dateien (ca. 1 MB).

Findet die Waage die Dateien nicht, steht in der Zeile „Ansage“ **keine Stimme**.
Dann Ordner und Dateinamen prüfen: /Waage/Stimme/gramm.wav muss existieren.

**Eigene Stimmpakete:** Jeder Unterordner in /Waage/Stimme ist ein Paket, zwischen
denen die Zeile „Stimme“ umschaltet. Zwei Wege:
- `stimme_erzeugen.py <Ordner>` erzeugt ein Paket mit espeak-ng (Paket „Roboter“).
- `stimme_schneiden.py aufnahme.mp4 <Paket>` schneidet eine Aufnahme in die
  einzelnen Wörter. `stimme_schneiden.py --liste` gibt die Wortliste in der
  richtigen Reihenfolge aus, die du in ein Sprachprogramm kopieren kannst.
  Wichtig: zwischen den Wörtern deutliche Pausen (ab ca. 0,3 s).

Dateiformat: 16 kHz, Mono, 16 Bit PCM.

## Töne (Details)

Lautsprecher über I2S (BCLK 48, LRC 38, DOUT 47). Parkpiepser im Ziel- und
Rezeptmodus: ab der Hälfte des Zielgewichts piept es, je näher desto schneller,
im Ziel ein langer Ton, darüber ein tiefer Warnton. Dazu Töne für Tara,
Speichern, Topf erkannt, Countdown, Überlast und Tastenklick im Ring.
Lautstärke: System -> Setup -> Ton (0 % = aus).

## Rezepte an der Waage anlegen

Modi -> Rezept -> „+ Neues Rezept“: Name über den Ring, Portionen, dann Zutaten
hinzufügen (Name über den Ring mit Vorschlägen, Menge einstellen oder mit
„Abwiegen“ direkt übernehmen). Gedrückt halten entfernt eine Zutat.
In der Vorschau eines Rezepts: „Löschen“ (zweimal tippen).

## Portoklassen

System -> Setup -> Porto: Klassen antippen zum Bearbeiten (Name, Gewichtsgrenze,
Preis), „+ Klasse“ zum Anlegen. Gespeichert in /Waage/Porto/porto.txt.

## Libelle

Setup -> Libelle: Messen, Waage um 180° drehen, erneut Messen. Der Mittelwert
ist der Einbaufehler des Displays. Läuft die Blase beim Anheben der Vorderkante
in die falsche Richtung, in `config.h` `DISPLAY_MOUNT` anpassen (0-3 = 0°/90°/180°/270°)
und danach die Libelle neu kalibrieren.

## Bluetooth (Zählmodus)

- Beim ersten Öffnen von „Zählen“ startet Bluetooth, die Waage heißt „Waage“.
- Anleitung an der Waage: Status-Chip im Zählmodus antippen oder Setup -> Bluetooth.
- Am PC: Bluetooth-Gerät hinzufügen → „Waage“ auswählen. Danach verbindet sie sich
  automatisch.
- „Senden“ tippt die Stückzahl + Enter in das aktive Feld (z. B. Excel-Zelle).
  Es werden nur Ziffern gesendet, das Tastaturlayout am PC ist daher egal.
- Gibt es beim Kompilieren Fehler rund um BLE: in `config.h` `USE_BLE_KBD` auf `0`
  setzen, dann läuft alles andere ohne Bluetooth.

## Waage anschließen (HX711)

Wägezelle -> HX711: Rot E+, Schwarz E-, Weiß A-, Grün A+ (B+/B- frei lassen)
HX711 -> UART-Header: VCC -> 3V3, GND -> GND, DT -> RXD (GPIO44), SCK -> TXD (GPIO43)

Bibliothek: „HX711 Arduino Library“ von Bogdan Necula (Bibliotheksverwalter).

Kalibrieren: Setup -> Waage -> Kalibrierung. (1) Waage leeren -> Weiter, (2) bekanntes
Gewicht auflegen, Wert mit +/- einstellen -> Kalibrieren, (3) Kontrolle.
Der Faktor wird im Flash gespeichert. Beim Einschalten muss die Waage leer sein,
dabei wird der Nullpunkt gemessen. `Waage_Phase1_HX711` ist nur noch zur
Fehlersuche an der Verdrahtung nötig.

## Simulation

`SIM_WAAGE 1` in `config.h` (ohne HX711 testen): Gewichte laufen automatisch durch
(0 g → 1234,5 g → 1840 g → 3120 g Überlast → 812 g → 0 g).

## Bekannte offene Punkte

- Läuft die Blase in die falsche Richtung: in `ui_read_tilt()` (ui_widgets.cpp)
  das Vorzeichen von `x` bzw. `y` umdrehen.
- LVGL-Speicher ist in der Waveshare-`lv_conf.h` auf 48 KB gesetzt, aktuell
  werden ca. 31 KB belegt. Für weitere Seiten wird der Wert erhöht.
