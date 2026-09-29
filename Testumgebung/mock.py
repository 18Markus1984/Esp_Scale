# Test-Server mit denselben Endpunkten wie die Waage
import json, time, math, http.server, urllib.parse, threading, os
# liefert die Quelle web/page.html aus (neben diesem Ordner)
PAGE_FILE=os.path.join(os.path.dirname(os.path.abspath(__file__)),'..','Waage','web','page.html')
def PAGE_(): return open(PAGE_FILE,'rb').read()
T0=time.time()
st={"pots":[{"name":"Großer Topf","grams":1840.2,"color":0},{"name":"Pfanne","grams":1210.0,"color":1},{"name":"Glass","grams":537.0,"color":2}],
    "spools":[{"name":"Bambu Lab","grams":247.0}],
    "cocktails":[{"file":"cuba_libre.txt","name":"Cuba Libre","portions":1,"ing":[{"name":"Rum braun","grams":50},{"name":"Cola","grams":120},{"name":"Limettensaft","grams":10}]},{"file":"gin_tonic.txt","name":"Gin Tonic","portions":1,"ing":[{"name":"Gin","grams":50},{"name":"Tonic Water","grams":150}]}],
    "recipes":[{"file":"pfannkuchen.txt","name":"Pfannkuchen","portions":2,"ing":[{"name":"Mehl","grams":250},{"name":"Milch","grams":500},{"name":"Eier","grams":110},{"name":"Zucker","grams":20},{"name":"Salz","grams":2}]}],
    "tare":0,"hide":False}
WL=["FRITZ!Box 7590","Makerspace THM"]
SET={"unit":0,"autosave":False,"autonext":True,"autooff":30,"idle":5,"autotara":True,"cd":3,"tol":5,"vol":60,"scheme":0,"speak":True,"voice":"Lena","voices":["Lena","KI-Stimme-2","Roboter"],
     "lang":int(os.environ.get("WAAGE_LANG","0")),"time":"14:32","ssid":"FRITZ!Box 7590","factor":-21.73,"porto":[{"name":"Standardbrief","max":20,"ct":95},{"name":"Kompaktbrief","max":50,"ct":110},{"name":"Großbrief","max":500,"ct":180},{"name":"Maxibrief","max":1000,"ct":290}]}
DAYS=[{"day":"2026-09-20","count":4},{"day":"2026-09-19","count":9},{"day":"2026-09-17","count":3},{"day":"2026-09-15","count":6}]
FORCE=[None]  # /mock/w?g=… setzt das Bruttogewicht fest (für Testabläufe), ohne g wieder automatisch
def gross(): return FORCE[0] if FORCE[0] is not None else (1234.5 if (time.time()-T0)%20>4 else 0.2)
# Timer und Langzeitmessung wie auf der Waage
TM=[{"end":0,"dur":0,"name":"","run":False} for _ in range(3)]
OTA={"t":0,"mode":""}
LT={"run":False,"iv":60,"t0":0}
def tm_left(t): return max(0,int(t["end"]-time.time()+0.999)) if t["run"] else -1
def tools():
    ts=[{"run":t["run"],"ring":t["run"] and tm_left(t)==0,"left":tm_left(t),"dur":t["dur"],"name":t["name"] or ("Timer %d"%(i+1))} for i,t in enumerate(TM)]
    h=(time.time()-LT["t0"])/3600 if LT["run"] else 0
    return {"timers":ts,"lt":{"run":LT["run"],"iv":LT["iv"],"n":int(h*3600/LT["iv"]) if LT["run"] else 0,"h":h,"trend":-1.2,"first":1000,"file":"2026-09-27_1015.csv","hist":[]}}
