/* See LICENSE file for copyright and license details.
 *
 * dynamic window manager is designed like any other X client as well. It is
 * driven through handling X events. In contrast to other X clients, a window
 * manager selects for SubstructureRedirectMask on the root window, to receive
 * events about window (dis-)appearance. Only one X connection at a time is
 * allowed to select for this event mask.
 *
 * The event handlers of dwm are organized in an array which is accessed
 * whenever a new event has been fetched. This allows event dispatching
 * in O(1) time.
 *
 * Each child of the root window is called a client, except windows which have
 * set the override_redirect flag. Clients are organized in a linked client
 * list on each monitor, the focus history is remembered through a stack list
 * on each monitor. Each client contains a bit array to indicate the tags of a
 * client.
 *
 * Keys and tagging rules are organized as arrays and defined in config.h.
 *
 * To understand everything else, start reading main().
 */
#include <locale.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/Xresource.h>
#ifdef XINERAMA
#include <X11/extensions/Xinerama.h>
#endif /* XINERAMA */
#include <X11/Xft/Xft.h>
#include <X11/Xlib-xcb.h>
#include <xcb/res.h>
#ifdef __OpenBSD__
#include <sys/sysctl.h>
#include <kvm.h>
#endif /* __OpenBSD */

#include "drw.h"
#include "util.h"

/* macros */
#define BUTTONMASK              (ButtonPressMask|ButtonReleaseMask)
#define CLEANMASK(mask)         (mask & ~(numlockmask|LockMask) & (ShiftMask|ControlMask|Mod1Mask|Mod2Mask|Mod3Mask|Mod4Mask|Mod5Mask))
#define GETINC(X)               ((X) - 2000)
#define INC(X)                  ((X) + 2000)
#define INTERSECT(x,y,w,h,m)    (MAX(0, MIN((x)+(w),(m)->wx+(m)->ww) - MAX((x),(m)->wx)) \
                               * MAX(0, MIN((y)+(h),(m)->wy+(m)->wh) - MAX((y),(m)->wy)))
#define ISINC(X)                ((X) > 1000 && (X) < 3000)
#define ISFULLSCREEN(C)         (C && (C->fstag != -1))
#define ISVISIBLEONTAG(C, T)    (C->tags & T)
#define ISVISIBLESTICKY(C)      (C->mon->sticky == C && (!C->mon->pertag->fullscreens[C->mon->pertag->curtag] || ISFULLSCREEN(C)))
#define ISVISIBLE(C)            (C && C->mon && (C->mon->seltags == 1 || C->mon->seltags == 0) && (ISVISIBLEONTAG(C, C->mon->tagset[C->mon->seltags]) || ISVISIBLESTICKY(C)))
#define PREVSEL                 3000
#define LEFTSEL                 2000
#define RIGHTSEL                1000
#define LENGTH(X)               (sizeof X / sizeof X[0])
#define MOD(N,M)                ((N)%(M) < 0 ? (N)%(M) + (M) : (N)%(M))
#define ROUNDTOZERO(X)          ((fmod(X, 0.5)) ? round(X) : floor(X))
#define MOUSEMASK               (BUTTONMASK|PointerMotionMask)
#define WIDTH(X)                ((X)->w + 2 * (X)->bw + gappx)
#define HEIGHT(X)               ((X)->h + 2 * (X)->bw + gappx)
#define TAGMASK                 ((1 << LENGTH(tags)) - 1)
#define TAGSLENGTH              (LENGTH(tags))
#define TEXTW(X)                (drw_fontset_getwidth(drw, (X)) + lrpad)
#define TRUNC(X,A,B)            (MAX((A), MIN((X), (B))))

#define SYSTEM_TRAY_REQUEST_DOCK    0
#define MAX_TOP_CLIENTS 100

/* XEMBED messages */
#define XEMBED_EMBEDDED_NOTIFY      0
#define XEMBED_WINDOW_ACTIVATE      1
#define XEMBED_FOCUS_IN             4
#define XEMBED_MODALITY_ON         10

#define XEMBED_MAPPED              (1 << 0)
#define XEMBED_WINDOW_ACTIVATE      1
#define XEMBED_WINDOW_DEACTIVATE    2

#define VERSION_MAJOR               0
#define VERSION_MINOR               0
#define XEMBED_EMBEDDED_VERSION (VERSION_MAJOR << 16) | VERSION_MINOR

/* enums */
enum { CurNormal, CurResize, CurMove, CurLast }; /* cursor */
enum { SchemeNorm, SchemeSel, SchemeUrg }; /* color schemes */
enum { NetSupported, NetWMName, NetWMState, NetWMStateAbove, NetWMCheck,
       NetSystemTray, NetSystemTrayOP, NetSystemTrayOrientation, NetSystemTrayOrientationHorz,
       NetWMFullscreen, NetWMWindowTypeDialog, NetWMWindowTypeSplash, NetWMWindowTypeToolbar, NetWMWindowTypeUtility, NetActiveWindow, NetWMWindowType,
       NetClientList, NetClientListStacking, NetDesktopNames, NetDesktopViewport, NetNumberOfDesktops, NetCurrentDesktop, NetLast }; /* EWMH atoms */
enum { Manager, Xembed, XembedInfo, XLast }; /* Xembed atoms */
enum { WMProtocols, WMDelete, WMState, WMTakeFocus, WMLast }; /* default atoms */
enum { ClkTagBar, ClkLtSymbol, ClkStatusText, ClkWinTitle,
       ClkClientWin, ClkRootWin, ClkLast }; /* clicks */

typedef union {
  int i;
  unsigned int ui;
  float f;
  const void *v;
} Arg;

typedef struct {
  unsigned int click;
  unsigned int mask;
  unsigned int button;
  void (*func)(const Arg *arg);
  const Arg arg;
} Button;

typedef struct Monitor Monitor;
typedef struct Client Client;

struct Client {
  char name[256];
  float mina, maxa;
  float cfact;
  int x, y, w, h;
  int sfx, sfy, sfw, sfh; /* stored float geometry, used on mode revert */
  int oldx, oldy, oldw, oldh;
  int basew, baseh, incw, inch, maxw, maxh, minw, minh, hintsvalid;
  int bw, oldbw;
  int initx, inity;
  unsigned int tags, cmesetfs;
  int fstag, isfixed, isfloating, isurgent, neverfocus, oldstate, needresize;
  int alwaysontop, ignoremoverequest, grabonurgent, noswallow, isterminal;
  pid_t pid;
  char scratchkey;
  Client *next;
  Client *snext;
  Client *swallowing;
  Monitor *mon;
  Window win;
};

typedef struct {
  unsigned int mod;
  KeySym keysym;
  void (*func)(const Arg *);
  const Arg arg;
} Key;

typedef struct {
  const char *symbol;
  void (*arrange)(Monitor *);
} Layout;

typedef struct Pertag Pertag;

struct Monitor {
  char ltsymbol[16];
  float mfact;
  int nmaster;
  int num;
  int by;               /* bar geometry */
  int mx, my, mw, mh;   /* screen size */
  int wx, wy, ww, wh;   /* window area  */
  unsigned int seltags;
  unsigned int sellt;
  unsigned int tagset[2];
  int showbar;
  int topbar;
  Client *sticky;
  Client *clients;
  Client *sel;
  Client *stack;
  Monitor *next;
  Window barwin;
  const Layout *lt[2];
  Pertag *pertag;
};

typedef struct {
  const char *class;
  const char *instance;
  const char *title;
  unsigned int tags;
  int isfloating;
  int monitor;
  int ignoremoverequest;
  int grabonurgent;
  const char scratchkey;
  int noswallow;
  int isterminal;
} Rule;

/* Xresources preferences */
enum resource_type {
  STRING = 0,
  INTEGER = 1,
  FLOAT = 2
};

typedef struct {
  char *name;
  enum resource_type type;
  void *dst;
} ResourcePref;

typedef struct {
  Window win;
  Client *icons;
} Systray;

typedef struct {
  unsigned int signum;
  void (*func)(const Arg *);
  const Arg arg;
} Signal;

/* function declarations */
static void applyrules(Client *c);
static void dwmdebug(void);
static int applysizehints(Client *c, int *x, int *y, int *w, int *h, int interact);
static void arrange(Monitor *m);
static void aspectresize(const Arg *arg);
static void arrangemon(Monitor *m);
static void attach(Client *c);
static void attachbelow(Client *c);
static void attachbottom(Client *c);
static void attachabove(Client *c);
static void cycleattachdir(const Arg *arg);
static void attachstack(Client *c);
static void buttonpress(XEvent *e);
static void checkotherwm(void);
static void cleanup(void);
static void cleanupmon(Monitor *mon);
static void clientmessage(XEvent *e);
static void configure(Client *c);
static void center(const Arg *arg);
static void configuremonlayout(Monitor *m);
static void configurenotify(XEvent *e);
static void configurerequest(XEvent *e);
static Monitor *createmon(void);
static void configureclientpos(Client *c, Window s, int pos);
static void destroynotify(XEvent *e);
static void deck(Monitor *m);
static void dwindle(Monitor *m);
static void detach(Client *c);
static void detachstack(Client *c);
static Monitor *dirtomon(int dir);
static void drawbar(Monitor *m);
static void drawbars(void);
static void enternotify(XEvent *e);
static void expose(XEvent *e);
static int fakesignal(void);
static void focus(Client *c);
static void focusin(XEvent *e);
static void focusmon(const Arg *arg);
static void focusstack(const Arg *arg);
static Atom getatomprop(Client *c, Atom prop);
static int getrootptr(int *x, int *y);
static long getstate(Window w);
static unsigned int getsystraywidth();
static int gettextprop(Window w, Atom atom, char *text, unsigned int size);
static void grabbuttons(Client *c, int focused);
static void grabkeys(void);
static void grid(Monitor *m);
static void incnmaster(const Arg *arg);
static void resetnmaster(const Arg *arg);
static void keypress(XEvent *e);
static void killclient(const Arg *arg);
static void manage(Window w, XWindowAttributes *wa);
static void mappingnotify(XEvent *e);
static void maprequest(XEvent *e);
static void monocle(Monitor *m);
static void motionnotify(XEvent *e);
static void movemouse(const Arg *arg);
static Client *nexttiled(Client *c);
static int parentiseditor(pid_t w);
static void propertynotify(XEvent *e);
static void pushstack(const Arg *arg);
static void quit(const Arg *arg);
static void raiseclient(Client *c);
static Monitor *recttomon(int x, int y, int w, int h);
static void removesystrayicon(Client *i);
static void resize(Client *c, int x, int y, int w, int h, int interact);
static void resizebarwin(Monitor *m);
static void resizeclient(Client *c, int x, int y, int w, int h);
static void resizemouse(const Arg *arg);
static void resizerequest(XEvent *e);
static void restack(Monitor *m);
static void resetfact(const Arg *arg);
static void run(void);
static void runautostart(void);
static void scan(void);
static int sendevent(Window w, Atom proto, int m, long d0, long d1, long d2, long d3, long d4);
static void sendmon(Client *c, Monitor *m);
static void setclientstate(Client *c, long state);
static void setcurrentdesktop(void);
static void setdesktopnames(void);
static void setdesktopforclient(Client *c, int tag);
static void setfocus(Client *c);
static void setclientgeo(Client *c, XWindowAttributes *wa);
static void sethidden(Client *c, int hidden);
static void setfullscreen(Client *c, int fullscreen, int f);
static void setfullscreenontag(Client *c, int fullscreen, int tag, int f);
static void setlayout(const Arg *arg);
static void setmfact(const Arg *arg);
static void setcfact(const Arg *arg);
static void setnumdesktops(void);
static void setup(void);
static void setviewport(void);
static void seturgent(Client *c, int urg);
static void showhide(Client *c);
static void spawn(const Arg *arg);
static int stackpos(const Arg *arg);
static Monitor *systraytomon(Monitor *m);
static void spawnscratch(const Arg *arg);
static void tag(const Arg *arg);
static void tagmon(const Arg *arg);
static void tile(Monitor *m);
static void togglebar(const Arg *arg);
static void toggleswal(const Arg *arg);
static void togglefloating(const Arg *arg);
static void togglefullscr(const Arg *arg);
static void togglescratch(const Arg *arg);
static void togglesticky(const Arg *arg);
static void fibonacci(Monitor *m, int s);
static int gcd(int a, int b);
static void grabfocus (Client *c);
static void unfocus(Client *c, int setfocus);
static void unfocusmon(Monitor *m);
static void unmanage(Client *c, int destroyed);
static void unmapnotify(XEvent *e);
static void updatecurrentdesktop(void);
static void updatebarpos(Monitor *m);
static void updatebars(void);
static void updateclientlist(void);
static int updategeom(void);
static void updatenumlockmask(void);
static void updatesizehints(Client *c);
static void updatestatus(void);
static void updatesystray(void);
static void updatesystrayicongeom(Client *i, int w, int h);
static void updatesystrayiconstate(Client *i, XPropertyEvent *ev);
static void updatetitle(Client *c);
static void updatewmhints(Client *c);
static void view(const Arg *arg);
static Client *wintoclient(Window w);
static Monitor *wintomon(Window w);
static Client *wintosystrayicon(Window w);
static int xerror(Display *dpy, XErrorEvent *ee);
static int xerrordummy(Display *dpy, XErrorEvent *ee);
static int xerrorstart(Display *dpy, XErrorEvent *ee);
static pid_t getparentprocess(pid_t p);
static int isdescprocess(pid_t p, pid_t c);
static Client *swallowingclient(Window w);
static Client *termforwin(const Client *c);
static pid_t winpid(Window w);
static void swallow(Client *p, Client* c);
static void unswallow(Client* c);
static void load_xresources(void);
static void resource_load(XrmDatabase db, char *name, enum resource_type rtype, void *dst);

/* variables */
static Systray *systray =  NULL;
static const char autostartblocksh[] = "autostart_blocking.sh";
static const char autostartsh[] = "autostart.sh";
static const char broken[] = "broken";
static const char dwmdir[] = "config/dwm";
static const char localshare[] = ".local/share";
static int scw, sch;         /*scratch width, height calced at runtime */
static char stext[256];
static int screen;
static int sw, sh;           /* X display screen geometry width, height */
static int bh;               /* bar height*/
static int lrpad;            /* sum of left and right padding for text */
static int (*xerrorxlib)(Display *, XErrorEvent *);
static unsigned int numlockmask = 0;
static void (*handler[LASTEvent]) (XEvent *) = {
  [ButtonPress] = buttonpress,
  [ClientMessage] = clientmessage,
  [ConfigureRequest] = configurerequest,
  [ConfigureNotify] = configurenotify,
  [DestroyNotify] = destroynotify,
  [EnterNotify] = enternotify,
  [Expose] = expose,
  [FocusIn] = focusin,
  [KeyPress] = keypress,
  [MappingNotify] = mappingnotify,
  [MapRequest] = maprequest,
  [MotionNotify] = motionnotify,
  [PropertyNotify] = propertynotify,
  [ResizeRequest] = resizerequest,
  [UnmapNotify] = unmapnotify
};
static Atom wmatom[WMLast], netatom[NetLast], xatom[XLast];
static int running = 1;
static int swal = 1;
static Cur *cursor[CurLast];
static Clr **scheme;
static Display *dpy;
static Drw *drw;
static Monitor *mons, *selmon;
static Window root, wmcheckwin;
static xcb_connection_t *xcon;

/* configuration, allows nested code to access above variables */
#include "config.h"

struct Pertag {
  unsigned int curtag, prevtag; /* current and previous tag */
  int nmasters[LENGTH(tags) + 1]; /* number of windows in master area */
  float mfacts[LENGTH(tags) + 1]; /* mfacts per tag */
  unsigned int sellts[LENGTH(tags) + 1]; /* selected layouts */
  const Layout *ltidxs[LENGTH(tags) + 1][2]; /* matrix of tags and layouts indexes  */
  int showbars[LENGTH(tags) + 1]; /* display bar for the current tag */
  int attachdir[LENGTH(tags) + 1];
  Client *fullscreens[LENGTH(tags) + 1]; /* array of fullscreen clients at pos tag */
};

/* compile-time check if all tags fit into an unsigned int bit array. */
struct NumTags { char limitexceeded[LENGTH(tags) > 31 ? -1 : 1]; };

static void
dwmdebug(void)
{
  volatile int a = 0;
  volatile int b = 0;
  while(a == 0){
    b = 1;
  }
}

