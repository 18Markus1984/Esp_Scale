from PIL import Image, ImageDraw, ImageFont, ImageFilter
W='/home/claude/webtest/en/'; OUT='/home/claude/readme/docs/images/'
FONT='/home/claude/fontwork/SG-Medium.ttf'; BG=(14,15,13); TXT=(243,240,232)
items=[('56_home_dienste','Dashboard'),('33_portion_run','Portioning'),('74_messung','Long-term log'),('63_akku','Battery'),('72_firmware_neu','OTA update')]
PW,PH=390,780; pad=14; gap=40
C=Image.new('RGBA',(len(items)*(PW+2*pad)+(len(items)-1)*gap+80,PH+2*pad+110),BG+(255,))
d=ImageDraw.Draw(C); f=ImageFont.truetype(FONT,26)
for i,(n,cap) in enumerate(items):
    im=Image.open(W+n+'.png').convert('RGB').crop((0,0,390,PH))
    x=40+i*(PW+2*pad+gap); y=30
    sh=Image.new('RGBA',C.size,(0,0,0,0)); ImageDraw.Draw(sh).rounded_rectangle((x+6,y+14,x+PW+2*pad+6,y+PH+2*pad+14),radius=52,fill=(0,0,0,160)); C.alpha_composite(sh.filter(ImageFilter.GaussianBlur(14)))
    d.rounded_rectangle((x,y,x+PW+2*pad,y+PH+2*pad),radius=52,fill=(40,41,37),outline=(75,77,70),width=3)
    m=Image.new('L',(PW,PH),0); ImageDraw.Draw(m).rounded_rectangle((0,0,PW-1,PH-1),radius=40,fill=255)
    C.paste(im,(x+pad,y+pad),m)
    w=d.textlength(cap,font=f); d.text((x+pad+PW/2-w/2,y+PH+2*pad+30),cap,font=f,fill=TXT)
C.convert('RGB').save(OUT+'web-ui.png',optimize=True)
