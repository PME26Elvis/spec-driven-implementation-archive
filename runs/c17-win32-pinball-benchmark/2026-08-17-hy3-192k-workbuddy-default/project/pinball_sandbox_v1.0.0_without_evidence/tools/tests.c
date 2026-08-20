/* tests.c — Unified automated test suite for Pinball Sandbox v1.0.0.
 * Links the real core library; no mocks. Counts every CHECK as one test.
 * Produces a JSON summary to stdout and exits non-zero if any fail.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>
#include <process.h>
#include <io.h>

#include "types.h"
#include "vec.h"
#include "scene.h"
#include "scene_parse.h"
#include "scene_write.h"
#include "scene_validate.h"
#include "sim.h"
#include "replay.h"
#include "render.h"
#include "png.h"

#define ROOT "D:/0814/pinball_sandbox_v1.0.0"

/* ---- test accounting ---- */
static long g_pass=0, g_fail=0;
static char g_fails[1500][220];
static int  g_fn=0;
static void rec_fail(const char*fmt,...){
    if(g_fn<1500){
        va_list a; va_start(a,fmt);
        vsnprintf(g_fails[g_fn],220,fmt,a);
        va_end(a);
        g_fn++;
    }
}
/* per-category accounting (9 logical test categories) */
enum { CAT_PARSE=0, CAT_DET_TS, CAT_DET_10X, CAT_STRESS, CAT_VALID,
       CAT_OBJT, CAT_REPLAY, CAT_MALF, CAT_TOOLS, CAT_N };
static const char* g_cat_names[CAT_N] = {
    "parse_roundtrip", "determinism_timescale", "determinism_10x",
    "stress_1m", "validation_triggers", "object_types",
    "replay", "malformed", "tools" };
static long g_cat_total[CAT_N], g_cat_pass[CAT_N];
static int  g_cur_cat = CAT_PARSE;
#define CHECKF(cond,...) do{ \
    g_cat_total[g_cur_cat]++; \
    if(cond){ g_pass++; g_cat_pass[g_cur_cat]++; } \
    else { g_fail++; rec_fail(__VA_ARGS__); } \
}while(0)
#define CHECKC(cond)     do{ if(cond) g_pass++; else { g_fail++; rec_fail("FAIL %s:%d",__FILE__,__LINE__); } }while(0)

/* ---- file helpers ---- */
static char *read_file(const char*path, size_t*outlen){
    FILE*f=fopen(path,"rb"); if(!f) return NULL;
    fseek(f,0,SEEK_END); long n=ftell(f); fseek(f,0,SEEK_SET);
    char*b=malloc(n+1); size_t rd=fread(b,1,n,f); b[rd]=0; fclose(f);
    if(outlen)*outlen=rd; return b;
}
static int list_dir(const char*pattern, char out[][256], int max){
    int n=0; WIN32_FIND_DATAA fd; HANDLE h=FindFirstFileA(pattern,&fd);
    if(h==INVALID_HANDLE_VALUE) return 0;
    do {
        if(!(fd.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)){
            if(n<max){ strcpy(out[n],fd.cFileName); n++; }
        }
    } while(FindNextFileA(h,&fd));
    FindClose(h); return n;
}
static int count_object_sections(const char*text){
    int c=0; const char*needle="[object ";
    for(const char*p=strstr(text,needle); p; p=strstr(p+1,needle)) c++;
    return c;
}

/* ---- sim helpers ---- */
static uint64_t run_sim_fp(const Scene*sc, long steps, int*out_err, long*steps_done){
    Sim s; sim_init(&s,(Scene*)sc); sim_reset(&s,(Scene*)sc);
    int err=0;
    for(long i=0;i<steps;i++){ if(sim_step(&s)!=0){ err=1; break; } }
    if(steps_done)*steps_done=(long)s.step;
    uint64_t fp=sim_fingerprint(&s);
    if(out_err)*out_err=err;
    sim_free(&s);
    return fp;
}

static int fin_d(double v){ return (v==v) && (v>-1e308) && (v<1e308); }
static int is_finite_ball(const Ball*b){
    return fin_d(b->pos.x)&&fin_d(b->pos.y)&&fin_d(b->vel.x)&&fin_d(b->vel.y);
}

