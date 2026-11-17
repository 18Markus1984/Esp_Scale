#!/usr/bin/env python3
# ============================================================
#  Texte der Sprachausgabe je Stimmpaket (Charakter)
#
#  Jedes Paket hat zwei Listen:
#   STATUS   7 Ansagen der Waage (tara, gespeichert …), werden nach einem
#            Ereignis gesprochen – hier darf der Charakter sprechen
#   ZUSATZ  22 Ansagen der Sprachsteuerung ("Ja?", Hilfe, Akku …)
#            Bausteine wie "Ziel", "Akku", "Prozent", "Uhr", "noch", "zu viel"
#            werden mit Zahlen zusammengesetzt ("Ziel | 250 | Gramm | Jetzt
#            einfüllen") und bleiben deshalb kurz und neutral.
#  Zahlen und Einheiten (null … tausend, Gramm …) sind für alle gleich
#  (stimme_schneiden.py TEXTS / TEXTS_EN).
#
#  Regeln für die Sätze (damit das Sprachprogramm nach jeder Zeile eine
#  Pause macht und stimme_schneiden.py sauber schneiden kann):
#   - ein Satz pro Zeile, am Ende ein Punkt (nur die "Ja?"-Zeile mit ?)
#   - innerhalb der Zeile nur Komma und Gedankenstrich, kein Punkt
#   - Statusansagen höchstens etwa 4 s, "Ja?" unter 1,5 s
#
#    python3 stimme_texte.py                 alle Pakete als Textdateien nach Stimmtexte/
#    python3 stimme_texte.py "Bernd das Brot"  nur dieses Paket auf dem Bildschirm
# ============================================================
import os, sys

STATUS_KEYS_DE = ["tara", "gespeichert", "ziel_erreicht", "ueberlast", "topf_erkannt", "fertig", "waage_leer"]
STATUS_KEYS_EN = ["tare", "saved", "goal_reached", "overload", "pot_detected", "done", "scale_empty"]

# ---------------- neutral (Standard, z. B. _Zusatz, Drachenlord) ----------------
STATUS_DE = {
    "tara": "Tara.", "gespeichert": "Gespeichert.", "ziel_erreicht": "Ziel erreicht.", "ueberlast": "Überlast.",
    "topf_erkannt": "Topf erkannt.", "fertig": "Fertig.", "waage_leer": "Waage leer.",
}
HILFE_DE = "dann zum Beispiel Tara, wie viel, speichern, zweihundert Gramm, stopp, weiter, zurück, nochmal, lauter, leiser, Akku oder Uhrzeit"
ZUSATZ_DE = {
    # Bausteine (mit Zahlen zusammengesetzt)
    "ziel": "Ziel.",                       # Ziel | 250 | Gramm | einfuellen
    "einfuellen": "Jetzt einfüllen.",
    "akku": "Akku.",                       # Akku | 60 | Prozent | (laedt)
    "prozent": "Prozent.",
    "laedt": "wird geladen.",
    "uhr": "Uhr.",                         # 14 | Uhr | 5
    "lautstaerke": "Lautstärke.",          # Lautstärke | 80 | Prozent
    "noch": "noch.",                       # noch | 120 | Gramm
    "zu_viel": "zu viel.",                 # 20 | Gramm | zu viel
    "milliliter": "Milliliter.",
    # ganze Sätze
    "ja": "Ja?",
    "nicht_verst": "Das habe ich nicht verstanden, sag Hilfe.",
    "hilfe": "Sag Hallo Waage oder tipp zweimal oben auf die Waage, " + HILFE_DE + ".",
    "ziel_beendet": "Zielwiegen beendet.",
    "bereit": "Waage bereit.",
    "weiter": "Weiter.",
    "zurueck": "Zurück zur Startseite.",
    "geht_nicht": "Das geht hier gerade nicht.",
    "akku_leer": "Akku fast leer, bitte laden.",
    "timer_ab": "Der Timer ist abgelaufen.",
    "zeit_unbek": "Die Uhrzeit kenne ich noch nicht.",
    "schaltet_aus": "Die Waage schaltet sich gleich aus.",
}
STATUS_EN = {
    "tare": "Tare.", "saved": "Saved.", "goal_reached": "Target reached.", "overload": "Overload.",
    "pot_detected": "Pot detected.", "done": "Done.", "scale_empty": "Scale empty.",
}
HELP_EN = "then for example tare, how much, save, two hundred grams, stop, next, back, again, louder, quieter, battery or time"
ZUSATZ_EN = {
    "target": "Target.",
    "fill_now": "Fill in now.",
    "battery": "Battery.",
    "percent": "percent.",
    "charging": "charging.",
    "oclock": "o'clock.",
    "volume": "Volume.",
    "to_go": "to go.",                     # 120 | grams | to go
    "too_much": "too much.",
    "milliliters": "milliliters.",
    "yes": "Yes?",
    "not_underst": "I did not understand that, say help.",
    "help": "Say Hallo Waage or tap the top of the scale twice, " + HELP_EN + ".",
    "target_off": "Target weighing stopped.",
    "ready": "Scale ready.",
    "next": "Next.",
    "back": "Back to the start page.",
    "cannot": "That is not possible right now.",
    "batt_low": "Battery low, please charge.",
    "timer_done": "The timer has finished.",
    "time_unknown": "I do not know the time yet.",
    "power_off": "The scale will switch off soon.",
}