/* function implementations */
void
applyrules(Client *c)
{
  const char *class, *instance;
  unsigned int i;
  const Rule *r;
  Monitor *m;
  Atom wintype;
  XClassHint ch = { NULL, NULL };

  /* rule matching */
  c->isfloating = 0;
  c->tags = 0;
  c->ignoremoverequest = 0;
  c->grabonurgent = 1;
  c->scratchkey = 0;
  c->fstag = -1;
  c->cmesetfs = 0;
  c->noswallow = 0;
  c->isterminal = 0;
  XGetClassHint(dpy, c->win, &ch);
  class    = ch.res_class ? ch.res_class : broken;
  instance = ch.res_name  ? ch.res_name  : broken;
  wintype  = getatomprop(c, netatom[NetWMWindowType]);

  for (i = 0; i < LENGTH(rules); i++) {
    r = &rules[i];
    if ((!r->title || strstr(c->name, r->title))
    && (!r->class || strstr(class, r->class))
    && (!r->instance || strstr(instance, r->instance)))
    {
      c->isfloating = r->isfloating;
      c->tags |= r->tags;
      c->scratchkey = r->scratchkey;
      c->noswallow= r->noswallow;
      c->isterminal= r->isterminal;
      c->ignoremoverequest = r->ignoremoverequest;
      c->grabonurgent = r->grabonurgent;
      for (m = mons; m && m->num != r->monitor; m = m->next);
      if (m)
        c->mon = m;
    }
  }

  c->alwaysontop = 1 ? ((wintype == netatom[NetWMWindowTypeSplash]) ||
    (wintype == netatom[NetWMWindowTypeToolbar]) ||
    (wintype == netatom[NetWMWindowTypeDialog]) ||
    (wintype == netatom[NetWMWindowTypeUtility])) : 0;

  if (ch.res_class)
    XFree(ch.res_class);
  if (ch.res_name)
    XFree(ch.res_name);

  c->tags = c->tags & TAGMASK ? c->tags & TAGMASK : c->mon->tagset[c->mon->seltags];
}

int
applysizehints(Client *c, int *x, int *y, int *w, int *h, int interact)
{
  int baseismin;
  Monitor *m = c->mon;

  /* set minimum possible */
  *w = MAX(1, *w);
  *h = MAX(1, *h);
  if (interact) {
    if (*x > sw)
      *x = sw - WIDTH(c);
    if (*y > sh)
      *y = sh - HEIGHT(c);
    if (*x + *w + 2 * c->bw < 0)
      *x = 0;
    if (*y + *h + 2 * c->bw < 0)
      *y = 0;
  } else {
    if (*x >= m->wx + m->ww)
      *x = m->wx + m->ww - WIDTH(c);
    if (*y >= m->wy + m->wh)
      *y = m->wy + m->wh - HEIGHT(c);
    if (*x + *w + 2 * c->bw <= m->wx)
      *x = m->wx;
    if (*y + *h + 2 * c->bw <= m->wy)
      *y = m->wy;
  }
  if (*h < bh)
    *h = bh;
  if (*w < bh)
    *w = bh;
  if (resizehints || c->isfloating || !c->mon->lt[c->mon->sellt]->arrange) {
    if (!c->hintsvalid)
      updatesizehints(c);
    /* see last two sentences in ICCCM 4.1.2.3 */
    baseismin = c->basew == c->minw && c->baseh == c->minh;
    if (!baseismin) { /* temporarily remove base dimensions */
      *w -= c->basew;
      *h -= c->baseh;
    }
    /* adjust for aspect limits */
    if (c->mina > 0 && c->maxa > 0) {
      if (c->maxa < (float)*w / *h)
        *w = *h * c->maxa + 0.5;
      else if (c->mina < (float)*h / *w)
        *h = *w * c->mina + 0.5;
    }
    if (baseismin) { /* increment calculation requires this */
      *w -= c->basew;
      *h -= c->baseh;
    }
    /* adjust for increment value */
    if (c->incw)
      *w -= *w % c->incw;
    if (c->inch)
      *h -= *h % c->inch;
    /* restore base dimensions */
    *w = MAX(*w + c->basew, c->minw);
    *h = MAX(*h + c->baseh, c->minh);
    if (c->maxw)
      *w = MIN(*w, c->maxw);
    if (c->maxh)
      *h = MIN(*h, c->maxh);
  }
  return *x != c->x || *y != c->y || *w != c->w || *h != c->h;
}

void
aspectresize(const Arg *arg)
{
  Client *c;
  c = selmon->sel;
  int cx = c->x;
  int cy = c->y;
  float r = gcd(selmon->mw, selmon->mh);
  float wratio = selmon->mw/r;
  float hratio = selmon->mh/r;
  if ((c->isfloating || !c->mon->lt[c->mon->sellt]->arrange) &&
  !((int)(10*wratio/hratio)-1 <= (int)(10*(float)c->w/c->h) && (int)(10*wratio/hratio)+1 >= (int)(10*(float)c->w/c->h))) {
    float base = (float)c->w/c->h < wratio/hratio ? c->w/wratio : c->h/hratio;
    int cw = base * wratio;
    int ch = base * hratio;
    resize(c, cx, cy, cw - (2 * c->bw), ch - (2 * c->bw), False);
  }
}

void
arrange(Monitor *m)
{
  if (m)
    showhide(m->stack);
  else for (m = mons; m; m = m->next)
    showhide(m->stack);
  if (m) {
    arrangemon(m);
    restack(m);
  } else for (m = mons; m; m = m->next) {
    arrangemon(m);
    restack(m);
  }
}

void
arrangemon(Monitor *m)
{
  strncpy(m->ltsymbol, m->lt[m->sellt]->symbol, sizeof m->ltsymbol);
  if (m->lt[m->sellt]->arrange)
    m->lt[m->sellt]->arrange(m);
}

void
attachtop(Client *c)
{
  c->next = c->mon->clients;
  c->mon->clients = c;
}

void
attachbelow(Client *c)
{
  //If there is nothing on the monitor or the selected client is floating, attach as normal
  if(c->mon->sel == NULL || c->mon->sel == c || (c->mon->sel->isfloating && !ISFULLSCREEN(c->mon->sel))) {
    attachbottom(c);
    return;
  }
  //Set the new client's next property to the same as the currently selected clients next
  c->next = c->mon->sel->next;
  //Set the currently selected clients next property to the new client
  c->mon->sel->next = c;
}

void
attachabove(Client *c)
{
  if (c->mon->sel == NULL || c->mon->sel == c->mon->clients || (c->mon->sel->isfloating && !ISFULLSCREEN(c->mon->sel))) {
    attachtop(c);
    return;
  }

  Client *at;
  for (at = c->mon->clients; at && at->next != c->mon->sel; at = at->next);
  c->next = at->next;
  at->next = c;
}

void
attachbottom(Client *c)
{
  Client *below = c->mon->clients;
  for (; below && below->next; below = below->next);
  c->next = NULL;
  if (below)
    below->next = c;
  else
    c->mon->clients = c;
}

void
attach(Client *c){
  switch (c->mon->pertag->attachdir[c->mon->pertag->curtag]) {
    case 0:
      attachbelow(c);
      break;
    case 1:
      attachbottom(c);
      break;
    case 2:
      attachabove(c);
      break;
    case 3:
      attachtop(c);
      break;
    default:
      attachtop(c);
  }
}

void
cycleattachdir(const Arg *arg)
{
  selmon->pertag->attachdir[selmon->pertag->curtag] = MOD(selmon->pertag->attachdir[selmon->pertag->curtag] + (int)arg->i, (int)LENGTH(stack_symbols));
  drawbar(selmon);
}

void
swallow(Client *p, Client *c)
{
  XWindowChanges wc;

  if (c->noswallow > 0 || (!swal && !strstr("st-vimmode", c->name)))
    return;

  XMapWindow(dpy, c->win);

  detach(c);
  detachstack(c);

  setclientstate(p, WithdrawnState);
  XUnmapWindow(dpy, p->win);

  p->swallowing = c;
  c->mon = p->mon;

  Window w = p->win;
  p->win = c->win;
  c->win = w;

  if (p->scratchkey)
    raiseclient(p);

  setdesktopforclient(p, p->mon->pertag->curtag);

  XChangeProperty(dpy, c->win, netatom[NetClientList], XA_WINDOW, 32, PropModeReplace,
    (unsigned char *) &(p->win), 1);

  updatetitle(p);

  wc.border_width = p->bw;
  XConfigureWindow(dpy, p->win, CWBorderWidth, &wc);
  XMoveResizeWindow(dpy, p->win, p->x, p->y, p->w, p->h);
  XSetWindowBorder(dpy, p->win, scheme[SchemeNorm][ColBorder].pixel);
  arrange(p->mon);
  configure(p);
  updateclientlist();
}

void
unswallow(Client *c)
{
  XWindowChanges wc;
  c->win = c->swallowing->win;

  /* unfullscreen the client */
  setfullscreen(c->swallowing, 0, 0);
  free(c->swallowing);
  c->swallowing = NULL;

  XDeleteProperty(dpy, c->win, netatom[NetClientList]);

  updatetitle(c);
  arrange(c->mon);
  XMapWindow(dpy, c->win);

  wc.border_width = c->bw;
  XConfigureWindow(dpy, c->win, CWBorderWidth, &wc);
  XMoveResizeWindow(dpy, c->win, c->x, c->y, c->w, c->h);
  setclientstate(c, NormalState);
  focus(NULL);
  arrange(c->mon);
  updateclientlist();
}

void
attachstack(Client *c)
{
  c->snext = c->mon->stack;
  c->mon->stack = c;
}

void
buttonpress(XEvent *e)
{
  unsigned int i, x, click, occ = 0;
  Arg arg = {0};
  Client *c;
  Monitor *m;
  XButtonPressedEvent *ev = &e->xbutton;

  click = ClkRootWin;
  /* focus monitor if necessary */
  if ((m = wintomon(ev->window)) && m != selmon) {
    unfocus(selmon->sel, 1);
    selmon = m;
    focus(NULL);
  }
  if (ev->window == selmon->barwin) {
    i = x = 0;
    for (c = m->clients; c; c = c->next)
      occ |= c->tags == 255 ? 0 : c->tags;
    do {
      /* do not reserve space for vacant tags */
      if (!(occ & 1 << i || m->tagset[m->seltags] & 1 << i))
        continue;
      x += TEXTW(tags[i]);
    } while (ev->x >= x && ++i < LENGTH(tags));
    if (i < LENGTH(tags)) {
      click = ClkTagBar;
      arg.ui = 1 << i;
    } else if (ev->x < x + TEXTW(selmon->ltsymbol))
      click = ClkLtSymbol;
    else if (ev->x > selmon->ww - (int)TEXTW(stext) - getsystraywidth())
      click = ClkStatusText;
    else
      click = ClkWinTitle;
  } else if ((c = wintoclient(ev->window))) {
    focus(c);
    restack(selmon);
    XAllowEvents(dpy, ReplayPointer, CurrentTime);
    click = ClkClientWin;
  }
  for (i = 0; i < LENGTH(buttons); i++)
    if (click == buttons[i].click && buttons[i].func && buttons[i].button == ev->button
    && CLEANMASK(buttons[i].mask) == CLEANMASK(ev->state))
      buttons[i].func(click == ClkTagBar && buttons[i].arg.i == 0 ? &arg : &buttons[i].arg);
}

void
checkotherwm(void)
{
  xerrorxlib = XSetErrorHandler(xerrorstart);
  /* this causes an error if some other window manager is running */
  XSelectInput(dpy, DefaultRootWindow(dpy), SubstructureRedirectMask);
  XSync(dpy, False);
  XSetErrorHandler(xerror);
  XSync(dpy, False);
}

void
cleanup(void)
{
  Arg a = {.ui = ~0};
  Layout foo = { "", NULL };
  Monitor *m;
  size_t i;

  view(&a);
  selmon->lt[selmon->sellt] = &foo;
  for (m = mons; m; m = m->next)
    while (m->stack)
      unmanage(m->stack, 0);
  XUngrabKey(dpy, AnyKey, AnyModifier, root);
  while (mons)
    cleanupmon(mons);
  if (showsystray) {
    XUnmapWindow(dpy, systray->win);
    XDestroyWindow(dpy, systray->win);
    free(systray);
  }
  for (i = 0; i < CurLast; i++)
    drw_cur_free(drw, cursor[i]);
  for (i = 0; i < LENGTH(colors); i++)
    free(scheme[i]);
  free(scheme);
  XDestroyWindow(dpy, wmcheckwin);
  drw_free(drw);
  XSync(dpy, False);
  XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
  XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
}

void
cleanupmon(Monitor *mon)
{
  Monitor *m;

  if (mon == mons)
    mons = mons->next;
  else {
    for (m = mons; m && m->next != mon; m = m->next);
    m->next = mon->next;
  }
  XUnmapWindow(dpy, mon->barwin);
  XDestroyWindow(dpy, mon->barwin);
  free(mon->pertag);
  free(mon);
}

void
clientmessage(XEvent *e)
{
  XWindowAttributes wa;
  XSetWindowAttributes swa;
  XClientMessageEvent *cme = &e->xclient;
  Client *c = wintoclient(cme->window);

  if (showsystray && cme->window == systray->win && cme->message_type == netatom[NetSystemTrayOP]) {
    /* add systray icons */
    if (cme->data.l[1] == SYSTEM_TRAY_REQUEST_DOCK) {
      if (!(c = (Client *)calloc(1, sizeof(Client))))
        die("fatal: could not malloc() %u bytes\n", sizeof(Client));
      if (!(c->win = cme->data.l[2])) {
        free(c);
        return;
      }
      c->mon = selmon;
      c->next = systray->icons;
      systray->icons = c;
      if (!XGetWindowAttributes(dpy, c->win, &wa)) {
        /* use sane defaults */
        wa.width = bh;
        wa.height = bh;
        wa.border_width = 0;
      }
      c->x = c->oldx = c->y = c->oldy = 0;
      c->w = c->oldw = wa.width;
      c->h = c->oldh = wa.height;
      c->oldbw = wa.border_width;
      c->bw = 0;
      c->isfloating = True;
      /* reuse tags field as mapped status */
      c->tags = 1;
      updatesizehints(c);
      updatesystrayicongeom(c, wa.width, wa.height);
      XAddToSaveSet(dpy, c->win);
      XSelectInput(dpy, c->win, StructureNotifyMask | PropertyChangeMask | ResizeRedirectMask);
      XReparentWindow(dpy, c->win, systray->win, 0, 0);
      /* use parents background color */
      swa.background_pixel  = scheme[SchemeNorm][ColBg].pixel;
      XChangeWindowAttributes(dpy, c->win, CWBackPixel, &swa);
      sendevent(c->win, netatom[Xembed], StructureNotifyMask, CurrentTime, XEMBED_EMBEDDED_NOTIFY, 0 , systray->win, XEMBED_EMBEDDED_VERSION);
      /* FIXME not sure if I have to send these events, too */
      sendevent(c->win, netatom[Xembed], StructureNotifyMask, CurrentTime, XEMBED_FOCUS_IN, 0 , systray->win, XEMBED_EMBEDDED_VERSION);
      sendevent(c->win, netatom[Xembed], StructureNotifyMask, CurrentTime, XEMBED_WINDOW_ACTIVATE, 0 , systray->win, XEMBED_EMBEDDED_VERSION);
      sendevent(c->win, netatom[Xembed], StructureNotifyMask, CurrentTime, XEMBED_MODALITY_ON, 0 , systray->win, XEMBED_EMBEDDED_VERSION);
      XSync(dpy, False);
      resizebarwin(selmon);
      updatesystray();
      setclientstate(c, NormalState);
    }
    return;
  }
  if (!c)
    return;
  if (cme->message_type == netatom[NetWMState]) {
    if (cme->data.l[1] == netatom[NetWMFullscreen]
    || cme->data.l[2] == netatom[NetWMFullscreen]) {
      if (cme->data.l[0] == 1) { /* _NET_WM_STATE_ADD */
        if (ISFULLSCREEN(c))
          c->cmesetfs = 1;
        else
          setfullscreen(c, 1, 1);
      }
      else if (cme->data.l[0] == 0 && ISFULLSCREEN(c)) { /* _NET_WM_STATE_REMOVE */
        if (c->cmesetfs)
          c->cmesetfs = 0;
        else
          setfullscreen(c, 0, 1);
      }
      else if (cme->data.l[0] == 2) /* _NET_WM_STATE_TOGGLE*/
        setfullscreen(c, !ISFULLSCREEN(c), 1);
    }
  /* else if (cme->data.l[1] == netatom[NetWMStateAbove] */
  /*   || cme->data.l[2] == netatom[NetWMStateAbove]) */
  /*   c->alwaysontop = (cme->data.l[0] || cme->data.l[1]); */
  } else if (cme->message_type == netatom[NetActiveWindow]) {
    seturgent(c, 1);
    if (c->grabonurgent)
      grabfocus(c);
  }
}

void
center(const Arg *arg)
{
  Client *c;
  c = selmon->sel;
  if (c->isfloating || !c->mon->lt[c->mon->sellt]->arrange) {
    c->x = selmon->mx + (selmon->mw / 2 - WIDTH(c) / 2);
    c->y = selmon->my + (selmon->mh / 2 - HEIGHT(c) / 2);
    arrange(selmon);
  }
}

void
configure(Client *c)
{
  XConfigureEvent ce;

  ce.type = ConfigureNotify;
  ce.display = dpy;
  ce.event = c->win;
  ce.window = c->win;
  ce.x = c->x;
  ce.y = c->y;
  ce.width = c->w;
  ce.height = c->h;
  ce.border_width = c->bw;
  ce.above = None;
  ce.override_redirect = False;
  XSendEvent(dpy, c->win, False, StructureNotifyMask, (XEvent *)&ce);
}

