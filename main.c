/*
 * main.c - antiquity entry point, initialization, event loop
 */

#include "antiquity.h"

/* global definitions */
Display *dpy;
int screen;
Window root;
int sw, sh;
XtAppContext app;
XFontStruct *font;
GC gc;
Client *clients = NULL;
Client *focused = NULL;

Settings settings = { BORDER_W, TITLE_H, PANEL_H, CLOSE_SZ, MIN_W, MIN_H, 0, 1, 1, 1, 1, 1 };
Colors colors = { "#b0b0b0", "#000000", "#000080", "#b0b0b0", "#ffffff", "#808080" };

Atom a_wm_protos, a_wm_delete, a_wm_take_focus;

unsigned long c_panel_bg, c_panel_fg;
unsigned long c_title_on, c_title_off, c_title_fg;
unsigned long c_border, c_white, c_black;

Widget panel_shell, panel_box, start_btn;
Widget menu_shell;
int menu_up = 0;
Widget lsh_shell, lsh_text;
int lsh_up = 0;

Widget clock_shell = NULL, clock_time = NULL, clock_date = NULL;
XtIntervalId clock_timer = 0;

int dragging = 0;
int resizing = 0;
Client *drag_c = NULL;
int drag_ox = 0, drag_oy = 0;
int resize_start_x = 0, resize_start_y = 0;
int resize_orig_x = 0, resize_orig_y = 0;
int resize_orig_w = 0, resize_orig_h = 0;
int resize_edge = 0;
Cursor resize_cursor = None;
Cursor pointer_cursor = None;
GC outline_gc = NULL;
int outline_on = 0;
int outline_x = 0, outline_y = 0, outline_w = 0, outline_h = 0;

MenuItem menu_items[MAX_MENU];
int menu_count = 0;

char *xstrdup(const char *s) {
    char *r;
    int n = (int)strlen(s) + 1;
    r = (char *)malloc(n);
    if (r) memcpy(r, s, n);
    return r;
}

void run_cmd(const char *cmd) {
    pid_t pid = fork();
    if (pid == 0) {
        /* child - detach from WM */
        setsid();
        signal(SIGCHLD, SIG_DFL);
        signal(SIGPIPE, SIG_DFL);
        execl("/bin/sh", "sh", "-c", cmd, NULL);
        _exit(127);
    }
}

static int xerror_handler(Display *d, XErrorEvent *e) {
    (void)d;
    /* ignore BadMatch, BadDrawable, BadWindow - common during setup/teardown */
    if (e->error_code == BadMatch ||
        e->error_code == BadDrawable ||
        e->error_code == BadWindow)
        return 0;
    fprintf(stderr, "antiquity: X error %d on request %d\n",
            e->error_code, e->request_code);
    return 0;
}

static int xerror_start(Display *d, XErrorEvent *e) {
    (void)d; (void)e;
    fprintf(stderr, "antiquity: another window manager is already running\n");
    exit(1);
    return -1;
}

static unsigned long alloc_color(const char *name) {
    XColor c, ex;
    Colormap cm = DefaultColormap(dpy, screen);
    if (XAllocNamedColor(dpy, cm, name, &c, &ex))
        return c.pixel;
    return BlackPixel(dpy, screen);
}

static int hex_digit(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    return -1;
}

static unsigned long parse_hex_color(const char *hex) {
    int r, g, b, a;
    int len, i, val;
    unsigned long pixel;

    if (!hex || hex[0] != '#') goto fallback;
    len = 0;
    while (hex[len + 1]) len++;
    if (len != 6 && len != 8) goto fallback;

    r = g = b = 0; a = 255;
    for (i = 1; i <= len; i++) {
        val = hex_digit(hex[i]);
        if (val < 0) goto fallback;
        switch (i) {
        case 1: r = val << 4; break;
        case 2: r |= val; break;
        case 3: g = val << 4; break;
        case 4: g |= val; break;
        case 5: b = val << 4; break;
        case 6: b |= val; break;
        case 7: a = val << 4; break;
        case 8: a |= val; break;
        }
    }

    if (!settings.alpha) a = 255;

    pixel = ((unsigned long)a << 24) | ((unsigned long)r << 16) |
            ((unsigned long)g << 8) | (unsigned long)b;
    return pixel;

fallback:
    return alloc_color("#808080");
}

static void reap(int sig) {
    (void)sig;
    while (waitpid(-1, NULL, WNOHANG) > 0)
        ;
}

static void launcher_action(Widget w, XEvent *ev, String *p, Cardinal *n) {
    (void)w; (void)ev; (void)p; (void)n;
    ui_launcher_exec();
}

static void dismiss_action(Widget w, XEvent *ev, String *p, Cardinal *n) {
    (void)w; (void)ev; (void)p; (void)n;
    ui_launcher_hide();
}

static XtActionsRec actions[] = {
    { "run-cmd", launcher_action },
    { "dismiss", dismiss_action },
};

