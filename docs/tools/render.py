import asyncio,sys
from playwright.async_api import async_playwright
async def main(svg,png,w,h):
    async with async_playwright() as p:
        b=await p.chromium.launch(); pg=await b.new_page(viewport={"width":w,"height":h})
        await pg.goto("file://"+svg); await pg.screenshot(path=png); await b.close()
asyncio.run(main(sys.argv[1],sys.argv[2],int(sys.argv[3]),int(sys.argv[4])))