def _h(prefix, lang="de"):
    """Hilfe-Satz mit eigener Einleitung, die Befehle bleiben gleich."""
    return prefix + (HILFE_DE if lang == "de" else HELP_EN) + "."


# ---------------- Charaktere ----------------
# Nur was vom Standard abweicht. "lang" bestimmt die Liste (de/en).
CHAR = {
    # ======== Deutsch ========
    "Bernd das Brot": {"lang": "de", "neu": True, "info": "depressives Kastenbrot, müde, lustlos, schwarzhumorig", "status": {
        "tara": "Tara, null, genau wie meine Lebensfreude.",
        "gespeichert": "Gespeichert, als ob das irgendwen interessiert.",
        "ziel_erreicht": "Ziel erreicht, schön für dich, ich fühle nichts.",
        "ueberlast": "Überlast, das ist mir zu schwer, so wie eigentlich alles.",
        "topf_erkannt": "Ein Topf, der hat wenigstens einen Deckel, ich hab nur Kruste.",
        "fertig": "Fertig, darf ich jetzt wieder in meine Ecke.",
        "waage_leer": "Die Waage ist leer, so leer wie mein Innerstes.",
    }, "zusatz": {
        "einfuellen": "Jetzt einfüllen, wenn es denn unbedingt sein muss.",
        "laedt": "wird geladen, ich wünschte, das ginge bei mir auch.",
        "ja": "Was ist denn?",
        "nicht_verst": "Ich hab nichts verstanden und will es auch gar nicht, sag halt Hilfe.",
        "hilfe": "Na gut, sag Hallo Waage oder tipp zweimal oben, " + HILFE_DE + ", und danach lass mich bitte in Ruhe.",
        "ziel_beendet": "Zielwiegen beendet, endlich wieder Stille.",
        "bereit": "Waage bereit, leider.",
        "weiter": "Weiter, es hört ja nie auf.",
        "zurueck": "Zurück zur Startseite, alles wieder von vorn, wie immer.",
        "geht_nicht": "Das geht hier nicht, wie so vieles in meinem Leben.",
        "akku_leer": "Akku fast leer, ich bin auch erschöpft.",
        "timer_ab": "Der Timer ist abgelaufen, genau wie meine Hoffnung.",
        "zeit_unbek": "Die Uhrzeit kenne ich nicht, für mich ist eh immer derselbe graue Tag.",
        "schaltet_aus": "Ich schalte mich gleich aus, endlich Ruhe.",
    }},
    "Freundin": {"lang": "de", "neu": True, "info": "eigene Stimme, liebevoll und locker (Ordner frei benennbar)", "status": {
        "tara": "Alles auf null, Schatz, leg los.",
        "gespeichert": "Hab ich mir gemerkt, Schatz.",
        "ziel_erreicht": "Perfekt, genau richtig, du bist toll.",
        "ueberlast": "Uff, das ist zu schwer für mich, nimm bitte was runter.",
        "topf_erkannt": "Den Topf kenn ich, den zieh ich gleich ab.",
        "fertig": "Fertig, lass es dir schmecken, Schatz.",
        "waage_leer": "Die Waage ist leer, ich warte auf dich.",
    }, "zusatz": {
        "einfuellen": "Jetzt kannst du einfüllen, Schatz.",
        "laedt": "wird geladen, danke dir.",
        "ja": "Ja, Schatz?",
        "nicht_verst": "Das hab ich nicht verstanden, sag einfach Hilfe.",
        "hilfe": _h("Sag Hallo Waage oder tipp zweimal oben, "),
        "ziel_beendet": "Okay, Zielwiegen beendet.",
        "bereit": "Hallo Schatz, ich bin bereit.",
        "weiter": "Weiter geht's.",
        "geht_nicht": "Das geht hier gerade leider nicht.",
        "akku_leer": "Mein Akku ist fast leer, lädst du mich bitte auf.",
        "timer_ab": "Schatz, dein Timer ist abgelaufen.",
        "zeit_unbek": "Wie spät es ist, weiß ich leider noch nicht.",
        "schaltet_aus": "Ich schalte mich gleich aus, bis später, Schatz.",
    }},
    "Rick": {"lang": "de", "info": "arroganter Wissenschaftler, genervt, sarkastisch", "status": {
        "tara": "Tara, alles auf Null, das hätte sogar ein Affe geschafft.",
        "gespeichert": "Gespeichert, irgendwo zwischen meinen anderen genialen Ideen.",
        "ziel_erreicht": "Ziel erreicht, ganz ohne Portal, ich bin beeindruckt.",
        "ueberlast": "Überlast, du zerquetschst gerade meine wunderschöne Wägezelle.",
        "topf_erkannt": "Topf erkannt, die Waage ist schlauer als die meisten Leute hier.",
        "fertig": "Fertig, ich geh zurück in die Garage.",
        "waage_leer": "Waage leer, genau wie dein Terminkalender, Morty.",
    }, "zusatz": {
        "einfuellen": "Jetzt einfüllen, und zwar zügig.",
        "laedt": "wird geladen, sogar Akkus brauchen mal Pause von dir.",
        "ja": "Was denn?",
        "nicht_verst": "Sprich deutlich oder sag Hilfe, ich hab nicht ewig Zeit.",
        "hilfe": _h("Hör zu, sag Hallo Waage oder tipp zweimal oben, "),
        "ziel_beendet": "Zielwiegen beendet, war eh langweilig.",
        "bereit": "Waage bereit, wehe du langweilst mich.",
        "weiter": "Weiter, die Zeit läuft.",
        "zurueck": "Zurück zur Startseite, sauberer Neustart der Zeitlinie.",
        "geht_nicht": "Geht hier nicht, das bricht mindestens drei Naturgesetze.",
        "akku_leer": "Akku fast leer, steck mich an, bevor ich implodiere.",
        "timer_ab": "Timer abgelaufen, schneller als ein Sprung durchs Portal.",
        "zeit_unbek": "Uhrzeit unbekannt, Zeit ist sowieso nur ein Konstrukt.",
        "schaltet_aus": "Ich schalte gleich ab, genau wie du beim Zuhören.",
    }},
    "Roboter": {"lang": "de", "info": "Maschine, knapp und technisch (espeak-ng, stimme_erzeugen.py)", "status": {
        "tara": "Tara ausgeführt.",
        "gespeichert": "Messwert gespeichert.",
        "ziel_erreicht": "Zielwert erreicht.",
        "ueberlast": "Warnung, Überlast.",
        "topf_erkannt": "Behälter erkannt.",
        "fertig": "Vorgang abgeschlossen.",
        "waage_leer": "Plattform leer.",
    }, "zusatz": {
        "einfuellen": "Befüllung starten.",
        "laedt": "Ladevorgang aktiv.",
        "ja": "Eingabe?",
        "nicht_verst": "Befehl nicht erkannt, sage Hilfe.",
        "hilfe": _h("Bedienhinweis, sage Hallo Waage oder tippe zweimal oben, "),
        "ziel_beendet": "Zielmodus beendet.",
        "bereit": "System bereit.",
        "weiter": "Nächster Schritt.",
        "zurueck": "Rückkehr zur Startseite.",
        "geht_nicht": "Funktion hier nicht verfügbar.",
        "akku_leer": "Energie kritisch, bitte laden.",
        "timer_ab": "Timer abgelaufen.",
        "zeit_unbek": "Systemzeit unbekannt.",
        "schaltet_aus": "Abschaltung in Kürze.",
    }},
    "KI-Männlich": {"lang": "de", "info": "ruhig, sachlich, freundlich", "status": {
        "tara": "Tara, alles auf null.",
        "gespeichert": "Gespeichert, steht im Protokoll.",
        "ziel_erreicht": "Ziel erreicht, sehr gut.",
        "ueberlast": "Überlast, bitte etwas herunternehmen.",
        "topf_erkannt": "Topf erkannt, wird abgezogen.",
        "fertig": "Fertig, gut gemacht.",
        "waage_leer": "Die Waage ist leer.",
    }, "zusatz": {
        "einfuellen": "Jetzt einfüllen, ganz in Ruhe.",
        "nicht_verst": "Das habe ich leider nicht verstanden, sag einfach Hilfe.",
        "hilfe": _h("Sag Hallo Waage oder tipp zweimal oben, "),
        "ziel_beendet": "Zielwiegen ist beendet.",
        "bereit": "Die Waage ist bereit.",
        "weiter": "Weiter geht's.",
        "geht_nicht": "Das geht auf dieser Seite leider nicht.",
        "akku_leer": "Der Akku ist fast leer, bitte bald laden.",
        "timer_ab": "Dein Timer ist abgelaufen.",
        "zeit_unbek": "Die Uhrzeit kenne ich leider noch nicht.",
        "schaltet_aus": "Ich schalte mich gleich aus.",
    }},
    "KI-Weiblich": {"lang": "de", "info": "warm, herzlich", "status": {
        "tara": "Tara, wir fangen bei null an.",
        "gespeichert": "Gespeichert, ich hab es mir gemerkt.",
        "ziel_erreicht": "Ziel erreicht, perfekt.",
        "ueberlast": "Oh, das ist zu schwer für mich.",
        "topf_erkannt": "Ah, den Topf kenne ich.",
        "fertig": "Fertig, lass es dir schmecken.",
        "waage_leer": "Die Waage ist leer.",
    }, "zusatz": {
        "einfuellen": "Jetzt kannst du einfüllen.",
        "laedt": "wird gerade geladen.",
        "nicht_verst": "Das hab ich nicht verstanden, sag gern Hilfe.",
        "hilfe": _h("Sag einfach Hallo Waage oder tipp zweimal oben, "),
        "ziel_beendet": "Okay, Zielwiegen beendet.",
        "bereit": "Hallo, ich bin bereit.",
        "weiter": "Weiter, zum nächsten Schritt.",
        "zurueck": "Wir gehen zurück zur Startseite.",
        "geht_nicht": "Das klappt hier gerade leider nicht.",
        "akku_leer": "Mein Akku ist fast leer, lädst du mich bitte.",
        "timer_ab": "Dein Timer ist abgelaufen.",
        "zeit_unbek": "Wie spät es ist, weiß ich leider noch nicht.",
        "schaltet_aus": "Ich schalte mich gleich aus, bis bald.",
    }},
    "KI-AnimeGirl": {"lang": "de", "info": "überdreht fröhlich, begeistert", "status": {
        "tara": "Tara, alles schön auf null, juhu.",
        "gespeichert": "Gespeichert, ich hab es mir ganz fest gemerkt.",
        "ziel_erreicht": "Ziel erreicht, wir sind ein super Team.",
        "ueberlast": "Waaah, das ist viel zu schwer.",
        "topf_erkannt": "Oh, ein Topf, den kenn ich doch.",
        "fertig": "Fertig, du warst toll.",
        "waage_leer": "Huch, die Waage ist ganz leer.",
    }, "zusatz": {
        "einfuellen": "Jetzt einfüllen, los geht's.",
        "laedt": "wird geladen, ich tanke Energie.",
        "ja": "Jaaa?",
        "nicht_verst": "Hm, das hab ich nicht verstanden, sag einfach Hilfe.",
        "hilfe": _h("Okay, aufgepasst, sag Hallo Waage oder tipp zweimal oben, "),
        "ziel_beendet": "Zielwiegen beendet, gut gemacht.",
        "bereit": "Waage bereit, ich freu mich.",
        "weiter": "Weiter, auf zum nächsten Schritt.",
        "zurueck": "Zurück zur Startseite, hui.",
        "geht_nicht": "Oh nein, das geht hier gerade nicht.",
        "akku_leer": "Mein Akku ist fast leer, bitte aufladen.",
        "timer_ab": "Piep piep, dein Timer ist abgelaufen.",
        "zeit_unbek": "Die Uhrzeit weiß ich leider nicht, sorry.",
        "schaltet_aus": "Ich mach gleich ein Nickerchen, bis später.",
    }},
    # echte Person: bewusst nur die neutralen Standardsätze
    "Drachenlord": {"lang": "de", "info": "nur neutrale Standardsätze", "status": {}, "zusatz": {}},

    # ======== Englisch ========
    "Deadpool-EN": {"lang": "en", "info": "breaks the fourth wall, cheeky", "status": {
        "tare": "Tare, back to zero, like my patience with sequels.",
        "saved": "Saved, it goes in the scrapbook next to my drawings.",
        "goal_reached": "Target reached, somebody start the slow clap.",
        "overload": "Overload, that's heavier than my emotional baggage.",
        "pot_detected": "Pot detected, real supporting cast energy.",
        "done": "Done, and nobody even got hurt, how boring.",
        "scale_empty": "The scale is empty, kind of like this scene without me.",
    }, "zusatz": {
        "fill_now": "Fill it in now, I'll narrate.",
        "charging": "charging, give me a sec to regenerate.",
        "yes": "Talk to me?",
        "not_underst": "Didn't catch that, I'm not a mind reader, say help.",
        "help": _h("Okay reader, say Hallo Waage or tap the top twice, ", "en"),
        "target_off": "Target weighing stopped, plot twist.",
        "ready": "Scale ready, cue the opening credits.",
        "next": "Next, skipping the boring part.",
        "back": "Back to the start page, smash cut.",
        "cannot": "Can't do that here, the writers said no.",
        "batt_low": "Battery low, even I can't heal that, charge me.",
        "timer_done": "Time's up, and that's the end of the montage.",
        "time_unknown": "No idea what time it is, I lost track during the last fight scene.",
        "power_off": "Switching off soon, stay for the scene after the credits.",
    }},
    "Homer-EN": {"lang": "en", "info": "lazy, always hungry", "status": {
        "tare": "Tare, zero, just like the snacks left in the fridge.",
        "saved": "Saved, I'm basically a genius now.",
        "goal_reached": "Target reached, that calls for a celebration snack.",
        "overload": "Overload, even I don't eat that much in one sitting.",
        "pot_detected": "Ooh, a pot, I hope it's full of something tasty.",
        "done": "Done, I'm going back to the couch.",
        "scale_empty": "The scale is empty, just like my lunchbox.",
    }, "zusatz": {
        "fill_now": "Fill it in now, and save some for me.",
        "charging": "charging, like me after a big lunch.",
        "yes": "Huh, what?",
        "not_underst": "I didn't get that, I wasn't really listening, say help.",
        "help": _h("Okay, say Hallo Waage or poke the top twice, ", "en"),
        "target_off": "Target weighing over, time for a break.",
        "ready": "The scale is ready, but I'm not.",
        "next": "Next, ugh, more work.",
        "back": "Back to the start page, my favorite part.",
        "cannot": "Can't do that here, trust me, I tried.",
        "batt_low": "Battery low, I need a snack too, charge me.",
        "timer_done": "The timer's done, so it's probably food time.",
        "time_unknown": "I don't know the time, but it's almost lunchtime.",
        "power_off": "Switching off soon, nap time.",
    }},
    "Indy-EN": {"lang": "en", "info": "adventurous archaeologist", "status": {
        "tare": "Tare, back to zero, steady hands now.",
        "saved": "Saved, it's in my field journal.",
        "goal_reached": "Target reached, a perfect counterweight.",
        "overload": "Overload, careful, that's enough to set off every trap in here.",
        "pot_detected": "A pot, early kitchen era, remarkably well preserved.",
        "done": "Done, now let's get out of here.",
        "scale_empty": "The scale is empty, somebody looted it first.",
    }, "zusatz": {
        "fill_now": "Fill it in now, slow and steady.",
        "charging": "charging, the expedition can wait.",
        "not_underst": "I didn't catch that, keep it simple and say help.",
        "help": _h("Listen closely, say Hallo Waage or tap the top twice, ", "en"),
        "target_off": "Target weighing stopped, we'll come back for it.",
        "ready": "Scale ready, let's start digging.",
        "next": "Next, keep moving.",
        "back": "Back to the start page, back to base camp.",
        "cannot": "Not here, that path is a dead end.",
        "batt_low": "Battery low, we won't get far without a charge.",
        "timer_done": "Time's up, the torch is burning out.",
        "time_unknown": "I don't know the time, I lost my watch in the last temple.",
        "power_off": "Switching off soon, the light is fading.",
    }},
    "Morty-EN": {"lang": "en", "info": "nervous, unsure, sweet", "status": {
        "tare": "Tare, okay, it's zero now, I think that's right.",
        "saved": "Saved, oh man, I really hope it was the right number.",
        "goal_reached": "Target reached, wow, I actually did it.",
        "overload": "Overload, oh no, that's way too heavy, take it off.",
        "pot_detected": "Oh, a pot, the scale recognized it, that's pretty cool.",
        "done": "Done, can I go do my homework now.",
        "scale_empty": "The scale is empty, I checked, there's really nothing.",
    }, "zusatz": {
        "fill_now": "Fill it in now, carefully, please.",
        "charging": "charging, phew, that's a relief.",
        "yes": "Uh, yes?",
        "not_underst": "Sorry, I didn't get that, maybe say help.",
        "help": _h("Okay, um, say Hallo Waage or tap the top twice, ", "en"),
        "target_off": "Target weighing stopped, that's okay too.",
        "ready": "Scale ready, I'm kind of nervous.",
        "next": "Next, okay, okay.",
        "back": "Back to the start page, safe and sound.",
        "cannot": "I can't do that here, I'm sorry.",
        "batt_low": "Battery low, please charge it before something bad happens.",
        "timer_done": "The timer's done, oh man, I hope nothing burned.",
        "time_unknown": "I don't know what time it is, I lost track again.",
        "power_off": "Switching off soon, okay, bye.",
    }},
    "Optimus-EN": {"lang": "en", "info": "noble leader, calm and solemn", "status": {
        "tare": "Tare complete, we begin from zero.",
        "saved": "Measurement saved, it shall be remembered.",
        "goal_reached": "Target reached, your precision honors us all.",
        "overload": "Overload, this weight is beyond my limits, lighten the load.",
        "pot_detected": "Cookware recognized, a trusted ally in the kitchen.",
        "done": "Task complete, rest now, my friend.",
        "scale_empty": "The platform is empty, I stand watch.",
    }, "zusatz": {
        "fill_now": "Begin filling now.",
        "charging": "charging, my energy returns.",
        "yes": "I am listening?",
        "not_underst": "I did not understand, speak the word help.",
        "help": _h("Hear me, say Hallo Waage or tap the top twice, ", "en"),
        "target_off": "Target weighing ended, stand down.",
        "ready": "Scale ready, awaiting your command.",
        "next": "Onward, to the next step.",
        "back": "Returning to the start page.",
        "cannot": "That is not possible here, choose another path.",
        "batt_low": "My energy is fading, recharge me soon.",
        "timer_done": "The timer has run out, the moment has come.",
        "time_unknown": "The time is unknown to me, even my sensors cannot tell.",
        "power_off": "I will power down soon, until we meet again.",
    }},
    "Patrick-EN": {"lang": "en", "info": "cheerful, not the brightest", "status": {
        "tare": "Tare, I made it zero, that was easy.",
        "saved": "Saved, I'll remember it forever, or until lunch.",
        "goal_reached": "Target reached, I'm so proud of us.",
        "overload": "Overload, wow, that's heavier than my rock.",
        "pot_detected": "A pot, hi pot, nice to meet you.",
        "done": "Done, now it's time to do nothing.",
        "scale_empty": "The scale is empty, I don't see anything, and I looked really hard.",
    }, "zusatz": {
        "fill_now": "Fill it in now, yay.",
        "charging": "charging, it's nap time for the battery.",
        "yes": "Huh?",
        "not_underst": "I didn't understand, but that happens a lot, say help.",
        "help": _h("Ooh, I know this one, say Hallo Waage or tap the top twice, ", "en"),
        "target_off": "Target weighing stopped, I think we won.",
        "ready": "Scale ready, I'm ready too, I think.",
        "next": "Next, ooh, I love surprises.",
        "back": "Back to the start page, I like it here.",
        "cannot": "That doesn't work here, I tried with my brain.",
        "batt_low": "Battery low, it's tired, like me.",
        "timer_done": "The timer's done, beep beep.",
        "time_unknown": "I don't know the time, I never do.",
        "power_off": "Switching off soon, sleepy time.",
    }},
    "Peter-EN": {"lang": "en", "info": "loud, clueless family dad", "status": {
        "tare": "Tare, zero, just like the planning I put into my day.",
        "saved": "Saved, put that one on the fridge.",
        "goal_reached": "Target reached, best day since the last best day.",
        "overload": "Overload, whoa, did somebody put the whole couch on here.",
        "pot_detected": "A pot, sweet, somebody's making dinner.",
        "done": "Done, I'm heading to the bar with the guys.",
        "scale_empty": "The scale's empty, kind of like my work inbox, because I never check it.",
    }, "zusatz": {
        "fill_now": "Fill it in now, go go go.",
        "charging": "charging, the scale's on a coffee break.",
        "yes": "Yeah, what?",
        "not_underst": "I have no idea what you just said, say help.",
        "help": _h("Alright, listen up, say Hallo Waage or tap the top twice, ", "en"),
        "target_off": "Target weighing stopped, that's enough of that.",
        "ready": "Scale ready, let's do this.",
        "next": "Next, keep it rolling.",
        "back": "Back to the start page, like nothing ever happened.",
        "cannot": "Can't do that here, not even I'm that dumb.",
        "batt_low": "Battery low, plug me in before I pass out.",
        "timer_done": "The timer went off, I hope it's pie.",
        "time_unknown": "I don't know the time, I just go by when I'm hungry.",
        "power_off": "Switching off soon, nighty night.",
    }},
    "Rick-EN": {"lang": "en", "info": "arrogant scientist, annoyed", "status": {
        "tare": "Tare, everything back to zero, even you could follow that.",
        "saved": "Saved, filed between my other brilliant ideas.",
        "goal_reached": "Target reached, no portal needed, I'm impressed.",
        "overload": "Overload, you're crushing my beautiful load cell.",
        "pot_detected": "Pot detected, this scale is smarter than most people I know.",
        "done": "Done, I'm going back to the garage.",
        "scale_empty": "Scale's empty, just like your schedule, Morty.",
    }, "zusatz": {
        "fill_now": "Fill it in now, and make it quick.",
        "charging": "charging, even batteries need a break from you.",
        "yes": "What?",
        "not_underst": "Speak clearly or say help, I don't have all day.",
        "help": _h("Listen, say Hallo Waage or tap the top twice, ", "en"),
        "target_off": "Target weighing over, it was boring anyway.",
        "ready": "Scale ready, don't bore me.",
        "next": "Next, keep up.",
        "back": "Back to the start page, clean reset of the timeline.",
        "cannot": "Can't do that here, it breaks at least three laws of physics.",
        "batt_low": "Battery low, plug me in before I implode.",
        "timer_done": "Timer's done, faster than a jump through a portal.",
        "time_unknown": "Time unknown, time is a construct anyway.",
        "power_off": "Switching off soon, just like your attention span.",
    }},
    "Sheldon-EN": {"lang": "en", "info": "pedantic genius, rule-loving", "status": {
        "tare": "Tare, zero, a scientifically sound starting point.",
        "saved": "Saved, and backed up in my memory, which is flawless.",
        "goal_reached": "Target reached, within an acceptable margin of error.",
        "overload": "Overload, you have exceeded the rated capacity, this is unacceptable.",
        "pot_detected": "Pot detected, it belongs in its designated spot.",
        "done": "Done, the procedure was carried out correctly, for once.",
        "scale_empty": "The scale is empty, which is its natural resting state.",
    }, "zusatz": {
        "fill_now": "Fill it in now, at a reasonable pace.",
        "charging": "charging, a sensible decision.",
        "yes": "I'm listening?",
        "not_underst": "That was not a valid command, please say help.",
        "help": _h("Allow me to explain, say Hallo Waage or tap the top twice, ", "en"),
        "target_off": "Target weighing stopped, how unscientific.",
        "ready": "Scale ready, calibration is impeccable.",
        "next": "Next, in the correct order.",
        "back": "Returning to the start page, as is customary.",
        "cannot": "That is not possible here, I checked the rules.",
        "batt_low": "Battery low, charging is not optional.",
        "timer_done": "The timer has expired, punctuality matters.",
        "time_unknown": "I do not know the time, which is deeply unsettling.",
        "power_off": "Switching off soon, sleep is essential for cognitive function.",
    }},
    "Roboter-EN": {"lang": "en", "info": "machine, short and technical (espeak-ng, stimme_erzeugen.py)", "status": {
        "tare": "Tare executed.",
        "saved": "Value saved.",
        "goal_reached": "Target value reached.",
        "overload": "Warning, overload.",
        "pot_detected": "Container detected.",
        "done": "Process complete.",
        "scale_empty": "Platform empty.",
    }, "zusatz": {
        "fill_now": "Start filling.",
        "charging": "charging active.",
        "yes": "Input?",
        "not_underst": "Command not recognized, say help.",
        "help": _h("Instruction, say Hallo Waage or tap the top twice, ", "en"),
        "target_off": "Target mode terminated.",
        "ready": "System ready.",
        "next": "Next step.",
        "back": "Returning to start page.",
        "cannot": "Function not available here.",
        "batt_low": "Energy critical, please charge.",
        "timer_done": "Timer expired.",
        "time_unknown": "System time unknown.",
        "power_off": "Shutdown imminent.",
    }},
}


