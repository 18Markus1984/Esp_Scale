# Waage auf GitHub (Repository `Esp_Scale`): automatisch bauen und online updaten

Ziel: Du änderst den Code, lädst ihn zu GitHub hoch und vergibst eine Versionsnummer.
GitHub kompiliert die Firmware selbst und legt sie als Release ab. Die Waage findet sie
unter **Setup → Waage → Firmware → „Online suchen“** (oder im Browser unter
**Firmware-Update → Online-Update**) und installiert sie.

```
du:      Code ändern → hochladen → Tag v1.2.0 setzen
GitHub:  Action baut Waage.ino.bin → Release „Waage v1.2.0“
Waage:   fragt github.com/18Markus1984/Esp_Scale/releases/latest → lädt → flasht → startet neu
```

Du exportierst also **keine** Binärdatei mehr aus der Arduino-IDE.

---

## Einmalig einrichten

### Schritt 1: GitHub-Konto und Repository

1. Auf <https://github.com> anmelden (oder ein kostenloses Konto anlegen).
2. Oben rechts **+ → New repository**.
3. Name: `Esp_Scale`. Sichtbarkeit: **Public**. Die Waage lädt die Updates ohne Passwort
   herunter, deshalb muss das Repository öffentlich sein (siehe „Hinweise“ unten).
4. **Kein** Häkchen bei „Add a README“, „.gitignore“ oder „license“. Das Repository soll leer sein.
5. **Create repository**.

