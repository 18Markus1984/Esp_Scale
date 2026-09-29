// Host-Ersatz für SD, Einstellungen, WLAN
#include "storage.h"
#include "settings.h"
#include "net.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
static bool ok=false;
static void full(const char*p,char*o){ snprintf(o,256,"sdcard%s",p); }
bool storage_begin(){ mkdir("sdcard",0755); const char*d[]={DIR_ROOT,DIR_LOG,DIR_POTS,DIR_RECIPES,DIR_PORTO,DIR_SOUNDS,DIR_SPOOLS,DIR_GAME,DIR_VOICE,DIR_COCKTAILS,DIR_BATT,DIR_MSA}; ok=true; for(auto x:d) storage_mkdir(x); return true;}
bool storage_ok(){return ok;}
int storage_read(const char*p,char*b,int m){ char f[256]; full(p,f); FILE*fp=fopen(f,"rb"); if(!fp) return -1; fseek(fp,0,SEEK_END); long sz=ftell(fp); if(sz>m-1) fseek(fp,sz-(m-1),SEEK_SET); else fseek(fp,0,SEEK_SET); int n=fread(b,1,m-1,fp); fclose(fp); b[n]=0; return n;}
bool storage_write(const char*p,const char*d){ char f[256]; full(p,f); FILE*fp=fopen(f,"wb"); if(!fp) return false; fputs(d,fp); fclose(fp); return true;}
bool storage_append(const char*p,const char*l){ char f[256]; full(p,f); FILE*fp=fopen(f,"ab"); if(!fp) return false; fprintf(fp,"%s\n",l); fclose(fp); return true;}
bool storage_remove(const char*p){ char f[256]; full(p,f); return unlink(f)==0;}
bool storage_exists(const char*p){ char f[256]; full(p,f); struct stat st; return stat(f,&st)==0;}
bool storage_mkdir(const char*p){ char f[256]; full(p,f); mkdir(f,0755); return true;}
int storage_list(const char*dir,char names[][STORAGE_NAME_LEN],int max){ char f[256]; full(dir,f); DIR*d=opendir(f); if(!d) return 0; int n=0; struct dirent*e; while((e=readdir(d))&&n<max){ if(e->d_type==DT_REG){ strncpy(names[n],e->d_name,STORAGE_NAME_LEN-1); names[n][STORAGE_NAME_LEN-1]=0; n++;} } closedir(d); return n;}
settings_t g_set;
void settings_load(){ g_set.autotara=true; g_set.countdown_s=3; g_set.tol_g=5; strcpy(g_set.ssid,"FRITZ!Box 7590"); g_set.pass[0]=0; g_set.volume=60; g_set.unit=0; g_set.autosave=false; g_set.auto_next=true; g_set.idle_min=5; g_set.auto_off_min=30; g_set.scheme=0; g_set.speak=false; g_set.voice_on=false; g_set.voice[0]=0; g_set.lvl_off_x=0; g_set.lvl_off_y=0; g_set.precise=false; g_set.azt=true; }
void settings_save(){}
static net_state_t ns=NET_OFF; static uint32_t last=0; extern uint32_t g_ms;
void net_begin(){} void net_loop(){} void net_sync_now(){ ns=NET_OK; last=g_ms?g_ms:1; }
net_state_t net_state(){return ns;}
// gespeicherte Netze (Simulator: im RAM, Start mit zwei Beispielnetzen)
static char nss[NET_MAX][33]={"FRITZ!Box 7590","Makerspace THM"}; static int nsn=2;
int net_count(){return nsn;} const char*net_ssid(int i){return (i>=0&&i<nsn)?nss[i]:"";}
bool net_has_credentials(){return nsn>0;}
void net_forget(int i){ if(i<0||i>=nsn) return; for(int k=i;k<nsn-1;k++) strcpy(nss[k],nss[k+1]); nsn--; if(i==0) strcpy(g_set.ssid,nsn?nss[0]:""); }
void net_store(const char*s,const char*p){ (void)p; int f=-1; for(int k=0;k<nsn;k++) if(!strcmp(nss[k],s)) f=k; if(f<0){ if(nsn<NET_MAX) nsn++; f=nsn-1; } for(int k=f;k>0;k--) strcpy(nss[k],nss[k-1]); strcpy(nss[0],s); }
void net_connect_start(bool){} bool net_connect_busy(){return false;}
void net_set_credentials(const char*s,const char*p){ net_store(s,p); strcpy(g_set.ssid,s); strcpy(g_set.pass,p); net_sync_now(); }
uint32_t net_last_sync_ms(){return last;}
static int scan_calls=0; void net_scan_start(){scan_calls=0;}
int net_scan_get(net_ap_t*o,int max){ if(++scan_calls<4) return -1; const char*n[]={"FRITZ!Box 7590","Makerspace THM","Nachbar-WLAN","Gast"}; int r[]={-48,-66,-80,-70}; for(int i=0;i<4;i++){ strcpy(o[i].ssid,n[i]); o[i].rssi=r[i]; o[i].open=(i==3);} return 4; }
int storage_list_dirs(const char*dir,char names[][STORAGE_NAME_LEN],int max){ char f[256]; snprintf(f,sizeof f,"sdcard%s",dir); DIR*d=opendir(f); if(!d) return 0; int n=0; struct dirent*e; while((e=readdir(d))&&n<max){ if(e->d_type==DT_DIR && e->d_name[0]!='.'){ strncpy(names[n],e->d_name,STORAGE_NAME_LEN-1); names[n][STORAGE_NAME_LEN-1]=0; n++; } } closedir(d); return n; }
#include "web.h"
static bool wr=false;
void web_start(){ wr=true; } void web_stop(){ wr=false; } void web_loop(){}
bool web_running(){ return wr; } bool web_ap_mode(){ return true; }
const char *web_url(){ return wr?"http://192.168.4.1":""; }
const char *web_ssid(){ return "Waage-Setup"; }
uint32_t web_seconds_left(){ return wr?583:0; }
bool web_hide_weight(){ return false; }
void storage_loop(){} void storage_lock(){} void storage_unlock(){} unsigned long storage_error_count(){ return 0; }
const char *web_ip(){ return wr?"192.168.178.57":""; }
bool voice_begin(){ return false; } bool voice_running(){ return false; } bool voice_awake(){ return false; } void voice_loop(){}
void settings_store_time(int,int,int,int,int){} bool settings_load_time(int*,int*,int*,int*,int*){ return false; }
const char *voice_last(){ return ""; } unsigned long voice_last_ms(){ return 0; }
bool voice_available(){ return false; } void voice_stop(){}
bool voice_starting(){ return false; }
bool voice_start_stuck(){ return false; }
bool voice_lack_memory(){ return false; }

void web_update_mode(uint32_t){} bool web_update_mode_active(){ return false; } uint32_t web_update_seconds_left(){ return 0; }

// Online-Update (Simulator: Zustände von außen setzbar)
#include "update_online.h"
upd_state_t sim_upd = UPD_IDLE; int sim_upd_prog = 0; const char *sim_upd_err = "Kein WLAN";
const char *fw_version(){ return "1.4.2"; } bool fw_is_local(){ return false; } const char *upd_repo(){ return "18Markus1984/Esp_Scale"; }
void upd_check(){ sim_upd = UPD_AVAILABLE; } void upd_install(){ sim_upd = UPD_DOWNLOAD; sim_upd_prog = 42; }
upd_state_t upd_state(){ return sim_upd; } bool upd_busy(){ return sim_upd==UPD_CONNECT||sim_upd==UPD_CHECK||sim_upd==UPD_DOWNLOAD; }
const char *upd_latest(){ return "1.5.0"; } int upd_progress(){ return sim_upd_prog; } const char *upd_error(){ return sim_upd_err; } void upd_loop(){}
void net_wifi_release(){}