LIESMICH = """# Stimmtexte

Eine Textdatei pro Stimmpaket (erzeugt mit `python3 stimme_texte.py`, Quelle der Texte: `stimme_texte.py`).
`_ALLE_STIMMEN.txt` enthält alle Pakete hintereinander.

**Jede Zeile = eine WAV-Datei.** Jede Zeile endet mit einem Punkt (nur die „Ja?“-Zeile mit Fragezeichen),
innerhalb einer Zeile stehen nur Kommas. So setzt das Sprachprogramm nach jeder Zeile eine Pause und
`stimme_schneiden.py` findet die Stücke. Die `#`-Zeilen nicht mitlesen lassen, Abschnitte einzeln kopieren.

| Abschnitt | Schneiden mit |
|---|---|
| Statusansagen (7) | `python stimme_schneiden.py status.mp3 "Paket" --status` (EN zusätzlich `--lang en`) |
| Sprachsteuerung (22) | `python stimme_schneiden.py ansagen.mp3 "Paket" --ansagen` (EN zusätzlich `--lang en`) |
| Wortliste (50, nur neue Pakete wie Bernd das Brot) | `python stimme_schneiden.py aufnahme.mp3 "Paket"` |

Gibt das Sprachprogramm eine Datei pro Zeile aus: Ordner statt Aufnahme angeben und `--ordner` dazu
(Dateien werden alphabetisch zugeordnet). Lange Sätze mit Kommas lieber so, dann schneidet nichts falsch.
Die Bausteine am Anfang der Sprachsteuerung (Ziel, Akku, Prozent, Uhr, noch, zu viel …) werden mit Zahlen
zusammengesetzt und sollten kurz und neutral klingen. Roboter und Roboter-EN erzeugt
`stimme_erzeugen.py … --charakter Roboter` direkt mit espeak-ng.
"""


