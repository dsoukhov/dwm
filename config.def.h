/* See LICENSE file for copyright and license details. */

/* appearance */
static const unsigned int borderpx  = 4;        /* border pixel of windows */
static const unsigned int gappx     = 3;        /* gaps between windows */
static const unsigned int snap      = 32;       /* snap pixel */
static const unsigned int systraypinning = 0;   /* 0: sloppy systray follows selected monitor, >0: pin systray to monitor X */
static const unsigned int systrayonleft = 0;   	/* 0: systray in the right corner, >0: systray on left of status text */
static const unsigned int systrayspacing = 2;   /* systray spacing */
static const int systraypinningfailfirst = 1;   /* 1: if pinning fails, display systray on the first monitor, False: display systray on the last monitor*/
static const int showsystray        = 1;     /* 0 means no systray */
static const int showbar            = 1;        /* 0 means no bar */
static const int topbar             = 1;        /* 0 means bottom bar */
static const char *fonts[]          = { "Hack Nerd Font Mono:size=9", "Noto Color Emoji:style=Regular:pixelsize=12:antialias=true:autohint=true" };
static const char dmenufont[]       = "Hack Nerd Font Mono:size=9";
static const char col_gray1[]       = "#222222";
static const char col_gray2[]       = "#444444";
static const char col_gray3[]       = "#bbbbbb";
static const char col_gray4[]       = "#eeeeee";
static const char col_cyan[]        = "#005577";
static const char col_urgborder[]   = "#ff0000";
static const char *colors[][3]      = {
  /*               fg         bg         border   */
  [SchemeNorm] = { col_gray3, col_gray1, col_gray2 },
  [SchemeSel]  = { col_gray4, col_cyan,  col_cyan  },
  [SchemeUrg]  = { col_gray4, col_cyan,  col_urgborder},
};

/* sticky symbol */
static const XPoint stickyicon[]    = { {0,0}, {4,0}, {4,8}, {2,6}, {0,8}, {0,0} }; /* represents the icon as an array of vertices */
static const XPoint stickyiconbb    = {4,8};	/* defines the bottom right corner of the polygon's bounding box (speeds up scaling) */

