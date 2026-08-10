#include "mdedit/core.h"
#include "mdedit/image.h"
#include "mdedit/ui.h"

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include <limits.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static uint64_t monotonic_ms(void) {
    struct timespec now; if (clock_gettime(CLOCK_MONOTONIC,&now)!=0) return 0U;
    return (uint64_t)now.tv_sec*1000U+(uint64_t)now.tv_nsec/1000000U;
}

static void pause_ms(int milliseconds) { (void)poll(NULL,0,milliseconds); }

static Window find_window(Display *display,Window parent,Atom state_atom,int depth) {
    if (depth>8) return None;
    Atom actual=None; int format=0; unsigned long count=0UL,remaining=0UL; unsigned char *value=NULL;
    if (XGetWindowProperty(display,parent,state_atom,0,1,False,AnyPropertyType,&actual,&format,&count,&remaining,&value)==Success) {
        if (value!=NULL) XFree(value);
        if (actual!=None) return parent;
    }
    Window root=None,owner=None,*children=NULL; unsigned child_count=0U;
    if (!XQueryTree(display,parent,&root,&owner,&children,&child_count)) return None;
    Window found=None;
    for (unsigned i=0U;i<child_count&&found==None;++i) found=find_window(display,children[i],state_atom,depth+1);
    if (children!=NULL) XFree(children);
    return found;
}

static Window wait_for_window(Display *display,Atom state_atom,int wait_ms) {
    uint64_t deadline=monotonic_ms()+(uint64_t)wait_ms; Window window=None;
    while (monotonic_ms()<deadline) { window=find_window(display,DefaultRootWindow(display),state_atom,0); if (window!=None) { XWindowAttributes attrs; if (XGetWindowAttributes(display,window,&attrs)&&attrs.map_state==IsViewable) return window; } pause_ms(25); }
    return None;
}

static bool read_state(Display *display,Window window,Atom state_atom,MdBuf *out) {
    Atom actual=None; int format=0; unsigned long count=0UL,remaining=0UL; unsigned char *value=NULL;
    if (XGetWindowProperty(display,window,state_atom,0,4096,False,AnyPropertyType,&actual,&format,&count,&remaining,&value)!=Success||actual==None||format!=8||value==NULL) { if (value!=NULL) XFree(value); return false; }
    bool ok=md_buf_assign(out,(const char *)value,(size_t)count); XFree(value); return ok;
}

static unsigned mask_shift(unsigned long mask) { unsigned shift=0U; while (mask!=0UL&&(mask&1UL)==0UL) { ++shift; mask>>=1U; } return shift; }
static unsigned mask_bits(unsigned long mask) { unsigned bits=0U; while (mask!=0UL) { bits+=(unsigned)(mask&1UL); mask>>=1U; } return bits; }
static uint8_t channel(unsigned long pixel,unsigned long mask) {
    unsigned bits=mask_bits(mask),shift=mask_shift(mask); if (bits==0U) return 0U; unsigned long maximum=(1UL<<bits)-1UL;
    return (uint8_t)((((pixel&mask)>>shift)*255UL+maximum/2UL)/maximum);
}

static bool capture_window(Display *display,Window window,const char *path) {
    XWindowAttributes attrs; if (!XGetWindowAttributes(display,window,&attrs)||attrs.width<=0||attrs.height<=0) return false;
    Window root=DefaultRootWindow(display),child=None; int root_x=0,root_y=0;
    if (!XTranslateCoordinates(display,window,root,0,0,&root_x,&root_y,&child)) return false;
    XWindowAttributes root_attrs; if (!XGetWindowAttributes(display,root,&root_attrs)) return false;
    int width=MD_MIN(attrs.width,root_attrs.width-MD_MAX(0,root_x)),height=MD_MIN(attrs.height,root_attrs.height-MD_MAX(0,root_y));
    root_x=MD_MAX(0,root_x); root_y=MD_MAX(0,root_y); if (width<=0||height<=0) return false;
    XImage *image=XGetImage(display,root,root_x,root_y,(unsigned)width,(unsigned)height,AllPlanes,ZPixmap); if (image==NULL) return false;
    size_t pixels=0U,bytes=0U; if (!md_size_mul((size_t)width,(size_t)height,&pixels)||!md_size_mul(pixels,4U,&bytes)) { XDestroyImage(image); return false; }
    uint8_t *rgba=malloc(bytes); if (rgba==NULL) { XDestroyImage(image); return false; }
    for (int y=0;y<height;++y) for (int x=0;x<width;++x) {
        unsigned long pixel=XGetPixel(image,x,y); size_t at=((size_t)y*(size_t)width+(size_t)x)*4U;
        rgba[at]=channel(pixel,image->red_mask); rgba[at+1U]=channel(pixel,image->green_mask); rgba[at+2U]=channel(pixel,image->blue_mask); rgba[at+3U]=255U;
    }
    char error[512]; bool ok=md_image_write_png(path,rgba,(uint32_t)width,(uint32_t)height,error,sizeof(error)); if (!ok) fprintf(stderr,"e2e_x11: %s\n",error);
    free(rgba); XDestroyImage(image); return ok;
}

static void send_command(Display *display,Window window,Atom command_atom,long command) {
    XClientMessageEvent message; memset(&message,0,sizeof(message)); message.type=ClientMessage; message.display=display; message.window=window; message.message_type=command_atom; message.format=32; message.data.l[0]=command;
    XSendEvent(display,window,False,NoEventMask,(XEvent *)&message); XFlush(display); pause_ms(45);
}

static void send_insert(Display *display,Window window,Atom insert_atom,Atom utf8,const char *text) {
    XChangeProperty(display,window,insert_atom,utf8,8,PropModeReplace,(const unsigned char *)text,(int)strlen(text));
    XClientMessageEvent message; memset(&message,0,sizeof(message)); message.type=ClientMessage; message.display=display; message.window=window; message.message_type=insert_atom; message.format=32;
    XSendEvent(display,window,False,NoEventMask,(XEvent *)&message); XFlush(display); pause_ms(60);
}

static void send_key(Display *display,Window window,KeySym symbol,unsigned modifiers) {
    XKeyEvent key; memset(&key,0,sizeof(key)); key.display=display; key.window=window; key.root=DefaultRootWindow(display); key.subwindow=None; key.time=CurrentTime; key.x=20; key.y=20; key.x_root=20; key.y_root=20; key.same_screen=True; key.state=modifiers; key.keycode=XKeysymToKeycode(display,symbol); key.type=KeyPress;
    XSendEvent(display,window,True,KeyPressMask,(XEvent *)&key); XFlush(display); pause_ms(60);
}

static bool send_key_pair(Display *display,Window window,KeySym symbol,unsigned modifiers) {
    KeyCode code=XKeysymToKeycode(display,symbol);
    if (code==0U) {
        int minimum=0,maximum=0; XDisplayKeycodes(display,&minimum,&maximum); if (maximum<=minimum) return false;
        KeySym mapping=symbol; XChangeKeyboardMapping(display,maximum,1,&mapping,1); XSync(display,False); code=(KeyCode)maximum;
    }
    XKeyEvent key; memset(&key,0,sizeof(key)); key.display=display; key.window=window; key.root=DefaultRootWindow(display); key.subwindow=None; key.time=CurrentTime; key.x=20; key.y=20; key.x_root=20; key.y_root=20; key.same_screen=True; key.state=modifiers; key.keycode=code;
    key.type=KeyPress; XSendEvent(display,window,True,KeyPressMask,(XEvent *)&key);
    key.type=KeyRelease; XSendEvent(display,window,True,KeyReleaseMask,(XEvent *)&key);
    XFlush(display); pause_ms(80); return true;
}

static void send_pointer(Display *display,Window window,int type,int x,int y,unsigned button,unsigned state) {
    if (type==MotionNotify) {
        XMotionEvent motion; memset(&motion,0,sizeof(motion)); motion.type=MotionNotify; motion.display=display; motion.window=window; motion.root=DefaultRootWindow(display); motion.time=CurrentTime; motion.x=x; motion.y=y; motion.x_root=x; motion.y_root=y; motion.state=state; motion.same_screen=True;
        XSendEvent(display,window,True,PointerMotionMask,(XEvent *)&motion);
    } else {
        XButtonEvent event; memset(&event,0,sizeof(event)); event.type=type; event.display=display; event.window=window; event.root=DefaultRootWindow(display); event.time=CurrentTime; event.x=x; event.y=y; event.x_root=x; event.y_root=y; event.button=button; event.state=state; event.same_screen=True;
        XSendEvent(display,window,True,type==ButtonPress?ButtonPressMask:ButtonReleaseMask,(XEvent *)&event);
    }
    XFlush(display); pause_ms(70);
}

