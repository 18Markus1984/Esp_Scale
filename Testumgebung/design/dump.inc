// ---- Objektbaum als JSON (für das gezeichnete UI-Artefakt) ----
#include "ui_theme.h"
extern "C" { extern const lv_font_t lv_font_montserrat_14; }
static const char* font_name(const lv_font_t*f){
  if(f==&font_sg_14) return "sg14"; if(f==&font_sg_18) return "sg18"; if(f==&font_sg_24) return "sg24";
  if(f==&font_sg_34) return "sg34"; if(f==&font_sg_80) return "sg80"; if(f==&font_sg_104) return "sg104";
  if(f==&font_icons_26) return "icons"; if(f==&lv_font_montserrat_14) return "mont14"; return "other"; }
static void jstr(FILE*o,const char*s){ fputc('"',o); for(;*s;s++){ unsigned char c=*s; if(c=='"'||c=='\\'){fputc('\\',o);fputc(c,o);} else if(c=='\n') fputs("\\n",o); else if(c<32) fprintf(o,"\\u%04x",c); else fputc(c,o);} fputc('"',o); }
static unsigned col(lv_color_t c){ return lv_color_to32(c) & 0xFFFFFF; }
static void dump_obj(FILE*o,lv_obj_t*ob){
  if(lv_obj_has_flag(ob,LV_OBJ_FLAG_HIDDEN)) { fputs("null",o); return; }
  lv_area_t a; lv_obj_get_coords(ob,&a);
  fprintf(o,"{\"x\":%d,\"y\":%d,\"w\":%d,\"h\":%d",a.x1,a.y1,a.x2-a.x1+1,a.y2-a.y1+1);
  const char*t="obj";
  if(lv_obj_check_type(ob,&lv_label_class)) t="label"; else if(lv_obj_check_type(ob,&lv_arc_class)) t="arc";
  else if(lv_obj_check_type(ob,&lv_line_class)) t="line"; else if(lv_obj_has_class(ob,&lv_canvas_class)) t="canvas";
  fprintf(o,",\"t\":\"%s\"",t);
  lv_part_t M=LV_PART_MAIN;
  fprintf(o,",\"opa\":%d,\"bg\":%u,\"bgo\":%d,\"bc\":%u,\"bw\":%d,\"bo\":%d,\"bs\":%d,\"r\":%d",
    lv_obj_get_style_opa(ob,M), col(lv_obj_get_style_bg_color(ob,M)), lv_obj_get_style_bg_opa(ob,M),
    col(lv_obj_get_style_border_color(ob,M)), lv_obj_get_style_border_width(ob,M), lv_obj_get_style_border_opa(ob,M),
    lv_obj_get_style_border_side(ob,M), lv_obj_get_style_radius(ob,M));
  fprintf(o,",\"ov\":%d",lv_obj_has_flag(ob,LV_OBJ_FLAG_OVERFLOW_VISIBLE)?1:0);
  fprintf(o,",\"wc\":%d",lv_obj_get_style_width(ob,M)==LV_SIZE_CONTENT?1:0);
  if(!strcmp(t,"label")){
    fputs(",\"text\":",o); jstr(o,lv_label_get_text(ob));
    const lv_font_t*f=lv_obj_get_style_text_font(ob,M);
    fprintf(o,",\"font\":\"%s\",\"lh\":%d,\"tc\":%u,\"to\":%d,\"ls\":%d,\"al\":%d,\"lm\":%d,\"pl\":%d,\"pr\":%d,\"pt\":%d,\"pb\":%d",
      font_name(f),f->line_height,col(lv_obj_get_style_text_color(ob,M)),lv_obj_get_style_text_opa(ob,M),
      lv_obj_get_style_text_letter_space(ob,M),lv_obj_get_style_text_align(ob,M),(int)lv_label_get_long_mode(ob),
      lv_obj_get_style_pad_left(ob,M),lv_obj_get_style_pad_right(ob,M),lv_obj_get_style_pad_top(ob,M),lv_obj_get_style_pad_bottom(ob,M));
  }
  if(!strcmp(t,"arc")){
    lv_arc_t*ar=(lv_arc_t*)ob;
    fprintf(o,",\"rot\":%d,\"bs0\":%d,\"bs1\":%d,\"is0\":%d,\"is1\":%d,\"aw\":%d,\"ac\":%u,\"ao\":%d,\"arr\":%d,\"iw\":%d,\"ic\":%u,\"io\":%d,\"irr\":%d,\"pad\":%d",
      ar->rotation,ar->bg_angle_start,ar->bg_angle_end,ar->indic_angle_start,ar->indic_angle_end,
      lv_obj_get_style_arc_width(ob,M),col(lv_obj_get_style_arc_color(ob,M)),lv_obj_get_style_arc_opa(ob,M),lv_obj_get_style_arc_rounded(ob,M),
      lv_obj_get_style_arc_width(ob,LV_PART_INDICATOR),col(lv_obj_get_style_arc_color(ob,LV_PART_INDICATOR)),lv_obj_get_style_arc_opa(ob,LV_PART_INDICATOR),lv_obj_get_style_arc_rounded(ob,LV_PART_INDICATOR),
      lv_obj_get_style_pad_left(ob,M));
  }
  if(!strcmp(t,"line")){
    lv_line_t*ln=(lv_line_t*)ob;
    fprintf(o,",\"lw\":%d,\"lc\":%u,\"pts\":[",lv_obj_get_style_line_width(ob,M),col(lv_obj_get_style_line_color(ob,M)));
    for(int i=0;i<ln->point_num;i++) fprintf(o,"%s[%d,%d]",i?",":"",ln->point_array[i].x,ln->point_array[i].y);
    fputs("]",o);
  }
  if(!strcmp(t,"canvas")){
    lv_img_dsc_t*d=&((lv_canvas_t*)ob)->dsc; int w=d->header.w,h=d->header.h;
    fprintf(o,",\"cw\":%d,\"ch\":%d,\"px\":\"",w,h);
    const uint8_t*buf=(const uint8_t*)d->data+8; int stride=(w+7)/8; for(int y=0;y<h;y++) for(int x=0;x<w;x++){ int bit=(buf[y*stride+x/8]>>(7-(x&7)))&1; fputc(bit?'0':'1',o);} fputs("\"",o);
  }
  fputs(",\"ch_\":[",o);
  bool first=true;
  for(uint32_t i=0;i<lv_obj_get_child_cnt(ob);i++){ lv_obj_t*c=lv_obj_get_child(ob,i); if(lv_obj_has_flag(c,LV_OBJ_FLAG_HIDDEN)) continue;
    if(!first) fputc(',',o); first=false; dump_obj(o,c); }
  fputs("]}",o);
}
static void dump_tree(const char*name){
  char p[160]; snprintf(p,sizeof p,"dump/%s.json",name); FILE*o=fopen(p,"w"); if(!o) return;
  fputs("{\"scr\":",o); dump_obj(o,lv_scr_act()); fputs(",\"top\":",o); dump_obj(o,lv_layer_top()); fputs("}",o); fclose(o);
}