void
configuremonlayout(Monitor *m)
{
  Client *c, *s = NULL, *f = NULL;
  Client *tops[MAX_TOP_CLIENTS];
  int i = 0;
  int hasfloat = 0;
  Window sib;

  for (c = m->stack; c; c = c->snext) {
    if (ISVISIBLE(c)) {
      if (i < MAX_TOP_CLIENTS && c->alwaysontop && c->isfloating) {
        tops[i] = c;
        i++;
      }
      if (c->scratchkey)
        s = c;
      if (ISFULLSCREEN(c))
        f = c;
      if (!hasfloat && (c->isfloating || !m->lt[m->sellt]->arrange))
        hasfloat = 1;
    }
  }

  if (!hasfloat && m->lt[m->sellt]->arrange != monocle && m->lt[m->sellt]->arrange != deck)
    return;

  if (i == 0) {
    sib = m->barwin;
    for (c = m->stack; c; c = c->snext) {
      if (ISVISIBLE(c)) {
        if ((!c->isfloating && m->lt[m->sellt]->arrange) || (f && (c->isfloating || !m->lt[m->sellt]->arrange) && c != f)) {
          configureclientpos(c, sib, Below);
          sib = c->win;
        } else {
          raiseclient(c);
        }
      }
    }
    if (f && s && f != s)
      configureclientpos(s, f->win, Above);
    else if (s)
      configureclientpos(s, m->stack->win, Above);
  } else {
    sib = m->barwin;
    for (c = m->stack; c; c = c->snext) {
      if (ISVISIBLE(c)) {
        if (ISFULLSCREEN(c))
          setfullscreen(c, 0, 0);
        if (!c->alwaysontop && !c->scratchkey) {
          configureclientpos(c, sib, Below);
          sib = c->win;
        }
      }
    }
    configureclientpos(tops[0], m->stack->win, TopIf);
    if (tops[0] && s && tops[0] != s)
      configureclientpos(s, tops[0]->win, Below);
    for (int k=1; k < i; k++) {
      configureclientpos(tops[k], tops[k-1]->win, Below);
    }
  }
}

void
configurenotify(XEvent *e)
{
  Monitor *m;
  Client *c;
  XConfigureEvent *ev = &e->xconfigure;
  int dirty;

  /* TODO: updategeom handling sucks, needs to be simplified */
  if (ev->window == root) {
    dirty = (sw != ev->width || sh != ev->height);
    sw = ev->width;
    sh = ev->height;
    if (updategeom() || dirty) {
      drw_resize(drw, sw, bh);
      updatebars();
      for (m = mons; m; m = m->next) {
        for (c = m->clients; c; c = c->next)
          if (ISFULLSCREEN(c) && ISVISIBLE(c))
            resizeclient(c, m->mx, m->my, m->mw, m->mh);
        resizebarwin(m);
      }
      focus(NULL);
      arrange(NULL);
    }
  }
}

void
configurerequest(XEvent *e)
{
  Client *c;
  Monitor *m;
  XConfigureRequestEvent *ev = &e->xconfigurerequest;
  XWindowChanges wc;

  if ((c = wintoclient(ev->window))) {
    if (ev->value_mask & CWBorderWidth)
      c->bw = ev->border_width;
    else if ((c->isfloating && !ISFULLSCREEN(c) && !c->swallowing) || !selmon->lt[selmon->sellt]->arrange) {
      m = c->mon;
      if (!c->ignoremoverequest) {
        if (ev->value_mask & CWX) {
          c->oldx = c->x;
          c->x = m->mx + ev->x;
        }
        if (ev->value_mask & CWY) {
          c->oldy = c->y;
          c->y = m->my + ev->y;
        }
      }
      if (ev->value_mask & CWX) {
        c->oldx = c->x;
        c->x = m->mx + ev->x;
      }
      if (ev->value_mask & CWY) {
        c->oldy = c->y;
        c->y = m->my + ev->y;
      }
      if (ev->value_mask & CWWidth) {
        c->oldw = c->w;
        c->w = ev->width;
      }
      if (ev->value_mask & CWHeight) {
        c->oldh = c->h;
        c->h = ev->height;
      }
      if ((c->x + c->w) > m->mx + m->mw && c->isfloating)
        c->x = m->mx + (m->mw / 2 - WIDTH(c) / 2); /* center in x direction */
      if ((c->y + c->h) > m->my + m->mh && c->isfloating)
        c->y = m->my + (m->mh / 2 - HEIGHT(c) / 2); /* center in y direction */
      if ((ev->value_mask & (CWX|CWY)) && !(ev->value_mask & (CWWidth|CWHeight)))
        configure(c);
      if (ISVISIBLE(c))
        XMoveResizeWindow(dpy, c->win, c->x, c->y, c->w, c->h);
      else
        c->needresize = 1;
    } else
      configure(c);
  } else {
    wc.x = ev->x;
    wc.y = ev->y;
    wc.width = ev->width;
    wc.height = ev->height;
    wc.border_width = ev->border_width;
    wc.sibling = ev->above;
    wc.stack_mode = ev->detail;
    XConfigureWindow(dpy, ev->window, ev->value_mask, &wc);
  }
  XSync(dpy, False);
}

void
configureclientpos(Client *c, Window s, int pos)
{
  XWindowChanges wc;
  wc.stack_mode = pos;
  wc.sibling = s;
  XConfigureWindow(dpy, c->win, CWSibling|CWStackMode, &wc);
}

Monitor *
createmon(void)
{
  Monitor *m;
  unsigned int i;

  m = ecalloc(1, sizeof(Monitor));
  m->tagset[0] = m->tagset[1] = 1;
  m->mfact = mfact;
  m->nmaster = nmaster;
  m->showbar = showbar;
  m->topbar = topbar;
  m->lt[0] = &layouts[0];
  m->lt[1] = &layouts[1 % LENGTH(layouts)];
  m->sticky = NULL;
  strncpy(m->ltsymbol, layouts[0].symbol, sizeof m->ltsymbol);
  m->pertag = ecalloc(1, sizeof(Pertag));
  m->pertag->curtag = m->pertag->prevtag = 1;

  for (i = 0; i <= LENGTH(tags); i++) {
    m->pertag->nmasters[i] = m->nmaster;
    m->pertag->mfacts[i] = m->mfact;
    if (i == 0)
      m->pertag->ltidxs[i][0] = &layouts[2];
    else
      m->pertag->ltidxs[i][0] = m->lt[0];
    m->pertag->ltidxs[i][1] = m->lt[1];
    m->pertag->sellts[i] = m->sellt;

    m->pertag->showbars[i] = m->showbar;
    m->pertag->attachdir[i] = defaultatchdir;
    m->pertag->fullscreens[i] = NULL;
  }

  return m;
}

void
destroynotify(XEvent *e)
{
  Client *c;
  XDestroyWindowEvent *ev = &e->xdestroywindow;

  if ((c = wintoclient(ev->window)))
    unmanage(c, 1);
  if ((c = wintosystrayicon(ev->window))) {
    removesystrayicon(c);
    resizebarwin(selmon);
    updatesystray();
  }
  if ((c = swallowingclient(ev->window)))
    unmanage(c->swallowing, 1);
}

void
deck(Monitor *m)
{
  unsigned int i, n, h, mw, my;
  float mfacts = 0;
  Client *c;

  for (n = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), n++) {
    if (n < m->nmaster)
      mfacts += c->cfact;
  }
  if(n == 0)
    return;

  if(n > m->nmaster) {
    mw = m->nmaster ? m->ww * m->mfact : 0;
    snprintf(m->ltsymbol, sizeof m->ltsymbol, "[%d]", n - m->nmaster);
  }
  else
    mw = m->ww;
  for(i = my = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), i++)
    if(i < m->nmaster) {
      h = (m->wh - my) * (c->cfact / mfacts);
      resize(c, m->wx, m->wy + my, mw - (2*c->bw), h - (2*c->bw), False);
      if (my + HEIGHT(c) < m->wh)
        my += HEIGHT(c);
      mfacts -= c->cfact;
    }
    else
      resize(c, m->wx + mw, m->wy, m->ww - mw - (2*c->bw), m->wh - (2*c->bw), False);
}

void
detach(Client *c)
{
  Client **tc;

  for (tc = &c->mon->clients; *tc && *tc != c; tc = &(*tc)->next);
  *tc = c->next;
}

void
detachstack(Client *c)
{
  Client **tc, *t;

  for (tc = &c->mon->stack; *tc && *tc != c; tc = &(*tc)->snext);
  *tc = c->snext;

  if (c == c->mon->sel) {
    for (t = c->mon->stack; t && !ISVISIBLE(t); t = t->snext);
    c->mon->sel = t;
  }
}

Monitor *
dirtomon(int dir)
{
  Monitor *m = NULL;

  if (dir > 0) {
    if (!(m = selmon->next))
      m = mons;
  } else if (selmon == mons)
    for (m = mons; m->next; m = m->next);
  else
    for (m = mons; m->next != selmon; m = m->next);
  return m;
}

void
drawbar(Monitor *m)
{
  int x, w, tw = 0, stw = 0;
  int boxs = drw->fonts->h / 9;
  int boxw = drw->fonts->h / 6 + 2;
  char symbol_and_orei[10];
  unsigned int i, occ = 0, urg = 0;
  Client *c;

  if (!m->showbar || ISFULLSCREEN(m->sel))
    return;

  if (showsystray && m == systraytomon(m) && !systrayonleft)
    stw = getsystraywidth();

  if (m == selmon) {
    /* draw status first so it can be overdrawn by tags later */
    drw_setscheme(drw, scheme[SchemeNorm]);
    tw = TEXTW(stext) - lrpad / 2 + 2; /* 2px extra right padding */
    drw_text(drw, m->ww - tw - stw, 0, tw, bh, lrpad / 2 - 2, stext, 0);
  }

  resizebarwin(m);
  for (c = m->clients; c; c = c->next) {
    occ |= c->tags == 255 ? 0 : c->tags;
    if (c->isurgent)
      urg |= c->tags;
  }
  x = 0;
  for (i = 0; i < LENGTH(tags); i++) {
    /* do not draw vacant tags */
    if (!(occ & 1 << i || m->tagset[m->seltags] & 1 << i))
      continue;
    w = TEXTW(tags[i]);
    drw_setscheme(drw, scheme[m->tagset[m->seltags] & 1 << i ? SchemeSel : SchemeNorm]);
    drw_text(drw, x, 0, w, bh, lrpad / 2, tags[i], urg & 1 << i);
    x += w;
  }
  strcat(strcpy(symbol_and_orei, m->ltsymbol), stack_symbols[m->pertag->attachdir[m->pertag->curtag]]);
  w = TEXTW(symbol_and_orei);
  drw_setscheme(drw, scheme[SchemeNorm]);
  x = drw_text(drw, x, 0, w, bh, lrpad / 2, symbol_and_orei, 0);

  if ((w = m->ww - tw - stw - x) > bh) {
    if (m->sel) {
      drw_setscheme(drw, scheme[m == selmon ? SchemeSel : SchemeNorm]);
      drw_text(drw, x, 0, w, bh, lrpad / 2, m->sel->name, 0);
      if (m->sel->isfloating)
        drw_rect(drw, x + boxs, boxs, boxw, boxw, m->sel->isfixed, 0);
      if (selmon->sticky == m->sel)
        drw_polygon(drw, x + boxs, m->sel->isfloating ? boxs * 2 + boxw : boxs, stickyiconbb.x, stickyiconbb.y, boxw, boxw * stickyiconbb.y / stickyiconbb.x, stickyicon, LENGTH(stickyicon), Nonconvex, m->sel->tags & m->tagset[m->seltags]);
    } else {
      drw_setscheme(drw, scheme[SchemeNorm]);
      drw_rect(drw, x, 0, w, bh, 1, 1);
    }
  }
  drw_map(drw, m->barwin, 0, 0, m->ww - stw, bh);
}

void
drawbars(void)
{
  Monitor *m;

  for (m = mons; m; m = m->next)
    drawbar(m);
}

void
enternotify(XEvent *e)
{
  Client *c;
  Monitor *m;
  XCrossingEvent *ev = &e->xcrossing;
  XEvent xev;

  if ((ev->mode != NotifyNormal || ev->detail == NotifyInferior) && ev->window != root)
    return;
  c = wintoclient(ev->window);
  m = c ? c->mon : wintomon(ev->window);
  if (m != selmon) {
    unfocus(selmon->sel, 1);
    selmon = m;
  } else if (!c || c == selmon->sel)
    return;
  focus(c);
  restack(selmon);
  while (XCheckMaskEvent(dpy, EnterWindowMask, &xev));
}

void
expose(XEvent *e)
{
  Monitor *m;
  XExposeEvent *ev = &e->xexpose;

  if (ev->count == 0 && (m = wintomon(ev->window))) {
    drawbar(m);
    if (m == selmon)
      updatesystray();
  }
}

int
fakesignal(void)
{
  char fsignal[256];
  char indicator[9] = "fsignal:";
  char str_signum[16];
  int i, v, signum;
  size_t len_fsignal, len_indicator = strlen(indicator);

  // Get root name property
  if (gettextprop(root, XA_WM_NAME, fsignal, sizeof(fsignal))) {
    len_fsignal = strlen(fsignal);

    // Check if this is indeed a fake signal
    if (len_indicator > len_fsignal ? 0 : strncmp(indicator, fsignal, len_indicator) == 0) {
      memcpy(str_signum, &fsignal[len_indicator], len_fsignal - len_indicator);
      str_signum[len_fsignal - len_indicator] = '\0';

      // Convert string value into managable integer
      for (i = signum = 0; i < strlen(str_signum); i++) {
        v = str_signum[i] - '0';
        if (v >= 0 && v <= 9) {
          signum = signum * 10 + v;
        }
      }

      // Check if a signal was found, and if so handle it
      if (signum)
        for (i = 0; i < LENGTH(signals); i++)
          if (signum == signals[i].signum && signals[i].func)
            signals[i].func(&(signals[i].arg));

      // A fake signal was sent
      return 1;
    }
  }

  // No fake signal was sent, so proceed with update
  return 0;
}

void
focus(Client *c)
{
  if (!c || !ISVISIBLE(c)) {
    c = selmon->stack;
    while (c) {
      if (selmon->sticky == c && selmon->sel != c && !ISFULLSCREEN(c))
        c = c->snext;
      else if (ISVISIBLE(c))
        break;
      else
        c = c->snext;
    }
  }
  if (!c && selmon->sticky)
    c = selmon->sticky;
  if (selmon->sel && selmon->sel != c)
    unfocus(selmon->sel, 0);
  if (c) {
    if (c->mon != selmon)
      selmon = c->mon;
    if (c->isurgent)
      seturgent(c, 0);
    detachstack(c);
    attachstack(c);
    grabbuttons(c, 1);
    XSetWindowBorder(dpy, c->win, scheme[SchemeSel][ColBorder].pixel);
    setfocus(c);
  } else {
    XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
    XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
  }

  selmon->sel = c;
  drawbars();
}

/* there are some broken focus acquiring clients needing extra handling */
void
focusin(XEvent *e)
{
  XFocusChangeEvent *ev = &e->xfocus;

  if (selmon->sel && ev->window != selmon->sel->win && wintoclient(ev->window))
    setfocus(selmon->sel);
}

void
focusmon(const Arg *arg)
{
  Monitor *m;

  if (!mons->next)
    return;
  if ((m = dirtomon(arg->i)) == selmon)
    return;
  unfocus(selmon->sel, 0);
  selmon = m;
  focus(NULL);
  if (selmon->sel)
    XWarpPointer(dpy, None, selmon->sel->win, 0, 0, 0, 0, selmon->sel->w/2, selmon->sel->h/2);
  else
    XWarpPointer(dpy, None, root, 0, 0, 0, 0, selmon->wx + selmon->ww / 2, selmon->wy + selmon->wh / 2);
}

void
focusstack(const Arg *arg)
{
  int i = stackpos(arg);
  Client *c, *p;
  XEvent xev;

  if (!selmon->sel)
    return;

  for (p = NULL, c = selmon->clients; c && (i || !ISVISIBLE(c));
      i -= ISVISIBLE(c) ? 1 : 0, p = c, c = c->next);
  c = c ? c : p;
  if(ISFULLSCREEN(selmon->sel) && !c->scratchkey)
    return;
  focus(c);
  restack(selmon);
  while (XCheckMaskEvent(dpy, EnterWindowMask, &xev));
}

Atom
getatomprop(Client *c, Atom prop)
{
  int di;
  unsigned long dl;
  unsigned char *p = NULL;
  Atom da, atom = None;
  /* FIXME getatomprop should return the number of items and a pointer to
   * the stored data instead of this workaround */
  Atom req = XA_ATOM;
  if (prop == xatom[XembedInfo])
    req = xatom[XembedInfo];

  if (XGetWindowProperty(dpy, c->win, prop, 0L, sizeof atom, False, req,
    &da, &di, &dl, &dl, &p) == Success && p) {
    atom = *(Atom *)p;
    if (da == xatom[XembedInfo] && dl == 2)
      atom = ((Atom *)p)[1];
    XFree(p);
  }
  return atom;
}

