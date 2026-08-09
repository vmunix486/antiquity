/*
 * ui.c - panel, start menu, command launcher using Athena Widgets
 */

#include "antiquity.h"

static void clock_timer_cb(XtPointer cd, XtIntervalId *id);

/* --- color config loader --- */

void colors_load(const char *path) {
    FILE *f;
    char line[MAX_LINE];
    int in_colors = 0;
    char *home;
    char buf[512];
    const char *p = path;

    /* expand ~ */
    if (p[0] == '~' && p[1] == '/') {
        home = getenv("HOME");
        if (home) {
            sprintf(buf, "%s%s", home, p + 1);
            p = buf;
        }
    }

    f = fopen(p, "r");
    if (!f) return;

    while (fgets(line, sizeof(line), f)) {
        char *s = line, *eq;
        while (*s == ' ' || *s == '\t') s++;
        if (*s == '\n' || *s == '\0' || *s == '#' || *s == ';') continue;
        if (*s == '[') {
            in_colors = (strncmp(s, "[colors]", 8) == 0);
            continue;
        }
        if (!in_colors) continue;
        eq = strchr(s, '=');
        if (!eq) continue;
        {
            char key[MAX_NAME] = {0};
            char val[MAX_NAME] = {0};
            char *a = s, *b = eq + 1;
            int i = 0;
            while (a < eq && *a != ' ' && *a != '\t' && i < MAX_NAME - 1)
                key[i++] = *a++;
            i = 0;
            while (*b == ' ' || *b == '\t') b++;
            while (*b && *b != '\n' && *b != '\r' && i < MAX_NAME - 1)
                val[i++] = *b++;
            while (i > 0 && (val[i-1] == ' ' || val[i-1] == '\t'))
                val[--i] = '\0';
            if (strcmp(key, "panel_bg") == 0) strcpy(colors.panel_bg, val);
            else if (strcmp(key, "panel_fg") == 0) strcpy(colors.panel_fg, val);
            else if (strcmp(key, "title_on") == 0) strcpy(colors.title_on, val);
            else if (strcmp(key, "title_off") == 0) strcpy(colors.title_off, val);
            else if (strcmp(key, "title_fg") == 0) strcpy(colors.title_fg, val);
            else if (strcmp(key, "border") == 0) strcpy(colors.border, val);
        }
    }
    fclose(f);
}

/* --- INI-style config parser --- */

void menu_load(const char *path) {
    FILE *f;
    char line[MAX_LINE];
    int in_menu = 0;
    char *home;
    char buf[512];
    const char *p = path;

    DBG("menu_load: path=%s", path);

    /* expand ~ */
    if (p[0] == '~' && p[1] == '/') {
        home = getenv("HOME");
        if (home) {
            sprintf(buf, "%s%s", home, p + 1);
            p = buf;
        }
    }

    DBG("menu_load: expanded path=%s", p);
    f = fopen(p, "r");
    if (!f) {
        DBG("menu_load: fopen FAILED for %s", p);
        return;
    }
    DBG0("menu_load: file opened OK");

    while (fgets(line, sizeof(line), f)) {
        char *s = line, *eq;
        while (*s == ' ' || *s == '\t') s++;
        if (*s == '\n' || *s == '\0' || *s == '#' || *s == ';') continue;
        if (*s == '[') {
            in_menu = (strncmp(s, "[menu]", 6) == 0);
            DBG("menu_load: section [%.*s] in_menu=%d", (int)strcspn(s, "]"), s, in_menu);
            continue;
        }
        if (!in_menu) continue;
        eq = strchr(s, '=');
        if (!eq) continue;
        {
            char name[MAX_NAME] = {0}, cmd[MAX_CMD] = {0};
            char *a = s, *b = eq + 1;
            int i = 0;
            while (a < eq && *a != ' ' && *a != '\t' && i < MAX_NAME - 1)
                name[i++] = *a++;
            i = 0;
            while (*b == ' ' || *b == '\t') b++;
            while (*b && *b != '\n' && *b != '\r' && i < MAX_CMD - 1)
                cmd[i++] = *b++;
            while (i > 0 && (cmd[i-1] == ' ' || cmd[i-1] == '\t'))
                cmd[--i] = '\0';
            DBG("menu_load: parsed name='%s' cmd='%s'", name, cmd);
            if (name[0] && cmd[0] && menu_count < MAX_MENU) {
                strcpy(menu_items[menu_count].name, name);
                strcpy(menu_items[menu_count].cmd, cmd);
                DBG("menu_load: added item %d: %s -> %s", menu_count, name, cmd);
                menu_count++;
            } else {
                DBG("menu_load: SKIPPED name='%s' cmd='%s' (empty or full)", name, cmd);
            }
        }
    }
    fclose(f);
    DBG("menu_load: done, menu_count=%d", menu_count);
}

