/* See LICENSE file for copyright and license details. */

/* media-key-mappings */
#define XF86AudioMute		0x1008ff12
#define XF86AudioLowerVolume	0x1008ff11
#define XF86AudioRaiseVolume	0x1008ff13
#define XF86TouchpadToggle	0x1008ffa9
#define XF86XK_MonBrightnessUp   0x1008FF02  /* Monitor/panel brightness */
#define XF86XK_MonBrightnessDown 0x1008FF03  /* Monitor/panel brightness */
#define XF86AudioMicMute 0x1008ffb2
#define XF86Display 0x1008ff59

/* appearance */
static unsigned int borderpx        = 3;   /* border pixel of windows */
static unsigned int gappx           = 4;   /* gaps between windows */
static unsigned int snap            = 32;  /* snap pixel */
static unsigned int systraypinning  = 0;   /* 0: sloppy systray follows selected monitor, >0: pin systray to monitor X */
static unsigned int systrayonleft   = 0;   /* 0: systray in the right corner, >0: systray on left of status text */
static unsigned int systrayspacing  = 2;   /* systray spacing */
static int systraypinningfailfirst  = 1;   /* 1: if pinning fails, display systray on the first monitor, False: display systray on the last monitor*/
static const int showsystray        = 1;   /* 0 means no systray */
static const int showbar            = 1;   /* 0 means no bar */
static int topbar                   = 1;   /* 0 means bottom bar */
static char font[]                  = "Hack Nerd Font Mono:size=9";
static char font2[]                 = "Noto Color Emoji:style=Regular:pixelsize=12:antialias=true:autohint=true";
static const char *fonts[]          = { font, font2 };
static char dmenufont[]             = "Hack Nerd Font Mono:size=9";
static char normbgcolor[]           = "#222222";
static char normbordercolor[]       = "#444444";
static char normfgcolor[]           = "#bbbbbb";
static char selfgcolor[]            = "#eeeeee";
static char selbordercolor[]        = "#005577";
static char selbgcolor[]            = "#005577";
static char urgborder[]             = "#ff0000";
static char *colors[][3]            = {
                                        /*               fg           bg           border   */
                                        [SchemeNorm] = { normfgcolor, normbgcolor, normbordercolor },
                                        [SchemeSel]  = { selfgcolor,  selbgcolor,  selbordercolor  },
                                        [SchemeUrg]  = { selfgcolor,  selbgcolor,  urgborder },
                                      };

/* sticky symbol */
static const XPoint stickyicon[] = { {0,0}, {4,0}, {4,8}, {2,6}, {0,8}, {0,0} }; /* represents the icon as an array of vertices */
static const XPoint stickyiconbb = {4,8};	/* defines the bottom right corner of the polygon's bounding box (speeds up scaling) */