int
getrootptr(int *x, int *y)
{
  int di;
  unsigned int dui;
  Window dummy;

  return XQueryPointer(dpy, root, &dummy, &dummy, x, y, &di, &di, &dui);
}

long
getstate(Window w)
{
  int format;
  long result = -1;
  unsigned char *p = NULL;
  unsigned long n, extra;
  Atom real;

  if (XGetWindowProperty(dpy, w, wmatom[WMState], 0L, 2L, False, wmatom[WMState],
    &real, &format, &n, &extra, (unsigned char **)&p) != Success)
    return -1;
  if (n != 0)
    result = *p;
  XFree(p);
  return result;
}

unsigned int
getsystraywidth()
{
  unsigned int w = 0;
  Client *i;
  if(showsystray)
    for(i = systray->icons; i; w += i->w + systrayspacing, i = i->next) ;
  return w ? w + systrayspacing : 1;
}

int
gettextprop(Window w, Atom atom, char *text, unsigned int size)
{
  char **list = NULL;
  int n;
  XTextProperty name;

  if (!text || size == 0)
    return 0;
  text[0] = '\0';
  if (!XGetTextProperty(dpy, w, &name, atom) || !name.nitems)
    return 0;
  if (name.encoding == XA_STRING)
    strncpy(text, (char *)name.value, size - 1);
  else {
    if (XmbTextPropertyToTextList(dpy, &name, &list, &n) >= Success && n > 0 && *list) {
      strncpy(text, *list, size - 1);
      XFreeStringList(list);
    }
  }
  text[size - 1] = '\0';
  XFree(name.value);
  return 1;
}

void
grabbuttons(Client *c, int focused)
{
  updatenumlockmask();
  {
    unsigned int i, j;
    unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
    XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
    if (!focused)
      XGrabButton(dpy, AnyButton, AnyModifier, c->win, False,
        BUTTONMASK, GrabModeSync, GrabModeSync, None, None);
    for (i = 0; i < LENGTH(buttons); i++)
      if (buttons[i].click == ClkClientWin)
        for (j = 0; j < LENGTH(modifiers); j++)
          XGrabButton(dpy, buttons[i].button,
            buttons[i].mask | modifiers[j],
            c->win, False, BUTTONMASK,
            GrabModeAsync, GrabModeSync, None, None);
  }
}

void
grid(Monitor *m)
{
  unsigned int i, n, cx, cy, cw, ch, ah, aw, cols, rows;
  Client *c;

  for(n = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next))
    n++;

  /* grid dimensions */
  for(cols= 0; cols<= n/2; cols++)
    if(cols*cols>= n)
      break;
  rows = (cols && (cols-1) * cols>= n) ? cols-1 : cols;

  /* window geoms (cell height/width) */
  ch = m->wh / (rows ? rows : 1);
  cw = m->ww / (cols ? cols : 1);
  for(i = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next)) {
    cx = m->wx + (i / rows) * cw;
    cy = m->wy + (i % rows) * ch;
    /* fill height of last client*/
    ah = (i+1 == n) ? ((rows*cols)-(i+1)) * ch : 0;
    aw = (i >= rows * (cols - 1)) ? m->ww - cw * cols : 0;
    resize(c, cx, cy, cw - 2 * c->bw + aw, ch - 2 * c->bw + ah, False);
    i++;
  }
}

void
grabkeys(void)
{
  updatenumlockmask();
  {
    unsigned int i, j, k;
    unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
    int start, end, skip;
    KeySym *syms;

    XUngrabKey(dpy, AnyKey, AnyModifier, root);
    XDisplayKeycodes(dpy, &start, &end);
    syms = XGetKeyboardMapping(dpy, start, end - start + 1, &skip);
    if (!syms)
      return;
    for (k = start; k <= end; k++)
      for (i = 0; i < LENGTH(keys); i++)
        /* skip modifier codes, we do that ourselves */
        if (keys[i].keysym == syms[(k - start) * skip])
          for (j = 0; j < LENGTH(modifiers); j++)
            XGrabKey(dpy, k,
               keys[i].mod | modifiers[j],
               root, True,
               GrabModeAsync, GrabModeAsync);
    XFree(syms);
  }
}

void
resetnmaster(const Arg *arg)
{
  selmon->nmaster = selmon->pertag->nmasters[selmon->pertag->curtag] = 1;
  arrange(selmon);
}

void
incnmaster(const Arg *arg)
{
  selmon->nmaster = selmon->pertag->nmasters[selmon->pertag->curtag] = MAX(selmon->nmaster + arg->i, 0);
  arrange(selmon);
}

#ifdef XINERAMA
static int
isuniquegeom(XineramaScreenInfo *unique, size_t n, XineramaScreenInfo *info)
{
  while (n--)
    if (unique[n].x_org == info->x_org && unique[n].y_org == info->y_org
    && unique[n].width == info->width && unique[n].height == info->height)
      return 0;
  return 1;
}
#endif /* XINERAMA */

void
keypress(XEvent *e)
{
  unsigned int i;
  KeySym keysym;
  XKeyEvent *ev;

  ev = &e->xkey;
  keysym = XKeycodeToKeysym(dpy, (KeyCode)ev->keycode, 0);
  for (i = 0; i < LENGTH(keys); i++)
    if (keysym == keys[i].keysym
    && CLEANMASK(keys[i].mod) == CLEANMASK(ev->state)
    && keys[i].func)
      keys[i].func(&(keys[i].arg));
}

void
killclient(const Arg *arg)
{
  if(!selmon->sel || (selmon->sel->scratchkey && !selmon->sel->swallowing))
    return;
  if (!sendevent(selmon->sel->win, wmatom[WMDelete], NoEventMask, wmatom[WMDelete], CurrentTime, 0, 0, 0)) {
    XGrabServer(dpy);
    XSetErrorHandler(xerrordummy);
    XSetCloseDownMode(dpy, DestroyAll);
    XKillClient(dpy, selmon->sel->win);
    XSync(dpy, False);
    XSetErrorHandler(xerror);
    XUngrabServer(dpy);
  }
}

void
manage(Window w, XWindowAttributes *wa)
{
  Client *c, *t = NULL, *term = NULL;
  Window trans = None;
  XWindowChanges wc;
  XEvent xev;

  c = ecalloc(1, sizeof(Client));
  c->win = w;
  c->pid = winpid(w);
  updatetitle(c);
  if (XGetTransientForHint(dpy, w, &trans) && (t = wintoclient(trans))) {
    c->mon = t->mon;
    c->tags = t->tags;
    c->alwaysontop = 1;
  } else {
    c->mon = selmon;
    applyrules(c);
    term = termforwin(c);
  }
  setclientgeo(c, wa);
  wc.border_width = c->bw;
  XConfigureWindow(dpy, w, CWBorderWidth, &wc);
  XSetWindowBorder(dpy, w, scheme[SchemeNorm][ColBorder].pixel);
  configure(c); /* propagates border_width, if size doesn't change */
  if (getatomprop(c, netatom[NetWMState]) == netatom[NetWMStateAbove] ||
    (getatomprop(c, netatom[NetWMWindowType]) == netatom[NetWMWindowTypeSplash]) ||
    (getatomprop(c, netatom[NetWMWindowType]) == netatom[NetWMWindowTypeToolbar]) ||
    (getatomprop(c, netatom[NetWMWindowType]) == netatom[NetWMWindowTypeDialog]) ||
    (getatomprop(c, netatom[NetWMWindowType]) == netatom[NetWMWindowTypeUtility]))
    c->alwaysontop = 1;
  updatesizehints(c);
  updatewmhints(c);
  XSelectInput(dpy, w, EnterWindowMask|FocusChangeMask|PropertyChangeMask|StructureNotifyMask);
  grabbuttons(c, 0);
  if (!c->isfloating)
    c->isfloating = c->oldstate = trans != None || c->isfixed;
  attach(c);
  attachstack(c);
  XChangeProperty(dpy, root, netatom[NetClientList], XA_WINDOW, 32, PropModeAppend,
    (unsigned char *) &(c->win), 1);
  XChangeProperty(dpy, root, netatom[NetClientListStacking], XA_WINDOW, 32, PropModePrepend,
    (unsigned char *) &(c->win), 1);
  XMoveResizeWindow(dpy, c->win, c->x + 2 * sw, c->y, c->w, c->h); /* some windows require this */
  setclientstate(c, NormalState);
  if (c->mon == selmon)
    unfocusmon(selmon);
  if (c->mon->pertag->fullscreens[c->mon->pertag->curtag] && !c->alwaysontop)
    focus(c->mon->pertag->fullscreens[c->mon->pertag->curtag]);
  if (c->scratchkey)
    focus(c);
  arrange(c->mon);
  XMapWindow(dpy, c->win);
  if (term)
    swallow(term, c);
  focus(NULL);
  setdesktopforclient(c, c->mon->pertag->curtag);
  while (XCheckMaskEvent(dpy, EnterWindowMask, &xev));
}

void
mappingnotify(XEvent *e)
{
  XMappingEvent *ev = &e->xmapping;

  XRefreshKeyboardMapping(ev);
  if (ev->request == MappingKeyboard)
    grabkeys();
}

void
maprequest(XEvent *e)
{
  static XWindowAttributes wa;
  XMapRequestEvent *ev = &e->xmaprequest;
  Client *i;

  if ((i = wintosystrayicon(ev->window))) {
    sendevent(i->win, netatom[Xembed], StructureNotifyMask, CurrentTime, XEMBED_WINDOW_ACTIVATE, 0, systray->win, XEMBED_EMBEDDED_VERSION);
    resizebarwin(selmon);
    updatesystray();
  }

  if (!XGetWindowAttributes(dpy, ev->window, &wa))
    return;
  if (wa.override_redirect)
    return;
  if (!wintoclient(ev->window))
    manage(ev->window, &wa);
}

void
monocle(Monitor *m)
{
  unsigned int n = 0;
  Client *c;

  for (c = m->clients; c; c = c->next)
    if (ISVISIBLE(c))
      n++;
  if (n > 0) /* override layout symbol */
    snprintf(m->ltsymbol, sizeof m->ltsymbol, "[%d]", n);
  for (c = nexttiled(m->clients); c; c = nexttiled(c->next))
    resize(c, m->wx, m->wy, m->ww - 2 * c->bw, m->wh - 2 * c->bw, 0);
}

void
motionnotify(XEvent *e)
{
  static Monitor *mon = NULL;
  Monitor *m;
  XMotionEvent *ev = &e->xmotion;

  if (ev->window != root)
    return;
  if ((m = recttomon(ev->x_root, ev->y_root, 1, 1)) != mon && mon) {
    unfocus(selmon->sel, 1);
    selmon = m;
    focus(NULL);
  }
  mon = m;
}

void
movemouse(const Arg *arg)
{
  int x, y, ocx, ocy, nx, ny;
  Client *c;
  Monitor *m;
  XEvent ev;
  Time lasttime = 0;

  if (!(c = selmon->sel))
    return;
  if (ISFULLSCREEN(c)) /* no support moving fullscreen windows by mouse */
    return;
  restack(selmon);
  ocx = c->x;
  ocy = c->y;
  if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
    None, cursor[CurMove]->cursor, CurrentTime) != GrabSuccess)
    return;
  if (!getrootptr(&x, &y))
    return;
  do {
    XMaskEvent(dpy, MOUSEMASK|ExposureMask|SubstructureRedirectMask, &ev);
    switch(ev.type) {
    case ConfigureRequest:
    case Expose:
    case MapRequest:
      handler[ev.type](&ev);
      break;
    case MotionNotify:
      if ((ev.xmotion.time - lasttime) <= (1000 / 60))
        continue;
      lasttime = ev.xmotion.time;

      nx = ocx + (ev.xmotion.x - x);
      ny = ocy + (ev.xmotion.y - y);
      if (abs(selmon->wx - nx) < snap)
        nx = selmon->wx;
      else if (abs((selmon->wx + selmon->ww) - (nx + WIDTH(c))) < snap)
        nx = selmon->wx + selmon->ww - WIDTH(c);
      if (abs(selmon->wy - ny) < snap)
        ny = selmon->wy;
      else if (abs((selmon->wy + selmon->wh) - (ny + HEIGHT(c))) < snap)
        ny = selmon->wy + selmon->wh - HEIGHT(c);
      if (!c->isfloating && selmon->lt[selmon->sellt]->arrange
      && (abs(nx - c->x) > snap || abs(ny - c->y) > snap))
        togglefloating(NULL);
      if (!selmon->lt[selmon->sellt]->arrange || c->isfloating)
        resize(c, nx, ny, c->w, c->h, 1);
      break;
    }
  } while (ev.type != ButtonRelease);
  XUngrabPointer(dpy, CurrentTime);
  if ((m = recttomon(c->x, c->y, c->w, c->h)) != selmon) {
    sendmon(c, m);
    selmon = m;
    focus(NULL);
  }
}

Client *
nexttiled(Client *c)
{
  for (; c && (c->isfloating || !ISVISIBLE(c)); c = c->next);
  return c;
}

int
parentiseditor(pid_t w)
{
#ifdef __linux__
  FILE *f;
  char buf[256];
  char comm[256];
  int c = getparentprocess(w);
  if (!c)
    return 0;
  snprintf(buf, sizeof(buf) - 1, "/proc/%u/comm", (unsigned)c);
  if (!(f = fopen(buf, "r")))
    return 0;
  fscanf(f, "%s", comm);
  fclose(f);
  if (strstr(comm, getenv("EDITOR")))
    return 1;
#endif /* __linux__*/
  return 0;
}

void
propertynotify(XEvent *e)
{
  Client *c;
  Window trans;
  XPropertyEvent *ev = &e->xproperty;

  if ((c = wintosystrayicon(ev->window))) {
    if (ev->atom == XA_WM_NORMAL_HINTS) {
      updatesizehints(c);
      updatesystrayicongeom(c, c->w, c->h);
    }
    else
      updatesystrayiconstate(c, ev);
    resizebarwin(selmon);
    updatesystray();
  }

  if ((ev->window == root) && (ev->atom == XA_WM_NAME)) {
    if (!fakesignal())
      updatestatus();
  }
  else if (ev->state == PropertyDelete)
    return; /* ignore */
  else if ((c = wintoclient(ev->window))) {
    switch(ev->atom) {
    default: break;
    case XA_WM_TRANSIENT_FOR:
      if (!c->isfloating && (XGetTransientForHint(dpy, c->win, &trans)) &&
        (c->isfloating = (wintoclient(trans)) != NULL))
        arrange(c->mon);
      break;
    case XA_WM_NORMAL_HINTS:
      c->hintsvalid = 0;
      break;
    case XA_WM_HINTS:
      updatewmhints(c);
      drawbars();
      break;
    }
    if (ev->atom == XA_WM_NAME || ev->atom == netatom[NetWMName]) {
      updatetitle(c);
      if (c == c->mon->sel)
        drawbar(c->mon);
    }
    if (ev->atom == netatom[NetWMWindowType]) {
      if ((getatomprop(c, netatom[NetWMWindowType]) == netatom[NetWMWindowTypeSplash]) ||
      (getatomprop(c, netatom[NetWMWindowType]) == netatom[NetWMWindowTypeToolbar]) ||
      (getatomprop(c, netatom[NetWMWindowType]) == netatom[NetWMWindowTypeDialog]) ||
      (getatomprop(c, netatom[NetWMWindowType]) == netatom[NetWMWindowTypeUtility]))
        c->alwaysontop = 1;
      if (getatomprop(c, netatom[NetWMState]) == netatom[NetWMFullscreen])
        setfullscreen(c, 1, 1);
    }
  }
}

void
pushstack(const Arg *arg)
{
  int i = stackpos(arg);
  Client *sel = selmon->sel, *c, *p;

  if(i < 0 || !sel)
    return;
  else if(i == 0) {
    detach(sel);
    attachtop(sel);
  }
  else {
    for(p = NULL, c = selmon->clients; c; p = c, c = c->next)
      if(!(i -= (ISVISIBLE(c) && c != sel)))
        break;
    c = c ? c : p;
    if (!c || (!c->next && !sel->next))
      return;
    detach(sel);
    sel->next = c->next;
    c->next = sel;
  }
  arrange(selmon);
}

void
quit(const Arg *arg)
{
  FILE *fd = NULL;
  struct stat filestat;

  if ((fd = fopen(lockfile, "r")) && stat(lockfile, &filestat) == 0) {
    fclose(fd);

    if (filestat.st_ctime <= time(NULL)-2)
      remove(lockfile);
  }

  if ((fd = fopen(lockfile, "r")) != NULL) {
    fclose(fd);
    remove(lockfile);
    running = 0;
  } else {
    if ((fd = fopen(lockfile, "a")) != NULL)
      fclose(fd);
  }
}

void
raiseclient(Client *c)
{
  configureclientpos(c, c->mon->barwin, Above);
}

Monitor *
recttomon(int x, int y, int w, int h)
{
  Monitor *m, *r = selmon;
  int a, area = 0;

  for (m = mons; m; m = m->next)
    if ((a = INTERSECT(x, y, w, h, m)) > area) {
      area = a;
      r = m;
    }
  return r;
}

