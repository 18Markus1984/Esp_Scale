import asyncio, json, sys, os, re
from playwright.async_api import async_playwright
LANG = sys.argv[1] if len(sys.argv) > 1 else "en"
OUT = f"/home/claude/webtest/{LANG}"
os.makedirs(OUT, exist_ok=True)
COLLECT = """() => { const out=new Set(); const w=document.createTreeWalker(document.body,NodeFilter.SHOW_TEXT);let n;
 while(n=w.nextNode()){const p=n.parentElement; if(p&&p.closest('script,style,[translate=no]'))continue; const t=n.nodeValue.trim(); if(t) out.add(t);}
 document.querySelectorAll('[placeholder],[title],[aria-label]').forEach(e=>['placeholder','title','aria-label'].forEach(a=>{if(e.hasAttribute(a)&&!e.closest('[translate=no]'))out.add(e.getAttribute(a))}));
 document.querySelectorAll('datalist option').forEach(o=>out.add('DL:'+o.value));
 out.add('TITLE:'+document.title); return [...out]; }"""
async def main():
    async with async_playwright() as p:
        b = await p.chromium.launch()
        pg = await b.new_page(viewport={"width": 390, "height": 900})
        errs = []; texts = set(); dialogs = []
        pg.on("pageerror", lambda e: errs.append(str(e)+" | "+str(getattr(e,"stack",""))[:400]))
        pg.on("console", lambda m: m.type == "error" and errs.append(m.text))
        async def on_dialog(d):
            dialogs.append(d.message); await d.dismiss()
        pg.on("dialog", lambda d: asyncio.ensure_future(on_dialog(d)))
        await pg.goto("http://127.0.0.1:8765/")
        await pg.wait_for_timeout(1500)
        n = [0]
        async def snap(name):
            await pg.wait_for_timeout(700)
            for t in await pg.evaluate(COLLECT): texts.add(t)
            n[0] += 1
            await pg.screenshot(path=f"{OUT}/{n[0]:02d}_{name}.png", full_page=True)
        async def go(h):
            await pg.evaluate(f"location.hash='#/{h}'"); await pg.wait_for_timeout(900)
        async def js(c):
            try: await pg.evaluate(c)
            except Exception as e: errs.append(f"JS {c}: {e}")
            await pg.wait_for_timeout(600)
        await go(""); await snap("home")
        await go("ziel"); await snap("ziel"); await js("P.ziel.start()"); await pg.wait_for_timeout(1500); await snap("ziel_run"); await js("P.ziel.save()"); await snap("ziel_toast"); await js("P.ziel.stop()")
        await go("rezept"); await snap("rezept"); await js("P.rezept.open(0)"); await pg.wait_for_timeout(600); await snap("rezept_prev")
        await js("P.rezept.begin()"); await pg.wait_for_timeout(1500); await snap("rezept_step"); await js("P.rezept.finish()"); await snap("rezept_fertig")
        await go("spule"); await pg.wait_for_timeout(1200); await snap("spule"); await js("P.spule.add()"); await js("P.spule.save()"); await snap("spule2")
        await go("zaehlen"); await pg.wait_for_timeout(1000); await snap("zaehlen"); await js("P.zaehlen.ref()"); await pg.wait_for_timeout(1500); await snap("zaehlen_run"); await js("P.zaehlen.add()"); await js("P.zaehlen.ble()"); await snap("zaehlen_list"); await js("ls('piece',0)")
        await go("porto"); await pg.wait_for_timeout(1500); await snap("porto")
        await go("spiel"); await pg.wait_for_timeout(1200); await snap("spiel"); await js("P.spiel.start()"); await pg.wait_for_timeout(1500); await snap("spiel_runde")
        await js("P.spiel.reveal()"); await pg.wait_for_timeout(2000); await snap("spiel_auf"); await js("P.spiel.round=4;P.spiel.board()"); await snap("spiel_end")
        await go("cocktail"); await snap("cocktail"); await js("P.cocktail.open(0)"); await snap("cocktail_prev"); await js("P.cocktail.begin()"); await pg.wait_for_timeout(1200); await snap("cocktail_step"); await js("P.cocktail.finish()"); await snap("cocktail_fertig")
        await go("cocktails"); await snap("cocktails"); await js("P.cocktails.neu()"); await pg.wait_for_timeout(800); await snap("cocktail_editor"); await js("P.cocktails.save()"); await js("P.cocktails.open(0)"); await js("P.cocktails.del()")
        await go("trink"); await pg.wait_for_timeout(1200); await snap("trink"); await js("P.trink.mode=1;P.trink.setup()"); await snap("trink_ko"); await js("P.trink.start()"); await snap("trink_turn")
        await js("P.trink.sip=[30,20,55];P.trink.player=0;P.trink.reveal()"); await snap("trink_reveal"); await js("P.trink.roundEnd()"); await snap("trink_runde"); await js("P.trink.board()"); await snap("trink_end")
        await js("P.trink.mode=0;P.trink.setup();P.trink.start();P.trink.sip=[30,20,55];P.trink.player=0;P.trink.reveal()"); await snap("trink_ziel_reveal"); await js("P.trink.st='setup'")
        async def W(g=None):
            await pg.evaluate("fetch('/mock/w"+("" if g is None else "?g=%s"%g)+"')"); await pg.wait_for_timeout(1500)
        # ---- neue Modi ----
        await W(0.3); await js("tare()"); await W(1200); await go("portion"); await pg.wait_for_timeout(1200); await snap("portion"); await js("P.portion.start()")
        await W(0.5); await W(0.5); await W(101); await snap("portion_run"); await W(0.5); await W(96); await W(96); await W(0.5); await snap("portion_run2"); await js("P.portion.finish()"); await snap("portion_ende")
        await go("timer"); await pg.wait_for_timeout(1300); await snap("timer"); await js("$('tn0').value='Nudeln';P.timer.start(0)"); await pg.wait_for_timeout(1600); await snap("timer_run")
        await js("fetch('/api/timer?i=2&s=1')"); await pg.wait_for_timeout(2500); await snap("timer_ring"); await go(""); await pg.wait_for_timeout(1500); await snap("home_alarm"); await js("get('/api/timer?ringoff=1')")
        await W(0.3); await go("tassen"); await js("tare()"); await W(250); await snap("tassen"); await js("P.tassen.step(8)"); await pg.wait_for_timeout(1200); await snap("tassen_ziel")
        await W(300); await go("mix"); await snap("mix"); await js("P.mix.start()"); await W(400.5); await snap("mix_a"); await js("P.mix.aDone()"); await W(407); await snap("mix_b"); await W(410.55); await pg.wait_for_timeout(1500); await snap("mix_fertig")
        await W(0); await go("muenzen"); await js("tare()"); await W(85.2); await snap("muenzen"); await js("P.muenzen.add()"); await snap("muenzen_summe"); await js("P.muenzen.clear()")
        await W(0.3); await go("blind"); await pg.wait_for_timeout(1200); await snap("blind"); await js("P.blind.start()"); await W(0.3); await W(310); await W(462); await snap("blind_zug")
        await js("P.blind.poured=[148,160,139];P.blind.turn=P.blind.players.length-1;P.blind.phase=2;P.blind.next()"); await pg.wait_for_timeout(2200); await snap("blind_ende")
        await W(0.3); await go("halb"); await pg.wait_for_timeout(1200); await snap("halb"); await js("P.halb.start()"); await W(0.3); await W(0.3); await W(212); await W(212); await W(0.3); await W(0.3); await W(101); await W(101); await pg.wait_for_timeout(1500); await snap("halb_zug")
        await js("P.halb.whole=[212,180,150];P.halb.half=[101,95,60];P.halb.turn=P.halb.players.length-1;P.halb.next()"); await snap("halb_ende")
        await W(); await go("messung"); await pg.wait_for_timeout(1500); await snap("messung_start"); await js("P.messung.lt(1)"); await pg.wait_for_timeout(2500); await snap("messung_laeuft")
        await js("fetch('/api/timer?i=1&s=754')"); await go(""); await pg.wait_for_timeout(1800); await snap("home_dienste"); await js("P.messung.lt(0);fetch('/api/timer?i=1&stop=1')")
        await go("rezepte"); await pg.wait_for_timeout(800); await snap("rezepte"); await js("P.rezepte.edit(0)"); await pg.wait_for_timeout(800); await snap("rezept_edit"); await js("P.rezepte.edit(null)"); await snap("rezept_neu"); await js("P.rezepte.del()")
        await go("toepfe"); await pg.wait_for_timeout(800); await snap("toepfe"); await js("P.toepfe.show(1)"); await snap("spulen"); await js("P.toepfe.add()"); await js("P.toepfe.del(0)")
        await go("protokoll"); await pg.wait_for_timeout(1200); await snap("protokoll")
        await go("akku"); await pg.wait_for_timeout(1200); await snap("akku")
        await go("msa"); await snap("msa"); await js("P.msa.start()"); await pg.wait_for_timeout(1500); await snap("msa_run")
        await js("P.msa.vals=Array.from({length:25},(_,i)=>500+Math.sin(i)*0.3);P.msa.result()"); await snap("msa_result"); await js("P.msa.st='setup'")
        await go("dateien"); await pg.wait_for_timeout(1000); await snap("dateien"); await js("P.dateien.go('/Waage/Rezepte')"); await snap("dateien2"); await js("P.dateien.del('/Waage/Rezepte/x.txt')"); await js("P.dateien.mkdir()"); await js("P.dateien.upload()")
        await go("portoklassen"); await pg.wait_for_timeout(800); await js("P.portoklassen.add()"); await snap("portoklassen")
        await go("spieler"); await pg.wait_for_timeout(800); await snap("spieler")
        await go("firmware"); await pg.wait_for_timeout(800); await snap("firmware"); await js("P.firmware.ota('check=1')"); await pg.wait_for_timeout(2500); await snap("firmware_neu")
        await js("P.firmware.ota('install=1')"); await pg.wait_for_timeout(1800); await snap("firmware_laden")
        await go("messung"); await pg.wait_for_timeout(1500); await snap("messung")
        await go("einstellungen"); await pg.wait_for_timeout(1000); await snap("einstellungen")
        await js("$('w_ssid').value='Ferienhaus';$('w_pass').value='geheim123';P.einstellungen.wadd()"); await pg.wait_for_timeout(800); await js("P.einstellungen.wdel(1)"); await js("P.einstellungen.wadd()")
        await pg.evaluate("document.getElementById('wl').scrollIntoView()"); await snap("wlan_netze")
        await pg.evaluate("window.onerror=null")
        json.dump({"texts": sorted(texts), "errors": errs, "dialogs": dialogs}, open(f"{OUT}/result.json", "w"), ensure_ascii=False, indent=1)
        print("Texte:", len(texts), "Fehler:", len(errs), "Dialoge:", len(dialogs))
        for e in errs: print("FEHLER:", e)
        await b.close()
asyncio.run(main())