void options_load(const char *path) {
    FILE *f;
    char line[MAX_LINE];
    int in_options = 0;
    char *home;
    char buf[512];
    const char *p = path;

    if (p[0] == '~' && p[1] == '/') {
        home = getenv("HOME");
        if (home) {
            sprintf(buf, "%s%s", home, p + 1);
            p = buf;
        }
    }

    f = fopen(p, "r");
    if (!f) return;

    while (fgets(line, sizeof(line), f)) {
        char *s = line, *eq;
        while (*s == ' ' || *s == '\t') s++;
        if (*s == '\n' || *s == '\0' || *s == '#' || *s == ';') continue;
        if (*s == '[') {
            in_options = (strncmp(s, "[options]", 9) == 0);
            continue;
        }
        if (!in_options) continue;
        eq = strchr(s, '=');
        if (!eq) continue;
        {
            char key[MAX_NAME] = {0};
            char val[MAX_CMD] = {0};
            char *a = s, *b = eq + 1;
            int i = 0;
            while (a < eq && *a != ' ' && *a != '\t' && i < MAX_NAME - 1)
                key[i++] = *a++;
            i = 0;
            while (*b == ' ' || *b == '\t') b++;
            while (*b && *b != '\n' && *b != '\r' && i < MAX_CMD - 1)
                val[i++] = *b++;
            while (i > 0 && (val[i-1] == ' ' || val[i-1] == '\t'))
                val[--i] = '\0';
            if (strcmp(key, "border_width") == 0) settings.border_w = atoi(val);
            else if (strcmp(key, "title_height") == 0) settings.title_h = atoi(val);
            else if (strcmp(key, "panel_height") == 0) settings.panel_h = atoi(val);
            else if (strcmp(key, "close_button_size") == 0) settings.close_sz = atoi(val);
            else if (strcmp(key, "min_width") == 0) settings.min_w = atoi(val);
            else if (strcmp(key, "min_height") == 0) settings.min_h = atoi(val);
            else if (strcmp(key, "outline_move") == 0) settings.outline_move = atoi(val);
            else if (strcmp(key, "alpha") == 0) settings.alpha = atoi(val);
            else if (strcmp(key, "show_clock") == 0) settings.show_clock = atoi(val);
            else if (strcmp(key, "show_date") == 0) settings.show_date = atoi(val);
            else if (strcmp(key, "hour_24") == 0) settings.hour_24 = atoi(val);
            else if (strcmp(key, "show_seconds") == 0) settings.show_seconds = atoi(val);
        }
    }
    fclose(f);
}

/* --- start menu --- */

static void menu_cb(Widget w, XtPointer client_data, XtPointer call_data) {
    char *cmd = (char *)client_data;
    (void)w; (void)call_data;
    if (cmd) run_cmd(cmd);
    ui_menu_hide();
}

static void menu_run_cb(Widget w, XtPointer cd, XtPointer cal) {
    (void)w; (void)cd; (void)cal;
    ui_menu_hide();
    ui_launcher_show();
}

static void menu_quit_cb(Widget w, XtPointer cd, XtPointer cal) {
    (void)w; (void)cd; (void)cal;
    XCloseDisplay(dpy);
    exit(0);
}

static int menu_realized = 0;