/* ---- replay helpers ---- */
static uint64_t run_with_recorder(const Scene*sc, int steps, int*ev_count, ReplayEvent**ev){
    Sim s; sim_init(&s,(Scene*)sc); sim_reset(&s,(Scene*)sc);
    sim_recorder_enable(&s,1);
    for(int i=0;i<steps;i++){
        int L=(i%20<3)?1:0, R=(i%30<4)?1:0, La=(i%50<2)?1:0;
        sim_input(&s,L,R,La,0,0,0);
        sim_step(&s);
    }
    int cnt; const int*st; const int*kn;
    sim_recorder_take(&s,&cnt,&st,&kn);
    ReplayEvent*e=malloc(sizeof(ReplayEvent)*(cnt>0?cnt:1));
    for(int i=0;i<cnt;i++){ e[i].step=st[i]; e[i].kind=kn[i]; }
    *ev=e; *ev_count=cnt;
    uint64_t fp=sim_fingerprint(&s);
    sim_free(&s);
    return fp;
}
static uint64_t apply_replay(const Scene*sc, const ReplayEvent*ev,int ev_count,int total_steps){
    Sim s; sim_init(&s,(Scene*)sc); sim_reset(&s,(Scene*)sc);
    int pL=0,pR=0,pLa=0; int ei=0;
    for(int i=0;i<total_steps;i++){
        while(ei<ev_count && ev[ei].step==i){
            switch(ev[ei].kind){
                case RPL_L_FLIPPER_DOWN: pL=1; break;
                case RPL_L_FLIPPER_UP:   pL=0; break;
                case RPL_R_FLIPPER_DOWN: pR=1; break;
                case RPL_R_FLIPPER_UP:   pR=0; break;
                case RPL_LAUNCH_DOWN:    pLa=1; break;
                case RPL_LAUNCH_UP:      pLa=0; break;
                default: break;
            }
            ei++;
        }
        sim_input(&s,pL,pR,pLa,0,0,0);
        sim_step(&s);
    }
    uint64_t fp=sim_fingerprint(&s);
    sim_free(&s);
    return fp;
}

/* ---- png verify ---- */
static int verify_png(const char*path, int w, int h){
    FILE*f=fopen(path,"rb"); if(!f) return 0;
    unsigned char hdr[33]; size_t rd=fread(hdr,1,33,f); fclose(f);
    if(rd<33) return 0;
    static const unsigned char sig[8]={137,80,78,71,13,10,26,10};
    if(memcmp(hdr,sig,8)!=0) return 0;
    if(memcmp(hdr+12,"IHDR",4)!=0) return 0;
    int iw=(hdr[16]<<24)|(hdr[17]<<16)|(hdr[18]<<8)|hdr[19];
    int ih=(hdr[20]<<24)|(hdr[21]<<16)|(hdr[22]<<8)|hdr[23];
    if(iw!=w||ih!=h) return 0;
    return 1;
}

/* ---- color count in framebuffer ---- */
static int fb_distinct_colors(const Framebuffer*fb){
    unsigned cnt=0; unsigned char seen[64]; int ns=0;
    int total=fb->w*fb->h;
    for(int i=0;i<total && cnt<60;i++){
        unsigned char r=fb->pix[i*3],g=fb->pix[i*3+1],b=fb->pix[i*3+2];
        int found=0;
        for(int k=0;k<ns;k++){ if(seen[k*3]==r&&seen[k*3+1]==g&&seen[k*3+2]==b){found=1;break;} }
        if(!found){ if(ns<64){seen[ns*3]=r;seen[ns*3+1]=g;seen[ns*3+2]=b;ns++;} cnt++; }
    }
    return ns;
}

/* ---- validation-trigger scene builders ---- */
static Scene make_minimal(void){
    Scene sc; scene_init(&sc);
    sc.world_size=v2(800,600); sc.gravity=v2(0,980);
    return sc;
}
static int scene_has_error(const Scene*sc){
    DiagList d; diag_list_init(&d);
    PbtCode c=scene_validate(sc,&d);
    int has=0;
    for(size_t i=0;i<d.count;i++) if(d.items[i].severity==SEV_ERROR) has=1;
    diag_list_free(&d);
    return has || c!=PBT_OK;
}

