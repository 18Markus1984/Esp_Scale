#include "update_online.h"
#include <lvgl.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/stat.h>
#include "ui.h"
#include "scale.h"
#include "ui_pages.h"
#include "data.h"
#include "settings.h"
#include "web.h"
extern float g_ax,g_ay,g_az; extern uint32_t g_ms;
#define W 412
static uint16_t fb[W*W];
static lv_disp_draw_buf_t db; static lv_color_t b1[W*W/10], b2[W*W/10];
static void rounder(lv_disp_drv_t*d, lv_area_t*a){ a->x1=(a->x1>>2)<<2; a->x2=((a->x2>>2)<<2)+3; }
static void flush(lv_disp_drv_t*d,const lv_area_t*a,lv_color_t*c){ int w=a->x2-a->x1+1;
  for(int y=a->y1;y<=a->y2;y++) for(int x=a->x1;x<=a->x2;x++) if(x<W&&y<W) fb[y*W+x]=c[(y-a->y1)*w+(x-a->x1)].full; lv_disp_flush_ready(d);}
static int tx=0,ty=0; static bool tp=false;
static void rd(lv_indev_drv_t*d, lv_indev_data_t*data){ data->point.x=tx; data->point.y=ty; data->state=tp?LV_INDEV_STATE_PR:LV_INDEV_STATE_REL; }
static void run(int ms){ for(int i=0;i<ms;i+=5){ lv_tick_inc(5); g_ms+=5; lv_timer_handler(); } }
static bool shot_norun=false; static void shot(const char*name){ if(!shot_norun) run(60); char p[128]; snprintf(p,sizeof p,"shots2/%s.ppm",name); FILE*f=fopen(p,"wb"); fprintf(f,"P6 %d %d 255\n",W,W);
  for(int i=0;i<W*W;i++){ int y=i/W,x=i%W; double dx=x-205.5,dy=y-205.5; uint16_t v=fb[i];
    unsigned char r=((v>>11)&0x1f)*255/31,g=((v>>5)&0x3f)*255/63,b=(v&0x1f)*255/31; if(dx*dx+dy*dy>206.0*206.0){r=g=b=40;} fputc(r,f);fputc(g,f);fputc(b,f);} fclose(f);
  lv_mem_monitor_t m; lv_mem_monitor(&m); printf("%-26s used %6u max %6u\n",name,(unsigned)(m.total_size-m.free_size),(unsigned)m.max_used); }
static void tap(int x,int y){ tx=x;ty=y;tp=true; run(80); tp=false; run(150); }
static void hold(int x,int y,int ms){ tx=x;ty=y;tp=true; run(ms); tp=false; run(150); }
static void drag(int x0,int y0,int x1,int y1,int ms){ tp=true; int steps=ms/10; for(int i=0;i<=steps;i++){ tx=x0+(x1-x0)*i/steps; ty=y0+(y1-y0)*i/steps; run(10);} tp=false; run(600); }
static bool visible(lv_obj_t*o){ for(lv_obj_t*p=o;p;p=lv_obj_get_parent(p)) if(lv_obj_has_flag(p,LV_OBJ_FLAG_HIDDEN)) return false; lv_area_t a; lv_obj_get_coords(o,&a); int cx=(a.x1+a.x2)/2, cy=(a.y1+a.y2)/2; return cx>0&&cx<412&&cy>0&&cy<412; }
static lv_obj_t* find_text(lv_obj_t*o,const char*t){ if(lv_obj_check_type(o,&lv_label_class)&&!strcmp(lv_label_get_text(o),t)&&visible(o)) return o;
  for(uint32_t i=0;i<lv_obj_get_child_cnt(o);i++){ lv_obj_t*r=find_text(lv_obj_get_child(o,i),t); if(r) return r;} return NULL; }