void ui_menu_create(void) {
    Widget item;
    int i;

    DBG("ui_menu_create: menu_count=%d", menu_count);

    /* SimpleMenu IS an override shell - use it directly as the popup */
    menu_shell = XtVaAppCreateShell("menu", "Antiquity",
        simpleMenuWidgetClass, dpy,
        XtNoverrideRedirect, True,
        NULL);

    for (i = 0; i < menu_count; i++) {
        char wname[32];
        sprintf(wname, "mi%d", i);
        DBG("ui_menu_create: adding item %d: '%s'", i, menu_items[i].name);
        item = XtVaCreateManagedWidget(wname, smeBSBObjectClass, menu_shell,
            XtNlabel, menu_items[i].name,
            NULL);
        XtAddCallback(item, XtNcallback, menu_cb, (XtPointer)menu_items[i].cmd);
    }

    if (menu_count > 0) {
        XtVaCreateManagedWidget("sep", smeLineObjectClass, menu_shell, NULL);
    }

    item = XtVaCreateManagedWidget("run", smeBSBObjectClass, menu_shell,
        XtNlabel, "Run Command...", NULL);
    XtAddCallback(item, XtNcallback, menu_run_cb, NULL);

    item = XtVaCreateManagedWidget("sep2", smeLineObjectClass, menu_shell, NULL);

    item = XtVaCreateManagedWidget("quit", smeBSBObjectClass, menu_shell,
        XtNlabel, "Quit Antiquity", NULL);
    XtAddCallback(item, XtNcallback, menu_quit_cb, NULL);

    /* add free-pointer motion handling for hover highlighting */
    {
        XtTranslations trans;
        trans = XtParseTranslationTable("<Motion>: highlight()");
        if (trans) {
            XtOverrideTranslations(menu_shell, trans);
            XtFree((char *)trans);
        }
    }
}

void ui_menu_show(void) {
    Position bx, by;
    Dimension bw, bh;
    Dimension mw, mh;

    if (menu_up) { ui_menu_hide(); return; }

    if (!menu_realized) {
        DBG0("ui_menu_show: first-time realize");
        XtRealizeWidget(menu_shell);
        XDefineCursor(dpy, XtWindow(menu_shell), pointer_cursor);
        menu_realized = 1;
    }

    DBG0("ui_menu_show: showing menu");
    XtTranslateCoords(start_btn, 0, 0, &bx, &by);
    XtVaGetValues(start_btn, XtNwidth, &bw, XtNheight, &bh, NULL);
    XtVaGetValues(menu_shell, XtNwidth, &mw, XtNheight, &mh, NULL);
    DBG("ui_menu_show: btn at (%d,%d) size=%dx%d menu_shell=%dx%d",
        (int)bx, (int)by, (int)bw, (int)bh, (int)mw, (int)mh);

    XtVaSetValues(menu_shell, XtNx, bx, XtNy, by - (int)mh, NULL);
    XtPopup(menu_shell, XtGrabExclusive);
    menu_up = 1;
    DBG("ui_menu_show: menu_up=1, shell mapped=%d", XtIsRealized(menu_shell));
}

void ui_menu_hide(void) {
    if (menu_up) {
        DBG("ui_menu_hide: hiding menu (was up=%d)", menu_up);
        XUngrabPointer(dpy, CurrentTime);
        XtPopdown(menu_shell);
        menu_up = 0;
    }
}

/* --- command launcher --- */

static int lsh_realized = 0;

static void lsh_run_cb(Widget w, XtPointer cd, XtPointer cal) {
    (void)w; (void)cd; (void)cal;
    ui_launcher_exec();
}

static void lsh_cancel_cb(Widget w, XtPointer cd, XtPointer cal) {
    (void)w; (void)cd; (void)cal;
    ui_launcher_hide();
}

