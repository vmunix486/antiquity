/*
 * wm.c - window management: frames, clients, focus, events
 */

#include "antiquity.h"

static int casc_x = 40, casc_y = 40;

void wm_init(void) {
    unsigned int mods[4] = {0, LockMask, Mod2Mask, LockMask|Mod2Mask};
    KeyCode kc;
    int i;
    XSetWindowAttributes sa;

    sa.event_mask = SubstructureRedirectMask | SubstructureNotifyMask |
                    KeyPressMask | ButtonPressMask | ButtonReleaseMask |
                    EnterWindowMask | PropertyChangeMask;
    XSelectInput(dpy, root, sa.event_mask);

    kc = XKeysymToKeycode(dpy, XK_Super_L);
    if (kc)
        for (i = 0; i < 4; i++)
            XGrabKey(dpy, kc, mods[i], root, True, GrabModeAsync, GrabModeAsync);
}

/* client list */

Client *client_add(Window w) {
    Client *c;
    XWindowAttributes wa;
    char *name;

    if (XGetWindowAttributes(dpy, w, &wa) == 0) return NULL;
    if (wa.override_redirect) return NULL;

    for (c = clients; c; c = c->next)
        if (c->win == w) return c;

    c = (Client *)calloc(1, sizeof(Client));
    if (!c) return NULL;
    c->win = w;
    c->title = NULL;
    c->btn = NULL;

    if (XFetchName(dpy, w, &name) && name) {
        c->title = xstrdup(name);
        XFree(name);
    }
    if (!c->title) c->title = xstrdup("(untitled)");

    {
        Atom *pa = NULL;
        int n = 0, i;
        if (XGetWMProtocols(dpy, w, &pa, &n))
            for (i = 0; i < n; i++)
                if (pa[i] == a_wm_delete) { c->supports_delete = 1; break; }
        if (pa) XFree(pa);
    }

    c->w = wa.width;
    c->h = wa.height;
    c->x = casc_x; c->y = casc_y;
    casc_x += 28; casc_y += 28;
    if (casc_x + c->w > sw) casc_x = 40;
    if (casc_y + c->h > sh - PANEL_H) casc_y = 40;
    c->ox = c->x; c->oy = c->y; c->ow = c->w; c->oh = c->h;
    c->maximized = 0;

    c->frame = XCreateSimpleWindow(dpy, root, c->x, c->y,
        c->w, c->h + TITLE_H, BORDER_W, c_border, c_panel_bg);
    XSelectInput(dpy, c->frame, ExposureMask | ButtonPressMask | ButtonReleaseMask |
                 SubstructureNotifyMask);
    XReparentWindow(dpy, w, c->frame, 0, TITLE_H);
    XMapWindow(dpy, w);
    XMapWindow(dpy, c->frame);

    c->next = clients;
    clients = c;

    ui_panel_add(c);
    focus_set(c);
    return c;
}

Client *client_by_frame(Window f) {
    Client *c;
    for (c = clients; c; c = c->next)
        if (c->frame == f) return c;
    return NULL;
}

Client *client_by_win(Window w) {
    Client *c;
    for (c = clients; c; c = c->next)
        if (c->win == w) return c;
    return NULL;
}

void client_del(Client *c) {
    Client **pp;
    if (!c) return;

    ui_panel_del(c);
    if (focused == c) focused = NULL;

    for (pp = &clients; *pp; pp = &(*pp)->next)
        if (*pp == c) { *pp = c->next; break; }

    if (c->title) free(c->title);
    free(c);
}

/* frame drawing */

void frame_draw(Client *c) {
    int focused_;
    int bx, by;
    int pad;

    if (!c || c->frame == None) return;
    focused_ = (focused == c);

    /* title bar bg */
    XSetForeground(dpy, gc, focused_ ? c_title_on : c_title_off);
    XFillRectangle(dpy, c->frame, gc, 0, 0, c->w, TITLE_H);

    /* title text */
    XSetForeground(dpy, gc, focused_ ? c_title_fg : c_black);
    pad = CLOSE_SZ + 8;
    XDrawString(dpy, c->frame, gc, pad,
        (TITLE_H - font->ascent) / 2 + font->ascent,
        c->title ? c->title : "(untitled)",
        (int)strlen(c->title ? c->title : "(untitled)"));

    /* close button */
    bx = c->w - CLOSE_SZ - 4;
    by = (TITLE_H - CLOSE_SZ) / 2;
    XSetForeground(dpy, gc, c_white);
    XFillRectangle(dpy, c->frame, gc, bx, by, CLOSE_SZ, CLOSE_SZ);
    XSetForeground(dpy, gc, c_black);
    XDrawString(dpy, c->frame, gc, bx + 3, by + font->ascent, "X", 1);

    /* bottom line */
    XSetForeground(dpy, gc, c_border);
    XDrawLine(dpy, c->frame, gc, 0, TITLE_H - 1, c->w, TITLE_H - 1);
}

/* focus */

void focus_set(Client *c) {
    if (focused == c) return;
    if (focused) focus_clear(focused);
    focused = c;
    if (!c) return;
    XRaiseWindow(dpy, c->frame);
    XSetInputFocus(dpy, c->win, RevertToPointerRoot, CurrentTime);
    frame_draw(c);
    ui_panel_rename(c);
}

void focus_clear(Client *c) {
    if (c) frame_draw(c);
}

/* close / maximize */

