#pragma once
// ============================================================
//  Unterseiten (werden beim Öffnen erzeugt, beim Verlassen gelöscht)
// ============================================================
#include <lvgl.h>

// Jede Seite hat eine Funktion, die sie erzeugt und zurückgibt
typedef lv_obj_t *(*page_create_fn)();

lv_obj_t *page_level_create();                        // Wasserwaage
lv_obj_t *page_ziel_create();                         // Zielgewicht (Parkpiepser)
lv_obj_t *page_portion_create();                      // Portionieren (Teig gleich teilen)
lv_obj_t *page_web_quick_create();                    // Weboberfläche aus der Systemliste: startet sie sofort, Fertig -> Wiegeseite
lv_obj_t *page_wlan_qr_create();                      // QR-Codes für WLAN und Weboberfläche
lv_obj_t *page_timer_create();                        // Küchentimer (drei Timer)
lv_obj_t *page_tassen_create();                       // Tassen & Löffel umrechnen
lv_obj_t *page_mix_create();                          // 2K-Harz/Silikon mischen
lv_obj_t *page_langzeit_create();                     // Langzeitmessung auf SD
lv_obj_t *page_muenzen_create();                      // Münzzähler / Kassensturz
lv_obj_t *page_blind_create();                        // Spiel Blindgießen
lv_obj_t *page_halb_create();                         // Spiel Halbe-Halbe (in der Mitte teilen)
lv_obj_t *page_zaehlen_create();                      // Zählen + Bluetooth
lv_obj_t *page_porto_create();                        // Portoklasse
lv_obj_t *page_rezept_create();                       // Rezepte von der SD-Karte
lv_obj_t *page_spiel_create();                        // Schätzspiel
lv_obj_t *page_spule_create();                        // Filament-Restmenge
lv_obj_t *page_msa_create();                          // Messmittelfähigkeit (Verfahren 1)
lv_obj_t *page_akku_create();                         // Ladestand und Verlauf
lv_obj_t *page_mic_create();                          // Mikrofon-Pegeltest
lv_obj_t *page_cocktail_create();                     // Cocktails in ml mixen
lv_obj_t *page_trink_create();                        // Trinkspiel (Schluckgröße schätzen)
lv_obj_t *page_players_create(page_create_fn back);   // Spielernamen (gemeinsam für alle Spiele)
lv_obj_t *page_toepfe_create();                       // Töpfe verwalten
lv_obj_t *page_protokoll_create();                    // Protokoll (heute)
lv_obj_t *page_setup_create();                        // Einstellungen (Übersicht)
lv_obj_t *page_setup_back();                          // zurück in die zuletzt offene Setup-Gruppe
lv_obj_t *page_display_create();                      // Display drehen (Feinausrichtung)
lv_obj_t *page_kalib_create();                        // Kalibrierung in 3 Schritten
lv_obj_t *page_refset_create();                       // Prüfgewicht: Einstellungen und letzte Prüfung
lv_obj_t *page_refcheck_create();                     // Prüfgewicht: Prüfung durchführen
lv_obj_t *page_refdue_create();                       // Prüfgewicht: Erinnerung „fällig“
lv_obj_t *page_setup_cat_create(int cat);             // Setup-Kategorie (0 Wiegen … 3 Waage)
lv_obj_t *page_bluetooth_create();                    // Anleitung Bluetooth-Kopplung
lv_obj_t *page_bluetooth_setup_create();              // dieselbe, aus dem Setup
lv_obj_t *page_porto_setup_create();                  // Portoklassen bearbeiten
lv_obj_t *page_level_setup_create();                  // Libelle kalibrieren
lv_obj_t *page_placeholder_create(const char *title); // Platzhalter für kommende Seiten