void ui_launcher(void) {
    lsh_shell = XtVaAppCreateShell("launcher", "Antiquity",
        overrideShellWidgetClass, dpy,
        XtNoverrideRedirect, True,
        XtNwidth, 360,
        XtNheight, 80,
        XtNx, (sw - 360) / 2,
        XtNy, (sh - 80) / 2,
        XtNbackground, c_panel_bg,
        NULL);

    {
        Widget form, label, runbtn, cancelbtn;

        form = XtVaCreateManagedWidget("form", formWidgetClass, lsh_shell,
            XtNdefaultDistance, 4, NULL);

        label = XtVaCreateManagedWidget("lbl", labelWidgetClass, form,
            XtNlabel, "Run command:",
            XtNbackground, c_panel_bg,
            XtNforeground, c_panel_fg,
            XtNborderWidth, 0,
            NULL);

        lsh_text = XtVaCreateManagedWidget("txt", asciiTextWidgetClass, form,
            XtNfromVert, label,
            XtNwidth, 240,
            XtNheight, 24,
            XtNeditType, XawtextEdit,
            XtNbackground, c_white,
            XtNforeground, c_black,
            NULL);

        runbtn = XtVaCreateManagedWidget("run", commandWidgetClass, form,
            XtNlabel, "Run",
            XtNfromVert, label,
            XtNfromHoriz, lsh_text,
            XtNbackground, c_panel_bg,
            XtNforeground, c_panel_fg,
            NULL);
        XtAddCallback(runbtn, XtNcallback, lsh_run_cb, NULL);

        cancelbtn = XtVaCreateManagedWidget("cancel", commandWidgetClass, form,
            XtNlabel, "Cancel",
            XtNfromVert, runbtn,
            XtNfromHoriz, lsh_text,
            XtNbackground, c_panel_bg,
            XtNforeground, c_panel_fg,
            NULL);
        XtAddCallback(cancelbtn, XtNcallback, lsh_cancel_cb, NULL);
    }

    lsh_up = 0;
    lsh_realized = 0;
}

void ui_launcher_show(void) {
    if (lsh_up) { ui_launcher_hide(); return; }
    if (!lsh_realized) {
        XtRealizeWidget(lsh_shell);
        lsh_realized = 1;
    }
    XtVaSetValues(lsh_shell,
        XtNx, (sw - 360) / 2,
        XtNy, (sh - 80) / 2,
        NULL);
    XtVaSetValues(lsh_text, XtNstring, "", NULL);
    XtPopup(lsh_shell, XtGrabExclusive);
    lsh_up = 1;
    XSetInputFocus(dpy, XtWindow(lsh_text), RevertToPointerRoot, CurrentTime);
}

void ui_launcher_hide(void) {
    if (lsh_up) { XtPopdown(lsh_shell); lsh_up = 0; }
}

void ui_launcher_exec(void) {
    char *text = NULL;
    XtVaGetValues(lsh_text, XtNstring, &text, NULL);
    if (text && text[0]) {
        run_cmd(text);
    }
    ui_launcher_hide();
}

/* --- panel --- */

static void start_cb(Widget w, XtPointer cd, XtPointer cal) {
    (void)w; (void)cd; (void)cal;
    DBG("start_cb: Start button clicked, menu_up=%d", menu_up);
    ui_menu_show();
}

static void win_btn_cb(Widget w, XtPointer cd, XtPointer cal) {
    Client *c = (Client *)cd;
    (void)w; (void)cal;
    if (!c) return;
    XRaiseWindow(dpy, c->frame);
    XMapWindow(dpy, c->frame);
    focus_set(c);
}