void
removesystrayicon(Client *i)
{
  Client **ii;

  if (!showsystray || !i)
    return;
  for (ii = &systray->icons; *ii && *ii != i; ii = &(*ii)->next);
  if (ii)
    *ii = i->next;
  free(i);
}

void
resize(Client *c, int x, int y, int w, int h, int interact)
{
  if (applysizehints(c, &x, &y, &w, &h, interact))
    resizeclient(c, x, y, w, h);
}

void
resizebarwin(Monitor *m)
{
  unsigned int w = m->ww;
  if (showsystray && m == systraytomon(m) && !systrayonleft)
    w -= getsystraywidth();
  XMoveResizeWindow(dpy, m->barwin, m->wx, m->by, w, bh);
}

void
resizeclient(Client *c, int x, int y, int w, int h)
{
  XWindowChanges wc;
  unsigned int n;
  unsigned int gapoffset;
  unsigned int gapincr;
  Client *nbc;

  wc.border_width = c->bw;

  /* Get number of clients for the client's monitor */
  for (n = 0, nbc = nexttiled(c->mon->clients); nbc; nbc = nexttiled(nbc->next), n++);

  /* Do nothing if layout is floating */
  if (c->isfloating || c->mon->lt[c->mon->sellt]->arrange == NULL) {
    gapincr = gapoffset = 0;
  } else {
    /* Remove border and gap if layout is monocle or only one client */
    if (c->mon->lt[c->mon->sellt]->arrange == monocle || n == 1) {
      gapoffset = 0;
      gapincr = -2 * borderpx;
      wc.border_width = 0;
    } else {
      gapoffset = gappx;
      gapincr = 2 * gappx;
    }
  }

  /*
  x == 0 , left touching
  y - bary == 0 top touching
  x + w == screen width, right touching
  y + h == screen height, bot touching
  */

  c->oldx = c->x; c->x = wc.x = x;
  c->oldw = c->w; c->w = wc.width = w - gapincr;
  c->oldh = c->h; c->h = wc.height = h - gapincr;

  if (selmon->topbar) {
    c->oldy = c->y; c->y = wc.y = y + gapoffset;
  } else {
    c->oldy = c->y; c->y = wc.y = y;
  }

  if (x + w + borderpx * 2 == selmon->ww && wc.border_width != 0) {
    c->oldw = c->w; c->w = wc.width = w;
  }

  if (selmon->topbar && (y + h + borderpx * 2 >= selmon->wh) && wc.border_width != 0) {
    c->oldh = c->h; c->h = wc.height = h - gapoffset;
  }

  if ((c->isfloating && !ISFULLSCREEN(c)) || c->mon->lt[c->mon->sellt]->arrange == NULL) {
    c->sfx = c->x;
    c->sfy = c->y;
    c->sfw = c->w;
    c->sfh = c->h;
  }

  XConfigureWindow(dpy, c->win, CWX|CWY|CWWidth|CWHeight|CWBorderWidth, &wc);
  configure(c);
  XSync(dpy, False);
}

void
resizemouse(const Arg *arg)
{
  int ocx, ocy, nw, nh;
  int ocx2, ocy2, nx, ny;
  Client *c;
  Monitor *m;
  XEvent ev;
  int horizcorner, vertcorner;
  int di;
  unsigned int dui;
  Window dummy;
  Time lasttime = 0;

  if (!(c = selmon->sel))
    return;
  if (ISFULLSCREEN(c)) /* no support resizing fullscreen windows by mouse */
    return;
  restack(selmon);
  ocx = c->x;
  ocy = c->y;
  ocx2 = c->x + c->w;
  ocy2 = c->y + c->h;
  if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
    None, cursor[CurResize]->cursor, CurrentTime) != GrabSuccess)
    return;
  if (!XQueryPointer (dpy, c->win, &dummy, &dummy, &di, &di, &nx, &ny, &dui))
    return;
  horizcorner = nx < c->w / 2;
  vertcorner  = ny < c->h / 2;
  XWarpPointer (dpy, None, c->win, 0, 0, 0, 0,
      horizcorner ? (-c->bw) : (c->w + c->bw -1),
      vertcorner  ? (-c->bw) : (c->h + c->bw -1));
  do {
    XMaskEvent(dpy, MOUSEMASK|ExposureMask|SubstructureRedirectMask, &ev);
    switch(ev.type) {
    case ConfigureRequest:
    case Expose:
    case MapRequest:
      handler[ev.type](&ev);
      break;
    case MotionNotify:
      if ((ev.xmotion.time - lasttime) <= (1000 / 60))
        continue;
      lasttime = ev.xmotion.time;

      nw = MAX(ev.xmotion.x - ocx - 2 * c->bw + 1, 1);
      nh = MAX(ev.xmotion.y - ocy - 2 * c->bw + 1, 1);
      nx = horizcorner ? ev.xmotion.x : c->x;
      ny = vertcorner ? ev.xmotion.y : c->y;
      nw = MAX(horizcorner ? (ocx2 - nx) : (ev.xmotion.x - ocx - 2 * c->bw + 1), 1);
      nh = MAX(vertcorner ? (ocy2 - ny) : (ev.xmotion.y - ocy - 2 * c->bw + 1), 1);

      if (c->mon->wx + nw >= selmon->wx && c->mon->wx + nw <= selmon->wx + selmon->ww
      && c->mon->wy + nh >= selmon->wy && c->mon->wy + nh <= selmon->wy + selmon->wh)
      {
        if (!c->isfloating && selmon->lt[selmon->sellt]->arrange
        && (abs(nw - c->w) > snap || abs(nh - c->h) > snap))
          togglefloating(NULL);
      }
      if (!selmon->lt[selmon->sellt]->arrange || c->isfloating)
        resize(c, nx, ny, nw, nh, 1);
      break;
    }
  } while (ev.type != ButtonRelease);
  XWarpPointer(dpy, None, c->win, 0, 0, 0, 0,
          horizcorner ? (-c->bw) : (c->w + c->bw - 1),
          vertcorner ? (-c->bw) : (c->h + c->bw - 1));
  XUngrabPointer(dpy, CurrentTime);
  while (XCheckMaskEvent(dpy, EnterWindowMask, &ev));
  if ((m = recttomon(c->x, c->y, c->w, c->h)) != selmon) {
    sendmon(c, m);
    selmon = m;
    focus(NULL);
  }
}

void
resizerequest(XEvent *e)
{
  XResizeRequestEvent *ev = &e->xresizerequest;
  Client *i;

  if ((i = wintosystrayicon(ev->window))) {
    updatesystrayicongeom(i, ev->width, ev->height);
    resizebarwin(selmon);
    updatesystray();
  }
}

void
resetfact(const Arg *arg)
{
  Client *c;
  selmon->mfact = selmon->pertag->mfacts[selmon->pertag->curtag] = mfact;
  for (c = nexttiled(selmon->clients); c; c = nexttiled(c->next))
    c->cfact = 1.0;
  arrange(selmon);
}

void
restack(Monitor *m)
{
  XEvent ev;

  drawbar(m);
  if (!m->sel)
    return;
  configuremonlayout(m);
  XSync(dpy, False);
  while (XCheckMaskEvent(dpy, EnterWindowMask, &ev));
}

void
run(void)
{
  XEvent ev;
  /* main event loop */
  XSync(dpy, False);
  while (running && !XNextEvent(dpy, &ev))
    if (handler[ev.type])
      handler[ev.type](&ev); /* call handler */
}

void
runautostart(void)
{
  char *pathpfx;
  char *path;
  char *xdgdatahome;
  char *home;
  struct stat sb;

  if ((home = getenv("HOME")) == NULL)
    /* this is almost impossible */
    return;

  /* if $XDG_DATA_HOME is set and not empty, use $XDG_DATA_HOME/dwm,
   * otherwise use ~/.local/share/dwm as autostart script directory
   */
  xdgdatahome = getenv("XDG_DATA_HOME");
  if (xdgdatahome != NULL && *xdgdatahome != '\0') {
    /* space for path segments, separators and nul */
    pathpfx = ecalloc(1, strlen(xdgdatahome) + strlen(dwmdir) + 2);

    if (sprintf(pathpfx, "%s/%s", xdgdatahome, dwmdir) <= 0) {
      free(pathpfx);
      return;
    }
  } else {
    /* space for path segments, separators and nul */
    pathpfx = ecalloc(1, strlen(home) + strlen(localshare)
                         + strlen(dwmdir) + 3);

    if (sprintf(pathpfx, "%s/%s/%s", home, localshare, dwmdir) < 0) {
      free(pathpfx);
      return;
    }
  }

  /* check if the autostart script directory exists */
  if (! (stat(pathpfx, &sb) == 0 && S_ISDIR(sb.st_mode))) {
    /* the XDG conformant path does not exist or is no directory
     * so we try ~/.dwm instead
     */
    char *pathpfx_new = realloc(pathpfx, strlen(home) + strlen(dwmdir) + 3);
    if(pathpfx_new == NULL) {
      free(pathpfx);
      return;
    }
    pathpfx = pathpfx_new;

    if (sprintf(pathpfx, "%s/.%s", home, dwmdir) <= 0) {
      free(pathpfx);
      return;
    }
  }

  /* try the blocking script first */
  path = ecalloc(1, strlen(pathpfx) + strlen(autostartblocksh) + 2);
  if (sprintf(path, "%s/%s", pathpfx, autostartblocksh) <= 0) {
    free(path);
    free(pathpfx);
  }

  if (access(path, X_OK) == 0)
    system(path);

  /* now the non-blocking script */
  if (sprintf(path, "%s/%s", pathpfx, autostartsh) <= 0) {
    free(path);
    free(pathpfx);
  }

  if (access(path, X_OK) == 0)
    system(strcat(path, " &"));

  free(pathpfx);
  free(path);
}

void
scan(void)
{
  unsigned int i, num;
  Window d1, d2, *wins = NULL;
  XWindowAttributes wa;

  if (XQueryTree(dpy, root, &d1, &d2, &wins, &num)) {
    for (i = 0; i < num; i++) {
      if (!XGetWindowAttributes(dpy, wins[i], &wa)
      || wa.override_redirect || XGetTransientForHint(dpy, wins[i], &d1))
        continue;
      if (wa.map_state == IsViewable || getstate(wins[i]) == IconicState)
        manage(wins[i], &wa);
    }
    for (i = 0; i < num; i++) { /* now the transients */
      if (!XGetWindowAttributes(dpy, wins[i], &wa))
        continue;
      if (XGetTransientForHint(dpy, wins[i], &d1)
      && (wa.map_state == IsViewable || getstate(wins[i]) == IconicState))
        manage(wins[i], &wa);
    }
    if (wins)
      XFree(wins);
  }
}

void
sendmon(Client *c, Monitor *m)
{
  int fs = 0, i;
  if (c->mon == m)
    return;
  unfocus(c, 1);
  if (ISFULLSCREEN(c)) {
    setfullscreen(c, 0, 0);
    fs = 1;
  }
  detach(c);
  detachstack(c);
  c->mon = m;
  c->tags = m->tagset[m->seltags]; /* assign tags of target monitor */
  for (i = 0; !(c->tags & 1 << i); i++);
  setdesktopforclient(c, i+1);
  if (selmon->sticky)
    selmon->sticky = NULL;
  attach(c);
  attachstack(c);
  if (m->pertag->fullscreens[m->pertag->curtag] && !c->alwaysontop) {
    detachstack(m->pertag->fullscreens[m->pertag->curtag]);
    attachstack(m->pertag->fullscreens[m->pertag->curtag]);
  }
  if (fs)
    setfullscreen(c, 1, 0);
  focus(NULL);
  arrange(NULL);
}

void
setcfact(const Arg *arg)
{
  float f;
  Client *c;

  c = selmon->sel;

  if (!arg || !c || !selmon->lt[selmon->sellt]->arrange)
    return;
  if (!arg->f)
    f = 1.0;
  else if (arg->f > 4.0) // set fact absolutely
    f = arg->f - 4.0;
  else
    f = arg->f + c->cfact;
  if (f < 0.25)
    f = 0.25;
  else if (f > 4.0)
    f = 4.0;
  c->cfact = f;
  arrange(selmon);
}

void
setclientstate(Client *c, long state)
{
  long data[] = { state, None };

  XChangeProperty(dpy, c->win, wmatom[WMState], wmatom[WMState], 32,
    PropModeReplace, (unsigned char *)data, 2);
}

void
setcurrentdesktop(void)
{
  long data[] = { 0 };
  XChangeProperty(dpy, root, netatom[NetCurrentDesktop], XA_CARDINAL, 32, PropModeReplace, (unsigned char *)data, 1);
}

void
setdesktopforclient(Client *c, int tag)
{
  long data[] = { tag };
  XChangeProperty(dpy, c->win, netatom[NetCurrentDesktop], XA_CARDINAL, 32, PropModeReplace, (unsigned char *)data, 1);
}

void
setdesktopnames(void)
{
  XTextProperty text;
  Xutf8TextListToTextProperty(dpy, (char**)tags, TAGSLENGTH, XUTF8StringStyle, &text);
  XSetTextProperty(dpy, root, &text, netatom[NetDesktopNames]);
}

int
sendevent(Window w, Atom proto, int mask, long d0, long d1, long d2, long d3, long d4)
{
  int n;
  Atom *protocols, mt;
  int exists = 0;
  XEvent ev;

  if (proto == wmatom[WMTakeFocus] || proto == wmatom[WMDelete]) {
    mt = wmatom[WMProtocols];
    if (XGetWMProtocols(dpy, w, &protocols, &n)) {
      while (!exists && n--)
        exists = protocols[n] == proto;
      XFree(protocols);
    }
  } else {
    exists = True;
    mt = proto;
  }
  if (exists) {
    ev.type = ClientMessage;
    ev.xclient.window = w;
    ev.xclient.message_type = mt;
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = d0;
    ev.xclient.data.l[1] = d1;
    ev.xclient.data.l[2] = d2;
    ev.xclient.data.l[3] = d3;
    ev.xclient.data.l[4] = d4;
    XSendEvent(dpy, w, False, mask, &ev);
  }
  return exists;
}

void
setnumdesktops(void)
{
  long data[] = { TAGSLENGTH };
  XChangeProperty(dpy, root, netatom[NetNumberOfDesktops], XA_CARDINAL, 32, PropModeReplace, (unsigned char *)data, 1);
}


void
sethidden(Client *c, int hidden)
{
  if (hidden) {
    c->tags = 0;
    setclientstate(c, WithdrawnState);
    focus(NULL);
  } else {
    c->tags = selmon->tagset[selmon->seltags];
    setdesktopforclient(c, c->mon->pertag->curtag);
    setclientstate(c, NormalState);
    focus(c);
  }
}

void
setfocus(Client *c)
{
  if (!c->neverfocus) {
    XSetInputFocus(dpy, c->win, RevertToPointerRoot, CurrentTime);
    XChangeProperty(dpy, root, netatom[NetActiveWindow],
      XA_WINDOW, 32, PropModeReplace,
      (unsigned char *) &(c->win), 1);
  }
  if (c->ignoremoverequest)
    setclientstate(c, NormalState);
  sendevent(c->win, wmatom[WMTakeFocus], NoEventMask, wmatom[WMTakeFocus], CurrentTime, 0, 0, 0);
}

void
setclientgeo(Client *c, XWindowAttributes *wa)
{
  int n;
  Client *b;
  c->bw = borderpx;
  if (!c->scratchkey) {
    c->w = c->oldw = wa->width;
    c->h = c->oldh = wa->height;
    c->oldbw = wa->border_width;
    if (c->isfloating && c->mon->lt[c->mon->sellt]->arrange) {
      c->x = selmon->mx + (selmon->mw / 2 - WIDTH(c) / 2);
      c->y = selmon->my + (selmon->mh / 2 - HEIGHT(c) / 2);
    }
    if (!c->mon->lt[c->mon->sellt]->arrange) {
      for(n = 0, b = nexttiled(selmon->clients); b; b = nexttiled(b->next))
        n++;
      switch (n) {
        //top left
        case 0:
          c->x = c->oldx = 0;
          c->y = c->oldy = 0;
          break;
        //top right
        case 1:
          c->x = c->oldx = selmon->mw - c->w;
          c->y = c->oldy = 0;
          break;
        //bot left
        case 2:
          c->x = c->oldx = 0;
          c->y = c->oldy = selmon->mh - c->h;
          break;
        //bot right
        case 3:
          c->x = c->oldx = selmon->mw - c->w;
          c->y = c->oldy = selmon->mh - c->h;
          break;
        //center
        default:
          c->x = selmon->mx + (selmon->mw / 2 - WIDTH(c) / 2);
          c->y = selmon->my + (selmon->mh / 2 - HEIGHT(c) / 2);
          break;
      }
    }
    else {
      c->x = c->oldx = wa->x;
      c->y = c->oldy = wa->y;
    }
  } else {
    c->w = scw * 10 + 2 * c->bw + gappx;
    c->h = sch * 22 + 2 * c->bw + gappx;
    c->x = selmon->mx + (selmon->mw / 2 - WIDTH(c) / 2);
    c->y = selmon->my + (selmon->mh / 2 - HEIGHT(c) / 2);
  }
  c->cfact = 1.0;
  if (c->x + WIDTH(c) > c->mon->wx + c->mon->ww)
    c->x = c->mon->wx + c->mon->ww - WIDTH(c);
  if (c->y + HEIGHT(c) > c->mon->wy + c->mon->wh)
    c->y = c->mon->wy + c->mon->wh - HEIGHT(c);
  c->x = MAX(c->x, c->mon->wx);
  c->y = MAX(c->y, c->mon->wy); 
  c->sfx = c->x;
  c->sfy = c->y;
  c->sfw = c->w;
  c->sfh = c->h;
}

