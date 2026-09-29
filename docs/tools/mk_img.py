from PIL import Image, ImageDraw, ImageFont, ImageFilter
import glob
SH='/home/claude/host/shots2/'
OUT='/home/claude/readme/docs/images/'
FONT='/home/claude/fontwork/SG-Medium.ttf'
BG=(14,15,13); ACC=(61,220,151); TXT=(243,240,232); MUT=(166,163,152)
def f(s): return ImageFont.truetype(FONT,s)
def rnd(name,size):
    im=Image.open(glob.glob(SH+'en_'+name+'.ppm')[0]).convert('RGB').resize((size,size),Image.LANCZOS)
    m=Image.new('L',(size*4,size*4),0); ImageDraw.Draw(m).ellipse((0,0,size*4-1,size*4-1),fill=255); m=m.resize((size,size),Image.LANCZOS)
    out=Image.new('RGBA',(size,size),(0,0,0,0)); out.paste(im,(0,0),m); return out
def bezel(canvas,img,x,y,shadow=True):
    s=img.width; pad=int(s*0.045)
    if shadow:
        sh=Image.new('RGBA',canvas.size,(0,0,0,0)); d=ImageDraw.Draw(sh)
        d.ellipse((x-pad+8,y-pad+18,x+s+pad+8,y+s+pad+18),fill=(0,0,0,150)); sh=sh.filter(ImageFilter.GaussianBlur(18)); canvas.alpha_composite(sh)
    ring=Image.new('RGBA',canvas.size,(0,0,0,0)); d=ImageDraw.Draw(ring)
    d.ellipse((x-pad,y-pad,x+s+pad,y+s+pad),fill=(38,39,35,255),outline=(70,72,66,255),width=3)
    canvas.alpha_composite(ring); canvas.alpha_composite(img,(x,y))
def ctext(d,cx,y,t,font,fill):
    w=d.textlength(t,font=font); d.text((cx-w/2,y),t,font=font,fill=fill)

# ---- Titelbild ----
W,H=1600,640
c=Image.new('RGBA',(W,H),BG+(255,))
g=Image.new('RGBA',(W,H),(0,0,0,0)); d=ImageDraw.Draw(g)
d.ellipse((900,-200,1700,600),fill=(61,220,151,40)); g=g.filter(ImageFilter.GaussianBlur(120)); c.alpha_composite(g)
d=ImageDraw.Draw(c)
d.text((90,170),"ESP Scale",font=f(92),fill=TXT)
d.text((94,290),"A smart kitchen & workshop scale",font=f(36),fill=MUT)
d.text((94,340),"ESP32-S3 · 1.46\" round touch display · LVGL",font=f(28),fill=MUT)
chips=["20+ modes","Web UI","OTA from GitHub","DE / EN"]
x=94
for t in chips:
    w=d.textlength(t,font=f(24))+36
    d.rounded_rectangle((x,420,x+w,466),radius=23,fill=(20,40,31),outline=(30,59,44),width=2); d.text((x+18,429),t,font=f(24),fill=ACC); x+=w+14
bezel(c,rnd('67_portion3_passt',300),1210,60)
bezel(c,rnd('71_praezision',360),860,150)
bezel(c,rnd('107_halb_aufloesung',260),1250,380)
c.convert('RGB').save(OUT+'hero.png',optimize=True)

# ---- Galerie Gerät ----
items=[('71_praezision','Weighing (precision mode)'),('101_ml_milch','Units incl. ml'),('28_rezept','Recipes from SD card'),('34_cocktail_vorschau','Cocktails in ml'),
('67_portion3_passt','Portioning'),('79_timer_laeuft','Kitchen timers'),('91_tassen_ziel','Cups & spoons'),('82_mix1','2-part resin mixing'),
('43_spule','Filament spool'),('96_muenzen_2euro','Coin counter'),('41_porto','Postage classes'),('107_halb_aufloesung','Game: Half & half'),
('60_libelle','Spirit level'),('57_msa','Gauge study Cg/Cgk'),('74_qr_wlan','Wi-Fi QR code'),('20_fw_neu','OTA update from GitHub')]
S=260; cols=4; cw=360; rh=360
G=Image.new('RGBA',(cols*cw+40,((len(items)+cols-1)//cols)*rh+30),BG+(255,)); d=ImageDraw.Draw(G)
for i,(n,cap) in enumerate(items):
    cx=20+(i%cols)*cw+cw//2; y=30+(i//cols)*rh+10
    bezel(G,rnd(n,S),cx-S//2,y,shadow=False); ctext(d,cx,y+S+24,cap,f(24),TXT)
G.convert('RGB').save(OUT+'screens.png',optimize=True)
