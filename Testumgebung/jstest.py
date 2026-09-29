import sys, gi
gi.require_version('Gtk','3.0'); gi.require_version('WebKit2','4.1')
from gi.repository import Gtk, WebKit2, GLib
url=sys.argv[1]; js=open(sys.argv[2]).read(); wait=int(sys.argv[3]) if len(sys.argv)>3 else 2500
win=Gtk.Window(); win.set_default_size(390,1200); wv=WebKit2.WebView(); win.add(wv); win.show_all()
def read_done(o,res):
    try: print("RESULT:", wv.evaluate_javascript_finish(res).to_string())
    except Exception as e: print("ERR", e)
    Gtk.main_quit()
def read(): wv.evaluate_javascript("String(document.title)",-1,None,None,None,read_done); return False
def inject_done(o,res):
    try: wv.evaluate_javascript_finish(res)
    except Exception as e: print("INJ-ERR", e)
    GLib.timeout_add(wait, read)
def run():
    code=js.replace("\\","\\\\").replace("`","\\`").replace("$","\\$")
    wrap="var s=document.createElement('script');s.textContent=`%s`;document.body.appendChild(s);0;"%code
    wv.evaluate_javascript(wrap,-1,None,None,None,inject_done); return False
def loaded(v,ev):
    if ev==WebKit2.LoadEvent.FINISHED: GLib.timeout_add(900,run)
wv.connect("load-changed",loaded); wv.load_uri(url)
GLib.timeout_add(25000,Gtk.main_quit); Gtk.main()