void client_close(Client *c) {
    XEvent ev;
    if (!c) return;
    if (c->supports_delete) {
        memset(&ev, 0, sizeof(ev));
        ev.xclient.type = ClientMessage;
        ev.xclient.window = c->win;
        ev.xclient.message_type = a_wm_protos;
        ev.xclient.format = 32;
        ev.xclient.data.l[0] = a_wm_delete;
        ev.xclient.data.l[1] = CurrentTime;
        XSendEvent(dpy, c->win, False, NoEventMask, &ev);
    } else {
        XKillClient(dpy, c->win);
    }
}

void client_max(Client *c) {
    if (!c) return;
    if (c->maximized) {
        XMoveResizeWindow(dpy, c->frame, c->ox, c->oy,
            c->ow, c->oh + TITLE_H);
        XMoveResizeWindow(dpy, c->win, 0, TITLE_H, c->ow, c->oh);
        c->x = c->ox; c->y = c->oy; c->w = c->ow; c->h = c->oh;
        c->maximized = 0;
    } else {
        c->ox = c->x; c->oy = c->y; c->ow = c->w; c->oh = c->h;
        c->w = sw; c->h = sh - PANEL_H;
        c->x = 0; c->y = 0;
        XMoveResizeWindow(dpy, c->frame, 0, 0, sw, sh - PANEL_H);
        XMoveResizeWindow(dpy, c->win, 0, TITLE_H, sw, sh - PANEL_H - TITLE_H);
        c->maximized = 1;
    }
    frame_draw(c);
}

/* event handlers */

void wm_map(XMapRequestEvent *e) {
    client_add(e->window);
}

void wm_config(XConfigureRequestEvent *e) {
    XWindowChanges wc;
    wc.x = e->x; wc.y = e->y;
    wc.width = e->width; wc.height = e->height;
    wc.border_width = e->border_width;
    wc.sibling = e->above;
    wc.stack_mode = e->detail;
    XConfigureWindow(dpy, e->window, e->value_mask, &wc);
}

void wm_unmap(XUnmapEvent *e) {
    Client *c;
    c = client_by_win(e->window);
    if (!c) return;
    XReparentWindow(dpy, c->win, root, 0, 0);
    if (c->frame != None) XDestroyWindow(dpy, c->frame);
    c->frame = None;
    client_del(c);
}

void wm_destroy(XDestroyWindowEvent *e) {
    Client *c;
    c = client_by_win(e->window);
    if (!c) return;
    if (c->frame != None) XDestroyWindow(dpy, c->frame);
    c->frame = None;
    client_del(c);
}

void wm_enter(XCrossingEvent *e) {
    Client *c;
    if (e->mode != NotifyNormal) return;
    c = client_by_win(e->window);
    if (!c) c = client_by_frame(e->window);
    if (c) focus_set(c);
}

void wm_key(XKeyEvent *e) {
    KeySym ks = XLookupKeysym(e, 0);
    if (ks == XK_Super_L) {
        if (lsh_up) ui_launcher_hide();
        else ui_launcher_show();
    } else if (ks == XK_Escape) {
        if (lsh_up) ui_launcher_hide();
        if (menu_up) ui_menu_hide();
    }
}

void wm_btn(XButtonEvent *e) {
    Client *c;

    /* if start menu is open, let Xt dispatch handle item clicks,
     * dismiss if click is outside menu */
    if (menu_up) {
        if (e->window != XtWindow(menu_shell))
            ui_menu_hide();
        return;
    }

    c = client_by_frame(e->window);
    if (!c) return;
    focus_set(c);
    if (e->y < TITLE_H) {
        int bx = c->w - CLOSE_SZ - 4;
        int by = (TITLE_H - CLOSE_SZ) / 2;
        if (e->x >= bx && e->x < bx + CLOSE_SZ &&
            e->y >= by && e->y < by + CLOSE_SZ) {
            client_close(c);
            return;
        }
        /* double-click maximizes */
        /* single click starts drag */
        dragging = 1;
        drag_c = c;
        drag_ox = e->x_root - c->x;
        drag_oy = e->y_root - c->y;
        XGrabPointer(dpy, c->frame, True,
            ButtonReleaseMask | PointerMotionMask,
            GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
    }
}

void wm_motion(XMotionEvent *e) {
    XEvent dum;
    if (!dragging || !drag_c) return;
    drag_c->x = e->x_root - drag_ox;
    drag_c->y = e->y_root - drag_oy;
    XMoveWindow(dpy, drag_c->frame, drag_c->x, drag_c->y);
    /* drain motion events for smoothness */
    while (XPending(dpy)) {
        XPeekEvent(dpy, &dum);
        if (dum.type == MotionNotify) XNextEvent(dpy, &dum);
        else break;
    }
}

void wm_btnup(XButtonEvent *e) {
    (void)e;
    if (dragging) {
        dragging = 0;
        drag_c = NULL;
        XUngrabPointer(dpy, CurrentTime);
    }
}

void wm_expose(XExposeEvent *e) {
    Client *c;
    if (e->count > 0) return;
    c = client_by_frame(e->window);
    if (c) frame_draw(c);
}

void wm_prop(XPropertyEvent *e) {
    Client *c;
    char *name;
    c = client_by_win(e->window);
    if (!c) return;
    if (e->atom != XA_WM_NAME) return;
    if (c->title) free(c->title);
    c->title = NULL;
    if (XFetchName(dpy, c->win, &name) && name) {
        c->title = xstrdup(name);
        XFree(name);
    }
    if (!c->title) c->title = xstrdup("(untitled)");
    frame_draw(c);
    ui_panel_rename(c);
}