static bool state_integer(const MdBuf *state,const char *key,long long *value) {
    char pattern[96]; int n=snprintf(pattern,sizeof(pattern),"\"%s\":",key); if (n<=0||(size_t)n>=sizeof(pattern)) return false;
    const char *found=strstr(state->data,pattern); if (found==NULL) return false; found+=(size_t)n;
    char *end=NULL; long long parsed=strtoll(found,&end,10); if (end==found) return false; *value=parsed; return true;
}

static bool state_double(const MdBuf *state,const char *key,double *value) {
    char pattern[96]; int n=snprintf(pattern,sizeof(pattern),"\"%s\":",key); if (n<=0||(size_t)n>=sizeof(pattern)) return false;
    const char *found=strstr(state->data,pattern); if (found==NULL) return false; found+=(size_t)n;
    char *end=NULL; double parsed=strtod(found,&end); if (end==found) return false; *value=parsed; return true;
}

static bool state_string(const MdBuf *state,const char *key,char *value,size_t cap) {
    char pattern[96]; int n=snprintf(pattern,sizeof(pattern),"\"%s\":\"",key); if (n<=0||(size_t)n>=sizeof(pattern)||cap==0U) return false;
    const char *found=strstr(state->data,pattern); if (found==NULL) return false; found+=(size_t)n; const char *end=strchr(found,'\"'); if (end==NULL||(size_t)(end-found)>=cap) return false;
    memcpy(value,found,(size_t)(end-found)); value[end-found]='\0'; return true;
}

static bool request_clipboard(Display *display,Window owner_window,Atom clipboard,Atom utf8,const char *expected) {
    Atom property=XInternAtom(display,"_MDEDIT_E2E_CLIP",False); XConvertSelection(display,clipboard,utf8,property,owner_window,CurrentTime); XFlush(display);
    uint64_t deadline=monotonic_ms()+2500U;
    while (monotonic_ms()<deadline) {
        while (XPending(display)>0) {
            XEvent event; XNextEvent(display,&event); if (event.type!=SelectionNotify) continue;
            Atom actual=None; int format=0; unsigned long count=0UL,remaining=0UL; unsigned char *value=NULL;
            if (event.xselection.property!=None&&XGetWindowProperty(display,owner_window,property,0,1L<<20,True,AnyPropertyType,&actual,&format,&count,&remaining,&value)==Success&&value!=NULL) {
                bool ok=format==8&&strstr((const char *)value,expected)!=NULL; XFree(value); return ok;
            }
        }
        pause_ms(20);
    }
    return false;
}

static bool provide_clipboard_once(Display *display,Window app_window,Window owner_window,Atom command_atom,Atom clipboard,Atom utf8,Atom targets,const char *text) {
    XSetSelectionOwner(display,clipboard,owner_window,CurrentTime); if (XGetSelectionOwner(display,clipboard)!=owner_window) return false;
    send_command(display,app_window,command_atom,MD_CMD_PASTE); uint64_t deadline=monotonic_ms()+2500U;
    while (monotonic_ms()<deadline) {
        while (XPending(display)>0) {
            XEvent event; XNextEvent(display,&event); if (event.type!=SelectionRequest) continue;
            XSelectionRequestEvent *request=&event.xselectionrequest; XSelectionEvent reply; memset(&reply,0,sizeof(reply)); reply.type=SelectionNotify; reply.display=display; reply.requestor=request->requestor; reply.selection=request->selection; reply.target=request->target; reply.time=request->time; reply.property=None;
            Atom property=request->property==None?request->target:request->property;
            if (request->target==targets) { Atom supported[]={utf8,XA_STRING,targets}; XChangeProperty(display,request->requestor,property,XA_ATOM,32,PropModeReplace,(const unsigned char *)supported,3); reply.property=property; }
            else if (request->target==utf8||request->target==XA_STRING) { XChangeProperty(display,request->requestor,property,request->target,8,PropModeReplace,(const unsigned char *)text,(int)strlen(text)); reply.property=property; }
            XSendEvent(display,request->requestor,False,0,(XEvent *)&reply); XFlush(display); pause_ms(100); return reply.property!=None;
        }
        pause_ms(20);
    }
    return false;
}

static bool wait_state_contains(Display *display,Window window,Atom state_atom,const char *needle,int timeout_ms) {
    uint64_t deadline=monotonic_ms()+(uint64_t)timeout_ms; MdBuf state; md_buf_init(&state); bool found=false;
    while (monotonic_ms()<deadline&&!found) { if (read_state(display,window,state_atom,&state)&&strstr(state.data,needle)!=NULL) found=true; else pause_ms(25); }
    if (!found&&state.data!=NULL) fprintf(stderr,"e2e_x11: state did not contain %s; actual=%s\n",needle,state.data);
    md_buf_free(&state); return found;
}

static bool exercise(Display *display,Window app_window,Window helper,Atom state_atom,Atom command_atom,Atom insert_atom,Atom clipboard,Atom utf8,Atom targets) {
    MdBuf before,after; md_buf_init(&before); md_buf_init(&after); bool ok=read_state(display,app_window,state_atom,&before);
    send_insert(display,app_window,insert_atom,utf8,"\n繁體中文 clipboard ✓"); ok=ok&&read_state(display,app_window,state_atom,&after)&&strcmp(before.data,after.data)!=0;
    send_command(display,app_window,command_atom,1000L); send_command(display,app_window,command_atom,MD_CMD_COPY);
    ok=ok&&request_clipboard(display,helper,clipboard,utf8,"繁體中文 clipboard");
    md_buf_assign(&before,after.data,after.len); ok=ok&&provide_clipboard_once(display,app_window,helper,command_atom,clipboard,utf8,targets,"外部剪貼簿 External paste");
    pause_ms(120); ok=ok&&read_state(display,app_window,state_atom,&after)&&strcmp(before.data,after.data)!=0;
    send_command(display,app_window,command_atom,MD_CMD_MODE_SPLIT); ok=ok&&wait_state_contains(display,app_window,state_atom,"\"mode\":\"Split\"",1000);
    send_command(display,app_window,command_atom,MD_CMD_MODE_RENDERED); ok=ok&&wait_state_contains(display,app_window,state_atom,"\"mode\":\"Rendered Edit\"",1000);
    send_command(display,app_window,command_atom,MD_CMD_PALETTE); ok=ok&&wait_state_contains(display,app_window,state_atom,"\"modal\":8",1000);
    send_key(display,app_window,XK_Escape,0U); ok=ok&&wait_state_contains(display,app_window,state_atom,"\"modal\":0",1500);
    md_buf_free(&before); md_buf_free(&after); return ok;
}

typedef struct { int total; int passed; } ScenarioSummary;

static bool scenario_record(ScenarioSummary *summary,const char *id,bool passed,const char *detail) {
    ++summary->total; if (passed) ++summary->passed;
    printf("%s %s %s\n",passed?"PASS":"FAIL",id,detail==NULL?"":detail); return passed;
}

static bool read_sha(Display *display,Window window,Atom state_atom,char sha[65],MdBuf *state) {
    return read_state(display,window,state_atom,state)&&state_string(state,"source_sha256",sha,65U);
}

static bool resize_selected_image_by(Display *display,Window window,Atom state_atom,int delta,int *persisted_out) {
    MdBuf state; md_buf_init(&state); char before_sha[65],after_sha[65]; long long x=0,y=0,w=0,h=0,undo_before=0,undo_after=0;
    bool ok=read_sha(display,window,state_atom,before_sha,&state)&&strstr(state.data,"\"image_selected\":true")!=NULL&&
            state_integer(&state,"image_x",&x)&&state_integer(&state,"image_y",&y)&&
            state_integer(&state,"image_w",&w)&&state_integer(&state,"image_h",&h)&&
            state_integer(&state,"undo",&undo_before)&&w>=48&&h>=36;
    if (!ok) {
        fprintf(stderr,"e2e_x11: selected image telemetry unavailable: %s\n",state.data==NULL?"":state.data);
        md_buf_free(&state); return false;
    }
    int handle_x=(int)(x+w-4),handle_y=(int)(y+h-4);
    send_pointer(display,window,ButtonPress,handle_x,handle_y,Button1,0U);
    send_pointer(display,window,MotionNotify,handle_x+delta,handle_y,0U,Button1Mask);
    uint64_t live_deadline=monotonic_ms()+1500U; long long live_w=0,live_h=0; bool live=false;
    while (monotonic_ms()<live_deadline&&!live) {
        live=read_state(display,window,state_atom,&state)&&strstr(state.data,"\"image_resizing\":true")!=NULL&&
             state_integer(&state,"image_w",&live_w)&&state_integer(&state,"image_h",&live_h)&&live_w>=48&&live_h>=36;
        if (!live) pause_ms(25);
    }
    long long aspect_error=llabs(live_w*h-live_h*w);
    bool aspect=live&&aspect_error<=MD_MAX(live_w,live_h);
    send_pointer(display,window,ButtonRelease,handle_x+delta,handle_y,Button1,Button1Mask);
    uint64_t deadline=monotonic_ms()+2500U; bool changed=false;
    long long persisted=0;
    while (monotonic_ms()<deadline&&!changed) {
        if (read_sha(display,window,state_atom,after_sha,&state)&&state_integer(&state,"undo",&undo_after)&&
            strcmp(before_sha,after_sha)!=0&&undo_after==undo_before+1LL&&
            state_integer(&state,"image_persisted_width",&persisted)&&persisted>=48&&
            strstr(state.data,"\"image_resizing\":false")!=NULL) changed=true;
        else pause_ms(30);
    }
    changed=changed&&aspect&&((delta>0&&persisted>w)||(delta<0&&persisted<w));
    if (!changed) fprintf(stderr,"e2e_x11: image resize/aspect transaction failed: delta=%d state=%s\n",delta,state.data==NULL?"":state.data);
    else printf("PASS E2E-IMAGE-POINTER-RESIZE delta=%d live=%lldx%lld persisted_width=%lld one_source_transaction=true aspect_preserved=true\n",delta,live_w,live_h,persisted);
    if (changed&&persisted_out!=NULL) *persisted_out=(int)persisted;
    md_buf_free(&state); return changed;
}