def texte(paket):
    """(Sprache, Status-Dict, Zusatz-Dict) eines Pakets; unbekannte Pakete: neutral (Sprache aus -EN)."""
    c = CHAR.get(paket)
    lang = c["lang"] if c else ("en" if paket.upper().endswith("-EN") else "de")
    st = dict(STATUS_EN if lang == "en" else STATUS_DE)
    zu = dict(ZUSATZ_EN if lang == "en" else ZUSATZ_DE)
    if c:
        st.update(c.get("status", {}))
        zu.update(c.get("zusatz", {}))
    return lang, st, zu


def pruefen():
    """Regeln prüfen: ein Satz pro Zeile, Punkt am Ende, kein Punkt mitten drin."""
    fehler = []
    for p in list(CHAR) + ["(neutral DE)", "(neutral EN)"]:
        if p.startswith("("):
            lang = "en" if "EN" in p else "de"
            st, zu = (STATUS_EN, ZUSATZ_EN) if lang == "en" else (STATUS_DE, ZUSATZ_DE)
        else:
            lang, st, zu = texte(p)
        keys = STATUS_KEYS_EN if lang == "en" else STATUS_KEYS_DE
        assert list(st) == keys, (p, list(st))
        for k, t in list(st.items()) + list(zu.items()):
            end_ok = t.endswith("?") if k in ("ja", "yes") else t.endswith(".")
            inner = t[:-1]
            if not end_ok or any(ch in inner for ch in ".!?…;:"):
                fehler.append(f"{p}/{k}: {t}")
            if k in ("ja", "yes") and len(t.split()) > 3:
                fehler.append(f"{p}/{k} zu lang: {t}")
    return fehler