/* tagging */
static const char *tags[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9" };

/* window swallowing */
static const int swaldecay = 3;
static const int swalretroactive = 1;
static const char swalsymbol[] = ">O";

static const Rule rules[] = {
  /* xprop(1):
   *	WM_CLASS(STRING) = instance, class
   *	WM_NAME(STRING) = title
   */
  /* class      instance    title       tags mask     isfloating   iscentered   ispermanent  monitor  ignoreReqest scratch key*/
  {  NULL,      NULL,       "scratchpad", 0,          1,           1,           1,               -1 , 0, 'S'},
  {  NULL,      NULL,       "floatterm", 0,          1,           1,           1,               -1 , 0, 'T'},
  { "net-runelite-client-RuneLite", NULL, NULL, 0, 0, 0, 0, -1, 1,0},
};

/* layout(s) */
static const float mfact     = 0.55; /* factor of master area size [0.05..0.95] */
static const int nmaster     = 1;    /* number of clients in master area */
static const int resizehints = 1;    /* 1 means respect size hints in tiled resizals */

static const int defaultatchdir = 0;  /*    set the default attach dir
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
  //{ "[M]",      monocle },
};

/* key definitions */
#define MODKEY Mod4Mask
#define TAGKEYS(KEY,TAG) \
  { MODKEY,                       KEY,      view,           {.ui = 1 << TAG} }, \
  { MODKEY|ControlMask,           KEY,      toggleview,     {.ui = 1 << TAG} }, \
  { MODKEY|ShiftMask,             KEY,      tag,            {.ui = 1 << TAG} }, \
  //{ MODKEY|ControlMask|ShiftMask, KEY,      toggletag,      {.ui = 1 << TAG} },

#define STACKKEYS(MOD,ACTION) \
  { MOD, XK_j,     ACTION##stack, {.i = INC(+1) } }, \
  { MOD, XK_k,    ACTION##stack, {.i = INC(-1) } }, \
  { MOD, XK_x,     ACTION##stack, {.i = PREVSEL } }, \
  { MOD, XK_q,     ACTION##stack, {.i = 0 } }, \
  { MOD, XK_a,     ACTION##stack, {.i = 1 } }, \
  { MOD, XK_s,     ACTION##stack, {.i = 2 } }, \
  { MOD, XK_d,     ACTION##stack, {.i = 3 } }, \
  { MOD, XK_z,     ACTION##stack, {.i = -1 } },

/* helper for spawning shell commands in the pre dwm-5.0 fashion */
#define SHCMD(cmd) { .v = (const char*[]){ "/bin/sh", "-c", cmd, NULL } }

/* commands */
static char dmenumon[2] = "0"; /* component of dmenucmd, manipulated in spawn() */
static const char *dmenucmd[] = { "dmenu_run", "-m", dmenumon, "-fn", dmenufont, "-nb", col_gray1, "-nf", col_gray3, "-sb", col_cyan, "-sf", col_gray4, NULL };
static const char *termcmd[]  = { "st", NULL };
static const char scratchpadname[] = "scratchpad";
static const char *scratchpadcmd[] = {"S", "st", "-t", scratchpadname, "-g","100x40", NULL };
static const char floattermname[] = "floatterm";
static const char *floattermcmd[] = {"T", "st", "-t", floattermname, "-g","100x40", NULL };
static Key keys[] = {
  /* modifier                     key        function        argument */
  { MODKEY,                       XK_e,      spawn,          {.v = dmenucmd } },
  { MODKEY,                       XK_Return, spawn,          {.v = termcmd } },
  { MODKEY,                       XK_c,      togglescratch,  {.v = scratchpadcmd } },
  { MODKEY,                       XK_v,      togglescratch,  {.v = floattermcmd} },
  { MODKEY,                       XK_b,      togglebar,      {0} },
  { MODKEY,                       XK_F10,    spawn,          SHCMD("pamixer -t && pkill -RTMIN+1 dwmblocks")},
  { MODKEY,                       XK_F11,    spawn,          SHCMD("pamixer -d 5 && pkill -RTMIN+1 dwmblocks")},
  { MODKEY,                       XK_F12,    spawn,          SHCMD("pamixer -i 5 && pkill -RTMIN+1 dwmblocks")},
  { MODKEY,                       XK_F6,     spawn,          SHCMD("pamixer --source \"alsa_input.pci-0000_00_1f.3.analog-stereo\" -t && pkill -RTMIN+2 dwmblocks")},
  { MODKEY,                       XK_F7,     spawn,          SHCMD("pamixer --source \"alsa_input.pci-0000_00_1f.3.analog-stereo\" -d 5 && pkill -RTMIN+2 dwmblocks")},
  { MODKEY,                       XK_F8,     spawn,          SHCMD("pamixer --source \"alsa_input.pci-0000_00_1f.3.analog-stereo\" -i 5 && pkill -RTMIN+2 dwmblocks")},
  { 0,                            XK_Print,  spawn,          SHCMD("sleep 0.2 && scrot -e 'mv $f ~/Pictures/screenshots && notify-send \"$f saved\"'")},
  { MODKEY,                       XK_Print,  spawn,          SHCMD("sleep 0.2 && scrot -s -e 'mv $f ~/Pictures/screenshots && notify-send \"$f saved\"'")},
  { MODKEY,                       XK_y,      spawn,          SHCMD("clipmenu-run")},
  { MODKEY,                       XK_p,      spawn,          SHCMD("dmenu-prockill")},
  { MODKEY,                       XK_Home,   spawn,          SHCMD("passmenu-otp")},
  { MODKEY,                       XK_Insert, spawn,          SHCMD("brave")},
  { MODKEY,                       XK_Prior,  spawn,          SHCMD("osrs-launcher")},
  { MODKEY,                       XK_Delete, spawn,          SHCMD("qutebrowser")},
  STACKKEYS(MODKEY,                          focus)
  STACKKEYS(MODKEY|ShiftMask,                push)
  STACKKEYS(MODKEY|ControlMask,              swalsel)
  { MODKEY|ControlMask,           XK_l,      setmfact,       {.f = +0.05} },
  { MODKEY|ControlMask,           XK_h,      setmfact,       {.f = -0.05} },
  { MODKEY,                       XK_i,      incnmaster,     {.i = +1 } },
  { MODKEY,                       XK_o,      incnmaster,     {.i = -1 } },
  { MODKEY|ShiftMask,             XK_o,      resetnmaster,   {0} },
  { MODKEY,                       XK_f,      togglefullscr,  {0} },
  { MODKEY|ShiftMask,             XK_Return, zoom,           {0} },
  { MODKEY,                       XK_Tab,    view,           {0} },
  { MODKEY,                       XK_bracketleft,      cycleattachdir,{.i = +1} },
  { MODKEY,                       XK_bracketright,      cycleattachdir,{.i = -1 } },
  { MODKEY|ShiftMask,             XK_w,      killclient,     {0} },
  { MODKEY,                       XK_r,      cyclelayout,    {.i = +1 } },
  { MODKEY,                       XK_t,      cyclelayout,    {.i = -1 } },
  { MODKEY,                       XK_space,  setlayout,      {0} },
  { MODKEY|ShiftMask,             XK_space,  togglefloating, {0} },
  { MODKEY,                       XK_0,      view,           {.ui = ~0 } },
  { MODKEY|ShiftMask,             XK_0,      tag,            {.ui = ~0 } },
  { MODKEY,                       XK_m,      togglesticky,   {0} },
  { MODKEY,                       XK_g,      swalstopsel,    {0} },
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
  { MODKEY|ShiftMask,             XK_F4,      quit,           {0} },
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
  { ClkClientWin,         MODKEY|ShiftMask, Button1,      swalmouse,      {0} },
};
