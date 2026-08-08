/*
 * antiquity - a simple X11 window manager
 * Uses Xlib for WM core, Athena Widgets for UI elements
 * Designed to run inside Xnest
 */

#ifndef ANTIQUITY_H
#define ANTIQUITY_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Xaw/Command.h>
#include <X11/Xaw/Box.h>
#include <X11/Xaw/Form.h>
#include <X11/Xaw/Label.h>
#include <X11/Xaw/SimpleMenu.h>
#include <X11/Xaw/SmeBSB.h>
#include <X11/Xaw/SmeLine.h>
#include <X11/Xaw/AsciiText.h>
#include <X11/Xaw/AsciiSrc.h>

#define PANEL_H      30
#define TITLE_H      20
#define BORDER_W     4
#define CLOSE_SZ     14
#define MAX_MENU     32
#define MAX_LINE     256
#define MAX_NAME     64
#define MAX_CMD      128
#define MIN_W        100
#define MIN_H        60

#ifdef _DEBUG
#define DBG0(msg)          fprintf(stderr, "DBG: " msg "\n")
#define DBG(fmt, ...)      fprintf(stderr, "DBG: " fmt "\n", __VA_ARGS__)
#else
#define DBG0(msg)          ((void)0)
#define DBG(fmt, ...)      ((void)0)
#endif

typedef struct Client {
    Window win, frame;
    int x, y, w, h;
    int ox, oy, ow, oh;
    char *title;
    int maximized;
    int supports_delete;
    Widget btn;
    struct Client *next;
} Client;

typedef struct {
    char name[MAX_NAME];
    char cmd[MAX_CMD];
} MenuItem;

/* globals - main.c */
extern Display *dpy;
extern int screen;
extern Window root;
extern int sw, sh;
extern XtAppContext app;
extern XFontStruct *font;
extern GC gc;
extern Client *clients;
extern Client *focused;

/* atoms */
extern Atom a_wm_protos, a_wm_delete, a_wm_take_focus;

/* colors */
extern unsigned long c_panel_bg, c_panel_fg;
extern unsigned long c_title_on, c_title_off, c_title_fg;
extern unsigned long c_border, c_white, c_black;

/* panel/menu/launcher widgets */
extern Widget panel_shell, panel_box, start_btn;
extern Widget menu_shell;
extern int menu_up;
extern Widget lsh_shell, lsh_text;
extern int lsh_up;

/* drag/resize state */
extern int dragging;
extern int resizing;
extern Client *drag_c;
extern int drag_ox, drag_oy;
extern int resize_start_x, resize_start_y;
extern int resize_orig_x, resize_orig_y;
extern int resize_orig_w, resize_orig_h;
extern int resize_edge;
extern Cursor resize_cursor;
extern Cursor pointer_cursor;

/* menu config */
extern MenuItem menu_items[];
extern int menu_count;

/* utils */
char *xstrdup(const char *s);
void run_cmd(const char *cmd);

/* wm.c */
void wm_init(void);
void wm_map(XMapRequestEvent *e);
void wm_config(XConfigureRequestEvent *e);
void wm_unmap(XUnmapEvent *e);
void wm_destroy(XDestroyWindowEvent *e);
void wm_enter(XCrossingEvent *e);
void wm_key(XKeyEvent *e);
void wm_btn(XButtonEvent *e);
void wm_motion(XMotionEvent *e);
void wm_btnup(XButtonEvent *e);
void wm_expose(XExposeEvent *e);
void wm_prop(XPropertyEvent *e);
Client *client_add(Window w);
Client *client_by_frame(Window f);
Client *client_by_win(Window w);
void client_del(Client *c);
void frame_draw(Client *c);
void focus_set(Client *c);
void focus_clear(Client *c);
void client_close(Client *c);
void client_max(Client *c);
void client_min(Client *c);

/* ui.c */
void ui_panel(void);
void ui_menu_create(void);
void ui_menu_show(void);
void ui_menu_hide(void);
void ui_launcher(void);
void ui_launcher_show(void);
void ui_launcher_hide(void);
void ui_launcher_exec(void);
void ui_panel_add(Client *c);
void ui_panel_del(Client *c);
void ui_panel_rename(Client *c);
void menu_load(const char *path);

#endif
