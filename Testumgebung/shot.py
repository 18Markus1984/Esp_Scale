# Seiten in WebKit (Handybreite 390 px) rendern und als PNG speichern
import sys, gi
gi.require_version('Gtk','3.0'); gi.require_version('WebKit2','4.1'); gi.require_version('Gdk','3.0')
from gi.repository import Gtk, WebKit2, GLib, Gdk
routes=sys.argv[1].split(','); out=sys.argv[2]; W=390; H=int(sys.argv[3]) if len(sys.argv)>3 else 1600
win=Gtk.Window(); win.set_default_size(W,H); win.move(0,0); win.set_decorated(False)
wv=WebKit2.WebView(); win.add(wv); win.show_all()
idx=[0]; started=[False]
def js(code): wv.evaluate_javascript(code,-1,None,None,None,None,None)
def snap(name):
    pb=Gdk.pixbuf_get_from_window(win.get_window(),0,0,W,H)
    pb.savev(f"{out}/{name}.png","png",[],[]); print("ok",name,flush=True); nxt(); return False
def nxt():
    if idx[0]>=len(routes): Gtk.main_quit(); return
    r=routes[idx[0]]; idx[0]+=1
    name,_,code=r.partition('!')
    js(f"location.hash='#/{name}'")
    def after():
        if code:
            for part in code.split('|'): js(part.replace('~',','))
        GLib.timeout_add(1400,lambda:snap((name or 'home')+('_'+str(idx[0]) if code else '')))
        return False
    GLib.timeout_add(1000,after)
def loaded(v,ev):
    if ev==WebKit2.LoadEvent.FINISHED and not started[0] and wv.get_uri().startswith("http"):
        started[0]=True; GLib.timeout_add(1200,lambda:(nxt(),False)[1])
wv.connect("load-changed",loaded)
wv.load_uri("http://127.0.0.1:8765/")
GLib.timeout_add(150000,Gtk.main_quit)
Gtk.main()