void
setfullscreenontag(Client *c, int fullscreen, int tag, int f)
{
  if (fullscreen && !ISFULLSCREEN(c)) {
    if(c->mon->pertag->fullscreens[tag])
      setfullscreen(c->mon->pertag->fullscreens[tag], 0, f);
    XChangeProperty(dpy, c->win, netatom[NetWMState], XA_ATOM, 32,
      PropModeReplace, (unsigned char*)&netatom[NetWMFullscreen], 1);
    c->mon->pertag->fullscreens[tag] = c;
    c->oldstate = c->isfloating;
    c->oldbw = c->bw;
    c->bw = 0;
    c->isfloating = 1;
    c->fstag = tag;
    resizeclient(c, c->mon->mx, c->mon->my, c->mon->mw, c->mon->mh);
    raiseclient(c);
    if (f)
      focus(c);
    arrange(c->mon);
  } else if (!fullscreen && ISFULLSCREEN(c)) {
    XChangeProperty(dpy, c->win, netatom[NetWMState], XA_ATOM, 32,
      PropModeReplace, (unsigned char*)0, 0);
    c->mon->pertag->fullscreens[tag] = NULL;
    c->isfloating = c->oldstate;
    c->bw = c->oldbw;
    c->x = c->oldx;
    c->y = c->oldy;
    c->w = c->oldw;
    c->h = c->oldh;
    c->fstag = -1;
    resizeclient(c, c->x, c->y, c->w, c->h);
    if (f)
      focus(NULL);
    arrange(c->mon);
  }
}

void
setfullscreen(Client *c, int fullscreen, int f)
{
  if (!c || !c->mon || !c->mon->pertag || !c->mon->pertag->curtag)
    return;

  int tag = c->mon->pertag->curtag;

  if ((c->scratchkey && !fullscreen))
    tag = c->fstag;

  setfullscreenontag(c, fullscreen, tag, f);
}

void
setlayout(const Arg *arg)
{
  const Layout *oldlayout = selmon->lt[selmon->sellt];
  if (!arg || !arg->v || arg->v != selmon->lt[selmon->sellt])
    selmon->sellt = selmon->pertag->sellts[selmon->pertag->curtag] ^= 1;
  if (arg && arg->v)
    selmon->lt[selmon->sellt] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt] = (Layout *)arg->v;
  //if null layout restore dims
  if (selmon->clients && selmon->lt[selmon->sellt]->arrange == NULL) {
    //loop through all non scratchpad clients and resize
    for (Client *c = selmon->clients; c != NULL; c = c->next)
      if (!c->scratchkey && !ISFULLSCREEN(c) && ISVISIBLE(c))
        resizeclient(c, c->sfx, c->sfy, c->sfw, c->sfh);
  }
  if (oldlayout && oldlayout->arrange == NULL) {
    for (Client *c = selmon->clients; c != NULL; c = c->next) {
      if (!c->scratchkey && !ISFULLSCREEN(c) && ISVISIBLE(c)) {
        c->sfx = c->x;
        c->sfy = c->y;
        c->sfw = c->w;
        c->sfh = c->h;
      }
    }
  }
  strncpy(selmon->ltsymbol, selmon->lt[selmon->sellt]->symbol, sizeof selmon->ltsymbol);
  if (selmon->sel)
    arrange(selmon);
  else
    drawbar(selmon);
}

/* arg > 1.0 will set mfact absolutely */
void
setmfact(const Arg *arg)
{
  float f;

  if (!arg || !selmon->lt[selmon->sellt]->arrange)
    return;
  f = arg->f < 1.0 ? arg->f + selmon->mfact : arg->f - 1.0;
  if (f < 0.05 || f > 0.95)
    return;
  selmon->mfact = selmon->pertag->mfacts[selmon->pertag->curtag] = f;
  arrange(selmon);
}

void
setup(void)
{
  int i;
  XSetWindowAttributes wa;
  Atom utf8string;
  struct sigaction sa;

  /* do not transform children into zombies when they terminate */
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_NOCLDSTOP | SA_NOCLDWAIT | SA_RESTART;
  sa.sa_handler = SIG_IGN;
  sigaction(SIGCHLD, &sa, NULL);

  /* clean up any zombies (inherited from .xinitrc etc) immediately */
  while (waitpid(-1, NULL, WNOHANG) > 0);

  /* init screen */
  screen = DefaultScreen(dpy);
  sw = DisplayWidth(dpy, screen);
  sh = DisplayHeight(dpy, screen);
  root = RootWindow(dpy, screen);
  drw = drw_create(dpy, screen, root, sw, sh);
  if (!drw_fontset_create(drw, fonts, LENGTH(fonts)))
    die("no fonts could be loaded.");
  lrpad = drw->fonts->h;
  bh = drw->fonts->h + 2;
  updategeom();
  /* init atoms */
  utf8string = XInternAtom(dpy, "UTF8_STRING", False);
  wmatom[WMProtocols] = XInternAtom(dpy, "WM_PROTOCOLS", False);
  wmatom[WMDelete] = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
  wmatom[WMState] = XInternAtom(dpy, "WM_STATE", False);
  wmatom[WMTakeFocus] = XInternAtom(dpy, "WM_TAKE_FOCUS", False);
  netatom[NetActiveWindow] = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
  netatom[NetSupported] = XInternAtom(dpy, "_NET_SUPPORTED", False);
  netatom[NetSystemTray] = XInternAtom(dpy, "_NET_SYSTEM_TRAY_S0", False);
  netatom[NetSystemTrayOP] = XInternAtom(dpy, "_NET_SYSTEM_TRAY_OPCODE", False);
  netatom[NetSystemTrayOrientation] = XInternAtom(dpy, "_NET_SYSTEM_TRAY_ORIENTATION", False);
  netatom[NetSystemTrayOrientationHorz] = XInternAtom(dpy, "_NET_SYSTEM_TRAY_ORIENTATION_HORZ", False);
  netatom[NetWMName] = XInternAtom(dpy, "_NET_WM_NAME", False);
  netatom[NetWMState] = XInternAtom(dpy, "_NET_WM_STATE", False);
  netatom[NetWMStateAbove] = XInternAtom(dpy, "_NET_WM_STATE_ABOVE", False);
  netatom[NetWMCheck] = XInternAtom(dpy, "_NET_SUPPORTING_WM_CHECK", False);
  netatom[NetWMFullscreen] = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
  netatom[NetWMWindowType] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
  netatom[NetWMWindowTypeDialog] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DIALOG", False);
  netatom[NetWMWindowTypeUtility] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_UTILITY", False);
  netatom[NetWMWindowTypeToolbar] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_TOOLBAR", False);
  netatom[NetWMWindowTypeSplash] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_SPLASH", False);
  netatom[NetClientList] = XInternAtom(dpy, "_NET_CLIENT_LIST", False);
  netatom[NetClientListStacking] = XInternAtom(dpy, "_NET_CLIENT_LIST_STACKING", False);
  netatom[NetDesktopViewport] = XInternAtom(dpy, "_NET_DESKTOP_VIEWPORT", False);
  netatom[NetNumberOfDesktops] = XInternAtom(dpy, "_NET_NUMBER_OF_DESKTOPS", False);
  netatom[NetCurrentDesktop] = XInternAtom(dpy, "_NET_CURRENT_DESKTOP", False);
  netatom[NetDesktopNames] = XInternAtom(dpy, "_NET_DESKTOP_NAMES", False);
  xatom[Manager] = XInternAtom(dpy, "MANAGER", False);
  xatom[Xembed] = XInternAtom(dpy, "_XEMBED", False);
  xatom[XembedInfo] = XInternAtom(dpy, "_XEMBED_INFO", False);
  /* init cursors */
  cursor[CurNormal] = drw_cur_create(drw, XC_left_ptr);
  cursor[CurResize] = drw_cur_create(drw, XC_sizing);
  cursor[CurMove] = drw_cur_create(drw, XC_fleur);
  /* init appearance */
  scheme = ecalloc(LENGTH(colors), sizeof(Clr *));
  for (i = 0; i < LENGTH(colors); i++)
    scheme[i] = drw_scm_create(drw, colors[i], 3);
  /* set scracpad dims from config */
  char *token = strtok(scratchdim, "x");
  if (token) {
    scw = atoi(token);
    token = strtok(NULL, "x");
  }
  if (token)
    sch = atoi(token);
  setenv("ISSWAL", "1", 1);
  /* init system tray */
  updatesystray();
  /* init bars */
  updatebars();
  updatestatus();
  /* supporting window for NetWMCheck */
  wmcheckwin = XCreateSimpleWindow(dpy, root, 0, 0, 1, 1, 0, 0, 0);
  XChangeProperty(dpy, wmcheckwin, netatom[NetWMCheck], XA_WINDOW, 32,
    PropModeReplace, (unsigned char *) &wmcheckwin, 1);
  XChangeProperty(dpy, wmcheckwin, netatom[NetWMName], utf8string, 8,
    PropModeReplace, (unsigned char *) "dwm", 3);
  XChangeProperty(dpy, root, netatom[NetWMCheck], XA_WINDOW, 32,
    PropModeReplace, (unsigned char *) &wmcheckwin, 1);
  /* EWMH support per view */
  XChangeProperty(dpy, root, netatom[NetSupported], XA_ATOM, 32,
    PropModeReplace, (unsigned char *) netatom, NetLast);
  XDeleteProperty(dpy, root, netatom[NetClientList]);
  XDeleteProperty(dpy, root, netatom[NetClientListStacking]);
  setnumdesktops();
  setcurrentdesktop();
  setdesktopnames();
  setviewport();
  /* select events */
  wa.cursor = cursor[CurNormal]->cursor;
  wa.event_mask = SubstructureRedirectMask|SubstructureNotifyMask
    |ButtonPressMask|PointerMotionMask|EnterWindowMask
    |LeaveWindowMask|StructureNotifyMask|PropertyChangeMask;
  XChangeWindowAttributes(dpy, root, CWEventMask|CWCursor, &wa);
  XSelectInput(dpy, root, wa.event_mask);
  grabkeys();
  focus(NULL);
}

void
setviewport(void)
{
  int nmons = 0;
  for (Monitor *m = mons; m; m = m->next) {
    nmons++;
  }

  long data[nmons * 2];

  Monitor *m = mons;
  for (int i = 0; i < nmons * 2; i+=2) {
    data[i] = (long)m->mx;
    data[i+1] = (long)m->my;
    m = m->next;
  }

  XChangeProperty(dpy, root, netatom[NetDesktopViewport], XA_CARDINAL, 32,
      PropModeReplace, (unsigned char *)data, nmons * 2);
}

void
seturgent(Client *c, int urg)
{
  XWMHints *wmh;

  c->isurgent = urg;
  if (!(wmh = XGetWMHints(dpy, c->win)))
    return;
  wmh->flags = urg ? (wmh->flags | XUrgencyHint) : (wmh->flags & ~XUrgencyHint);
  XSetWMHints(dpy, c->win, wmh);
  XFree(wmh);
}

void
showhide(Client *c)
{
  if (!c)
    return;
  if (ISVISIBLE(c)) {
    /* show clients top down */
    if (c->win)
      XMoveWindow(dpy, c->win, c->x, c->y);
    if (c->needresize) {
      c->needresize = 0;
      XMoveResizeWindow(dpy, c->win, c->x, c->y, c->w, c->h);
    } else {
      if (c->win)
        XMoveWindow(dpy, c->win, c->x, c->y);
    }
    if ((!c->mon->lt[c->mon->sellt]->arrange || c->isfloating) && !ISFULLSCREEN(c))
      resize(c, c->x, c->y, c->w, c->h, 0);
    showhide(c->snext);
  } else {
    /* hide clients bottom up */
    if (c && c->snext != c)
      showhide(c->snext);
    else
      return;
    if (c->win)
      XMoveWindow(dpy, c->win, WIDTH(c) * -2, c->y);
  }
}

void
spawn(const Arg *arg)
{
  struct sigaction sa;

  if (arg->v == dmenucmd)
    dmenumon[0] = '0' + selmon->num;
  if (fork() == 0) {
    if (dpy)
      close(ConnectionNumber(dpy));
    setsid();

    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = SIG_DFL;
    sigaction(SIGCHLD, &sa, NULL);

    execvp(((char **)arg->v)[0], (char **)arg->v);
    fprintf(stderr, "dwm: execvp %s", ((char **)arg->v)[0]);
    perror(" failed");
    exit(EXIT_SUCCESS);
  }
}

int
stackpos(const Arg *arg) {
  int n, i, k, lf, rf;
  float f;
  Client *c, *l;

  if (!selmon->clients)
    return -1;

  if (arg->i == PREVSEL) {
    for (l = selmon->stack; l && (!ISVISIBLE(l) || l == selmon->sel); l = l->snext);
    if (!l)
      return -1;
    for (i = 0, c = selmon->clients; c != l; i += ISVISIBLE(c) ? 1 : 0, c = c->next);
    return i;
  }
  else if (arg->i == LEFTSEL) {
    for (i = 0, c = selmon->clients; c != selmon->sel; i += ISVISIBLE(c) ? 1 : 0, c = c->next);
    for (n = 0, c = selmon->clients; c; n += ISVISIBLE(c) ? 1 : 0, c = c->next);
    if (selmon->lt[selmon->sellt]->arrange == dwindle) {
      k = MOD(i+1, 2) + 1;
    } else if (selmon->lt[selmon->sellt]->arrange == grid) {
      k = round(sqrt(n));
    } else if (selmon->lt[selmon->sellt]->arrange == tile) {
      f = (float)(n-selmon->nmaster) / selmon->nmaster;
      lf = ceil((i+1-selmon->nmaster) / f) - 1;
      k = i - lf;
    }
    else {
      k = 0;
    }
    return i-k >= 0 ? i-k : i;
  }
  else if (arg->i == RIGHTSEL) {
    for (i = 0, c = selmon->clients; c != selmon->sel; i += ISVISIBLE(c) ? 1 : 0, c = c->next);
    for (n = 0, c = selmon->clients; c; n += ISVISIBLE(c) ? 1 : 0, c = c->next);
    if (selmon->lt[selmon->sellt]->arrange == dwindle) {
      k = !MOD(i, 2) ? 2 : 0;
    } else if (selmon->lt[selmon->sellt]->arrange == grid) {
      int c = round(sqrt(n));
      k = c+i > n-1 ? 1 : c;
    } else if (selmon->lt[selmon->sellt]->arrange == tile) {
      f = (float)(n-selmon->nmaster) / selmon->nmaster;
      rf = ((selmon->nmaster-1) + floor(i*f)) + 1 ;
      k = rf - i;
    }
    else {
      k = 0;
    }
    return i+k <= n ? i+k : i;
  }
  else if (ISINC(arg->i)) {
    if (!selmon->sel)
      return -1;
    for (i = 0, c = selmon->clients; c != selmon->sel; i += ISVISIBLE(c) ? 1 : 0, c = c->next);
    for (n = 0, c = selmon->clients; c; n += ISVISIBLE(c) ? 1 : 0, c = c->next);
    return MIN(MAX(i + GETINC(arg->i), 0), n-1);
  }
  else if (arg->i < 0) {
    for (n = 0, c = selmon->clients; c; n += ISVISIBLE(c) ? 1 : 0, c = c->next);
    return MAX(n + arg->i, 0);
  }
  else
    return arg->i;
}

void
spawnscratch(const Arg *arg)
{
  if (fork() == 0) {
    if (dpy)
      close(ConnectionNumber(dpy));
    setsid();
    execvp(((char **)arg->v)[1], ((char **)arg->v)+1);
    fprintf(stderr, "dwm: execvp %s", ((char **)arg->v)[1]);
    perror(" failed");
    exit(EXIT_SUCCESS);
  }
}

void
tag(const Arg *arg)
{
  int i, fs;
  if ((arg->ui & TAGMASK) == selmon->tagset[selmon->seltags])
    return;
  if (selmon->sel && arg->ui & TAGMASK) {
    Client *c = selmon->sel;
    fs = ISFULLSCREEN(c);
    if (fs) setfullscreen(c, 0, 0);
    c->tags = arg->ui & TAGMASK;
    for (i = 0; !(arg->ui & 1 << i); i++);
    setdesktopforclient(c, i+1);
    if (selmon->sticky != c) {
      detach(c);
      if (selmon->pertag->attachdir[arg->ui & TAGMASK] > 1)
        attachtop(c);
      else
        attachbottom(c);
      if (fs) setfullscreenontag(c, 1, i+1, 0);
      focus(NULL);
      arrange(selmon);
    }
  }
}