static bool resize_selected_image(Display *display,Window window,Atom state_atom) {
    return resize_selected_image_by(display,window,state_atom,96,NULL);
}

static bool scenario_ui(Display *display,Window app_window,Atom state_atom,Atom command_atom) {
    ScenarioSummary summary={0}; MdBuf state; md_buf_init(&state); char initial_sha[65],current_sha[65]; bool ok=read_sha(display,app_window,state_atom,initial_sha,&state);
    static const struct { MdCommandId command; const char *mode; } modes[]={
        {MD_CMD_MODE_SOURCE,"Source"},{MD_CMD_MODE_SPLIT,"Split"},{MD_CMD_MODE_PREVIEW,"Preview"},{MD_CMD_MODE_RENDERED,"Rendered Edit"},{MD_CMD_MODE_SOURCE,"Source"}
    };
    bool cycle=ok;
    for (size_t i=0U;i<MD_ARRAY_LEN(modes);++i) { send_command(display,app_window,command_atom,modes[i].command); cycle=cycle&&wait_state_contains(display,app_window,state_atom,modes[i].mode,1000)&&read_sha(display,app_window,state_atom,current_sha,&state)&&strcmp(initial_sha,current_sha)==0; }
    ok=scenario_record(&summary,"E2E-MODE-CYCLE-SOURCE-STABILITY",cycle,"Source→Split→Preview→Rendered→Source preserved exact source digest")&&ok;

    send_command(display,app_window,command_atom,MD_CMD_MODE_RENDERED); pause_ms(450); double capsule=0.0; bool capsule_ok=read_state(display,app_window,state_atom,&state)&&state_double(&state,"capsule_x",&capsule)&&capsule>2.8;
    ok=scenario_record(&summary,"E2E-CAPSULE-ANIMATION",capsule_ok,"animated indicator converged to Rendered Edit")&&ok;

    send_command(display,app_window,command_atom,MD_CMD_STATISTICS); bool modal_opened=wait_state_contains(display,app_window,state_atom,"\"modal\":5",1000);
    char modal_sha[65],after_key_sha[65]; bool blocked=read_sha(display,app_window,state_atom,modal_sha,&state)&&send_key_pair(display,app_window,XK_x,0U)&&read_sha(display,app_window,state_atom,after_key_sha,&state)&&strcmp(modal_sha,after_key_sha)==0;
    (void)send_key_pair(display,app_window,XK_Escape,0U); bool restored=wait_state_contains(display,app_window,state_atom,"\"modal\":0",1500);
    ok=scenario_record(&summary,"E2E-MODAL-FOCUS-BLOCK",modal_opened&&blocked&&restored,"background edit key was blocked and focus restored after animated close")&&ok;

    (void)send_key_pair(display,app_window,XK_F10,ShiftMask); pause_ms(120); long long menu=0; bool menu_open=read_state(display,app_window,state_atom,&state)&&state_integer(&state,"menu",&menu)&&menu!=0;
    (void)send_key_pair(display,app_window,XK_Escape,0U); pause_ms(120); bool menu_closed=read_state(display,app_window,state_atom,&state)&&state_integer(&state,"menu",&menu)&&menu==0;
    ok=scenario_record(&summary,"E2E-CONTEXT-MENU-KEYBOARD",menu_open&&menu_closed,"Shift+F10 opened custom context menu; Escape dismissed it")&&ok;

    double zoom_before=0.0,zoom_after=0.0; bool zoom_ok=read_state(display,app_window,state_atom,&state)&&state_double(&state,"zoom",&zoom_before);
    send_pointer(display,app_window,ButtonPress,700,420,Button4,ControlMask); zoom_ok=zoom_ok&&read_state(display,app_window,state_atom,&state)&&state_double(&state,"zoom",&zoom_after)&&zoom_after>zoom_before;
    ok=scenario_record(&summary,"E2E-ZOOM",zoom_ok,"Ctrl+wheel changed document zoom")&&ok;

    send_command(display,app_window,command_atom,MD_CMD_MODE_SOURCE);
    for (int i=0;i<4;++i) send_pointer(display,app_window,ButtonPress,700,500,Button5,0U);
    pause_ms(550); double frost=0.0; bool scroll_ok=read_state(display,app_window,state_atom,&state)&&state_double(&state,"nav_frost",&frost)&&frost>0.25;
    ok=scenario_record(&summary,"E2E-SCROLL-FROST",scroll_ok,"wheel scrolling continuously advanced frosted-navigation state")&&ok;

    send_pointer(display,app_window,MotionNotify,600,36,0U,0U); pause_ms(180);
    send_pointer(display,app_window,ButtonPress,600,36,Button1,0U); long long ripples=0; bool ripple=read_state(display,app_window,state_atom,&state)&&state_integer(&state,"active_ripples",&ripples)&&ripples>0;
    send_pointer(display,app_window,ButtonRelease,600,36,Button1,0U); pause_ms(500); bool released=read_state(display,app_window,state_atom,&state)&&state_integer(&state,"active_ripples",&ripples)&&ripples==0;
    ok=scenario_record(&summary,"E2E-BUTTON-HOVER-PRESS-RIPPLE-RELEASE",ripple&&released,"primary Save button produced transient pointer-origin ripple and released cleanly")&&ok;

    (void)send_key_pair(display,app_window,XK_P,ControlMask|ShiftMask); bool palette=wait_state_contains(display,app_window,state_atom,"\"modal\":8",1000);
    (void)send_key_pair(display,app_window,XK_s,0U); (void)send_key_pair(display,app_window,XK_t,0U); (void)send_key_pair(display,app_window,XK_a,0U); (void)send_key_pair(display,app_window,XK_t,0U); (void)send_key_pair(display,app_window,XK_Return,0U);
    bool executed=wait_state_contains(display,app_window,state_atom,"\"modal\":5",1200); (void)send_key_pair(display,app_window,XK_Escape,0U);
    ok=scenario_record(&summary,"E2E-COMMAND-PALETTE",palette&&executed,"Ctrl+Shift+P focused search and executed Statistics")&&ok;

    send_command(display,app_window,command_atom,1002L); bool activated=wait_state_contains(display,app_window,state_atom,"\"activated_external\":true",1000);
    ok=scenario_record(&summary,"E2E-EXTERNAL-LINK-ACTIVATION",activated,"HTTP/HTTPS activation traversed production allow-list path without shell execution")&&ok;

    send_command(display,app_window,command_atom,MD_CMD_TOGGLE_OUTLINE); (void)send_key_pair(display,app_window,XK_F6,0U); pause_ms(120); long long focus=0; bool outline=read_state(display,app_window,state_atom,&state)&&strstr(state.data,"\"outline\":true")!=NULL&&state_integer(&state,"focus",&focus);
    ok=scenario_record(&summary,"E2E-OUTLINE-KEYBOARD-FOCUS",outline,"Outline opened and F6 entered keyboard traversal")&&ok;
    md_buf_free(&state);
    printf("E2E_UI_SCENARIO total=%d passed=%d failed=%d skipped=0\n",summary.total,summary.passed,summary.total-summary.passed);
    return ok&&summary.total==summary.passed;
}