/* ===================================================================== */
int main(void){
    char fixpat[512]; snprintf(fixpat,sizeof(fixpat),"%s/fixtures/*.pbt",ROOT);
    char malpat[512]; snprintf(malpat,sizeof(malpat),"%s/tests/malformed/*.pbt",ROOT);

    char fixes[256][256]; int nfix=list_dir(fixpat,fixes,256);
    char mals[256][256];  int nmal=list_dir(malpat,mals,256);

    CHECKF(nfix>0, "expected fixtures present (found %d)", nfix);
    CHECKF(nmal>0, "expected malformed fixtures present (found %d)", nmal);

    /* ---------- 1. PARSE ORACLE + ROUND TRIP + FINGERPRINT ---------- */
    for(int fi=0; fi<nfix; fi++){
        char path[512]; snprintf(path,sizeof(path),"%s/fixtures/%s",ROOT,fixes[fi]);
        size_t len; char*text=read_file(path,&len);
        CHECKF(text!=NULL, "read fixture %s", fixes[fi]);
        if(!text) continue;

        Scene sc; scene_init(&sc); DiagList d; diag_list_init(&d);
        PbtCode c=parse_scene(text,len,&sc,&d);
        int secs=count_object_sections(text);
        diag_list_free(&d);

        CHECKF(c==PBT_OK, "parse OK %s (code %d)", fixes[fi], c);
        CHECKF(sc.obj_count==secs, "object count %s parsed=%d expected=%d", fixes[fi], sc.obj_count, secs);
        CHECKF(sc.obj_count>0, "non-empty %s", fixes[fi]);

        /* validate passes on well-formed fixtures */
        CHECKF(!scene_has_error(&sc), "validate clean %s", fixes[fi]);

        /* fingerprint stable across 3 parses */
        uint64_t fp1=scene_fingerprint(&sc);
        Scene sc2; scene_init(&sc2); DiagList d2; diag_list_init(&d2);
        parse_scene(text,len,&sc2,&d2); diag_list_free(&d2);
        uint64_t fp2=scene_fingerprint(&sc2);
        Scene sc3; scene_init(&sc3); DiagList d3; diag_list_init(&d3);
        parse_scene(text,len,&sc3,&d3); diag_list_free(&d3);
        uint64_t fp3=scene_fingerprint(&sc3);
        CHECKF(fp1==fp2 && fp2==fp3, "fingerprint stable %s", fixes[fi]);
        CHECKF(fp1!=0, "nonzero fingerprint %s", fixes[fi]);

        /* round trip write->parse */
        char*out=scene_write(&sc);
        CHECKF(out!=NULL, "scene_write %s", fixes[fi]);
        if(out){
            CHECKF(strstr(out,"PINBALL_TABLE")!=NULL, "write header %s", fixes[fi]);
            Scene rt; scene_init(&rt); DiagList d4; diag_list_init(&d4);
            PbtCode rc=parse_scene(out,strlen(out),&rt,&d4);
            diag_list_free(&d4);
            CHECKF(rc==PBT_OK, "roundtrip parse OK %s", fixes[fi]);
            CHECKF(rt.obj_count==sc.obj_count, "roundtrip count %s (%d vs %d)", fixes[fi], rt.obj_count, sc.obj_count);
            CHECKF(scene_fingerprint(&rt)==fp1, "roundtrip fingerprint %s", fixes[fi]);
            free(out); scene_free(&rt);
        }

        /* per-object sanity */
        for(int oi=0; oi<sc.obj_count; oi++){
            const Obj*o=&sc.objects[oi];
            CHECKF(o->type>=0 && o->type<OBJ_COUNT, "valid type %s obj %d", fixes[fi], oi);
            switch(o->type){
                case OBJ_WALL: case OBJ_RAMP: case OBJ_ONE_WAY_GATE:
                case OBJ_SLINGSHOT: case OBJ_DROP_TARGET: case OBJ_STANDUP_TARGET:
                case OBJ_ROLLOVER:
                    CHECKF(o->u.cap.thickness>0, "cap thickness %s obj %d", fixes[fi], oi);
                    break;
                case OBJ_BUMPER:
                    CHECKF(o->u.bumper.radius>0, "bumper radius %s obj %d", fixes[fi], oi);
                    break;
                case OBJ_FLIPPER:
                    CHECKF(o->u.flipper.length>0, "flipper length %s obj %d", fixes[fi], oi);
                    break;
                case OBJ_KICKOUT:
                    CHECKF(o->u.kickout.capture_radius>0, "kickout radius %s obj %d", fixes[fi], oi);
                    break;
                case OBJ_SENSOR: case OBJ_DRAIN:
                    CHECKF(o->u.sensor.w>0 && o->u.sensor.h>0, "sensor dims %s obj %d", fixes[fi], oi);
                    break;
                default: break;
            }
        }

        /* render produces a non-blank framebuffer */
        if(sc.world_size.x>0 && sc.world_size.y>0){
            double s=600.0/sc.world_size.y; if(600.0/sc.world_size.x < s) s=600.0/sc.world_size.x;
            int w=(int)(sc.world_size.x*s), h=(int)(sc.world_size.y*s);
            if(w>0&&h>0){
                Framebuffer fb; fb_init(&fb,w,h);
                render_scene(&sc,NULL,&fb,s);
                int cols=fb_distinct_colors(&fb);
                CHECKF(cols>=3, "render non-blank %s (colors=%d)", fixes[fi], cols);
                /* write PNG and verify */
                char pngpath[512]; snprintf(pngpath,sizeof(pngpath),"%s/out/evidence/_test_%s.png",ROOT,fixes[fi]);
                int wr=png_write_rgb(pngpath,w,h,fb.pix);
                CHECKF(wr==0, "png write %s", fixes[fi]);
                CHECKF(verify_png(pngpath,w,h), "png valid %s", fixes[fi]);
                fb_free(&fb);
            }
        }

        scene_free(&sc); scene_free(&sc2); scene_free(&sc3);
    }

    /* ---------- 2. DETERMINISM (per fixture, multiple horizons) ---------- */
    g_cur_cat=CAT_DET_TS;
    int det_fixtures[8];
    int ndet=0;
    for(int fi=0; fi<nfix && ndet<8; fi++){
        char path[512]; snprintf(path,sizeof(path),"%s/fixtures/%s",ROOT,fixes[fi]);
        size_t len; char*text=read_file(path,&len); if(!text) continue;
        Scene sc; scene_init(&sc); DiagList d; diag_list_init(&d);
        if(parse_scene(text,len,&sc,&d)!=PBT_OK){ diag_list_free(&d); free(text); continue; }
        diag_list_free(&d); free(text);
        det_fixtures[ndet++]=fi;

        long horizons[]={120, 600, 1800, 6000};
        for(int h=0;h<4;h++){
            int e1,e2; long sd1,sd2;
            uint64_t a=run_sim_fp(&sc,horizons[h],&e1,&sd1);
            uint64_t b=run_sim_fp(&sc,horizons[h],&e2,&sd2);
            CHECKF(a==b, "determinism %s steps %ld", fixes[fi], horizons[h]);
            CHECKF(!e1 && !e2, "no runtime error %s steps %ld", fixes[fi], horizons[h]);
            CHECKF(sd1==sd2 && sd1>0, "steps consistent %s (%ld vs %ld)", fixes[fi], sd1, sd2);
        }
        scene_free(&sc);
    }

    /* ---------- 3. 10x DETERMINISM GATE ---------- */
    g_cur_cat=CAT_DET_10X;
    if(ndet>0){
        char path[512]; snprintf(path,sizeof(path),"%s/fixtures/%s",ROOT,fixes[det_fixtures[0]]);
        size_t len; char*text=read_file(path,&len);
        Scene sc; scene_init(&sc); DiagList d; diag_list_init(&d);
        parse_scene(text,len,&sc,&d); diag_list_free(&d); free(text);
        uint64_t base=0; int ok=1; int e;
        for(int r=0;r<10;r++){
            long sd; uint64_t fp=run_sim_fp(&sc,2000,&e,&sd);
            if(r==0) base=fp; else if(fp!=base) ok=0;
            CHECKC(fp==base);
        }
        CHECKF(ok, "10x determinism gate %s", fixes[det_fixtures[0]]);
        scene_free(&sc);
    }

    /* ---------- 4. 1,000,000 STEP SCALE GATE ---------- */
    g_cur_cat=CAT_STRESS;
    {
        char path[512]; snprintf(path,sizeof(path),"%s/fixtures/free_flight_v2.pbt",ROOT);
        size_t len; char*text=read_file(path,&len);
        if(text){
            Scene sc; scene_init(&sc); DiagList d; diag_list_init(&d);
            if(parse_scene(text,len,&sc,&d)==PBT_OK){
                int e; long sd;
                uint64_t fp=run_sim_fp(&sc,1000000,&e,&sd);
                CHECKF(sd==1000000, "1M steps executed (%ld)", sd);
                CHECKF(!e, "1M steps no error");
                CHECKF(fp!=0, "1M steps fingerprint nonzero");
                /* all balls finite */
                Sim s; sim_init(&s,&sc); sim_reset(&s,&sc);
                int allfinite=1;
                for(long i=0;i<1000000;i++){ if(sim_step(&s)!=0) break; }
                for(int bi=0;bi<s.ball_count;bi++) if(!is_finite_ball(&s.balls[bi])) allfinite=0;
                CHECKF(allfinite, "1M steps all balls finite");
                sim_free(&s);
            }
            diag_list_free(&d); free(text); scene_free(&sc);
        }
    }

    /* ---------- 5. VALIDATION TRIGGERS (each VAL_E_ forced) ---------- */
    g_cur_cat=CAT_VALID;
    {
        /* no spawn */
        Scene sc=make_minimal();
        Obj drain; memset(&drain,0,sizeof(drain)); drain.type=OBJ_DRAIN;
        drain.u.sensor.x=0; drain.u.sensor.y=0; drain.u.sensor.w=100; drain.u.sensor.h=20;
        scene_add_object(&sc,&drain);
        CHECKF(scene_has_error(&sc), "VAL_E_NO_SPAWN triggered");
        scene_free(&sc);
    }
    {
        /* no drain */
        Scene sc=make_minimal();
        Obj sp; memset(&sp,0,sizeof(sp)); sp.type=OBJ_BALL_SPAWN; sp.u.spawn.position=v2(100,100); sp.u.spawn.enabled=1;
        scene_add_object(&sc,&sp);
        CHECKF(scene_has_error(&sc), "VAL_E_NO_DRAIN triggered");
        scene_free(&sc);
    }
    {
        /* degenerate geometry: zero-length wall */
        Scene sc=make_minimal();
        Obj sp; memset(&sp,0,sizeof(sp)); sp.type=OBJ_BALL_SPAWN; sp.u.spawn.position=v2(100,100); sp.u.spawn.enabled=1; scene_add_object(&sc,&sp);
        Obj dr; memset(&dr,0,sizeof(dr)); dr.type=OBJ_DRAIN; dr.u.sensor.x=0;dr.u.sensor.y=0;dr.u.sensor.w=100;dr.u.sensor.h=20; scene_add_object(&sc,&dr);
        Obj w; memset(&w,0,sizeof(w)); w.type=OBJ_WALL; w.u.cap.start=v2(10,10); w.u.cap.end=v2(10,10); w.u.cap.thickness=4; scene_add_object(&sc,&w);
        CHECKF(scene_has_error(&sc), "VAL_E_DEGENERATE_GEOMETRY triggered");
        scene_free(&sc);
    }
    {
        /* degenerate: non-positive thickness */
        Scene sc=make_minimal();
        Obj sp; memset(&sp,0,sizeof(sp)); sp.type=OBJ_BALL_SPAWN; sp.u.spawn.position=v2(100,100); sp.u.spawn.enabled=1; scene_add_object(&sc,&sp);
        Obj dr; memset(&dr,0,sizeof(dr)); dr.type=OBJ_DRAIN; dr.u.sensor.x=0;dr.u.sensor.y=0;dr.u.sensor.w=100;dr.u.sensor.h=20; scene_add_object(&sc,&dr);
        Obj w; memset(&w,0,sizeof(w)); w.type=OBJ_WALL; w.u.cap.start=v2(10,10); w.u.cap.end=v2(200,10); w.u.cap.thickness=0; scene_add_object(&sc,&w);
        CHECKF(scene_has_error(&sc), "VAL_E_DEGENERATE_GEOMETRY thickness triggered");
        scene_free(&sc);
    }
    {
        /* valid scene with spawn+drain must NOT error */
        Scene sc=make_minimal();
        Obj sp; memset(&sp,0,sizeof(sp)); sp.type=OBJ_BALL_SPAWN; sp.u.spawn.position=v2(100,100); sp.u.spawn.enabled=1; scene_add_object(&sc,&sp);
        Obj dr; memset(&dr,0,sizeof(dr)); dr.type=OBJ_DRAIN; dr.u.sensor.x=0;dr.u.sensor.y=0;dr.u.sensor.w=100;dr.u.sensor.h=20; scene_add_object(&sc,&dr);
        Obj w; memset(&w,0,sizeof(w)); w.type=OBJ_WALL; w.u.cap.start=v2(10,10); w.u.cap.end=v2(200,10); w.u.cap.thickness=4; scene_add_object(&sc,&w);
        Obj bm; memset(&bm,0,sizeof(bm)); bm.type=OBJ_BUMPER; bm.u.bumper.center=v2(150,80); bm.u.bumper.radius=14; bm.u.bumper.base_score=100; scene_add_object(&sc,&bm);
        CHECKF(!scene_has_error(&sc), "valid scene passes validation");
        scene_free(&sc);
    }
    {
        /* launcher ownership / missing field */
        Scene sc=make_minimal();
        Obj sp; memset(&sp,0,sizeof(sp)); sp.type=OBJ_BALL_SPAWN; sp.u.spawn.position=v2(100,100); sp.u.spawn.enabled=1; scene_add_object(&sc,&sp);
        Obj dr; memset(&dr,0,sizeof(dr)); dr.type=OBJ_DRAIN; dr.u.sensor.x=0;dr.u.sensor.y=0;dr.u.sensor.w=100;dr.u.sensor.h=20; scene_add_object(&sc,&dr);
        Obj la; memset(&la,0,sizeof(la)); la.type=OBJ_LAUNCHER; la.u.launcher.position=v2(50,50);
        /* launcher without a spawn_id is still syntactically ok but may warn; ensure no false crash */
        scene_add_object(&sc,&la);
        CHECKF(sc.obj_count==3, "launcher added ok");
        scene_free(&sc);
    }

    /* ---------- 6. OBJECT TYPE CONSTRUCTION + ROUND TRIP ----------
       For each object type, locate a real authored object inside one of the
       fixtures, isolate it in a copy of that fixture's scene (preserving the
       source's defined layers so the layer reference stays valid, and clearing
       dangling events/groups), then verify scene_write -> parse_scene round-trips
       with the same object count and type. This exercises every object type
       through the real serialization path without synthetic defaults. */
    g_cur_cat=CAT_OBJT;
    for(int t=0;t<OBJ_COUNT;t++){
        int found_fi=-1, found_oi=-1;
        for(int fi=0; fi<nfix && found_fi<0; fi++){
            char path[512]; snprintf(path,sizeof(path),"%s/fixtures/%s",ROOT,fixes[fi]);
            size_t len; char*text=read_file(path,&len); if(!text) continue;
            Scene sc; scene_init(&sc); DiagList d; diag_list_init(&d);
            PbtCode pc=parse_scene(text,len,&sc,&d); free(text);
            if(pc==PBT_OK){
                for(int oi=0; oi<sc.obj_count; oi++){
                    if(sc.objects[oi].type==(ObjType)t){ found_fi=fi; found_oi=oi; break; }
                }
            }
            diag_list_free(&d); scene_free(&sc);
        }
        CHECKF(found_fi>=0, "type %d present in some fixture", t);
        if(found_fi<0) continue;

        char path[512]; snprintf(path,sizeof(path),"%s/fixtures/%s",ROOT,fixes[found_fi]);
        size_t len; char*text=read_file(path,&len);
        Scene sc; scene_init(&sc); DiagList d; diag_list_init(&d);
        PbtCode pc=parse_scene(text,len,&sc,&d); free(text);
        if(pc!=PBT_OK){ diag_list_free(&d); scene_free(&sc); CHECKF(0,"type %d reload parse",t); continue; }

        /* isolate the chosen object; keep layers, drop dangling events/groups,
           and also keep any object referenced by the isolated one (e.g. a
           LAUNCHER's spawn_id -> BALL_SPAWN). */
        Obj keep; memcpy(&keep,&sc.objects[found_oi],sizeof(Obj));
        Obj refs[16]; int nrefs=0;
        if(keep.type==OBJ_LAUNCHER && keep.u.launcher.spawn_id[0]){
            Obj*o=scene_find_object(&sc, keep.u.launcher.spawn_id);
            if(o && nrefs<16) refs[nrefs++]=*o;
        }
        sc.obj_count=0; sc.event_count=0; sc.group_count=0;
        scene_add_object(&sc,&keep);
        for(int r=0;r<nrefs;r++) scene_add_object(&sc,&refs[r]);

        char*out=scene_write(&sc);
        int ok=0;
        if(out){
            Scene rt; scene_init(&rt); DiagList d2; diag_list_init(&d2);
            PbtCode c=parse_scene(out,strlen(out),&rt,&d2);
            int type_match=0, id_match=0;
            for(int i=0;i<rt.obj_count;i++){
                if(rt.objects[i].type==(ObjType)t){ type_match++; if(strcmp(rt.objects[i].id,keep.id)==0) id_match++; }
            }
            if(!(c==PBT_OK && type_match==1 && id_match==1)){
                fprintf(stderr,"DBG rtt type %d c=%d oc=%d type_match=%d id_match=%d\n",
                        t,(int)c,rt.obj_count,type_match,id_match);
            }
            if(c==PBT_OK && type_match==1 && id_match==1) ok=1;
            diag_list_free(&d2); scene_free(&rt); free(out);
        }
        CHECKF(ok, "type %d roundtrip", t);
        diag_list_free(&d); scene_free(&sc);
    }

    /* ---------- 7. REPLAY RECORD / VERIFY / DETERMINISM ---------- */
    g_cur_cat=CAT_REPLAY;
    for(int fi=0; fi<nfix && fi<6; fi++){
        char path[512]; snprintf(path,sizeof(path),"%s/fixtures/%s",ROOT,fixes[fi]);
        size_t len; char*text=read_file(path,&len); if(!text) continue;
        Scene sc; scene_init(&sc); DiagList d; diag_list_init(&d);
        if(parse_scene(text,len,&sc,&d)!=PBT_OK){ diag_list_free(&d); free(text); continue; }
        diag_list_free(&d); free(text);

        int ec; ReplayEvent*ev=NULL;
        uint64_t fp_orig=run_with_recorder(&sc,1500,&ec,&ev);
        CHECKF(ec>0, "replay captured events %s (n=%d)", fixes[fi], ec);
        CHECKF(fp_orig!=0, "replay orig fingerprint %s", fixes[fi]);

        /* write + parse back */
        char rpath[512]; snprintf(rpath,sizeof(rpath),"%s/out/evidence/_replay_%s.pbr",ROOT,fixes[fi]);
        int wr=replay_write_file(rpath, scene_fingerprint(&sc), sc.has_seed?sc.scene_seed:0, 1, ev, ec);
        CHECKF(wr==0, "replay write %s", fixes[fi]);
        Replay rp; int pr=replay_parse_file(rpath,&rp);
        CHECKF(pr==0, "replay parse %s", fixes[fi]);
        CHECKF(rp.ev_count==ec, "replay event count %s (%d vs %d)", fixes[fi], rp.ev_count, ec);
        int vr=replay_verify(&sc,&rp);
        CHECKF(vr==PBT_OK, "replay verify %s (code %d)", fixes[fi], vr);

        /* apply replay and compare fingerprint */
        uint64_t fp_rep=apply_replay(&sc,ev,ec,1500);
        if(fp_rep!=fp_orig){
            fprintf(stderr,"DBG replay %s orig=%016llx rep=%016llx ec=%d\n",
                    fixes[fi],(unsigned long long)fp_orig,(unsigned long long)fp_rep,ec);
            for(int k=0;k<(ec<16?ec:16);k++)
                fprintf(stderr,"  ev[%d] step=%d kind=%d\n", k, ev[k].step, ev[k].kind);
        }
        CHECKF(fp_rep==fp_orig, "replay determinism %s", fixes[fi]);

        free(ev); replay_free(&rp); scene_free(&sc);
    }

    /* ---------- 8. MALFORMED FIXTURES PARSE TO ERROR ---------- */
    g_cur_cat=CAT_MALF;
    for(int mi=0; mi<nmal; mi++){
        char path[512]; snprintf(path,sizeof(path),"%s/tests/malformed/%s",ROOT,mals[mi]);
        size_t len; char*text=read_file(path,&len); if(!text){ CHECKF(0,"read malformed %s",mals[mi]); continue; }
        Scene sc; scene_init(&sc); DiagList d; diag_list_init(&d);
        PbtCode c=parse_scene(text,len,&sc,&d);
        diag_list_free(&d); free(text); scene_free(&sc);
        CHECKF(c!=PBT_OK, "malformed %s rejected (code %d)", mals[mi], c);
    }

    /* ---------- 9. TOOL INVOCATION (spawn built .exe, no shell) ---------- */
    g_cur_cat=CAT_TOOLS;
    {
        char simchk[512], scenchk[512], frgen[512];
        snprintf(simchk,sizeof(simchk),"%s/build/simcheck.exe",ROOT);
        snprintf(scenchk,sizeof(scenchk),"%s/build/scenecheck.exe",ROOT);
        snprintf(frgen,sizeof(frgen),"%s/build/framegen.exe",ROOT);
        char fix[512]; snprintf(fix,sizeof(fix),"%s/fixtures/reference_full_game_v2.pbt",ROOT);
        char outpng[512]; snprintf(outpng,sizeof(outpng),"%s/out/evidence/_tool_frame.png",ROOT);

        int r;
        /* redirect child stdout to NUL so the machine-readable summary on our
           stdout stays clean; restore afterwards */
        int oldfd = _dup(_fileno(stdout));
        FILE*sink = freopen("nul","w",stdout);
        (void)sink;
        r=(int)_spawnl(_P_WAIT, simchk, simchk, "--headless", fix, "--steps", "600", (const char*)NULL);
        CHECKF(r==0, "tool simcheck rc=%d", r);
        r=(int)_spawnl(_P_WAIT, scenchk, scenchk, fix, (const char*)NULL);
        CHECKF(r==0, "tool scenecheck rc=%d", r);
        r=(int)_spawnl(_P_WAIT, frgen, frgen, "--scene", fix, "--out", outpng, "--steps", "120", (const char*)NULL);
        CHECKF(r==0 && verify_png(outpng,900,562), "tool framegen rc=%d", r);
        fflush(stdout);
        _dup2(oldfd, _fileno(stdout));
        _close(oldfd);
    }

    /* ---------- emit JSON summary ---------- */
    printf("{\"tests\":%ld,\"passed\":%ld,\"failed\":%ld,\"fixtures\":%d,\"malformed\":%d,\"categories\":{",
           g_pass+g_fail, g_pass, g_fail, nfix, nmal);
    for(int c=0;c<CAT_N;c++){
        printf("%s\"%s\":{\"total\":%ld,\"passed\":%ld,\"failed\":%ld,\"skipped\":0}",
               c?",":"", g_cat_names[c], g_cat_total[c], g_cat_pass[c], g_cat_total[c]-g_cat_pass[c]);
    }
    printf("}}\n");
    if(g_fail){
        fprintf(stderr,"FAILED TESTS:\n");
        for(int i=0;i<g_fn;i++) fprintf(stderr,"  %s\n", g_fails[i]);
    }
    return g_fail? 1 : 0;
}