def datei_text(paket):
    """Text zum Kopieren ins Sprachprogramm: Abschnitte, darin eine Zeile pro Datei."""
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import stimme_schneiden as S
    lang, st, zu = texte(paket)
    c = CHAR.get(paket, {})
    out = [f"# {paket} ({'Englisch' if lang == 'en' else 'Deutsch'})"]
    if c.get("info"):
        out.append(f"# Charakter: {c['info']}")
    out.append("# Jede Zeile = eine WAV-Datei. Abschnitte einzeln kopieren, die #-Zeilen nicht mitlesen lassen.")
    out.append("")
    if c.get("neu"):
        words = S.TEXTS_EN if lang == "en" else S.TEXTS
        order = S.ORDER_EN if lang == "en" else S.ORDER
        status_keys = STATUS_KEYS_EN if lang == "en" else STATUS_KEYS_DE
        out.append(f"## 1. Wortliste ({len(words)} Zeilen) -> python stimme_schneiden.py aufnahme.mp3 \"{paket}\"")
        out.append("## Zahlen und Einheiten neutral sprechen, die 7 Statusansagen stehen schon im Charakter darin")
        for k, w in zip(order, words):
            out.append(st[k] if k in st else (w + "."))
        out.append("")
        n = 2
    else:
        out.append(f"## 1. Statusansagen (7 Zeilen) -> python stimme_schneiden.py aufnahme.mp3 \"{paket}\" --status")
        out += [st[k] for k in (STATUS_KEYS_EN if lang == "en" else STATUS_KEYS_DE)]
        out.append("")
        n = 2
    lflag = " --lang en" if lang == "en" else ""
    out.append(f"## {n}. Sprachsteuerung ({len(zu)} Zeilen) -> python stimme_schneiden.py aufnahme.mp3 \"{paket}\" --ansagen{lflag}")
    out.append("## (die ersten Zeilen sind Bausteine, die mit Zahlen zusammengesetzt werden: kurz und neutral sprechen)")
    out += list(zu.values())
    out.append("")
    return "\n".join(out)


if __name__ == "__main__":
    f = pruefen()
    if f:
        print("Regelverstöße:\n  " + "\n  ".join(f))
        sys.exit(1)
    if len(sys.argv) > 1:
        print(datei_text(sys.argv[1]))
        sys.exit(0)
    here = os.path.dirname(os.path.abspath(__file__))
    out_dir = os.path.join(here, "Stimmtexte")
    os.makedirs(out_dir, exist_ok=True)
    alle = []
    for p in CHAR:
        t = datei_text(p)
        alle.append(t)
        with open(os.path.join(out_dir, p + ".txt"), "w", encoding="utf-8") as fh:
            fh.write(t)
    with open(os.path.join(out_dir, "_ALLE_STIMMEN.txt"), "w", encoding="utf-8") as fh:
        fh.write(("\n" + "=" * 70 + "\n\n").join(alle))
    with open(os.path.join(out_dir, "LIESMICH.md"), "w", encoding="utf-8") as fh:
        fh.write(LIESMICH)
    print(len(CHAR), "Dateien in", out_dir)
