import asyncio, json, os, re
from playwright.async_api import async_playwright
src=open('tour.py').read()
DOMSNAP=re.search(r'DOMSNAP = """(.*?)"""', src, re.S).group(1)
OUT="/home/claude/webtest/de_white_dom"; os.makedirs(OUT, exist_ok=True)
async def main():
    async with async_playwright() as p:
        b=await p.chromium.launch(); pg=await b.new_page(viewport={"width":390,"height":900})
        await pg.goto("http://127.0.0.1:8765/"); await pg.wait_for_timeout(2500)
        n=0
        for h,name,js in [("","home",None),("rezept","rezept",None),("rezept","rezept_step",["P.rezept.open(0)","P.rezept.begin()"]),("timer","timer",None),("protokoll","protokoll",None),("einstellungen","einstellungen",None)]:
            await pg.evaluate(f"location.hash='#/{h}'"); await pg.wait_for_timeout(1200)
            for c in (js or []):
                try: await pg.evaluate(c)
                except Exception as e: print(e)
                await pg.wait_for_timeout(1500)
            n+=1; d=await pg.evaluate(DOMSNAP); json.dump(d,open(f"{OUT}/{n:02d}_{name}.json","w"))
            await pg.screenshot(path=f"/tmp/w_{name}.png", full_page=True)
        await b.close()
asyncio.run(main())