void ui_panel(void) {
    panel_shell = XtVaAppCreateShell("panel", "Antiquity",
        overrideShellWidgetClass, dpy,
        XtNoverrideRedirect, True,
        XtNborderWidth, 0,
        XtNwidth, sw,
        XtNheight, settings.panel_h,
        XtNx, 0,
        XtNy, sh - settings.panel_h,
        XtNbackground, c_panel_bg,
        NULL);

    panel_box = XtVaCreateManagedWidget("box", boxWidgetClass, panel_shell,
        XtNorientation, XtorientHorizontal,
        XtNbackground, c_panel_bg,
        XtNborderWidth, 0,
        NULL);

    start_btn = XtVaCreateManagedWidget("start", commandWidgetClass, panel_box,
        XtNlabel, "  Start  ",
        XtNbackground, c_panel_bg,
        XtNforeground, c_panel_fg,
        XtNborderWidth, 2,
        NULL);
    XtAddCallback(start_btn, XtNcallback, start_cb, NULL);

    XtRealizeWidget(panel_shell);

    /* create clock as override shell at right edge of panel */
    if (settings.show_clock || settings.show_date) {
        Widget clock_form;
        int ch = settings.panel_h / 2;

        clock_shell = XtVaAppCreateShell("clock", "Antiquity",
            overrideShellWidgetClass, dpy,
            XtNoverrideRedirect, True,
            XtNborderWidth, 0,
            XtNwidth, 150,
            XtNheight, settings.panel_h,
            XtNx, sw - 125,
            XtNy, sh - settings.panel_h,
            XtNbackground, c_panel_bg,
            NULL);

        clock_form = XtVaCreateWidget("form", formWidgetClass, clock_shell,
            XtNbackground, c_panel_bg,
            NULL);

        clock_time = XtVaCreateManagedWidget("time", labelWidgetClass, clock_form,
            XtNlabel, "",
            XtNbackground, c_panel_bg,
            XtNforeground, c_panel_fg,
            XtNborderWidth, 0,
            XtNwidth, 150,
            XtNheight, ch,
            NULL);

        clock_date = XtVaCreateManagedWidget("date", labelWidgetClass, clock_form,
            XtNlabel, "",
            XtNbackground, c_panel_bg,
            XtNforeground, c_panel_fg,
            XtNborderWidth, 0,
            XtNwidth, 150,
            XtNheight, ch,
            XtNfromVert, clock_time,
            NULL);

        XtManageChild(clock_form);
        XtRealizeWidget(clock_shell);
        ui_panel_clock_update();
        clock_timer = XtAppAddTimeOut(app, 1000, clock_timer_cb, NULL);
    }
}

/* --- clock --- */

static void clock_timer_cb(XtPointer cd, XtIntervalId *id) {
    (void)cd; (void)id;
    ui_panel_clock_update();
    clock_timer = XtAppAddTimeOut(app,
        settings.show_seconds ? 1000 : 60000,
        clock_timer_cb, NULL);
}

void ui_panel_clock_update(void) {
    time_t now;
    struct tm *tm;

    if (!clock_time || !clock_date) return;

    now = time(NULL);
    tm = localtime(&now);

    if (settings.show_clock) {
        char tbuf[32];
        char *fmt;
        if (settings.hour_24) {
            fmt = settings.show_seconds ? "%H:%M:%S" : "%H:%M";
        } else {
            fmt = settings.show_seconds ? "%I:%M:%S %p" : "%I:%M %p";
        }
        strftime(tbuf, sizeof(tbuf), fmt, tm);
        XtVaSetValues(clock_time, XtNlabel, tbuf, NULL);
    } else {
        XtVaSetValues(clock_time, XtNlabel, "", NULL);
    }

    if (settings.show_date) {
        char dbuf[32];
        strftime(dbuf, sizeof(dbuf), "%a %b %d", tm);
        XtVaSetValues(clock_date, XtNlabel, dbuf, NULL);
    } else {
        XtVaSetValues(clock_date, XtNlabel, "", NULL);
    }
}

void ui_panel_add(Client *c) {
    char wname[32];
    char label[40];
    char *t;
    int n;

    if (!c) return;
    sprintf(wname, "w%lx", (unsigned long)c->win);

    t = c->title ? c->title : "(untitled)";
    n = (int)strlen(t);
    if (n > 20) {
        memcpy(label, t, 17);
        label[17] = '.'; label[18] = '.'; label[19] = '.'; label[20] = '\0';
    } else {
        strcpy(label, t);
    }

    c->btn = XtVaCreateManagedWidget(wname, commandWidgetClass, panel_box,
        XtNlabel, label,
        XtNbackground, c_panel_bg,
        XtNforeground, c_panel_fg,
        XtNborderWidth, 1,
        NULL);
    XtAddCallback(c->btn, XtNcallback, win_btn_cb, (XtPointer)c);
}

void ui_panel_del(Client *c) {
    if (c && c->btn) { XtDestroyWidget(c->btn); c->btn = NULL; }
}

void ui_panel_rename(Client *c) {
    char label[40];
    char *t;
    int n;

    if (!c || !c->btn) return;
    t = c->title ? c->title : "(untitled)";
    n = (int)strlen(t);
    if (n > 20) {
        memcpy(label, t, 17);
        label[17] = '.'; label[18] = '.'; label[19] = '.'; label[20] = '\0';
    } else {
        strcpy(label, t);
    }
    XtVaSetValues(c->btn, XtNlabel, label, NULL);
}