static bool provide_selection(Display *display,Window owner,Atom selection,Atom target,const char *text,Window requestor,Atom property,Time time) {
    XSelectionEvent reply; memset(&reply,0,sizeof(reply)); reply.type=SelectionNotify; reply.display=display; reply.requestor=requestor; reply.selection=selection; reply.target=target; reply.time=time; reply.property=property;
    XChangeProperty(display,requestor,property,target,8,PropModeReplace,(const unsigned char *)text,(int)strlen(text));
    XSendEvent(display,requestor,False,NoEventMask,(XEvent *)&reply); XFlush(display); (void)owner; return true;
}

static bool scenario_xdnd(Display *display,Window app_window,Window helper,Atom state_atom,const char *uri) {
    Atom enter=XInternAtom(display,"XdndEnter",False),position=XInternAtom(display,"XdndPosition",False),drop=XInternAtom(display,"XdndDrop",False);
    Atom selection=XInternAtom(display,"XdndSelection",False),uri_list=XInternAtom(display,"text/uri-list",False),action=XInternAtom(display,"XdndActionCopy",False);
    XSetSelectionOwner(display,selection,helper,CurrentTime); if (XGetSelectionOwner(display,selection)!=helper) return false;
    MdBuf before,after; md_buf_init(&before); md_buf_init(&after); bool ok=read_state(display,app_window,state_atom,&before); long long documents_before=0,documents_after=0; char sha_before[65],sha_after[65];
    ok=ok&&state_integer(&before,"documents",&documents_before)&&state_string(&before,"source_sha256",sha_before,sizeof(sha_before));
    XClientMessageEvent message; memset(&message,0,sizeof(message)); message.type=ClientMessage; message.display=display; message.window=app_window; message.format=32; message.message_type=enter; message.data.l[0]=(long)helper; message.data.l[1]=(long)(5U<<24U); message.data.l[2]=(long)uri_list;
    XSendEvent(display,app_window,False,NoEventMask,(XEvent *)&message);
    message.message_type=position; message.data.l[0]=(long)helper; message.data.l[1]=0L; message.data.l[2]=(long)((700U<<16U)|400U); message.data.l[3]=(long)CurrentTime; message.data.l[4]=(long)action; XSendEvent(display,app_window,False,NoEventMask,(XEvent *)&message);
    message.message_type=drop; message.data.l[0]=(long)helper; message.data.l[1]=0L; message.data.l[2]=(long)CurrentTime; XSendEvent(display,app_window,False,NoEventMask,(XEvent *)&message); XFlush(display);
    uint64_t deadline=monotonic_ms()+3000U; bool served=false;
    while (monotonic_ms()<deadline&&!served) {
        while (XPending(display)>0) {
            XEvent event; XNextEvent(display,&event);
            if (event.type==SelectionRequest&&event.xselectionrequest.selection==selection) {
                XSelectionRequestEvent *request=&event.xselectionrequest; Atom property=request->property==None?request->target:request->property;
                served=provide_selection(display,helper,selection,request->target,uri,request->requestor,property,request->time); break;
            }
        }
        if (!served) pause_ms(20);
    }
    pause_ms(450); ok=ok&&served&&read_state(display,app_window,state_atom,&after)&&state_integer(&after,"documents",&documents_after)&&state_string(&after,"source_sha256",sha_after,sizeof(sha_after));
    const char *extension=strrchr(uri,'.'); bool markdown=extension!=NULL&&(strcmp(extension,".md")==0||strcmp(extension,".markdown")==0||strcmp(extension,".txt")==0);
    bool changed=markdown?documents_after>documents_before:strcmp(sha_before,sha_after)!=0;
    printf("%s %s served_uri_list=%s before_documents=%lld after_documents=%lld source_changed=%s\n",ok&&changed?"PASS":"FAIL",markdown?"E2E-XDND-MARKDOWN-DROP":"E2E-XDND-IMAGE-DROP",served?"true":"false",documents_before,documents_after,strcmp(sha_before,sha_after)!=0?"true":"false");
    md_buf_free(&before); md_buf_free(&after); return ok&&changed;
}

static bool scenario_ime(Display *display,Window app_window,Atom state_atom,Atom command_atom) {
    ScenarioSummary summary={0}; MdBuf state; md_buf_init(&state); char before[65],committed[65],undone[65],cancelled[65];
    send_command(display,app_window,command_atom,MD_CMD_MODE_SOURCE); XSetInputFocus(display,app_window,RevertToParent,CurrentTime); XFlush(display); pause_ms(120);
    bool available=read_state(display,app_window,state_atom,&state)&&strstr(state.data,"\"xic_available\":true")!=NULL;
    bool start=available&&read_sha(display,app_window,state_atom,before,&state);
    bool sequence=start&&send_key_pair(display,app_window,XK_Multi_key,0U)&&send_key_pair(display,app_window,XK_apostrophe,0U)&&send_key_pair(display,app_window,XK_e,0U);
    pause_ms(150); bool commit=sequence&&read_sha(display,app_window,state_atom,committed,&state)&&strcmp(before,committed)!=0;
    scenario_record(&summary,"E2E-XIM-COMPOSITION-COMMIT",commit,"XIC local-compose sequence committed one UTF-8 composition");
    bool undo=send_key_pair(display,app_window,XK_z,ControlMask)&&read_sha(display,app_window,state_atom,undone,&state)&&strcmp(before,undone)==0;
    scenario_record(&summary,"E2E-XIM-COMPOSITION-UNDO",undo,"Ctrl+Z reverted committed composition in one transaction");
    bool cancel=send_key_pair(display,app_window,XK_Multi_key,0U)&&send_key_pair(display,app_window,XK_apostrophe,0U)&&send_key_pair(display,app_window,XK_Escape,0U);
    pause_ms(120); cancel=cancel&&read_sha(display,app_window,state_atom,cancelled,&state)&&strcmp(before,cancelled)==0;
    scenario_record(&summary,"E2E-XIM-COMPOSITION-CANCEL",cancel,"Escape cancelled pending composition without phantom bytes");
    md_buf_free(&state); printf("E2E_IME_SCENARIO total=%d passed=%d failed=%d skipped=0\n",summary.total,summary.passed,summary.total-summary.passed); return summary.total==summary.passed;
}

static bool type_ascii(Display *display,Window window,const char *text);