static bool tap_text(const char*t){ lv_obj_t*l=find_text(lv_scr_act(),t); if(!l){printf("!! not found: %s\n",t);return false;} lv_area_t a; lv_obj_get_coords(l,&a); tap((a.x1+a.x2)/2,(a.y1+a.y2)/2); return true;}
static void hold_text(const char*t,int ms){ lv_obj_t*l=find_text(lv_scr_act(),t); if(!l){printf("!! not found: %s\n",t);return;} lv_area_t a; lv_obj_get_coords(l,&a); hold((a.x1+a.x2)/2,(a.y1+a.y2)/2,ms); }
static void open_item(const char*t){ tap_text(t); run(500); tap(206,206); run(500); }
static void pick(const char*t){ for(int i=0;i<8 && !find_text(lv_scr_act(),t);i++) drag(206,300,206,190,300); tap_text(t); run(500); tap(206,206); run(500); }
static void back(){ drag(40,300,360,300,200); run(400); }
static void tile(int n){ for(int i=0;i<3;i++){ drag(40,200,360,200,250);} for(int i=0;i<n;i++) drag(360,200,40,200,250); run(300); }
static void wait_weight(float lo,float hi){ for(int i=0;i<600 && !(scale_stable()&&scale_net()>lo&&scale_net()<hi);i++) run(100); }
static void put(const char*p,const char*d){ FILE*f=fopen(p,"w"); fputs(d,f); fclose(f); }