int main(int argc, char **argv) {
    XEvent ev;
    XtTranslations trans;
    const char *cfg;

    signal(SIGCHLD, reap);

    XtToolkitInitialize();
    app = XtCreateApplicationContext();
    dpy = XtOpenDisplay(app, NULL, "antiquity", "Antiquity", NULL, 0, &argc, argv);
    if (!dpy) {
        fprintf(stderr, "antiquity: cannot open display\n");
        return 1;
    }

    screen = DefaultScreen(dpy);
    root = RootWindow(dpy, screen);
    sw = DisplayWidth(dpy, screen);
    sh = DisplayHeight(dpy, screen);

    font = XLoadQueryFont(dpy, "fixed");
    if (!font) {
        fprintf(stderr, "antiquity: no font\n");
        return 1;
    }

    {
        XGCValues gv;
        gv.font = font->fid;
        gc = XCreateGC(dpy, root, GCFont, &gv);
    }

    a_wm_protos     = XInternAtom(dpy, "WM_PROTOCOLS", False);
    a_wm_delete     = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    a_wm_take_focus = XInternAtom(dpy, "WM_TAKE_FOCUS", False);

    /* set error handler to detect another WM */
    XSetErrorHandler(xerror_start);
    wm_init();
    /* switch to lenient error handler */
    XSetErrorHandler(xerror_handler);

    /* load options: env var, ~/.antiquity, /etc, same dir as binary */
    {
        char path[512];
        char *home = getenv("HOME");
        const char *env;
        FILE *f;
        int loaded = 0;

        env = getenv("ANTIQUITY_OPTIONS");
        if (env) {
            options_load(env);
            loaded = 1;
        }
        if (!loaded && home) {
            sprintf(path, "%s/.antiquity/options.ini", home);
            f = fopen(path, "r");
            if (f) { fclose(f); options_load(path); loaded = 1; }
        }
        if (!loaded) {
            f = fopen("/etc/antiquity/options.ini", "r");
            if (f) { fclose(f); options_load("/etc/antiquity/options.ini"); loaded = 1; }
        }
        if (!loaded) {
            f = fopen("options.ini", "r");
            if (f) { fclose(f); options_load("options.ini"); }
        }
    }

    /* load colors: env var, ~/.antiquity, /etc, same dir as binary */
    {
        char path[512];
        char *home = getenv("HOME");
        const char *env;
        FILE *f;
        int loaded = 0;

        env = getenv("ANTIQUITY_COLORS");
        if (env) {
            colors_load(env);
            loaded = 1;
        }
        if (!loaded && home) {
            sprintf(path, "%s/.antiquity/colors.ini", home);
            f = fopen(path, "r");
            if (f) { fclose(f); colors_load(path); loaded = 1; }
        }
        if (!loaded) {
            f = fopen("/etc/antiquity/colors.ini", "r");
            if (f) { fclose(f); colors_load("/etc/antiquity/colors.ini"); loaded = 1; }
        }
        if (!loaded) {
            f = fopen("colors.ini", "r");
            if (f) { fclose(f); colors_load("colors.ini"); }
        }
    }

    /* allocate parsed colors */
    c_panel_bg   = parse_hex_color(colors.panel_bg);
    c_panel_fg   = parse_hex_color(colors.panel_fg);
    c_title_on   = parse_hex_color(colors.title_on);
    c_title_off  = parse_hex_color(colors.title_off);
    c_title_fg   = parse_hex_color(colors.title_fg);
    c_border     = parse_hex_color(colors.border);
    c_white      = alloc_color("#ffffff");
    c_black      = alloc_color("#000000");

    /* find menu config: env var, ~/.antiquity, /etc, same dir as binary */
    cfg = getenv("ANTIQUITY_MENU");
    if (cfg) {
        DBG("config: ANTIQUITY_MENU=%s", cfg);
        menu_load(cfg);
    } else {
        char path[512];
        char *home = getenv("HOME");
        FILE *f;

        DBG0("config: no ANTIQUITY_MENU env, searching...");
        /* try ~/.antiquity/menu.ini */
        if (home) {
            sprintf(path, "%s/.antiquity/menu.ini", home);
            DBG("config: trying %s", path);
            f = fopen(path, "r");
            if (f) { fclose(f); menu_load(path); }
        }
            /* try /etc/antiquity/menu.ini */
        if (menu_count == 0) {
            DBG0("config: trying /etc/antiquity/menu.ini");
            f = fopen("/etc/antiquity/menu.ini", "r");
            if (f) { fclose(f); menu_load("/etc/antiquity/menu.ini"); }
        }
            /* try ./menu.ini */
        if (menu_count == 0) {
            DBG0("config: trying ./menu.ini");
            f = fopen("menu.ini", "r");
            if (f) { fclose(f); menu_load("menu.ini"); }
        }
        DBG("config: final menu_count=%d", menu_count);
    }

    ui_menu_create();
    ui_launcher();
    ui_panel();

    XtAppAddActions(app, actions, 1);
    trans = XtParseTranslationTable(
        "<Key>Return: run-cmd()\n<Key>Escape: dismiss()");
    if (trans && lsh_text)
        XtOverrideTranslations(lsh_text, trans);

    for (;;) {
        XtAppNextEvent(app, &ev);
        switch (ev.type) {
        case MapRequest:      wm_map(&ev.xmaprequest); break;
        case ConfigureRequest: wm_config(&ev.xconfigurerequest); break;
        case UnmapNotify:     wm_unmap(&ev.xunmap); break;
        case DestroyNotify:   wm_destroy(&ev.xdestroywindow); break;
        case EnterNotify:     wm_enter(&ev.xcrossing); break;
        case KeyPress:        wm_key(&ev.xkey); break;
        case ButtonPress:     wm_btn(&ev.xbutton); break;
        case ButtonRelease:   wm_btnup(&ev.xbutton); break;
        case MotionNotify:    wm_motion(&ev.xmotion); break;
        case Expose:          wm_expose(&ev.xexpose); break;
        case PropertyNotify:  wm_prop(&ev.xproperty); break;
        }
        XtDispatchEvent(&ev);
    }

    return 0;
}