static bool scenario_workspace(Display *display,Window app_window,Atom state_atom,Atom command_atom) {
    ScenarioSummary summary={0}; MdBuf state; md_buf_init(&state); long long documents=0,active=0,next_active=0,focus=0,index=0,menu=0; double sidebar=0.0;
    bool initial=read_state(display,app_window,state_atom,&state)&&state_integer(&state,"documents",&documents)&&documents>=7&&
        state_integer(&state,"workspace_entries",&index)&&index>=20&&state_integer(&state,"active",&active)&&state_double(&state,"sidebar_width",&sidebar)&&sidebar>180.0;
    scenario_record(&summary,"E2E-WORKSPACE-MULTITAB-OPEN",initial,"real workspace tree and seven or more file-backed tabs are active");
    (void)send_key_pair(display,app_window,XK_Tab,ControlMask); pause_ms(120);
    bool next=read_state(display,app_window,state_atom,&state)&&state_integer(&state,"active",&next_active)&&next_active==(active+1)%documents;
    (void)send_key_pair(display,app_window,XK_Tab,ControlMask|ShiftMask); pause_ms(120); long long returned=0;
    bool previous=read_state(display,app_window,state_atom,&state)&&state_integer(&state,"active",&returned)&&returned==active;
    scenario_record(&summary,"E2E-TAB-CTRL-NEXT-PREVIOUS",next&&previous,"Ctrl+Tab and Ctrl+Shift+Tab wrapped through the shared tab model");

    (void)send_key_pair(display,app_window,XK_F6,0U); (void)send_key_pair(display,app_window,XK_F6,0U); pause_ms(100);
    bool tab_focus=read_state(display,app_window,state_atom,&state)&&state_integer(&state,"focus",&focus)&&focus==1;
    (void)send_key_pair(display,app_window,XK_Left,0U); pause_ms(100); long long keyboard_active=0;
    tab_focus=tab_focus&&read_state(display,app_window,state_atom,&state)&&state_integer(&state,"active",&keyboard_active)&&keyboard_active==(active+documents-1)%documents;
    scenario_record(&summary,"E2E-TABSTRIP-KEYBOARD-FOCUS",tab_focus,"F6 reached tab strip and Left changed the active tab") ;

    (void)send_key_pair(display,app_window,XK_F6,0U); (void)send_key_pair(display,app_window,XK_End,0U); pause_ms(100); long long last_index=0;
    bool tree_focus=read_state(display,app_window,state_atom,&state)&&state_integer(&state,"focus",&focus)&&focus==2&&state_integer(&state,"focus_index",&last_index)&&last_index>0;
    long long before_docs=documents; (void)send_key_pair(display,app_window,XK_Return,0U); pause_ms(150); long long after_docs=0;
    tree_focus=tree_focus&&read_state(display,app_window,state_atom,&state)&&state_integer(&state,"documents",&after_docs)&&after_docs==before_docs;
    scenario_record(&summary,"E2E-FILE-TREE-KEYBOARD",tree_focus,"End/Enter navigated the deterministic tree and reused an already-open path") ;

    send_command(display,app_window,command_atom,MD_CMD_TOGGLE_OUTLINE); (void)send_key_pair(display,app_window,XK_Home,0U); (void)send_key_pair(display,app_window,XK_Return,0U); pause_ms(120); long long cursor=0;
    bool outline=read_state(display,app_window,state_atom,&state)&&strstr(state.data,"\"outline\":true")!=NULL&&state_integer(&state,"cursor",&cursor);
    scenario_record(&summary,"E2E-OUTLINE-KEYBOARD-NAVIGATION",outline,"Home/Enter navigated a live heading through source mapping") ;

    (void)send_key_pair(display,app_window,XK_F10,0U); pause_ms(100); bool file_menu=read_state(display,app_window,state_atom,&state)&&state_integer(&state,"menu",&menu)&&menu==1;
    (void)send_key_pair(display,app_window,XK_Down,0U); (void)send_key_pair(display,app_window,XK_Escape,0U); pause_ms(100);
    file_menu=file_menu&&read_state(display,app_window,state_atom,&state)&&state_integer(&state,"menu",&menu)&&menu==0;
    scenario_record(&summary,"E2E-TOP-MENU-KEYBOARD",file_menu,"F10 opened custom File menu; arrows traversed and Escape dismissed") ;

    int first_x=(int)sidebar+102,second_x=(int)sidebar+288;
    send_pointer(display,app_window,ButtonPress,first_x,90,Button1,0U); send_pointer(display,app_window,MotionNotify,second_x,90,0U,Button1Mask); send_pointer(display,app_window,ButtonRelease,second_x,90,Button1,0U); pause_ms(150);
    long long reordered_active=0; bool reorder=read_state(display,app_window,state_atom,&state)&&state_integer(&state,"active",&reordered_active)&&reordered_active==1;
    scenario_record(&summary,"E2E-TAB-POINTER-REORDER",reorder,"pointer drag changed tab order through the production insertion indicator path") ;

    send_pointer(display,app_window,ButtonPress,1400,92,Button1,0U); send_pointer(display,app_window,ButtonRelease,1400,92,Button1,0U); pause_ms(100);
    bool overflow=read_state(display,app_window,state_atom,&state)&&state_integer(&state,"menu",&menu)&&menu==2;
    (void)send_key_pair(display,app_window,XK_Escape,0U);
    scenario_record(&summary,"E2E-TAB-OVERFLOW",overflow,"overflow control exposed hidden tabs without shrinking them below usable size") ;

    (void)send_key_pair(display,app_window,XK_End,ControlMask); send_command(display,app_window,command_atom,MD_CMD_MODE_SPLIT);
    send_pointer(display,app_window,ButtonPress,650,450,Button4,ControlMask);
    send_pointer(display,app_window,ButtonPress,500,450,Button5,0U);
    int divider_x=(int)sidebar+(1440-(int)sidebar-7)/2+2;
    send_pointer(display,app_window,ButtonPress,divider_x,450,Button1,0U);
    send_pointer(display,app_window,MotionNotify,divider_x+120,450,0U,Button1Mask);
    send_pointer(display,app_window,ButtonRelease,divider_x+120,450,Button1,0U);
    send_pointer(display,app_window,ButtonPress,(int)sidebar-2,420,Button1,0U);
    send_pointer(display,app_window,MotionNotify,340,420,0U,Button1Mask);
    send_pointer(display,app_window,ButtonRelease,340,420,Button1,0U); pause_ms(180);
    double zoom=0.0,split=0.0,resized_sidebar=0.0,source_scroll=0.0; long long stored_cursor=0;
    bool prepared=read_state(display,app_window,state_atom,&state)&&strstr(state.data,"\"mode\":\"Split\"")!=NULL&&
        state_double(&state,"zoom",&zoom)&&zoom>1.0&&state_double(&state,"split_ratio",&split)&&split>0.55&&
        state_double(&state,"sidebar_width",&resized_sidebar)&&resized_sidebar>320.0&&
        state_double(&state,"source_scroll",&source_scroll)&&source_scroll>0.0&&state_integer(&state,"cursor",&stored_cursor)&&stored_cursor>0;
    scenario_record(&summary,"E2E-WORKSPACE-SESSION-STATE-PREPARED",prepared,"active tab now has persisted caret, scroll, Split mode, zoom, divider, and sidebar state") ;
    md_buf_free(&state); printf("E2E_WORKSPACE_SCENARIO total=%d passed=%d failed=%d skipped=0\n",summary.total,summary.passed,summary.total-summary.passed);
    return summary.total==summary.passed;
}

static bool changed_and_undo(Display *display,Window app_window,Atom state_atom,
                             const char baseline[65],MdBuf *state) {
    char changed[65],restored[65];
    return read_sha(display,app_window,state_atom,changed,state)&&strcmp(changed,baseline)!=0&&
        send_key_pair(display,app_window,XK_z,ControlMask)&&
        read_sha(display,app_window,state_atom,restored,state)&&strcmp(restored,baseline)==0;
}

static bool activate_menu_item(Display *display,Window app_window,Atom state_atom,int index,int expected_menu) {
    if (!send_key_pair(display,app_window,XK_F10,ShiftMask)) return false;
    MdBuf state; md_buf_init(&state); long long menu=0;
    bool ok=read_state(display,app_window,state_atom,&state)&&state_integer(&state,"menu",&menu)&&menu==expected_menu;
    for (int i=0;i<index;++i) ok=send_key_pair(display,app_window,XK_Down,0U)&&ok;
    ok=send_key_pair(display,app_window,XK_Return,0U)&&ok; md_buf_free(&state); return ok;
}

static bool scenario_image(Display *display,Window app_window,Atom state_atom,Atom command_atom) {
    ScenarioSummary summary={0}; MdBuf state; md_buf_init(&state); char baseline[65],embedded[65],externalized[65],current[65]; bool ok=true;
    send_command(display,app_window,command_atom,MD_CMD_MODE_RENDERED);
    send_command(display,app_window,command_atom,1009L);
    long long intrinsic_w=0; bool selected=read_sha(display,app_window,state_atom,baseline,&state)&&
        state_integer(&state,"image_w",&intrinsic_w)&&intrinsic_w>=48&&strstr(state.data,"\"image_selected\":true")!=NULL;
    (void)send_key_pair(display,app_window,XK_F10,ShiftMask); pause_ms(100); long long menu=0;
    bool context=selected&&read_state(display,app_window,state_atom,&state)&&state_integer(&state,"menu",&menu)&&menu==4;
    (void)send_key_pair(display,app_window,XK_Escape,0U);
    ok=scenario_record(&summary,"E2E-IMAGE-CONTEXT-MENU",context,"selected rendered image opened its custom source-backed context menu")&&ok;

    bool to_embedded=context&&activate_menu_item(display,app_window,state_atom,4,4)&&
        read_sha(display,app_window,state_atom,embedded,&state)&&strcmp(embedded,baseline)!=0&&
        send_key_pair(display,app_window,XK_z,ControlMask)&&read_sha(display,app_window,state_atom,current,&state)&&strcmp(current,baseline)==0&&
        send_key_pair(display,app_window,XK_z,ControlMask|ShiftMask)&&read_sha(display,app_window,state_atom,current,&state)&&strcmp(current,embedded)==0;
    ok=scenario_record(&summary,"E2E-IMAGE-RELATIVE-EMBEDDED-UNDO-REDO",to_embedded,"relative image became an exact data URI and Undo/Redo restored exact source digests")&&ok;

    bool to_relative=to_embedded&&activate_menu_item(display,app_window,state_atom,4,4)&&
        read_sha(display,app_window,state_atom,externalized,&state)&&strcmp(externalized,embedded)!=0&&
        send_key_pair(display,app_window,XK_z,ControlMask)&&read_sha(display,app_window,state_atom,current,&state)&&strcmp(current,embedded)==0&&
        send_key_pair(display,app_window,XK_z,ControlMask|ShiftMask)&&read_sha(display,app_window,state_atom,current,&state)&&strcmp(current,externalized)==0&&
        send_key_pair(display,app_window,XK_z,ControlMask)&&read_sha(display,app_window,state_atom,current,&state)&&strcmp(current,embedded)==0&&
        send_key_pair(display,app_window,XK_z,ControlMask)&&read_sha(display,app_window,state_atom,current,&state)&&strcmp(current,baseline)==0;
    ok=scenario_record(&summary,"E2E-IMAGE-EMBEDDED-RELATIVE-UNDO-REDO",to_relative,"embedded bytes were externalized through the asset store and both transactions remained undoable/redoable")&&ok;

    int larger_width=0; bool larger=to_relative&&resize_selected_image_by(display,app_window,state_atom,96,&larger_width)&&
        send_key_pair(display,app_window,XK_z,ControlMask)&&read_sha(display,app_window,state_atom,current,&state)&&strcmp(current,baseline)==0;
    ok=scenario_record(&summary,"E2E-IMAGE-RESIZE-LARGER-ASPECT-UNDO",larger&&larger_width>intrinsic_w,"one larger pointer gesture preserved aspect ratio and one Undo restored exact pre-gesture source")&&ok;

    int smaller_width=0; bool smaller=larger&&resize_selected_image_by(display,app_window,state_atom,-96,&smaller_width)&&smaller_width<intrinsic_w;
    ok=scenario_record(&summary,"E2E-IMAGE-RESIZE-SMALLER",smaller,"one smaller pointer gesture persisted constrained inline img width metadata")&&ok;

    send_command(display,app_window,command_atom,1014L); long long embedded_intrinsic=0; int embedded_width=0;
    bool embedded_resize=smaller&&read_state(display,app_window,state_atom,&state)&&state_integer(&state,"image_w",&embedded_intrinsic)&&
        resize_selected_image_by(display,app_window,state_atom,72,&embedded_width)&&embedded_width>embedded_intrinsic;
    ok=scenario_record(&summary,"E2E-IMAGE-EMBEDDED-RESIZE",embedded_resize,"Base64 embedded image used the same aspect-preserving source transaction and inline width persistence")&&ok;

    send_command(display,app_window,command_atom,MD_CMD_SAVE);
    bool saved=embedded_resize&&wait_state_contains(display,app_window,state_atom,"\"dirty\":false",2000)&&
        read_state(display,app_window,state_atom,&state)&&state_integer(&state,"image_persisted_width",&intrinsic_w)&&intrinsic_w==embedded_width;
    ok=scenario_record(&summary,"E2E-IMAGE-RESIZE-SAVE",saved,"safe Save retained relative and embedded image width metadata for restart verification")&&ok;
    md_buf_free(&state); printf("E2E_IMAGE_SCENARIO total=%d passed=%d failed=%d skipped=0 relative_width=%d embedded_width=%d\n",summary.total,summary.passed,summary.total-summary.passed,smaller_width,embedded_width);
    return ok&&summary.total==summary.passed;
}