/* tagging */
static const char *tags[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9" };

static const XPoint swaliconbb = { 2,16 }; /* defines the bottom right corner of the polygon's bounding box (speeds up scaling) */

static const Rule rules[] = {
  /* xprop(1):
   *	WM_CLASS(STRING) = instance, class
   *	WM_NAME(STRING) = title
   */
  /* class      instance    title               tags-mask    isfloating     ispermanent  monitor  ignoreReqest scratch-key  grabfocus-on-urgent no-swallow   is-term*/
  {  "st",      NULL,       NULL,               0,           0,             0,           0,       0,            0,          0,                  0,           1},
  {  NULL,      NULL,       "scratchpad",       0,           1,             1,           0,       0,           'S',         0,                  0,           1},
  {  NULL,      NULL,       "floatterm",        0,           1,             1,           0,       0,           'T',         0,                  0,           1},
  {  NULL,      NULL,       "st-vimmode",       0,           1,             1,           0,       0,           'T',         0,                  0,           0},
  { "net-runelite-client-RuneLite", NULL, NULL, 1 << 0,      0,             0,           0,       1,            0,          1,                  0,           0},
  {  NULL, NULL, "Event Tester",                0,           0,             0,           0,       0,            0,          0,                  1,           0},
};

/* layout(s) */
static float mfact     = 0.55; /* factor of master area size [0.05..0.95] */
static int nmaster     = 1;    /* number of clients in master area */
static int resizehints = 0;    /* 1 means respect size hints in tiled resizals */

static const int defaultatchdir = 0;
                            /*    set the default attach dir
                                  0, new goes below focused
                                  1, new goes bottommost
                                  2, new goes above focused
                                  3, new goes above master
                            */
static const char *stack_symbols[] = { "*∨", "∨", "*∧", "∧" };

static const Layout layouts[] = {
  /* symbol     arrange function */
  { "[\\]",     dwindle }, /* first entry is default */
  { "[]=",      tile },
  { "=[]",      lefttile },
  { "[D]",      deck },
  { "><>",      NULL },    /* no layout function means floating behavior */
};

/* key definitions */
#define MODKEY Mod4Mask
#define AltMask Mod1Mask
#define TAGKEYS(KEY,TAG) \
  { MODKEY,                       KEY,      view,           {.ui = 1 << TAG} }, \
  { MODKEY|ControlMask,           KEY,      toggleview,     {.ui = 1 << TAG} }, \
  { MODKEY|ShiftMask,             KEY,      tag,            {.ui = 1 << TAG} },

#define STACKKEYS(MOD,ACTION) \
  { MOD, XK_j,     ACTION##stack, {.i = INC(+1) } }, \
  { MOD, XK_k,     ACTION##stack, {.i = INC(-1) } }, \
  { MOD, XK_x,     ACTION##stack, {.i = PREVSEL } }, \
  { MOD, XK_q,     ACTION##stack, {.i = 0 } }, \
  { MOD, XK_a,     ACTION##stack, {.i = 1 } }, \
  { MOD, XK_s,     ACTION##stack, {.i = 2 } }, \
  { MOD, XK_d,     ACTION##stack, {.i = 3 } }, \
  { MOD, XK_z,     ACTION##stack, {.i = -1 } },

/* helper for spawning shell commands in the pre dwm-5.0 fashion */
#define SHCMD(cmd) { .v = (const char*[]){ "/bin/sh", "-c", cmd, NULL } }

/* commands */
static char dmenumon[2]            = "0"; /* component of dmenucmd, manipulated in spawn() */
static const char *dmenucmd[]      = { "dmenu_run", "-m", dmenumon, "-fn", dmenufont, "-nb", normbgcolor, "-nf", normfgcolor, "-sb", selbordercolor, "-sf", selfgcolor, NULL };
static const char *termcmd[]       = { "st", NULL };
static const char scratchpadname[] = "scratchpad";
static const char *scratchpadcmd[] = {"S", "st", "-t", scratchpadname, "-g","100x40", NULL };
static const char floattermname[]  = "floatterm";
static const char *floattermcmd[]  = {"T", "st", "-t", floattermname, "-g","100x40", NULL };

/*
 * Xresources preferences to load at startup
 */
ResourcePref resources[] = {
  { "font",               STRING,  &font },
  { "font2",              STRING,  &font2 },
  { "dmenufont",          STRING,  &dmenufont },
  { "normbgcolor",        STRING,  &normbgcolor },
  { "normbordercolor",    STRING,  &normbordercolor },
  { "normfgcolor",        STRING,  &normfgcolor },
  { "selbgcolor",         STRING,  &selbgcolor },
  { "selbordercolor",     STRING,  &selbordercolor },
  { "selfgcolor",         STRING,  &selfgcolor },
  { "borderpx",           INTEGER, &borderpx },
  { "gappx",              INTEGER, &gappx},
  { "urgborder",          INTEGER, &urgborder },
  { "snap",               INTEGER, &snap },
  { "topbar",             INTEGER, &topbar },
  { "nmaster",            INTEGER, &nmaster },
  { "resizehints",        INTEGER, &resizehints },
  { "mfact",              FLOAT,   &mfact },
  { "systraypinning",     INTEGER, &systraypinning },
  { "systrayonleft",      INTEGER, &systrayonleft },
  { "systrayspacing",     INTEGER, &systrayspacing },
};

static Key keys[] = {
  /* modifier                     key        function        argument */
  { MODKEY,                       XK_e,      spawn,          {.v = dmenucmd } },
  { MODKEY,                       XK_Return, spawn,          {.v = termcmd } },
  { MODKEY|ShiftMask,             XK_c,      togglescratch,  {.v = scratchpadcmd } },
  { MODKEY,                       XK_c,      togglescratch,  {.v = floattermcmd} },
  { MODKEY,                       XK_b,      togglebar,      {0} },
  { 0,                            XF86AudioMute,          spawn,          SHCMD("volume mute && pkill -RTMIN+1 dwmblocks")},
  { 0,                            XF86AudioLowerVolume,   spawn,          SHCMD("volume down && pkill -RTMIN+1 dwmblocks")},
  { 0,                            XF86AudioRaiseVolume,   spawn,          SHCMD("volume up && pkill -RTMIN+1 dwmblocks")},
  { 0,                            XF86AudioMicMute,       spawn,          SHCMD("mic mute")},
  { MODKEY,                       XF86AudioLowerVolume,   spawn,          SHCMD("mic down")},
  { MODKEY,                       XF86AudioRaiseVolume,   spawn,          SHCMD("mic up")},
  { MODKEY,                       XK_F9,     spawn,          SHCMD("pavucontrol && pkill -RTMIN+1 dwmblocks && pkill -RTMIN+2 dwmblocks")},
  { 0,                            XK_Print,  spawn,          SHCMD("sleep 0.2 && scrot -e 'mv $f ~/Pictures/screenshots && notify-send \"$f saved\"'")},
  { MODKEY,                       XK_Print,  spawn,          SHCMD("sleep 0.2 && scrot -s -e 'mv $f ~/Pictures/screenshots && notify-send \"$f saved\"'")},
  { MODKEY,                       XK_y,      spawn,          SHCMD("clipmenu-run")},
  { MODKEY,                       XK_p,      spawn,          SHCMD("dmenu-prockill")},
  { MODKEY,                       XK_F12,    spawn,          SHCMD("passmenu-otp")},
  { MODKEY,                       XK_Insert, spawn,          SHCMD("brave")},
  { MODKEY,                       XK_Delete, spawn,          SHCMD("smplayer")},
  { MODKEY,                       XK_F5,     spawn,          SHCMD("pkill wpa_gui; wpa_gui")},
  { MODKEY,                       XK_space,  spawn,          SHCMD("dmenu-winswitch")},
  { 0,                            XF86TouchpadToggle, spawn, SHCMD("toggle-touchpad")},
  { 0,                            XF86XK_MonBrightnessUp,  spawn, SHCMD("sleep 0.1 && brightness")},
  { 0,                            XF86XK_MonBrightnessDown,spawn, SHCMD("sleep 0.1 && brightness")},
  STACKKEYS(MODKEY,                          focus)
  STACKKEYS(MODKEY|ShiftMask,                push)
  { MODKEY|ControlMask,           XK_l,      setmfact,       {.f = +0.05} },
  { MODKEY|ControlMask,           XK_h,      setmfact,       {.f = -0.05} },
  { MODKEY,                       XK_i,      incnmaster,     {.i = +1 } },
  { MODKEY|ShiftMask,             XK_i,      incnmaster,     {.i = -1 } },
  { MODKEY,                       XK_o,      resetnmaster,   {0} },
  { MODKEY,                       XK_f,      togglefullscr,  {0} },
  { MODKEY|ShiftMask,             XK_Return, zoom,           {0} },
  { MODKEY,                       XK_Tab,    view,           {0} },
  { MODKEY,                       XK_bracketleft, cycleattachdir,{.i = +1 } },
  { MODKEY,                       XK_bracketright,cycleattachdir,{.i = -1 } },
  { MODKEY|ShiftMask,             XK_w,      killclient,     {0} },
  { MODKEY,                       XK_r,      cyclelayout,    {.i = +1 } },
  { MODKEY|ShiftMask,             XK_r,      cyclelayout,    {.i = -1 } },
  { MODKEY|ShiftMask,             XK_space,  togglefloating, {0} },
  { MODKEY,                       XK_0,      view,           {.ui = ~0 } },
  { MODKEY|ShiftMask,             XK_0,      tag,            {.ui = ~0 } },
  { MODKEY,                       XK_m,      togglesticky,   {0} },
  { MODKEY,                       XK_comma,  focusmon,       {.i = -1 } },
  { MODKEY,                       XK_period, focusmon,       {.i = +1 } },
  { MODKEY|ShiftMask,             XK_comma,  tagmon,         {.i = -1 } },
  { MODKEY|ShiftMask,             XK_period, tagmon,         {.i = +1 } },
  TAGKEYS(                        XK_1,                      0)
  TAGKEYS(                        XK_2,                      1)
  TAGKEYS(                        XK_3,                      2)
  TAGKEYS(                        XK_4,                      3)
  TAGKEYS(                        XK_5,                      4)
  TAGKEYS(                        XK_6,                      5)
  TAGKEYS(                        XK_7,                      6)
  TAGKEYS(                        XK_8,                      7)
  TAGKEYS(                        XK_9,                      8)
  { MODKEY|ShiftMask,             XK_F4,      quit,          {0} },
};

/* button definitions */
/* click can be ClkTagBar, ClkLtSymbol, ClkStatusText, ClkWinTitle, ClkClientWin, or ClkRootWin */
static Button buttons[] = {
  /* click                event mask      button          function        argument */
  //{ ClkWinTitle,          0,              Button2,        zoom,           {0} },
  { ClkClientWin,         MODKEY,         Button1,        movemouse,      {0} },
  { ClkClientWin,         MODKEY,         Button2,        togglefloating, {0} },
  { ClkClientWin,         MODKEY,         Button3,        resizemouse,    {0} },
  { ClkTagBar,            0,              Button1,        view,           {0} },
  { ClkTagBar,            0,              Button3,        toggleview,     {0} },
  { ClkTagBar,            MODKEY,         Button1,        tag,            {0} },
  { ClkTagBar,            MODKEY,         Button3,        toggletag,      {0} },
};