void
tagmon(const Arg *arg)
{
  if (!selmon->sel || !mons->next || selmon->sel->scratchkey)
    return;
  sendmon(selmon->sel, dirtomon(arg->i));
}

void
tile(Monitor *m)
{
  unsigned int i, n, h, mw, my, ty;
  float mfacts = 0, sfacts = 0;
  Client *c;

  for (n = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), n++) {
    if (n < m->nmaster)
      mfacts += c->cfact;
    else
      sfacts += c->cfact;
  }
  if (n == 0)
    return;

  if (n > m->nmaster)
    mw = m->nmaster ? m->ww * m->mfact : 0;
  else
    mw = m->ww;
  for (i = my = ty = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), i++)
  if (i < m->nmaster) {
    h = (m->wh - my) * (c->cfact / mfacts);
    resize(c, m->wx, m->wy + my, mw - (2*c->bw), h - (2*c->bw), 0);
    if (my + HEIGHT(c) < m->wh)
      my += HEIGHT(c);
    mfacts -= c->cfact;
  } else {
    h = (m->wh - ty) * (c->cfact / sfacts);
    resize(c, m->wx + mw, m->wy + ty, m->ww - mw - (2*c->bw), h - (2*c->bw), 0);
    if (ty + HEIGHT(c) < m->wh)
      ty += HEIGHT(c);
    sfacts -= c->cfact;
  }
}

void
fibonacci(Monitor *m, int s)
{
  unsigned int i, n;
  int nx, ny, nw, nh;
  int nv, hrest = 0, wrest = 0, r = 1;
  const float MAX_SCALE = 1.75;
  const float MIN_CFACT = 0.5;
  float scale = 1.0;
  Client *c, *next, *n1 = NULL, *n2 = NULL, *j;

  for (n = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), n++);
  if (n == 0)
    return;

  nx = m->wx;
  ny = m->wy;
  nw = m->ww;
  nh = m->wh;

  for (i = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next)) {
    if (r) {
      if ((i % 2 && nh / 2 <= (bh + 2*c->bw))
         || (!(i % 2) && nw / 2 <= (bh + 2*c->bw))) {
        r = 0;
      }
      if (r && i < n - 1) {
        if (i % 2) {
          next = nexttiled(c->next);
          if (((n - 1) % 2) && (i + 2) == (n - 1)) {
            n1 = next;
            n2 = nexttiled(n1->next);
            for (j = m->stack; j; j = j->snext) {
              if (n1 == j) {
                n2->cfact = n1->cfact;
                break;
              }
              if (n2 == j) {
                next = j;
                n1->cfact = n2->cfact;
                break;
              }
            }
          }
          scale = MIN(MAX_SCALE, c->cfact / next->cfact);
          if (scale == 1.0)
            c->cfact = next->cfact = 1.0;
          if (scale == MAX_SCALE && c->cfact > 1.0) {
            c->cfact = MAX_SCALE;
            next->cfact = 1.0;
          }
          if (scale == MAX_SCALE && next->cfact < 1.0) {
            c->cfact = 1.0;
            next->cfact = MIN_CFACT;
          }
          if (n1 && n2) {
            if (n2 == next)
              n1->cfact = n2->cfact;
            else
              n2->cfact = n1->cfact;
          }
          nv = nh / 2;
          nv *= scale;
          hrest = nh - 2*nv;
          nh = nv;
        } else {
          nv = nw / 2;
          wrest = nw - 2*nv;
          nw = nv;
        }

        if ((i % 4) == 2 && !s)
          nx += nw;
        else if ((i % 4) == 3 && !s)
          ny += nh;
      }

      if ((i % 4) == 0) {
        if (s) {
          ny += nh;
          nh += hrest;
        }
        else {
          nh -= hrest;
          ny -= nh;
        }
      }
      else if ((i % 4) == 1) {
        nx += nw;
        nw += wrest;
      }
      else if ((i % 4) == 2) {
        ny += nh;
        nh += hrest;
        if (i < n - 1)
          nw += wrest;
      }
      else if ((i % 4) == 3) {
        if (s) {
          nx += nw;
          nw -= wrest;
        } else {
          nw -= wrest;
          nx -= nw;
          nh += hrest;
        }
      }
      if (i == 0) {
        if (n != 1) {
          nw = m->ww - m->ww * (1 - m->mfact);
          wrest = 0;
        }
        ny = m->wy;
      }
      else if (i == 1)
        nw = m->ww - nw;
      i++;
    }
    resize(c, nx, ny, nw - (2*c->bw), nh - (2*c->bw), False);
  }
}

int
gcd(int a, int b)
{
 return (b == 0 ? a : gcd(b, MOD(a, b)));
}

void
grabfocus(Client *c)
{
  int i;
  for(i=0; i < LENGTH(tags) && !((1 << i) & c->tags); i++);
  if(i < LENGTH(tags)) {
    const Arg a = {.ui = 1 << i};
    if (c->mon->sticky != c) {
      selmon = c->mon;
      view(&a);
    }
    Client *fs = c->mon->pertag->fullscreens[c->mon->pertag->curtag];
    if (fs && fs != c)
      setfullscreen(fs, 0, 0);
    if (c->isfloating || !c->mon->lt[c->mon->sellt]->arrange
      || c->mon->lt[c->mon->sellt]->arrange == deck
      || c->mon->lt[c->mon->sellt]->arrange == monocle) {
      detachstack(c);
      attachstack(c);
      restack(c->mon);
    }
    focus(c);
  }
}

void
dwindle(Monitor *mon) {
  fibonacci(mon, 1);
}


void
toggleswal(const Arg *arg)
{
  char s[10];
  swal = !swal;
  sprintf(s,"%d", swal);
  setenv("ISSWAL", s, 1);
}

void
togglebar(const Arg *arg)
{
  selmon->showbar = selmon->pertag->showbars[selmon->pertag->curtag] = !selmon->showbar;
  updatebarpos(selmon);
  resizebarwin(selmon);
  if (showsystray) {
    XWindowChanges wc;
    if (!selmon->showbar)
      wc.y = -bh;
    else if (selmon->showbar) {
      wc.y = 0;
      if (!selmon->topbar)
        wc.y = selmon->mh - bh;
    }
    XConfigureWindow(dpy, systray->win, CWY, &wc);
  }
  arrange(selmon);
}

void
togglefloating(const Arg *arg)
{
  if (!selmon->sel || selmon->sel->scratchkey || ISFULLSCREEN(selmon->sel))
    return;
  selmon->sel->isfloating = !selmon->sel->isfloating || selmon->sel->isfixed;
  if (selmon->sel->isfloating)
    /* restore last known float dimensions */
    resize(selmon->sel, selmon->sel->sfx, selmon->sel->sfy,
           selmon->sel->sfw, selmon->sel->sfh, False);
  arrange(selmon);
}

void
togglefullscr(const Arg *arg)
{
  if(selmon->sel)
    setfullscreen(selmon->sel, !ISFULLSCREEN(selmon->sel), 1);
}

void
togglescratch(const Arg *arg)
{
  Client *k, *c = NULL;
  Monitor *m;
  unsigned int found = 0;
  int vis;

  for (m = mons; m; m = m->next) {
    for (c = m->clients; c; c = c->next) {
      found = c->scratchkey == ((char**)arg->v)[0][0];
      if (found)
        break;
    }
    if (found)
      break;
  }

  if (found) {
    vis = ISVISIBLE(c);
    setfullscreen(c, 0, 0);
    if (m == selmon) {
      if (!vis) {
        sethidden(c, 0);
      } else {
        sethidden(c, 1);
      }
    } else {
      sendmon(c, selmon);
      focus(c);
      if (!vis)
        sethidden(c, 0);
    }
    for (k = selmon->clients; k; k = k->next) {
      if (c != k && k->scratchkey && ISVISIBLE(k)) {
        setfullscreen(k, 0, 0);
        sethidden(k, 1);
      }
    }
    setclientgeo(c, NULL);
  } else {
    spawnscratch(arg);
    for (k = selmon->clients; k; k = k->next) {
      if (k->scratchkey && ISVISIBLE(k)) {
        setfullscreen(k, 0, 0);
        sethidden(k, 1);
      }
    }
  }
  arrange(selmon);
}

void
togglesticky(const Arg *arg)
{
  if (!selmon->sel || selmon->sel->scratchkey)
    return;
  setfullscreen(selmon->sel, 0, 0);
  if (selmon->sticky)
    selmon->sticky = NULL;
  else if(!selmon->sticky)
    selmon->sticky = selmon->sel;
  focus(NULL);
  arrange(selmon);
}

void
unfocusmon(Monitor *m)
{
  if (!m)
    return;
  for (Client *c = m->stack; c ; c = c->snext) {
    unfocus(c, 0);
  }
}

void
unfocus(Client *c, int setfocus)
{
  if (!c)
    return;
  grabbuttons(c, 0);
  XSetWindowBorder(dpy, c->win, scheme[SchemeNorm][ColBorder].pixel);
  if (setfocus) {
    XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
    XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
  }
}

void
unmanage(Client *c, int destroyed)
{
  Monitor *m = c->mon;
  XWindowChanges wc;
  int vis = 0;

  if (c->swallowing) {
    unswallow(c);
    return;
  }

  Client *s = swallowingclient(c->win);
  if (s) {
    free(s->swallowing);
    s->swallowing = NULL;
    arrange(m);
    focus(NULL);
    return;
  }

  detach(c);
  detachstack(c);
  if (!destroyed) {
    wc.border_width = c->oldbw;
    XGrabServer(dpy); /* avoid race conditions */
    XSetErrorHandler(xerrordummy);
    XSelectInput(dpy, c->win, NoEventMask);
    XConfigureWindow(dpy, c->win, CWBorderWidth, &wc); /* restore border */
    XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
    setclientstate(c, WithdrawnState);
    XSync(dpy, False);
    XSetErrorHandler(xerror);
    XUngrabServer(dpy);
  }
  if (ISFULLSCREEN(c))
    setfullscreen(c, 0, 0);
  if (m->sticky == c)
    m->sticky = NULL;
  free(c);
  for (c = m->stack; c; c = c->snext) {
    if (ISVISIBLE(c)) {
      vis = 1;
      break;
    }
  }
  if (m->pertag->curtag == 0 && !vis) {
    Arg a = {.ui = m->pertag->prevtag};
    view(&a);
  }
  if (!s) {
    focus(NULL);
    updateclientlist();
    arrange(m);
  }
}

void
unmapnotify(XEvent *e)
{
  Client *c;
  XUnmapEvent *ev = &e->xunmap;

  if ((c = wintoclient(ev->window))) {
    if (ev->send_event)
      setclientstate(c, WithdrawnState);
    else
      unmanage(c, 0);
  }
  if ((c = wintosystrayicon(ev->window))) {
    /* KLUDGE! sometimes icons occasionally unmap their windows, but do
     * _not_ destroy them. We map those windows back */
    XMapRaised(dpy, c->win);
    updatesystray();
  }
}

void
updatebars(void)
{
  unsigned int w;
  Monitor *m;
  XSetWindowAttributes wa = {
    .override_redirect = True,
    .background_pixmap = ParentRelative,
    .event_mask = ButtonPressMask|ExposureMask
  };
  XClassHint ch = {"dwm", "dwm"};
  for (m = mons; m; m = m->next) {
    if (m->barwin)
      continue;
    w = m->ww;
    if (showsystray && m == systraytomon(m))
      w -= getsystraywidth();
    m->barwin = XCreateWindow(dpy, root, m->wx, m->by, w, bh, 0, DefaultDepth(dpy, screen),
        CopyFromParent, DefaultVisual(dpy, screen),
        CWOverrideRedirect|CWBackPixmap|CWEventMask, &wa);
    XDefineCursor(dpy, m->barwin, cursor[CurNormal]->cursor);
    if (showsystray && m == systraytomon(m))
      XMapRaised(dpy, systray->win);
    XMapRaised(dpy, m->barwin);
    XSetClassHint(dpy, m->barwin, &ch);
  }
}

void
updatebarpos(Monitor *m)
{
  m->wy = m->my;
  m->wh = m->mh;
  if (m->showbar) {
    m->wh -= bh;
    m->by = m->topbar ? m->wy : m->wy + m->wh;
    m->wy = m->topbar ? m->wy + bh : m->wy;
  } else
    m->by = -bh;
}

void
updateclientlist()
{
  Client *c;
  Monitor *m;

  XDeleteProperty(dpy, root, netatom[NetClientList]);
  for (m = mons; m; m = m->next)
    for (c = m->clients; c; c = c->next)
      XChangeProperty(dpy, root, netatom[NetClientList],
        XA_WINDOW, 32, PropModeAppend,
        (unsigned char *) &(c->win), 1);
}

void updatecurrentdesktop(void)
{
  long rawdata[] = { selmon->tagset[selmon->seltags] };
  int i=0;
  while(*rawdata >> (i+1)){
    i++;
  }
  long data[] = { i };
  XChangeProperty(dpy, root, netatom[NetCurrentDesktop], XA_CARDINAL, 32, PropModeReplace, (unsigned char *)data, 1);
}

int
updategeom(void)
{
  int dirty = 0;

#ifdef XINERAMA
  if (XineramaIsActive(dpy)) {
    int i, j, n, nn;
    Client *c;
    Monitor *m;
    XineramaScreenInfo *info = XineramaQueryScreens(dpy, &nn);
    XineramaScreenInfo *unique = NULL;

    for (n = 0, m = mons; m; m = m->next, n++);
    /* only consider unique geometries as separate screens */
    unique = ecalloc(nn, sizeof(XineramaScreenInfo));
    for (i = 0, j = 0; i < nn; i++)
      if (isuniquegeom(unique, j, &info[i]))
        memcpy(&unique[j++], &info[i], sizeof(XineramaScreenInfo));
    XFree(info);
    nn = j;
    /* new monitors if nn > n */
    for (i = n; i < nn; i++) {
      for (m = mons; m && m->next; m = m->next);
      if (m)
        m->next = createmon();
      else
        mons = createmon();
    }
    for (i = 0, m = mons; i < nn && m; m = m->next, i++)
      if (i >= n
      || unique[i].x_org != m->mx || unique[i].y_org != m->my
      || unique[i].width != m->mw || unique[i].height != m->mh)
      {
        dirty = 1;
        m->num = i;
        m->mx = m->wx = unique[i].x_org;
        m->my = m->wy = unique[i].y_org;
        m->mw = m->ww = unique[i].width;
        m->mh = m->wh = unique[i].height;
        updatebarpos(m);
      }
    /* removed monitors if n > nn */
    for (i = nn; i < n; i++) {
      for (m = mons; m && m->next; m = m->next);
      while ((c = m->clients)) {
        dirty = 1;
        m->clients = c->next;
        setfullscreen(c, 0, 1);
        detachstack(c);
        c->mon = mons;
        attach(c);
        attachstack(c);
      }
      if (m == selmon)
        selmon = mons;
      cleanupmon(m);
    }
    free(unique);
  } else
#endif /* XINERAMA */
  { /* default monitor setup */
    if (!mons)
      mons = createmon();
    if (mons->mw != sw || mons->mh != sh) {
      dirty = 1;
      mons->mw = mons->ww = sw;
      mons->mh = mons->wh = sh;
      updatebarpos(mons);
    }
  }
  if (dirty) {
    selmon = mons;
    selmon = wintomon(root);
  }
  return dirty;
}

void
updatenumlockmask(void)
{
  unsigned int i, j;
  XModifierKeymap *modmap;

  numlockmask = 0;
  modmap = XGetModifierMapping(dpy);
  for (i = 0; i < 8; i++)
    for (j = 0; j < modmap->max_keypermod; j++)
      if (modmap->modifiermap[i * modmap->max_keypermod + j]
        == XKeysymToKeycode(dpy, XK_Num_Lock))
        numlockmask = (1 << i);
  XFreeModifiermap(modmap);
}

void
updatesizehints(Client *c)
{
  long msize;
  XSizeHints size;

  if (!XGetWMNormalHints(dpy, c->win, &size, &msize))
    /* size is uninitialized, ensure that size.flags aren't used */
    size.flags = 0;
  if (size.flags & PBaseSize) {
    c->basew = size.base_width;
    c->baseh = size.base_height;
  } else if (size.flags & PMinSize) {
    c->basew = size.min_width;
    c->baseh = size.min_height;
  } else
    c->basew = c->baseh = 0;
  if (size.flags & PResizeInc) {
    c->incw = size.width_inc;
    c->inch = size.height_inc;
  } else
    c->incw = c->inch = 0;
  if (size.flags & PMaxSize) {
    c->maxw = size.max_width;
    c->maxh = size.max_height;
  } else
    c->maxw = c->maxh = 0;
  if (size.flags & PMinSize) {
    c->minw = size.min_width;
    c->minh = size.min_height;
  } else if (size.flags & PBaseSize) {
    c->minw = size.base_width;
    c->minh = size.base_height;
  } else
    c->minw = c->minh = 0;
  if (size.flags & PAspect) {
    c->mina = (float)size.min_aspect.y / size.min_aspect.x;
    c->maxa = (float)size.max_aspect.x / size.max_aspect.y;
  } else
    c->maxa = c->mina = 0.0;
  if(size.flags & PSize) {
    c->basew = size.base_width;
    c->baseh = size.base_height;
    //c->isfloating = True;
  }
  c->isfixed = (c->maxw && c->maxh && c->maxw == c->minw && c->maxh == c->minh);
  c->hintsvalid = 1;
}