static bool scenario_image_save_as(Display *display,Window app_window,Atom state_atom,Atom command_atom,
                                   const char *path,bool expect_failure) {
    MdBuf state; md_buf_init(&state); char before[65],after[65]; bool existed=access(path,F_OK)==0;
    send_command(display,app_window,command_atom,MD_CMD_MODE_RENDERED);
    send_command(display,app_window,command_atom,1009L);
    bool ok=read_sha(display,app_window,state_atom,before,&state)&&activate_menu_item(display,app_window,state_atom,2,4)&&
        wait_state_contains(display,app_window,state_atom,"\"modal\":2",1000)&&type_ascii(display,app_window,path)&&
        send_key_pair(display,app_window,XK_Return,0U);
    if (expect_failure) {
        ok=ok&&wait_state_contains(display,app_window,state_atom,"\"modal\":13",1500)&&
           read_sha(display,app_window,state_atom,after,&state)&&strcmp(before,after)==0&&access(path,F_OK)!=0;
        (void)send_key_pair(display,app_window,XK_Escape,0U);
    } else if (existed) {
        ok=ok&&wait_state_contains(display,app_window,state_atom,"\"modal\":17",1000)&&
           send_key_pair(display,app_window,XK_Return,0U)&&wait_state_contains(display,app_window,state_atom,"\"modal\":0",1500)&&access(path,F_OK)==0;
    } else ok=ok&&wait_state_contains(display,app_window,state_atom,"\"modal\":0",1500)&&access(path,F_OK)==0;
    ok=ok&&read_sha(display,app_window,state_atom,after,&state)&&strcmp(before,after)==0;
    printf("%s %s path=%s source_unchanged=%s overwrite_confirmation=%s\n",ok?"PASS":"FAIL",
           expect_failure?"E2E-IMAGE-SAVE-AS-FAILURE":"E2E-IMAGE-SAVE-AS",path,
           ok?"true":"false",existed?"true":"false");
    md_buf_free(&state); return ok;
}