class H(http.server.BaseHTTPRequestHandler):
    def log_message(self,*a): pass
    def j(self,o,code=200):
        b=json.dumps(o).encode(); self.send_response(code); self.send_header("Content-Type","application/json"); self.end_headers(); self.wfile.write(b)
    def do_GET(self):
        u=urllib.parse.urlparse(self.path); q=dict(urllib.parse.parse_qsl(u.query)); p=u.path
        if p=="/" : self.send_response(200); self.send_header("Content-Type","text/html; charset=utf-8"); self.end_headers(); self.wfile.write(PAGE_()); return
        if p=="/api/state":
            g=gross()-st["tare"]; return self.j({"g":round(g,1),"gross":gross(),"disp":("%.1f"%g).replace(".",","),"unit":"g","stable":True,"over":False,"pot":"","off":583,"time":"14:32","bat":87,"ble":True,"max":3000,
                "tm":min([tm_left(t) for t in TM if t["run"] and tm_left(t)>0] or [-1]),"ring":any(t["run"] and tm_left(t)==0 for t in TM),"lt":LT["run"]})
        if p=="/mock/w": FORCE[0]=float(q["g"]) if "g" in q else None; return self.j({"ok":1})
        if p=="/api/tools": return self.j(tools())
        if p=="/api/ota":  # Online-Update: suchen dauert 1 s, laden 4 s
            if "check" in q: OTA.update(t=time.time(),mode="check")
            if "install" in q: OTA.update(t=time.time(),mode="install")
            dt=time.time()-OTA["t"]; ost="idle"; pr=0
            if OTA["mode"]=="check": ost="check" if dt<1 else "available"
            if OTA["mode"]=="install": pr=min(100,int(dt*25)); ost="download" if pr<100 else "done"
            return self.j({"ver":"1.4.2","local":False,"repo":"18Markus1984/Esp_Scale","state":ost,"latest":"1.5.0","progress":pr,"error":""})
        if p=="/api/timer":
            if "ringoff" in q:
                for t in TM:
                    if t["run"] and tm_left(t)==0: t["run"]=False
                return self.j({"ok":1})
            t=TM[int(q.get("i","0"))]
            if "stop" in q: t["run"]=False
            elif "add" in q: t["end"]+=int(q["add"])
            else: t.update(run=True,dur=int(q["s"]),end=time.time()+int(q["s"]),name=q.get("name",""))
            return self.j({"ok":1})
        if p=="/api/lt":
            if q.get("on")=="1": LT.update(run=True,iv=int(q.get("iv","60")),t0=time.time()-3600)
            else: LT["run"]=False
            return self.j({"ok":1})
        if p=="/api/tara": st["tare"]=gross(); return self.j({"ok":1})
        if p=="/api/wlan": return self.j({"cur":WL[0] if WL else "","max":5,"nets":WL})
        if p=="/api/wlan/del":
            i=int(q.get("i","-1"))
            if 0<=i<len(WL): WL.pop(i)
            return self.j({"ok":1})
        if p=="/api/lang": SET["lang"]=int(q.get("l","0")); return self.j({"ok":1})
        if p=="/api/log_add": print("NOTIZ:",q.get("note",""),flush=True); return self.j({"ok":1})
        if p in("/api/save","/api/log_add","/api/park","/api/sound","/api/hide","/api/sync","/api/level_reset","/api/speak_test","/api/ble_send","/api/tick","/api/battery/test","/api/player_add"): return self.j({"ok":1})
        if p=="/api/recipes":
            src=st["cocktails"] if q.get("dir")=="cocktails" else st["recipes"]
            return self.j([{"file":r["file"],"name":r["name"],"count":len(r["ing"])} for r in src])
        if p=="/api/recipe":
            src=st["cocktails"] if q.get("dir")=="cocktails" else st["recipes"]
            return self.j(next((r for r in src if r["file"]==q.get("file")),{}))
        if p=="/api/recipe/del": return self.j({"ok":1})
        if p=="/api/pots": return self.j({"pots":st["pots"],"spools":st["spools"]})
        if p=="/api/log/days": return self.j(DAYS)
        if p=="/api/log": return self.j([{"time":"14:32:05","value":"1234,5","unit":"g","note":"Großer Topf"},{"time":"12:40:13","value":"980,0","unit":"g","note":""},{"time":"08:12:01","value":"456,2","unit":"g","note":"Rezept: Pfannkuchen"},{"time":"21:13:40","value":"124,6","unit":"g","note":"Schätzspiel: Sieger Spieler 1"},{"time":"21:08:02","value":"1625,9","unit":"g","note":""},{"time":"13:44:10","value":"2000,0","unit":"g","note":"Porto: Großbrief 1,80 €"}])
        if p=="/api/battery": return self.j({"pct":72,"volts":3.88,"state":"akku","per":8.4,"left":8.6,"interval":5,"test":False,"test_h":0,"test_v0":0,"test_n":0,"history":[100,98,96,95,93,91,90,88,86,85,83,81,80,78,77,75,74,73,72]})
        if p=="/file":  # Langzeitmessung: Filament trocknen (fällt langsam)
            rows=["Uhrzeit;Minuten;Gewicht_g"]+["%02d:%02d:00;%s;%s"%(20+(i//60),i%60,("%.2f"%i).replace(".",","),("%.2f"%(1000-30*(1-math.exp(-i/180))+math.sin(i)/5)).replace(".",",")) for i in range(0,360,2)]
            b="\n".join(rows).encode(); self.send_response(200); self.send_header("Content-Type","text/csv"); self.end_headers(); self.wfile.write(b); return
        if p=="/api/files":
            d=q.get("dir","/Waage")
            if d=="/Waage/Messung": return self.j({"dir":d,"dirs":[],"files":[{"name":"2026-09-26_2010.csv","size":4200},{"name":"2026-09-25_0900.csv","size":900}],"total":7841796,"used":5520})
            if d=="/Waage": return self.j({"dir":d,"dirs":["Rezepte","Cocktails","Stimme","Protokoll","Akku","Spiel","Toepfe"],"files":[],"total":7841796,"used":5520})
            return self.j({"dir":d,"dirs":[],"files":[{"name":"pfannkuchen.txt","size":118},{"name":"waffeln.txt","size":142},{"name":"pizzateig.txt","size":131}],"total":31000000,"used":2400000})
        if p=="/api/scores": return self.j([
            {"day":"2026-09-20","game":"Schaetzspiel","name":"Lena","value":38.0,"unit":"g"},
            {"day":"2026-09-20","game":"Schaetzspiel","name":"Tom","value":52.0,"unit":"g"},
            {"day":"2026-09-21","game":"Trinkspiel","name":"Lena","value":14.0,"unit":"ml"},
            {"day":"2026-09-21","game":"Trinkspiel","name":"Mia","value":9.0,"unit":"ml"}])
        if p=="/api/player_add": return self.j({"ok":1})
        if p=="/api/settings": return self.j(SET)
        if p=="/api/players": return self.j(st.get("names",["Lena","Tom","Mia"]))
        self.j({"err":p},404)
    def do_POST(self):
        n=int(self.headers.get("Content-Length",0)); body=json.loads(self.rfile.read(n) or b"{}"); p=self.path
        if p=="/api/pots": st["pots"]=body["pots"]; st["spools"]=body["spools"]
        if p=="/api/players": st["names"]=body["names"]
        if p=="/api/wlan":
            if not body.get("ssid"): return self.j({"ok":0,"msg":"Name oder Passwort ungültig"})
            if body["ssid"] in WL: WL.remove(body["ssid"])
            WL.insert(0,body["ssid"]); del WL[5:]
        self.j({"ok":1})
http.server.ThreadingHTTPServer(("127.0.0.1",8765),H).serve_forever()