void
updatestatus(void)
{
  if (!gettextprop(root, XA_WM_NAME, stext, sizeof(stext)))
    strcpy(stext, "dwm-"VERSION);
  drawbar(selmon);
  updatesystray();
}

void
updatesystrayicongeom(Client *i, int w, int h)
{
  if (i) {
    i->h = bh;
    if (w == h)
      i->w = bh;
    else if (h == bh)
      i->w = w;
    else
      i->w = (int) ((float)bh * ((float)w / (float)h));
    applysizehints(i, &(i->x), &(i->y), &(i->w), &(i->h), False);
    /* force icons into the systray dimensions if they don't want to */
    if (i->h > bh) {
      if (i->w == i->h)
        i->w = bh;
      else
        i->w = (int) ((float)bh * ((float)i->w / (float)i->h));
      i->h = bh;
    }
  }
}

void
updatesystrayiconstate(Client *i, XPropertyEvent *ev)
{
  long flags;
  int code = 0;

  if (!showsystray || !i || ev->atom != xatom[XembedInfo] ||
      !(flags = getatomprop(i, xatom[XembedInfo])))
    return;

  if (flags & XEMBED_MAPPED && !i->tags) {
    i->tags = 1;
    code = XEMBED_WINDOW_ACTIVATE;
    XMapRaised(dpy, i->win);
    setclientstate(i, NormalState);
  }
  else if (!(flags & XEMBED_MAPPED) && i->tags) {
    i->tags = 0;
    code = XEMBED_WINDOW_DEACTIVATE;
    XUnmapWindow(dpy, i->win);
    setclientstate(i, WithdrawnState);
  }
  else
    return;
  sendevent(i->win, xatom[Xembed], StructureNotifyMask, CurrentTime, code, 0,
      systray->win, XEMBED_EMBEDDED_VERSION);
}

void
updatesystray(void)
{
  XSetWindowAttributes wa;
  XWindowChanges wc;
  Client *i;
  Monitor *m = systraytomon(NULL);
  unsigned int x = m->mx + m->mw;
  unsigned int sw = TEXTW(stext) - lrpad + systrayspacing;
  unsigned int w = 1;

  if (!showsystray)
    return;
  if (systrayonleft)
    x -= sw + lrpad / 2;
  if (!systray) {
    /* init systray */
    if (!(systray = (Systray *)calloc(1, sizeof(Systray))))
      die("fatal: could not malloc() %u bytes\n", sizeof(Systray));
    systray->win = XCreateSimpleWindow(dpy, root, x, m->by, w, bh, 0, 0, scheme[SchemeSel][ColBg].pixel);
    wa.event_mask        = ButtonPressMask | ExposureMask;
    wa.override_redirect = True;
    wa.background_pixel  = scheme[SchemeNorm][ColBg].pixel;
    XSelectInput(dpy, systray->win, SubstructureNotifyMask);
    XChangeProperty(dpy, systray->win, netatom[NetSystemTrayOrientation], XA_CARDINAL, 32,
        PropModeReplace, (unsigned char *)&netatom[NetSystemTrayOrientationHorz], 1);
    XChangeWindowAttributes(dpy, systray->win, CWEventMask|CWOverrideRedirect|CWBackPixel, &wa);
    XMapRaised(dpy, systray->win);
    XSetSelectionOwner(dpy, netatom[NetSystemTray], systray->win, CurrentTime);
    if (XGetSelectionOwner(dpy, netatom[NetSystemTray]) == systray->win) {
      sendevent(root, xatom[Manager], StructureNotifyMask, CurrentTime, netatom[NetSystemTray], systray->win, 0, 0);
      XSync(dpy, False);
    }
    else {
      fprintf(stderr, "dwm: unable to obtain system tray.\n");
      free(systray);
      systray = NULL;
      return;
    }
  }
  for (w = 0, i = systray->icons; i; i = i->next) {
    /* make sure the background color stays the same */
    wa.background_pixel  = scheme[SchemeNorm][ColBg].pixel;
    XChangeWindowAttributes(dpy, i->win, CWBackPixel, &wa);
    XMapRaised(dpy, i->win);
    w += systrayspacing;
    i->x = w;
    XMoveResizeWindow(dpy, i->win, i->x, 0, i->w, i->h);
    w += i->w;
    if (i->mon != m)
      i->mon = m;
  }
  w = w ? w + systrayspacing : 1;
  x -= w;
  XMoveResizeWindow(dpy, systray->win, x, m->by, w, bh);
  wc.x = x; wc.y = m->by; wc.width = w; wc.height = bh;
  wc.stack_mode = Below; wc.sibling = m->barwin;
  XConfigureWindow(dpy, systray->win, CWX|CWY|CWWidth|CWHeight|CWSibling|CWStackMode, &wc);
  XMapWindow(dpy, systray->win);
  XMapSubwindows(dpy, systray->win);
  /* redraw background */
  XSetForeground(dpy, drw->gc, scheme[SchemeNorm][ColBg].pixel);
  XFillRectangle(dpy, systray->win, drw->gc, 0, 0, w, bh);
  XSync(dpy, False);
}

void
updatetitle(Client *c)
{
  if (!gettextprop(c->win, netatom[NetWMName], c->name, sizeof c->name))
    gettextprop(c->win, XA_WM_NAME, c->name, sizeof c->name);
  if (c->name[0] == '\0') /* hack to mark broken clients */
    strcpy(c->name, broken);
}

void
updatewmhints(Client *c)
{
  XWMHints *wmh;

  if ((wmh = XGetWMHints(dpy, c->win))) {
    if (c == selmon->sel && wmh->flags & XUrgencyHint) {
      wmh->flags &= ~XUrgencyHint;
      XSetWMHints(dpy, c->win, wmh);
    } else {
      c->isurgent = (wmh->flags & XUrgencyHint) ? 1 : 0;
      if (c->isurgent && c->grabonurgent) {
        grabfocus(c);
      }
    }
    if (wmh->flags & InputHint)
      c->neverfocus = !wmh->input;
    else
      c->neverfocus = 0;
    XFree(wmh);
  }
}

void
view(const Arg *arg)
{
  int i;
  unsigned int tmptag;

  if ((arg->ui & TAGMASK) == selmon->tagset[selmon->seltags])
    return;
  if (selmon->sticky)
      setfullscreen(selmon->sticky, 0, 0);
  selmon->seltags ^= 1; /* toggle sel tagset */
  if (arg->ui & TAGMASK) {
    selmon->tagset[selmon->seltags] = arg->ui & TAGMASK;
    selmon->pertag->prevtag = selmon->pertag->curtag;

    if (arg->ui == ~0) {
      selmon->pertag->curtag = 0;
      for (i = 0; i <= LENGTH(tags); i++)
        setfullscreenontag(selmon->pertag->fullscreens[i], 0, i, 0);
    } else {
      for (i = 0; !(arg->ui & 1 << i); i++) ;
      selmon->pertag->curtag = i + 1;
      Client *fs = selmon->pertag->fullscreens[i];
      if (fs)
        focus(fs);
    }
  } else {
    tmptag = selmon->pertag->prevtag;
    selmon->pertag->prevtag = selmon->pertag->curtag;
    selmon->pertag->curtag = tmptag;
  }

  selmon->nmaster = selmon->pertag->nmasters[selmon->pertag->curtag];
  selmon->mfact = selmon->pertag->mfacts[selmon->pertag->curtag];
  selmon->sellt = selmon->pertag->sellts[selmon->pertag->curtag];
  selmon->lt[selmon->sellt] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt];
  selmon->lt[selmon->sellt^1] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt^1];

  if (selmon->showbar != selmon->pertag->showbars[selmon->pertag->curtag])
    togglebar(NULL);

  if (selmon->pertag->prevtag == 0) {
    setfullscreenontag(selmon->pertag->fullscreens[0], 0, 0, 0);
    for (Client *k = selmon->clients; k; k = k->next) {
      if (ISVISIBLE(k)) {
        for (i = 0; !(arg->ui & 1 << i); i++);
        setdesktopforclient(k, i+1);
        k->tags = arg->ui & TAGMASK;
      }
    }
  }

  focus(NULL);
  arrange(selmon);
  updatecurrentdesktop();
}

pid_t
winpid(Window w)
{

  pid_t result = 0;

#ifdef __linux__
  xcb_res_client_id_spec_t spec = {0};
  spec.client = w;
  spec.mask = XCB_RES_CLIENT_ID_MASK_LOCAL_CLIENT_PID;

  xcb_generic_error_t *e = NULL;
  xcb_res_query_client_ids_cookie_t c = xcb_res_query_client_ids(xcon, 1, &spec);
  xcb_res_query_client_ids_reply_t *r = xcb_res_query_client_ids_reply(xcon, c, &e);

  if (!r)
    return (pid_t)0;

  xcb_res_client_id_value_iterator_t i = xcb_res_query_client_ids_ids_iterator(r);
  for (; i.rem; xcb_res_client_id_value_next(&i)) {
    spec = i.data->spec;
    if (spec.mask & XCB_RES_CLIENT_ID_MASK_LOCAL_CLIENT_PID) {
      uint32_t *t = xcb_res_client_id_value_value(i.data);
      result = *t;
      break;
    }
  }

  free(r);

  if (result == (pid_t)-1)
    result = 0;

#endif /* __linux__ */

#ifdef __OpenBSD__
        Atom type;
        int format;
        unsigned long len, bytes;
        unsigned char *prop;
        pid_t ret;

        if (XGetWindowProperty(dpy, w, XInternAtom(dpy, "_NET_WM_PID", 0), 0, 1, False, AnyPropertyType, &type, &format, &len, &bytes, &prop) != Success || !prop)
               return 0;

        ret = *(pid_t*)prop;
        XFree(prop);
        result = ret;

#endif /* __OpenBSD__ */
  return result;
}

pid_t
getparentprocess(pid_t p)
{
  unsigned int v = 0;

#ifdef __linux__
  FILE *f;
  char buf[256];
  snprintf(buf, sizeof(buf) - 1, "/proc/%u/stat", (unsigned)p);

  if (!(f = fopen(buf, "r")))
    return 0;

  fscanf(f, "%*u %*s %*c %u", &v);
  fclose(f);
#endif /* __linux__*/

#ifdef __OpenBSD__
  int n;
  kvm_t *kd;
  struct kinfo_proc *kp;

  kd = kvm_openfiles(NULL, NULL, NULL, KVM_NO_FILES, NULL);
  if (!kd)
    return 0;

  kp = kvm_getprocs(kd, KERN_PROC_PID, p, sizeof(*kp), &n);
  v = kp->p_ppid;
#endif /* __OpenBSD__ */

  return (pid_t)v;
}

int
isdescprocess(pid_t p, pid_t c)
{
  int d = 0;
  while (p != c && c != 0)  {
    c = getparentprocess(c);
    d++;
  }
  d = ((int)c) ? d : 0;
  return d;
}

Client *
termforwin(const Client *w)
{
  Client *c, *p = NULL;
  Monitor *m;
  int mindepth = 999;

  if (!w->pid || w->isterminal || parentiseditor(w->pid))
    return NULL;

  for (m = mons; m; m = m->next) {
    for (c = m->clients; c; c = c->next) {
      if (c->isterminal && !c->swallowing && c->pid) {
        int d = isdescprocess(c->pid, w->pid);
        if (d && mindepth > d) {
          mindepth = d;
          p = c;
        }
      }
    }
  }

  return p;
}

Client *
swallowingclient(Window w)
{
  Client *c;
  Monitor *m;

  for (m = mons; m; m = m->next) {
    for (c = m->clients; c; c = c->next) {
      if (c->swallowing && c->swallowing->win == w)
        return c;
    }
  }

  return NULL;
}

Client *
wintoclient(Window w)
{
  Client *c;
  Monitor *m;

  for (m = mons; m; m = m->next)
    for (c = m->clients; c; c = c->next)
      if (c->win == w)
        return c;
  return NULL;
}

Client *
wintosystrayicon(Window w)
{
  Client *i = NULL;

  if (!showsystray || !w)
    return i;
  for (i = systray->icons; i && i->win != w; i = i->next) ;
  return i;
}

Monitor *
wintomon(Window w)
{
  int x, y;
  Client *c;
  Monitor *m;

  if (w == root && getrootptr(&x, &y))
    return recttomon(x, y, 1, 1);
  for (m = mons; m; m = m->next)
    if (w == m->barwin)
      return m;
  if ((c = wintoclient(w)))
    return c->mon;
  return selmon;
}

/* There's no way to check accesses to destroyed windows, thus those cases are
 * ignored (especially on UnmapNotify's). Other types of errors call Xlibs
 * default error handler, which may call exit. */
int
xerror(Display *dpy, XErrorEvent *ee)
{
  if (ee->error_code == BadWindow
  || (ee->request_code == X_SetInputFocus && ee->error_code == BadMatch)
  || (ee->request_code == X_PolyText8 && ee->error_code == BadDrawable)
  || (ee->request_code == X_PolyFillRectangle && ee->error_code == BadDrawable)
  || (ee->request_code == X_PolySegment && ee->error_code == BadDrawable)
  || (ee->request_code == X_ConfigureWindow && ee->error_code == BadMatch)
  || (ee->request_code == X_GrabButton && ee->error_code == BadAccess)
  || (ee->request_code == X_GrabKey && ee->error_code == BadAccess)
  || (ee->request_code == X_CopyArea && ee->error_code == BadDrawable))
    return 0;
  fprintf(stderr, "dwm: fatal error: request code=%d, error code=%d\n",
    ee->request_code, ee->error_code);
  return xerrorxlib(dpy, ee); /* may call exit */
}

int
xerrordummy(Display *dpy, XErrorEvent *ee)
{
  return 0;
}

/* Startup Error handler to check if another window manager
 * is already running. */
int
xerrorstart(Display *dpy, XErrorEvent *ee)
{
  die("dwm: another window manager is already running");
  return -1;
}

Monitor *
systraytomon(Monitor *m) {
  Monitor *t;
  int i, n;
  if(!systraypinning) {
    if(!m)
      return selmon;
    return m == selmon ? m : NULL;
  }
  for(n = 1, t = mons; t && t->next; n++, t = t->next) ;
  for(i = 1, t = mons; t && t->next && i < systraypinning; i++, t = t->next) ;
  if(systraypinningfailfirst && n < systraypinning)
    return mons;
  return t;
}

void
resource_load(XrmDatabase db, char *name, enum resource_type rtype, void *dst)
{
  char *sdst = NULL;
  int *idst = NULL;
  float *fdst = NULL;

  sdst = dst;
  idst = dst;
  fdst = dst;

  char fullname[256];
  char *type;
  XrmValue ret;

  snprintf(fullname, sizeof(fullname), "%s.%s", "dwm", name);
  fullname[sizeof(fullname) - 1] = '\0';

  XrmGetResource(db, fullname, "*", &type, &ret);
  if (!(ret.addr == NULL || strncmp("String", type, 64)))
  {
    switch (rtype) {
    case STRING:
      strcpy(sdst, ret.addr);
      break;
    case INTEGER:
      *idst = strtoul(ret.addr, NULL, 10);
      break;
    case FLOAT:
      *fdst = strtof(ret.addr, NULL);
      break;
    }
  }
}

void
load_xresources(void)
{
  Display *display;
  char *resm;
  XrmDatabase db;
  ResourcePref *p;

  display = XOpenDisplay(NULL);
  resm = XResourceManagerString(display);
  if (!resm)
    return;

  db = XrmGetStringDatabase(resm);
  for (p = resources; p < resources + LENGTH(resources); p++)
    resource_load(db, p->name, p->type, p->dst);
  XCloseDisplay(display);
}

int
main(int argc, char *argv[])
{
#ifdef DEBUG
  dwmdebug();
#else
  runautostart();
#endif
  if (argc == 2 && !strcmp("-v", argv[1]))
    die("dwm-"VERSION);
  else if (argc != 1)
    die("usage: dwm [-v]");
  if (!setlocale(LC_CTYPE, "") || !XSupportsLocale())
    fputs("warning: no locale support\n", stderr);
  if (!(dpy = XOpenDisplay(NULL)))
    die("dwm: cannot open display");
  if (!(xcon = XGetXCBConnection(dpy)))
    die("dwm: cannot get xcb connection\n");
  checkotherwm();
  XrmInitialize();
  load_xresources();
  setup();
#ifdef __OpenBSD__
  if (pledge("stdio rpath proc exec ps", NULL) == -1)
    die("pledge");
#endif /* __OpenBSD__ */
  scan();
  run();
  cleanup();
  XCloseDisplay(dpy);
  return EXIT_SUCCESS;
}