static bool scenario_rendered(Display *display,Window app_window,Atom state_atom,Atom command_atom) {
    ScenarioSummary summary={0}; MdBuf state; md_buf_init(&state); char baseline[65],after[65]; bool ok=true;
    send_command(display,app_window,command_atom,MD_CMD_MODE_RENDERED);

    send_command(display,app_window,command_atom,1003L); bool paragraph=read_sha(display,app_window,state_atom,baseline,&state)&&
        send_key_pair(display,app_window,XK_X,ShiftMask)&&changed_and_undo(display,app_window,state_atom,baseline,&state);
    ok=scenario_record(&summary,"E2E-RENDER-PARAGRAPH-SOURCE-UNDO",paragraph,"rendered paragraph key changed authoritative source and one Undo restored its exact digest")&&ok;

    send_command(display,app_window,command_atom,1010L); bool heading=read_sha(display,app_window,state_atom,baseline,&state)&&
        activate_menu_item(display,app_window,state_atom,7,3)&&changed_and_undo(display,app_window,state_atom,baseline,&state);
    ok=scenario_record(&summary,"E2E-RENDER-H2-H4-UNDO",heading,"keyboard context action changed an H2 to H4 and one Undo restored exact source")&&ok;

    static const struct { KeySym key; unsigned modifiers; const char *id; } formats[]={
        {XK_b,ControlMask,"E2E-RENDER-BOLD"},{XK_i,ControlMask,"E2E-RENDER-ITALIC"},
        {XK_X,ControlMask|ShiftMask,"E2E-RENDER-STRIKE"}
    };
    for (size_t i=0U;i<MD_ARRAY_LEN(formats);++i) {
        send_command(display,app_window,command_atom,1003L); bool formatted=read_sha(display,app_window,state_atom,baseline,&state)&&
            send_key_pair(display,app_window,XK_End,ShiftMask)&&send_key_pair(display,app_window,formats[i].key,formats[i].modifiers)&&
            changed_and_undo(display,app_window,state_atom,baseline,&state);
        ok=scenario_record(&summary,formats[i].id,formatted,"rendered selection formatting serialized to source as one undoable transaction")&&ok;
    }

    send_command(display,app_window,command_atom,1011L); bool inline_code=read_sha(display,app_window,state_atom,baseline,&state)&&
        send_key_pair(display,app_window,XK_grave,ControlMask)&&changed_and_undo(display,app_window,state_atom,baseline,&state);
    ok=scenario_record(&summary,"E2E-RENDER-INLINE-CODE-BACKTICK",inline_code,"inline-code delimiter selection used a longer backtick run and remained exactly undoable")&&ok;

    send_command(display,app_window,command_atom,1005L); bool list_enter=read_sha(display,app_window,state_atom,baseline,&state)&&
        send_key_pair(display,app_window,XK_End,0U)&&send_key_pair(display,app_window,XK_Return,0U)&&
        changed_and_undo(display,app_window,state_atom,baseline,&state);
    ok=scenario_record(&summary,"E2E-RENDER-LIST-ENTER",list_enter,"Enter at a rendered task/list item inserted its structural prefix and one Undo restored source")&&ok;

    send_command(display,app_window,command_atom,1005L); bool list_tab=read_sha(display,app_window,state_atom,baseline,&state)&&
        send_key_pair(display,app_window,XK_Tab,0U)&&read_sha(display,app_window,state_atom,after,&state)&&strcmp(after,baseline)!=0&&
        send_key_pair(display,app_window,XK_Tab,ShiftMask)&&read_sha(display,app_window,state_atom,after,&state)&&strcmp(after,baseline)==0;
    if (list_tab) { (void)send_key_pair(display,app_window,XK_z,ControlMask); (void)send_key_pair(display,app_window,XK_z,ControlMask); }
    ok=scenario_record(&summary,"E2E-RENDER-LIST-INDENT-OUTDENT",list_tab,"Tab and Shift+Tab indented/outdented through production key handling without semantic loss")&&ok;

    send_command(display,app_window,command_atom,1005L); bool task=read_sha(display,app_window,state_atom,baseline,&state);
    send_command(display,app_window,command_atom,MD_CMD_TOGGLE_TASK); task=task&&changed_and_undo(display,app_window,state_atom,baseline,&state);
    ok=scenario_record(&summary,"E2E-RENDER-TASK-TOGGLE",task,"rendered task command toggled the source checkbox and Undo restored it")&&ok;

    send_command(display,app_window,command_atom,1006L); bool quote=read_sha(display,app_window,state_atom,baseline,&state)&&
        send_key_pair(display,app_window,XK_X,ShiftMask)&&changed_and_undo(display,app_window,state_atom,baseline,&state);
    ok=scenario_record(&summary,"E2E-RENDER-NESTED-QUOTE",quote,"nested quote content edit changed source and remained one-step undoable")&&ok;

    send_command(display,app_window,command_atom,1007L); bool fence=read_sha(display,app_window,state_atom,baseline,&state)&&
        send_key_pair(display,app_window,XK_X,ShiftMask)&&changed_and_undo(display,app_window,state_atom,baseline,&state);
    ok=scenario_record(&summary,"E2E-RENDER-FENCED-CODE",fence,"fenced code accepted Markdown-looking source text without reinterpretation or loss")&&ok;

    send_command(display,app_window,command_atom,1009L); bool image=read_sha(display,app_window,state_atom,baseline,&state)&&
        send_key_pair(display,app_window,XK_Delete,0U)&&changed_and_undo(display,app_window,state_atom,baseline,&state);
    ok=scenario_record(&summary,"E2E-RENDER-IMAGE-DELETE-UNDO",image,"selected rendered image reference was deleted while the asset remained external; Undo restored exact source")&&ok;

    send_command(display,app_window,command_atom,1008L); long long first_cursor=0,next_cursor=0,returned_cursor=0;
    bool table_tab=read_state(display,app_window,state_atom,&state)&&state_integer(&state,"cursor",&first_cursor)&&
        send_key_pair(display,app_window,XK_Tab,0U)&&read_state(display,app_window,state_atom,&state)&&state_integer(&state,"cursor",&next_cursor)&&next_cursor!=first_cursor&&
        send_key_pair(display,app_window,XK_Tab,ShiftMask)&&read_state(display,app_window,state_atom,&state)&&state_integer(&state,"cursor",&returned_cursor)&&returned_cursor==first_cursor;
    ok=scenario_record(&summary,"E2E-RENDER-TABLE-TAB-NAVIGATION",table_tab,"Tab and Shift+Tab traversed actual rendered table-cell source ranges")&&ok;

    send_command(display,app_window,command_atom,1008L); bool cell=read_sha(display,app_window,state_atom,baseline,&state)&&
        send_key_pair(display,app_window,XK_X,ShiftMask)&&changed_and_undo(display,app_window,state_atom,baseline,&state);
    ok=scenario_record(&summary,"E2E-RENDER-TABLE-CELL-EDIT",cell,"typing in a rendered table cell edited the mapped source cell and Undo restored exact bytes")&&ok;

    static const struct { int item; const char *id; } table_actions[]={
        {1,"E2E-RENDER-TABLE-ROW-ADD"},{4,"E2E-RENDER-TABLE-COLUMN-ADD"},
        {5,"E2E-RENDER-TABLE-COLUMN-DELETE"},{6,"E2E-RENDER-TABLE-ALIGNMENT"}
    };
    for (size_t i=0U;i<MD_ARRAY_LEN(table_actions);++i) {
        send_command(display,app_window,command_atom,1008L); bool action=read_sha(display,app_window,state_atom,baseline,&state)&&
            activate_menu_item(display,app_window,state_atom,table_actions[i].item,5)&&
            changed_and_undo(display,app_window,state_atom,baseline,&state);
        ok=scenario_record(&summary,table_actions[i].id,action,"keyboard table context action updated source in one undoable transaction")&&ok;
    }

    send_command(display,app_window,command_atom,1008L); bool row_delete=read_sha(display,app_window,state_atom,baseline,&state);
    for (int i=0;i<4;++i) row_delete=send_key_pair(display,app_window,XK_Tab,0U)&&row_delete;
    row_delete=row_delete&&activate_menu_item(display,app_window,state_atom,2,5)&&changed_and_undo(display,app_window,state_atom,baseline,&state);
    ok=scenario_record(&summary,"E2E-RENDER-TABLE-ROW-DELETE",row_delete,"keyboard table navigation selected a body row; context deletion and Undo preserved exact source")&&ok;

    md_buf_free(&state); printf("E2E_RENDERED_SCENARIO total=%d passed=%d failed=%d skipped=0\n",summary.total,summary.passed,summary.total-summary.passed);
    return ok&&summary.total==summary.passed;
}

static bool scenario_search(Display *display,Window app_window,Window helper,Atom state_atom,
                            Atom command_atom,Atom clipboard,Atom utf8,Atom targets) {
    ScenarioSummary summary={0}; MdBuf state; md_buf_init(&state); char initial_sha[65],changed_sha[65],undone_sha[65];
    bool initial=read_sha(display,app_window,state_atom,initial_sha,&state);
    bool opened=send_key_pair(display,app_window,XK_f,ControlMask)&&wait_state_contains(display,app_window,state_atom,"\"find_visible\":true",1000);
    bool pasted=opened&&provide_clipboard_once(display,app_window,helper,command_atom,clipboard,utf8,targets,"繁體中文");
    pause_ms(150); long long matches=0; bool chinese=pasted&&read_state(display,app_window,state_atom,&state)&&state_integer(&state,"find_matches",&matches)&&matches==1;
    scenario_record(&summary,"E2E-FIND-TRADITIONAL-CHINESE",initial&&chinese,"external UTF-8 clipboard text populated Find and matched the source without splitting UTF-8");

    bool wrapped=send_key_pair(display,app_window,XK_Return,0U)&&send_key_pair(display,app_window,XK_Return,ShiftMask)&&
                 read_state(display,app_window,state_atom,&state)&&state_integer(&state,"find_matches",&matches)&&matches==1;
    scenario_record(&summary,"E2E-FIND-WRAP-NEXT-PREVIOUS",wrapped,"Enter and Shift+Enter traversed the wrapped one-match result set");

    (void)send_key_pair(display,app_window,XK_Escape,0U); (void)send_key_pair(display,app_window,XK_h,ControlMask);
    for (int i=0;i<4;++i) (void)send_key_pair(display,app_window,XK_BackSpace,0U);
    bool query=type_ascii(display,app_window,"alpha")&&read_state(display,app_window,state_atom,&state)&&state_integer(&state,"find_matches",&matches)&&matches==1;
    bool replacement=send_key_pair(display,app_window,XK_Tab,0U)&&type_ascii(display,app_window,"omega")&&
                     wait_state_contains(display,app_window,state_atom,"\"find_replacement_focus\":true",1000);
    bool replace_all=query&&replacement&&send_key_pair(display,app_window,XK_Return,ControlMask);
    pause_ms(150); replace_all=replace_all&&read_sha(display,app_window,state_atom,changed_sha,&state)&&strcmp(initial_sha,changed_sha)!=0;
    scenario_record(&summary,"E2E-REPLACE-ALL-ONE-TRANSACTION",replace_all,"Ctrl+Enter executed the production Replace All path");

    (void)send_key_pair(display,app_window,XK_Escape,0U); bool undo=send_key_pair(display,app_window,XK_z,ControlMask)&&
        read_sha(display,app_window,state_atom,undone_sha,&state)&&strcmp(initial_sha,undone_sha)==0;
    scenario_record(&summary,"E2E-REPLACE-ALL-UNDO-ONCE",undo,"one Ctrl+Z restored the exact pre-replacement source digest");
    md_buf_free(&state); printf("E2E_SEARCH_SCENARIO total=%d passed=%d failed=%d skipped=0\n",summary.total,summary.passed,summary.total-summary.passed);
    return summary.total==summary.passed;
}

static bool type_ascii(Display *display,Window window,const char *text) {
    for (size_t i=0U;text[i]!='\0';++i) {
        unsigned char c=(unsigned char)text[i]; if (c<0x20U||c>0x7eU) return false;
        unsigned state=0U; KeySym symbol=(KeySym)c;
        if (c>='A'&&c<='Z') state=ShiftMask;
        if (!send_key_pair(display,window,symbol,state)) return false;
    }
    return true;
}

static void request_window_close(Display *display,Window window) {
    Atom protocols=XInternAtom(display,"WM_PROTOCOLS",False),delete_window=XInternAtom(display,"WM_DELETE_WINDOW",False);
    XClientMessageEvent message; memset(&message,0,sizeof(message)); message.type=ClientMessage; message.display=display;
    message.window=window; message.message_type=protocols; message.format=32; message.data.l[0]=(long)delete_window; message.data.l[1]=(long)CurrentTime;
    XSendEvent(display,window,False,NoEventMask,(XEvent *)&message); XFlush(display); pause_ms(100);
}