Dein Repository ist dann `18Markus1984/Esp_Scale` (https://github.com/18Markus1984/Esp_Scale).

### Schritt 2: Repository-Ordner auf dem PC vorbereiten

Lege einen neuen Ordner an, z. B. `C:\Esp_Scale\`. Er muss am Ende so aussehen:

```
Esp_Scale/
├── .github/
│   └── workflows/
│       └── firmware.yml      ← aus dem ZIP-Ordner "GitHub"
├── .gitignore                ← aus dem ZIP-Ordner "GitHub"
├── GITHUB.md                 ← diese Anleitung
├── lv_conf.h                 ← deine LVGL-Einstellungen (siehe unten)
└── Waage/                    ← dein kompletter Sketch-Ordner
    ├── Waage.ino
    ├── *.cpp, *.h, font_*.c
    ├── Display_SPD2010.*, Touch_SPD2010.*, LVGL_Driver.* …   ← Waveshare-Treiber!
    └── web/
```

- **Sketch-Ordner `Waage`:** Er heißt weiter `Waage`, weil die Arduino-IDE verlangt, dass Ordner und `Waage.ino` gleich heißen. Nur das Repository heißt `Esp_Scale`. Den ganzen Ordner kopieren, mit dem du in der Arduino-IDE
  kompilierst, **inklusive der Waveshare-Treiberdateien** (BAT_Driver, Display_SPD2010,
  esp_lcd_spd2010, Gyro_QMI8658, I2C_Driver, LVGL_Driver, PWR_Key, RTC_PCF85063, SD_Card,
  TCA9554PWR, Touch_SPD2010). Ohne sie kann GitHub nicht kompilieren.
- **`lv_conf.h`:** die Datei, in der du `LV_MEM_CUSTOM 1` und `LV_USE_QRCODE 1` gesetzt hast.
  Sie liegt bei dir unter `Dokumente\Arduino\libraries\lvgl\src\lv_conf.h`
  (manchmal auch `Dokumente\Arduino\libraries\lv_conf.h`). Eine **Kopie** davon
  kommt in den Repository-Ordner.
- **`.github` und `.gitignore`:** Diese Namen beginnen mit einem Punkt. Auf dem Mac sind sie
  im Finder versteckt (Anzeigen mit `Cmd + Shift + .`). Unter Windows sind sie normal sichtbar.

### Schritt 3: Repository in der Firmware (schon erledigt)

In `Waage/config.h` ganz unten:

```c
#define OTA_REPO "18Markus1984/Esp_Scale"
```

Das ist bereits eingetragen. Nur ändern, falls sich dein GitHub-Name oder der Name des
Repositorys ändert.

### Schritt 4: Hochladen mit GitHub Desktop

Am einfachsten geht es mit **GitHub Desktop** (<https://desktop.github.com>):

1. GitHub Desktop installieren und mit deinem Konto anmelden.
2. **File → Add local repository…** → Ordner `C:\Esp_Scale` wählen.
   Es erscheint „This directory does not appear to be a Git repository“:
   auf **create a repository** klicken, dann **Create repository**.
3. Links stehen alle Dateien. Unten bei „Summary“ z. B. `Erste Version` eintragen →
   **Commit to main**.
4. Oben **Publish repository**. Den Namen `Esp_Scale` lassen und das Häkchen bei
   **„Keep this code private“ entfernen**. Dann **Publish**.

   Hast du das Repository in Schritt 1 schon auf der Webseite angelegt, meldet GitHub
   Desktop eventuell, dass es den Namen schon gibt. Dann entweder das leere Repository auf
   der Webseite löschen (Settings → ganz unten „Delete this repository“) und nochmal
   veröffentlichen, oder unter **Repository → Repository settings → Remote** die Adresse
   `https://github.com/18Markus1984/Esp_Scale.git` eintragen und **Push origin** drücken.

### Schritt 5: Probelauf der Action

1. Auf github.com dein Repository öffnen → Reiter **Actions**.
2. Links **Firmware bauen** → rechts **Run workflow** → **Run workflow**.
3. Nach etwa 5–10 Minuten (der erste Lauf lädt das ESP32-Paket) sollte ein **grüner Haken**
   erscheinen. Unten beim Lauf liegt unter „Artifacts“ die Datei `Waage-0.0.0-test…`
   (enthält `Waage.ino.bin`).
4. Bei einem **roten Kreuz** auf den Lauf klicken → auf den roten Schritt klicken.
   Die Meldung zeigt, was fehlt (siehe „Fehlersuche“ unten).

### Schritt 6: Erste Version veröffentlichen

1. In GitHub Desktop: Reiter **History** → Rechtsklick auf den obersten Eintrag →
   **Create Tag…** → `v1.0.0` → **Create Tag**.
2. Oben **Push origin** drücken. Das schickt auch den Tag zu GitHub.
3. Im Reiter **Actions** läuft jetzt „Firmware bauen“ für `v1.0.0`. Ist er grün, gibt es rechts
   auf der Startseite des Repositorys unter **Releases** den Eintrag „Waage v1.0.0“ mit
   `Waage.ino.bin` und `Waage.ino.merged.bin`.

### Schritt 7: Die Waage einmalig auf v1.0.0 bringen

Die Waage kennt das Online-Update erst, wenn diese Firmware einmal drauf ist:

1. Aus dem Release `Waage.ino.bin` herunterladen.
2. An der Waage die Weboberfläche starten → im Browser **Firmware-Update → Firmware hochladen**
   → Datei wählen. Alternativ wie bisher per USB aus der Arduino-IDE.
3. Danach steht unter **Setup → Waage → Firmware** „Version 1.0.0“.

Ab jetzt geht jedes Update über das Internet.

---

## Jedes weitere Update

1. Code ändern (wie gewohnt in der Arduino-IDE, aber im Ordner `C:\Esp_Scale\Waage`).
   Tipp: In der Arduino-IDE direkt `C:\Esp_Scale\Waage\Waage.ino` öffnen, dann gibt es nur
   noch eine Kopie des Codes.
2. Nach Änderungen an `web/page.html` wie immer `python3 web/build.py` ausführen.
   Die Action macht das zur Sicherheit aber auch selbst.
3. GitHub Desktop: kurze Beschreibung eintragen → **Commit to main** → **Push origin**.
4. **History** → Rechtsklick auf den neuen Eintrag → **Create Tag…** → neue Nummer, z. B.
   `v1.0.1` → **Push origin**.
5. Etwa 5 Minuten warten, bis die Action grün ist.
6. An der Waage: **Setup → Waage → Firmware → Online suchen → Installieren**.
   Der Ring zeigt den Fortschritt. Danach spielt die Waage die Fertig-Melodie und startet neu.

Ein Push **ohne** Tag baut nichts und ändert nichts an der Waage. Du kannst also beliebig
oft zwischenspeichern.

### Versionsnummern

`vHAUPT.NEBEN.FEHLER`, also z. B. `v1.4.2`:

- **Fehler** (`v1.4.2 → v1.4.3`): nur Fehler behoben
- **Neben** (`v1.4.3 → v1.5.0`): neue Funktion
- **Haupt** (`v1.5.0 → v2.0.0`): große Umbauten

Die Waage installiert nur, was **neuer** ist als die eigene Version. Eine Nummer darf nie
zweimal vergeben werden.

### In der Arduino-IDE selbst kompilieren

Das geht weiterhin. Die Waage zeigt dann „lokal gebaut“, und jede Version auf GitHub gilt
als neuer. So kommst du mit „Online suchen“ jederzeit auf den offiziellen Stand zurück.

---

## Fehlersuche

| Meldung | Ursache und Abhilfe |
|---|---|
| Action rot bei „Kompilieren“: `LV_MEM_CUSTOM` / `lv_conf.h` | `lv_conf.h` fehlt im Repository oder ist nicht deine geänderte Fassung (Schritt 2). |
| Action rot: `Display_SPD2010.h: No such file` o. Ä. | Die Waveshare-Treiberdateien fehlen im Ordner `Waage/`. |
| Action rot mit Fehlern in LVGL-Dateien | Das Waveshare-Paket bringt evtl. eine angepasste LVGL mit. Dann deinen Ordner `Dokumente\Arduino\libraries\lvgl` komplett nach `Esp_Scale\libraries\lvgl` kopieren. Die Action nimmt ihn statt der Standardfassung. |
| Action rot: `HX711.h: No such file` | Die Bibliothek heißt anders als erwartet. In `firmware.yml` bei „Bibliotheken installieren“ den Namen aus dem Bibliotheksverwalter der IDE eintragen. |
| Action rot: `sketch too big` | Die Firmware ist größer als 3 MB. |
| Action rot bei „Release anlegen“: `Resource not accessible` | Repository → Settings → Actions → General → Workflow permissions → **Read and write permissions** → Save. Danach den Lauf mit „Re-run jobs“ wiederholen. |
| Waage: „Repository in config.h eintragen“ | Schritt 3, danach neu flashen. |
| Waage: „Kein WLAN“ | Kein gespeichertes Netz in Reichweite (Setup → Zeit & Funk → WLAN). |
| Waage: „Kein Release gefunden“ | Es gibt noch kein Release, das Repository ist privat, oder der Name in `config.h` stimmt nicht. |
| Waage: „Keine Firmware im Release“ | Die Action läuft noch, oder ist fehlgeschlagen. |
| Waage: „Download fehlgeschlagen“ | WLAN zu schwach oder abgebrochen. Einfach nochmal versuchen. Die alte Firmware bleibt erhalten. |
| Waage: „Update fehlgeschlagen“ | Datei war beschädigt. Die Waage startet mit der alten Version weiter. |

Ein abgebrochenes Update ist ungefährlich. Der ESP32 schreibt in den zweiten
Programmbereich und schaltet erst um, wenn die neue Datei vollständig und geprüft ist.

---

## Hinweise

- **Öffentlich heißt öffentlich:** Jeder kann den Code sehen. Passwörter stehen nicht im
  Code (WLAN-Zugänge liegen im Flash der Waage), aber prüfe das trotzdem vor dem ersten Hochladen.
- **Waveshare-Treiber:** Sie stammen aus der Waveshare-Demo. Prüfe, ob deren Lizenz das
  Veröffentlichen erlaubt. Wenn nicht, gibt es zwei Möglichkeiten: den Quellcode privat
  halten und nur die Releases in ein zweites, öffentliches Repository schieben (braucht einen
  Zugangs-Token in der Action), oder die `.bin` von Hand in ein öffentliches Release laden.
- **Sicherheit:** Die Waage prüft beim Download das HTTPS-Zertifikat nicht, weil die
  Arduino-IDE keinen Zertifikatsspeicher mitliefert. Die Datei selbst prüft der ESP32
  (Prüfsumme des Abbilds). Für eine Küchenwaage im Heimnetz ist das vertretbar.
- **Spracherkennung:** Baust du mit `USE_VOICE 1`, muss in `firmware.yml` bei `FQBN` das
  Partitionsschema `esp_sr_16` stehen. Das Sprachmodell wird dabei nicht mit aktualisiert.
- **Versionen fest:** Die Action baut mit esp32 **3.1.1** und LVGL **8.3.10**, genau wie
  deine IDE. Änderst du das in der IDE, auch oben in `firmware.yml` unter `env:` anpassen.