#include <set>
#include <string>
#include "i18n.h"
static std::set<std::string> g_miss;
void i18n_miss(const char *de){ g_miss.insert(de); }
static std::set<std::string> g_shown; void i18n_shown(const char*t){ g_shown.insert(t); }
static bool tt(const char*de){ return tap_text(T(de)); }
static int shotn=0; static char pfx[8]="en";
static void S(const char*n){ char b[64]; snprintf(b,sizeof b,"%s_%02d_%s",pfx,shotn++,n); shot(b); }
static void home(){ ui_go_home(); run(500); }
static void openp(lv_obj_t*p,const char*n){ home(); ui_open_page(p); run(700); S(n); }
int main(int argc,char**argv){
  bool en = argc>1 && !strcmp(argv[1],"en"); if(!en) strcpy(pfx,"de");
  system("rm -rf sdcard shots2; mkdir -p shots2 sdcard/Waage/Toepfe sdcard/Waage/Protokoll");
  put("sdcard/Waage/Toepfe/toepfe.txt","# Name;Gewicht;Farbe;angelegt\nGroßer Topf;1840,2;0;2026-09-12\nPfanne;1210,0;1;2026-09-12\nSchüssel;1234,5;2;2026-09-14\n");
  put("sdcard/Waage/Protokoll/2026-09-17.txt","08:12:01;456,2;g;\n12:40:13;980,0;g;Pfanne\n18:02:55;1234,5;g;\n");
  put("sdcard/Waage/Protokoll/2026-09-26.txt","08:12:01;456,2;g;\n12:40:13;980,0;g;Pfanne\n");
  setvbuf(stdout,NULL,_IONBF,0); lv_init(); lv_disp_draw_buf_init(&db,b1,b2,W*W/10);
  static lv_disp_drv_t dd; lv_disp_drv_init(&dd); dd.hor_res=W; dd.ver_res=W; dd.flush_cb=flush; dd.rounder_cb=rounder; dd.draw_buf=&db; lv_disp_drv_register(&dd);
  static lv_indev_drv_t id; lv_indev_drv_init(&id); id.type=LV_INDEV_TYPE_POINTER; id.read_cb=rd; lv_indev_drv_register(&id);
  g_set.lang = en ? LANG_EN : LANG_DE;
  ui_init(); run(300); S("lagecheck"); run(4000);
  S("wiegen");
  for(int uu=1;uu<4;uu++){ g_set.unit=uu; run(400); char nb[32]; snprintf(nb,sizeof nb,"wiegen_unit%d",uu); S(nb);} g_set.unit=0; run(300);
  scale_tare(); run(2500); S("wiegen_null");
  tile(1); S("modi"); tile(2); S("system"); tile(0);
  openp(page_setup_create(),"setup");
  const char*cats[]={"Wiegen","Zeit & Funk","Ton","Waage"};
  for(int c=0;c<4;c++){ home(); ui_open_page(page_setup_create()); run(600); tt(cats[c]); run(700); S(cats[c]); if(c==3){ drag(206,280,206,150,300); S("waage_unten"); } }
  home(); ui_open_page(page_setup_create()); run(600); tt("Zeit & Funk"); run(600); tt("WLAN"); run(900); S("wlan");
  tap(206,236); run(900); S("wlan_liste"); drag(206,300,206,200,300); run(900); S("wlan_liste2"); tap(206,206); run(700); S("wlan_entf");
  tt("Zurück"); run(700); S("wlan_liste3");
  { extern upd_state_t sim_upd; extern int sim_upd_prog;
    home(); ui_open_page(page_setup_create()); run(600); tt("Waage"); run(700); drag(206,280,206,120,300); run(600); tt("Firmware"); run(900); S("fw");
    tt("Online suchen"); run(700); S("fw_neu"); tt("Installieren"); run(700); S("fw_laden");
    sim_upd=UPD_ERROR; run(500); S("fw_fehler"); sim_upd=UPD_IDLE; }
  home(); ui_open_page(page_setup_create()); run(600); tt("Zeit & Funk"); run(600); tt("Weboberfläche"); run(900); S("web");
  home(); ui_open_page(page_setup_create()); run(600); tt("Zeit & Funk"); run(600); tt("Uhrzeit"); run(900); S("uhrzeit");
  home(); ui_open_page(page_setup_create()); run(600); tt("Zeit & Funk"); run(600); tt("Datum"); run(900); S("datum");
  home(); ui_open_page(page_setup_create()); run(600); tt("Ton"); run(600); tt("Lautstärke"); run(900); S("lautst");
  home(); ui_open_page(page_setup_create()); run(600); tt("Wiegen"); run(600); tt("Auto-Tara"); run(900); S("autotara");
  openp(page_rezept_create(),"rezept"); tap(206,206); run(800); S("rezept_vorschau"); tt("Starten"); run(900); S("rezept_schritt");
  openp(page_rezept_create(),"rezept2"); tt("+ Neues Rezept"); run(900); S("rezept_neu");
  openp(page_cocktail_create(),"cocktail"); tap(206,206); run(800); S("cocktail_vorschau"); tt("Mixen"); run(900); S("cocktail_schritt");
  openp(page_cocktail_create(),"cocktail2"); tt("+ Neuer Cocktail"); run(900); S("cocktail_neu");
  openp(page_ziel_create(),"ziel");
  openp(page_zaehlen_create(),"zaehlen");
  openp(page_bluetooth_create(),"bluetooth");
  openp(page_porto_create(),"porto");
  openp(page_porto_setup_create(),"porto_setup");
  openp(page_spule_create(),"spule"); tap(206,206); run(800); S("spule2"); tap(206,206); run(800); S("spule3");
  openp(page_spiel_create(),"spiel"); tt("Auswählen ›"); run(800); S("spieler"); tt("Fertig"); run(700); tt("Los"); run(1500); S("spiel_runde");
  openp(page_trink_create(),"trink"); tt("Los"); run(1500); S("trink_runde");
  openp(page_protokoll_create(),"protokoll");
  openp(page_toepfe_create(),"toepfe"); tt("+ Neuer Topf"); run(900); S("topf_neu");
  openp(page_toepfe_create(),"toepfe2"); hold(206,206,900); run(600); S("topf_bearbeiten");
  openp(page_kalib_create(),"kalib");
  openp(page_msa_create(),"msa");
  openp(page_akku_create(),"akku");
  openp(page_mic_create(),"mic");
  openp(page_level_create(),"libelle");
  openp(page_level_setup_create(),"libelle_setup");
  openp(page_placeholder_create("Test"),"platzhalter");
  ui_go_home(); run(600);

  // ---- Neu: Portionieren, Präzision, QR ----
  { extern float sim_force_g;
  auto FW=[&](float g,int ms){ sim_force_g=g; run(ms); };
  home(); FW(0,4000); scale_tare(); run(300);
  ui_open_page(page_portion_create()); FW(1234.5f,4500); S("portion1");
  tt("Weiter"); run(600); S("portion2_leeren");
  FW(0,4000); S("portion2b_bereit");
  FW(96.0f,4000); S("portion3_zuleicht");
  FW(103.1f,4000); S("portion3_passt");
  FW(0,4000); S("portion4_naechstes");
  for(int k=0;k<11;k++){ FW(100.0f+(k%3)*2.5f,4000); FW(0,4000);} run(800); S("portion5_ergebnis");
  home(); g_set.precise=true; FW(0,4000); scale_tare(); FW(7.3f,1200); S("praezision_mittelt"); run(4000); S("praezision");
  FW(612.34f,6000); S("praezision2");
  g_set.precise=false; scale_tare(); FW(-1234.5f,4000); S("negativ_klein"); FW(0,4000); sim_force_g=-1; }
  web_start(); ui_open_page(page_wlan_qr_create()); run(800); S("qr_wlan"); tap(206,190); run(500); S("qr_url");
  home(); tile(1); tap(206,206); run(600); for(int k=0;k<3;k++) drag(206,300,206,190,300); run(500); S("kueche_portion");
  // ---- Neu: Timer, 2K, Langzeit, Tassen, Blindgießen, Münzen, Auto-Null ----
  { extern float sim_force_g; auto FW=[&](float g,int ms){ sim_force_g=g; run(ms); };
  home(); FW(0,4000); scale_tare(); run(300);
  // Timer
  ui_open_page(page_timer_create()); run(700); S("timer_liste");
  tap(206,144); run(700); S("timer_edit"); tt("1"); run(300); tt("Start"); run(1500); S("timer_laeuft");
  home(); tile(0); run(1500); S("home_mit_timer");
  run(62000); S("timer_alarm"); tap(206,206); run(500);
  // 2K
  ui_open_page(page_mix_create()); run(700); S("mix1"); tt("Start"); run(600);
  FW(250,4500); S("mix2_a"); tt("A fertig"); run(600);
  FW(262,4000); S("mix3_b_nochnicht"); FW(275.0f,5000); S("mix4_fertig");
  home(); FW(0,4000); scale_tare();
  // Langzeit
  ui_open_page(page_langzeit_create()); run(700); S("lz_setup"); tt("10 s"); tt("Start"); run(300);
  for(int k=0;k<30;k++) FW(500.0f-k*0.8f,10000); S("lz_laeuft");
  home(); tile(0); run(800); S("home_mit_messung");
  ui_open_page(page_langzeit_create()); run(800); tt("Stopp"); run(600); home();
  FW(0,4000); scale_tare();
  // Tassen
  ui_open_page(page_tassen_create()); run(700); S("tassen_liste"); tap(206,206); run(700);
  FW(190,4000); S("tassen_mehl"); tap(206+120,206+30); tap(206+120,206+30); tap(206+120,206+30); tap(206+120,206+30); tap(206+120,206+30); tap(206+120,206+30); run(600); S("tassen_ziel");
  home(); FW(0,4000); scale_tare();
  // Blindgießen
  ui_open_page(page_blind_create()); run(700); S("blind_setup"); tt("Los"); run(600);
  FW(0,3000); FW(210,4000); S("blind_giessen"); FW(360,4000); tt("Fertig"); run(500);
  for(int p=1;p<3;p++){ FW(0,4000); FW(210,4000); FW(210+140+p*7,4000); tt("Fertig"); run(500); }
  run(2000); S("blind_ergebnis");
  home(); FW(0,4000); scale_tare();
  // Münzen
  ui_open_page(page_muenzen_create()); run(700); S("muenzen_liste"); tap(206,206); run(700);
  FW(8.5f*37,4000); S("muenzen_2euro"); tt("+ Summe"); run(800); S("muenzen_summe");
  FW(8.5f*37+3.1f,4000); S("muenzen_unsicher");
  home(); FW(0,4000); scale_tare();
  // Auto-Null: langsame Drift 0,3 g über 60 s
  home(); tile(0); g_set.precise=true; for(int k=0;k<60;k++) FW(0.005f*k,1000); { float pn=0,pu=0; scale_precise(&pn,&pu,NULL); printf("AZT drift 0,30 g -> netto %.3f g\n",pn);} S("autonull_drift");
  FW(0.3f+0.35f,4000); { float pn=0,pu=0; scale_precise(&pn,&pu,NULL); printf("AZT 0,35 g aufgelegt -> netto %.3f g\n",pn);} run(20000); { float pn=0,pu=0; scale_precise(&pn,&pu,NULL); printf("AZT nach 20 s -> netto %.3f g\n",pn);} S("autonull_last"); g_set.precise=false;
  FW(0,3000); sim_force_g=-1; }
  // ---- Einheit ml und Halbe-Halbe ----
  { extern float sim_force_g; auto FW=[&](float g,int ms){ sim_force_g=g; run(ms); };
  home(); tile(0); FW(0,4000); scale_tare(); g_set.unit=4; g_set.liquid=1; FW(515,4000); S("ml_milch");
  { lv_obj_t*l=find_text(lv_scr_act(),"ml"); if(l){ lv_area_t a; lv_obj_get_coords(l,&a); tap((a.x1+a.x2)/2,(a.y1+a.y2)/2);} } run(600); S("ml_sahne");
  g_set.unit=0; g_set.liquid=0; FW(0,4000); scale_tare();
  ui_open_page(page_halb_create()); run(700); S("halb_setup"); tt("Los"); run(500);
  float halves[3]={57.0f,61.5f,58.6f};
  for(int p=0;p<3;p++){ FW(0,5000); if(p==0) S("halb_leeren"); FW(118.0f,5000); if(p==0) S("halb_ganz_erkannt"); FW(0,5000); if(p==0) S("halb_teilen"); FW(halves[p],5000); run(2000); if(p==0) S("halb_aufloesung"); tt(p<2?"Nächster":"Ergebnis"); run(600); }
  S("halb_endstand"); FW(0,3000); sim_force_g=-1; }
  // Gruppen
  home(); tile(1); tap(206,206); run(600); S("gruppe_kueche"); for(int k=0;k<4;k++) drag(206,300,206,190,300); run(500); S("gruppe_kueche_unten");
  home(); tile(1); drag(206,300,206,190,300); run(400); tap(206,206); run(600); for(int k=0;k<4;k++) drag(206,300,206,190,300); run(500); S("gruppe_werkstatt_unten");
  // Sprachwechsel über das Setup
  { FILE*f=fopen(en?"shown_en.txt":"shown_de.txt","w"); for(auto&x:g_shown) fprintf(f,"%s\n---\n",x.c_str()); fclose(f);} g_shown.clear();
  home(); ui_open_page(page_setup_create()); run(600); tt("Waage"); run(700); drag(206,280,206,150,300); tt("Sprache"); run(900); S("nach_wechsel");
  ui_go_home(); run(800); S("home_nach_wechsel");
  printf("---- MISS %zu ----\n", g_miss.size());
  for(auto&s:g_miss){ bool letters=false; for(char c: s) if((c>='A'&&c<='Z')||(c>='a'&&c<='z')) letters=true; if(letters) printf("MISS: %s\n", s.c_str()); }
  return 0;
}