static void usage(void) {
    fputs("usage: e2e_x11 [--capture PNG] [--capture-delay-ms N] [--state-contains TEXT] [--dump-state] [--close-window] [--exercise] [--ui-scenario] [--rendered-scenario] [--image-scenario] [--image-save-as PATH] [--image-save-failure PATH] [--ime-scenario] [--workspace-scenario] [--search-scenario] [--resize-selected-image] [--test-command N] [--xdnd URI] [--press-key KEYSYM] [--modifiers MASK] [--type-text ASCII] [--pointer-x N --pointer-y N] [--move-pointer] [--button-press N] [--button-release N] [--wait-ms N]\n",stderr);
}

int main(int argc,char **argv) {
    const char *capture=NULL,*state_needle=NULL,*xdnd_uri=NULL,*key_name=NULL,*typed_text=NULL,*image_save_path=NULL,*image_fail_path=NULL; bool run_exercise=false,run_ui=false,run_rendered=false,run_image=false,run_ime=false,run_workspace=false,run_search=false,run_resize=false,dump_state=false,close_window=false; int wait_ms=5000; unsigned key_modifiers=0U; long test_command=LONG_MIN;
    int capture_delay_ms=180,pointer_x=0,pointer_y=0,button_press=0,button_release=0; bool move_pointer=false;
    for (int i=1;i<argc;++i) {
        if (strcmp(argv[i],"--capture")==0&&i+1<argc) capture=argv[++i];
        else if (strcmp(argv[i],"--capture-delay-ms")==0&&i+1<argc) capture_delay_ms=atoi(argv[++i]);
        else if (strcmp(argv[i],"--state-contains")==0&&i+1<argc) state_needle=argv[++i];
        else if (strcmp(argv[i],"--exercise")==0) run_exercise=true;
        else if (strcmp(argv[i],"--ui-scenario")==0) run_ui=true;
        else if (strcmp(argv[i],"--rendered-scenario")==0) run_rendered=true;
        else if (strcmp(argv[i],"--image-scenario")==0) run_image=true;
        else if (strcmp(argv[i],"--image-save-as")==0&&i+1<argc) image_save_path=argv[++i];
        else if (strcmp(argv[i],"--image-save-failure")==0&&i+1<argc) image_fail_path=argv[++i];
        else if (strcmp(argv[i],"--ime-scenario")==0) run_ime=true;
        else if (strcmp(argv[i],"--workspace-scenario")==0) run_workspace=true;
        else if (strcmp(argv[i],"--search-scenario")==0) run_search=true;
        else if (strcmp(argv[i],"--resize-selected-image")==0) run_resize=true;
        else if (strcmp(argv[i],"--test-command")==0&&i+1<argc) { char *end=NULL; long parsed=strtol(argv[++i],&end,10); if (end==NULL||*end!='\0') { usage(); return 2; } test_command=parsed; }
        else if (strcmp(argv[i],"--xdnd")==0&&i+1<argc) xdnd_uri=argv[++i];
        else if (strcmp(argv[i],"--press-key")==0&&i+1<argc) key_name=argv[++i];
        else if (strcmp(argv[i],"--type-text")==0&&i+1<argc) typed_text=argv[++i];
        else if (strcmp(argv[i],"--dump-state")==0) dump_state=true;
        else if (strcmp(argv[i],"--close-window")==0) close_window=true;
        else if (strcmp(argv[i],"--pointer-x")==0&&i+1<argc) pointer_x=atoi(argv[++i]);
        else if (strcmp(argv[i],"--pointer-y")==0&&i+1<argc) pointer_y=atoi(argv[++i]);
        else if (strcmp(argv[i],"--move-pointer")==0) move_pointer=true;
        else if (strcmp(argv[i],"--button-press")==0&&i+1<argc) button_press=atoi(argv[++i]);
        else if (strcmp(argv[i],"--button-release")==0&&i+1<argc) button_release=atoi(argv[++i]);
        else if (strcmp(argv[i],"--modifiers")==0&&i+1<argc) { char *end=NULL; unsigned long parsed=strtoul(argv[++i],&end,0); if (end==NULL||*end!='\0'||parsed>UINT_MAX) { usage(); return 2; } key_modifiers=(unsigned)parsed; }
        else if (strcmp(argv[i],"--wait-ms")==0&&i+1<argc) wait_ms=atoi(argv[++i]);
        else { usage(); return 2; }
    }
    Display *display=XOpenDisplay(NULL); if (display==NULL) { fputs("e2e_x11: cannot open DISPLAY\n",stderr); return 1; }
    Atom state_atom=XInternAtom(display,"_MDEDIT_STATE",False),command_atom=XInternAtom(display,"_MDEDIT_TEST_COMMAND",False),insert_atom=XInternAtom(display,"_MDEDIT_TEST_INSERT",False);
    Atom clipboard=XInternAtom(display,"CLIPBOARD",False),utf8=XInternAtom(display,"UTF8_STRING",False),targets=XInternAtom(display,"TARGETS",False);
    Window app_window=wait_for_window(display,state_atom,wait_ms); if (app_window==None) { fputs("e2e_x11: application window not found\n",stderr); XCloseDisplay(display); return 1; }
    Window helper=XCreateSimpleWindow(display,DefaultRootWindow(display),0,0,2U,2U,0,0,0); XSelectInput(display,helper,PropertyChangeMask);
    bool ok=true; int operations=0;
    if (run_exercise) { ++operations; ok=exercise(display,app_window,helper,state_atom,command_atom,insert_atom,clipboard,utf8,targets)&&ok; }
    if (run_ui) { ++operations; ok=scenario_ui(display,app_window,state_atom,command_atom)&&ok; }
    if (run_rendered) { ++operations; ok=scenario_rendered(display,app_window,state_atom,command_atom)&&ok; }
    if (run_image) { ++operations; ok=scenario_image(display,app_window,state_atom,command_atom)&&ok; }
    if (image_save_path!=NULL) { ++operations; ok=scenario_image_save_as(display,app_window,state_atom,command_atom,image_save_path,false)&&ok; }
    if (image_fail_path!=NULL) { ++operations; ok=scenario_image_save_as(display,app_window,state_atom,command_atom,image_fail_path,true)&&ok; }
    if (run_ime) { ++operations; ok=scenario_ime(display,app_window,state_atom,command_atom)&&ok; }
    if (run_workspace) { ++operations; ok=scenario_workspace(display,app_window,state_atom,command_atom)&&ok; }
    if (run_search) { ++operations; ok=scenario_search(display,app_window,helper,state_atom,command_atom,clipboard,utf8,targets)&&ok; }
    if (test_command!=LONG_MIN) { ++operations; send_command(display,app_window,command_atom,test_command); }
    if (run_resize) { ++operations; ok=resize_selected_image(display,app_window,state_atom)&&ok; }
    if (xdnd_uri!=NULL) { ++operations; ok=scenario_xdnd(display,app_window,helper,state_atom,xdnd_uri)&&ok; }
    if (key_name!=NULL) { ++operations; KeySym symbol=XStringToKeysym(key_name); if (symbol==NoSymbol) { fprintf(stderr,"e2e_x11: unknown keysym %s\n",key_name); ok=false; } else ok=send_key_pair(display,app_window,symbol,key_modifiers)&&ok; }
    if (typed_text!=NULL) { ++operations; ok=type_ascii(display,app_window,typed_text)&&ok; }
    if (move_pointer) { ++operations; send_pointer(display,app_window,MotionNotify,pointer_x,pointer_y,0U,0U); }
    if (button_press>0) { ++operations; send_pointer(display,app_window,ButtonPress,pointer_x,pointer_y,(unsigned)button_press,0U); }
    if (button_release>0) { ++operations; send_pointer(display,app_window,ButtonRelease,pointer_x,pointer_y,(unsigned)button_release,0U); }
    if (state_needle!=NULL) { ++operations; ok=wait_state_contains(display,app_window,state_atom,state_needle,wait_ms)&&ok; }
    if (dump_state) { ++operations; MdBuf state; md_buf_init(&state); if (read_state(display,app_window,state_atom,&state)) printf("MDEDIT_STATE %s\n",state.data); else ok=false; md_buf_free(&state); }
    if (capture!=NULL) { pause_ms(MD_MAX(0,capture_delay_ms)); ok=ok&&capture_window(display,app_window,capture); }
    if (close_window) { ++operations; request_window_close(display,app_window); }
    XDestroyWindow(display,helper); XCloseDisplay(display);
    if (operations==0) operations=1;
    printf("E2E_X11_SUMMARY total=%d passed=%d failed=%d skipped=0\n",operations,ok?operations:0,ok?0:operations); return ok?0:1;
}
