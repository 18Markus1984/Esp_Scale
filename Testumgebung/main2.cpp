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
int main(){
  system("rm -rf sdcard shots2; mkdir -p shots2 sdcard/Waage/Toepfe sdcard/Waage/Protokoll");
  put("sdcard/Waage/Toepfe/toepfe.txt","# Name;Gewicht;Farbe;angelegt\nGroßer Topf;1840,2;0;2026-09-12\nPfanne;1210,0;1;2026-09-12\nSchüssel;1234,5;2;2026-09-14\n");
  put("sdcard/Waage/Protokoll/2026-09-17.txt","08:12:01;456,2;g;\n12:40:13;980,0;g;Pfanne\n18:02:55;1234,5;g;\n");
  setvbuf(stdout,NULL,_IONBF,0); lv_init(); lv_disp_draw_buf_init(&db,b1,b2,W*W/10);
  static lv_disp_drv_t dd; lv_disp_drv_init(&dd); dd.hor_res=W; dd.ver_res=W; dd.flush_cb=flush; dd.rounder_cb=rounder; dd.draw_buf=&db; lv_disp_drv_register(&dd);
  static lv_indev_drv_t id; lv_indev_drv_init(&id); id.type=LV_INDEV_TYPE_POINTER; id.read_cb=rd; lv_indev_drv_register(&id);
  lv_label_set_text(lv_label_create(lv_scr_act()),"Hello");
  ui_init(); run(2600);
  run(2000);
  ui_open_page(page_spiel_create()); run(800); shot("g1_spiel");
  tap_text("Auswählen ›"); run(800); shot("g2_auswahl");
  tap(206,150); run(500); shot("g3_abgewaehlt");
  tap_text("+ Spieler"); run(700); shot("g4_neuer");
  tap_text("Abbrechen"); run(700);
  tap_text("Fertig"); run(800); shot("g5_zurueck");
  ui_go_home(); run(400);
  ui_open_page(page_trink_create()); run(800); shot("g6_trink");
  return 0;
}
