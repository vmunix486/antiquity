/*
 * wm.c - window management: frames, clients, focus, events
 */

#include "antiquity.h"

static int casc_x = 40, casc_y = 40;

#define EDGE_NONE  0
#define EDGE_LEFT  1
#define EDGE_RIGHT 2
#define EDGE_TOP   4
#define EDGE_BOT   8
#define EDGE_TL    (EDGE_TOP|EDGE_LEFT)
#define EDGE_TR    (EDGE_TOP|EDGE_RIGHT)
#define EDGE_BL    (EDGE_BOT|EDGE_LEFT)
#define EDGE_BR    (EDGE_BOT|EDGE_RIGHT)

/* top edge gets a smaller threshold so title bar dragging isn't interrupted */
#define EDGE_TOP_TZ   4
#define EDGE_OTHER_TZ settings.border_w

static Cursor wm_edge_cursor(int edge) {
    unsigned int shape;
    switch (edge) {
    case EDGE_LEFT:  shape = 70;  break; /* XC_left_side */
    case EDGE_RIGHT: shape = 96;  break; /* XC_right_side */
    case EDGE_TOP:   shape = 138; break; /* XC_top_side */
    case EDGE_BOT:   shape = 16;  break; /* XC_bottom_side */
    case EDGE_TL:    shape = 134; break; /* XC_top_left_corner */
    case EDGE_TR:    shape = 136; break; /* XC_top_right_corner */
    case EDGE_BL:    shape = 12;  break; /* XC_bottom_left_corner */
    case EDGE_BR:    shape = 14;  break; /* XC_bottom_right_corner */
    default:         shape = 68;  break; /* XC_left_ptr */
    }
    return XCreateFontCursor(dpy, shape);
}

static int wm_edge_detect(Client *c, int mx, int my) {
    int fw, fh, edge;
    if (!c) return EDGE_NONE;
    fw = c->w + 2 * settings.border_w;
    fh = c->h + settings.title_h + 2 * settings.border_w;
    edge = EDGE_NONE;
    if (mx < EDGE_OTHER_TZ) edge |= EDGE_LEFT;
    else if (mx >= fw - EDGE_OTHER_TZ) edge |= EDGE_RIGHT;
    /* top uses a smaller threshold to avoid interfering with title bar clicks */
    if (my < EDGE_TOP_TZ) edge |= EDGE_TOP;
    else if (my >= fh - EDGE_OTHER_TZ) edge |= EDGE_BOT;
    return edge;
}

/* detect edge from client-relative coordinates */
static int wm_edge_from_client(Client *c, int wx, int wy) {
    int edge, tz;
    if (!c) return EDGE_NONE;
    edge = EDGE_NONE;
    tz = EDGE_OTHER_TZ;
    /* client has 0 border, content area: x in [0, c->w], y in [0, c->h] */
    if (wx < tz) edge |= EDGE_LEFT;
    else if (wx >= c->w - tz) edge |= EDGE_RIGHT;
    if (wy < tz) edge |= EDGE_TOP;
    else if (wy >= c->h - tz) edge |= EDGE_BOT;
    return edge;
}

static void wm_set_frame_cursor(Client *c, int edge) {
    Cursor cur;
    if (edge == EDGE_NONE) {
        XUndefineCursor(dpy, c->frame);
    } else {
        cur = wm_edge_cursor(edge);
        XDefineCursor(dpy, c->frame, cur);
        XFreeCursor(dpy, cur);
    }
}

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

    resize_cursor = wm_edge_cursor(EDGE_BR);
    pointer_cursor = XCreateFontCursor(dpy, 68); /* XC_left_ptr */
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
    if (casc_y + c->h > sh - settings.panel_h) casc_y = 40;
    c->ox = c->x; c->oy = c->y; c->ow = c->w; c->oh = c->h;
    c->maximized = 0;

    c->frame = XCreateSimpleWindow(dpy, root, c->x, c->y,
        c->w + 2 * settings.border_w, c->h + settings.title_h + 2 * settings.border_w,
        0, c_border, c_panel_bg);
    XSelectInput(dpy, c->frame, ExposureMask | ButtonPressMask | ButtonReleaseMask |
                 PointerMotionMask | SubstructureNotifyMask);
    /* set client border to 0 so frame borders handle all edge input */
    XSetWindowBorderWidth(dpy, w, 0);
    XReparentWindow(dpy, w, c->frame, settings.border_w, settings.border_w + settings.title_h);
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
    int fw, fh;

    if (!c || c->frame == None) return;
    focused_ = (focused == c);

    /* full frame dimensions (content area, no X border) */
    fw = c->w + 2 * settings.border_w;
    fh = c->h + settings.title_h + 2 * settings.border_w;

    /* fill entire frame with border color - this IS the single border */
    XSetForeground(dpy, gc, c_border);
    XFillRectangle(dpy, c->frame, gc, 0, 0, fw, fh);

    /* title bar bg - inset by settings.border_w so border shows on all sides */
    XSetForeground(dpy, gc, focused_ ? c_title_on : c_title_off);
    XFillRectangle(dpy, c->frame, gc, settings.border_w, settings.border_w, c->w, settings.title_h);

    /* title text */
    XSetForeground(dpy, gc, focused_ ? c_title_fg : c_black);
    pad = settings.border_w + settings.close_sz + 8;
    XDrawString(dpy, c->frame, gc, pad,
        settings.border_w + (settings.title_h - font->ascent) / 2 + font->ascent,
        c->title ? c->title : "(untitled)",
        (int)strlen(c->title ? c->title : "(untitled)"));

    /* buttons: _ □ X from left to right, right-aligned */
    by = settings.border_w + (settings.title_h - settings.close_sz) / 2;

    /* close button (rightmost) */
    bx = settings.border_w + c->w - settings.close_sz - 4;
    XSetForeground(dpy, gc, c_white);
    XFillRectangle(dpy, c->frame, gc, bx, by, settings.close_sz, settings.close_sz);
    XSetForeground(dpy, gc, c_black);
    XDrawString(dpy, c->frame, gc, bx + 3, by + font->ascent, "X", 1);

    /* maximize button */
    bx -= settings.close_sz + 2;
    XSetForeground(dpy, gc, c_white);
    XFillRectangle(dpy, c->frame, gc, bx, by, settings.close_sz, settings.close_sz);
    XSetForeground(dpy, gc, c_black);
    XDrawRectangle(dpy, c->frame, gc, bx + 3, by + 3,
        settings.close_sz - 7, settings.close_sz - 7);
    XFillRectangle(dpy, c->frame, gc, bx + 3, by + 3,
        settings.close_sz - 7, 2);

    /* minimize button */
    bx -= settings.close_sz + 2;
    XSetForeground(dpy, gc, c_white);
    XFillRectangle(dpy, c->frame, gc, bx, by, settings.close_sz, settings.close_sz);
    XSetForeground(dpy, gc, c_black);
    XDrawString(dpy, c->frame, gc, bx + 2, by + font->ascent, "_", 1);
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

/* close / maximize / minimize */

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
            c->ow + 2 * settings.border_w, c->oh + settings.title_h + 2 * settings.border_w);
        XMoveResizeWindow(dpy, c->win, settings.border_w, settings.title_h + settings.border_w, c->ow, c->oh);
        c->x = c->ox; c->y = c->oy; c->w = c->ow; c->h = c->oh;
        c->maximized = 0;
    } else {
        c->ox = c->x; c->oy = c->y; c->ow = c->w; c->oh = c->h;
        c->w = sw - 2 * settings.border_w;
        c->h = sh - settings.panel_h - settings.title_h - 2 * settings.border_w;
        c->x = 0; c->y = 0;
        XMoveResizeWindow(dpy, c->frame, 0, 0, sw, sh - settings.panel_h);
        XMoveResizeWindow(dpy, c->win, settings.border_w, settings.title_h + settings.border_w, c->w, c->h);
        c->maximized = 1;
    }
    frame_draw(c);
}

void client_min(Client *c) {
    if (!c) return;
    XUnmapWindow(dpy, c->frame);
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
    int edge;

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

    /* check for resize edge/corner anywhere in the frame */
    edge = wm_edge_detect(c, e->x, e->y);
    if (edge) {
        resizing = 1;
        drag_c = c;
        resize_edge = edge;
        resize_start_x = e->x_root;
        resize_start_y = e->y_root;
        resize_orig_x = c->x;
        resize_orig_y = c->y;
        resize_orig_w = c->w;
        resize_orig_h = c->h;
        XGrabPointer(dpy, c->frame, True,
            ButtonReleaseMask | PointerMotionMask,
            GrabModeAsync, GrabModeAsync,
            wm_edge_cursor(edge), None, CurrentTime);
        return;
    }

    if (e->y < settings.title_h) {
        int bx, by;
        by = (settings.title_h - settings.close_sz) / 2;

        /* close button (rightmost) */
        bx = c->w - settings.close_sz - 4;
        if (e->x >= bx && e->x < bx + settings.close_sz &&
            e->y >= by && e->y < by + settings.close_sz) {
            client_close(c);
            return;
        }
        /* maximize button */
        bx -= settings.close_sz + 2;
        if (e->x >= bx && e->x < bx + settings.close_sz &&
            e->y >= by && e->y < by + settings.close_sz) {
            client_max(c);
            return;
        }
        /* minimize button */
        bx -= settings.close_sz + 2;
        if (e->x >= bx && e->x < bx + settings.close_sz &&
            e->y >= by && e->y < by + settings.close_sz) {
            client_min(c);
            return;
        }

        /* title bar click starts move drag */
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
    int nw, nh, nx, ny;
    Window rw, cr;
    int rx, ry, wx, wy;
    unsigned int mask;
    Client *c;
    int edge;

    if (resizing && drag_c) {
        c = drag_c;
        nw = resize_orig_w;
        nh = resize_orig_h;
        nx = resize_orig_x;
        ny = resize_orig_y;
        if (resize_edge & EDGE_RIGHT)
            nw = resize_orig_w + (e->x_root - resize_start_x);
        if (resize_edge & EDGE_BOT)
            nh = resize_orig_h + (e->y_root - resize_start_y);
        if (resize_edge & EDGE_LEFT) {
            nw = resize_orig_w - (e->x_root - resize_start_x);
            nx = resize_orig_x + (e->x_root - resize_start_x);
        }
        if (resize_edge & EDGE_TOP) {
            nh = resize_orig_h - (e->y_root - resize_start_y);
            ny = resize_orig_y + (e->y_root - resize_start_y);
        }
        if (nw < settings.min_w) { nw = settings.min_w; if (resize_edge & EDGE_LEFT) nx = resize_orig_x + resize_orig_w - settings.min_w; }
        if (nh < settings.min_h) { nh = settings.min_h; if (resize_edge & EDGE_TOP) ny = resize_orig_y + resize_orig_h - settings.min_h; }
        c->w = nw; c->h = nh; c->x = nx; c->y = ny;
        XMoveResizeWindow(dpy, c->frame, nx, ny,
            nw + 2 * settings.border_w, nh + settings.title_h + 2 * settings.border_w);
        XResizeWindow(dpy, c->win, nw, nh);
        frame_draw(c);
    } else if (dragging && drag_c) {
        drag_c->x = e->x_root - drag_ox;
        drag_c->y = e->y_root - drag_oy;
        XMoveWindow(dpy, drag_c->frame, drag_c->x, drag_c->y);
    }

    /* when button is held and pointer is near a client edge, start resize */
    if (!dragging && !resizing) {
        if (XQueryPointer(dpy, e->window, &rw, &cr, &rx, &ry, &wx, &wy, &mask)) {
            if (mask & Button1Mask) {
                c = client_by_frame(e->window);
                if (c) edge = wm_edge_detect(c, wx, wy);
                else {
                    c = client_by_win(e->window);
                    if (c) edge = wm_edge_from_client(c, wx, wy);
                    else edge = EDGE_NONE;
                }
                if (c && edge) {
                    resizing = 1;
                    drag_c = c;
                    resize_edge = edge;
                    resize_start_x = e->x_root;
                    resize_start_y = e->y_root;
                    resize_orig_x = c->x;
                    resize_orig_y = c->y;
                    resize_orig_w = c->w;
                    resize_orig_h = c->h;
                    XGrabPointer(dpy, c->frame, True,
                        ButtonReleaseMask | PointerMotionMask,
                        GrabModeAsync, GrabModeAsync,
                        wm_edge_cursor(edge), None, CurrentTime);
                    return;
                }
            }

            /* update cursor on hover (only when no grab is active) */
            c = client_by_frame(e->window);
            if (c) {
                edge = wm_edge_detect(c, wx, wy);
                if (edge) {
                    wm_set_frame_cursor(c, edge);
                } else if (wy >= settings.border_w && wy < settings.border_w + settings.title_h) {
                    /* in title bar - check if over a button */
                    int bx, by_;
                    by_ = settings.border_w + (settings.title_h - settings.close_sz) / 2;
                    if (wy >= by_ && wy < by_ + settings.close_sz) {
                        bx = settings.border_w + c->w - settings.close_sz - 4;
                        if (wx >= bx && wx < bx + settings.close_sz) {
                            XDefineCursor(dpy, c->frame, pointer_cursor);
                            return;
                        }
                        bx -= settings.close_sz + 2;
                        if (wx >= bx && wx < bx + settings.close_sz) {
                            XDefineCursor(dpy, c->frame, pointer_cursor);
                            return;
                        }
                        bx -= settings.close_sz + 2;
                        if (wx >= bx && wx < bx + settings.close_sz) {
                            XDefineCursor(dpy, c->frame, pointer_cursor);
                            return;
                        }
                    }
                    XUndefineCursor(dpy, c->frame);
                } else {
                    XUndefineCursor(dpy, c->frame);
                }
            } else {
                c = client_by_win(e->window);
                if (c) {
                    edge = wm_edge_from_client(c, wx, wy);
                    /* set cursor on the client window, not the frame */
                    if (edge == EDGE_NONE)
                        XUndefineCursor(dpy, c->win);
                    else {
                        Cursor cur = wm_edge_cursor(edge);
                        XDefineCursor(dpy, c->win, cur);
                        XFreeCursor(dpy, cur);
                    }
                }
            }
        }
    }

    /* drain motion events for smoothness */
    while (XPending(dpy)) {
        XPeekEvent(dpy, &dum);
        if (dum.type == MotionNotify) XNextEvent(dpy, &dum);
        else break;
    }
}

void wm_btnup(XButtonEvent *e) {
    (void)e;
    if (resizing) {
        resizing = 0;
        resize_edge = EDGE_NONE;
        drag_c = NULL;
        XUngrabPointer(dpy, CurrentTime);
    } else if (dragging) {
        dragging = 0;
        drag_c = NULL;
        XUngrabPointer(dpy, CurrentTime);
    }
}

void wm_expose(XExposeEvent *e) {
    Client *c;
    Window rw, cr;
    int rx, ry, wx, wy;
    unsigned int mask;
    int edge;

    if (e->count > 0) return;
    c = client_by_frame(e->window);
    if (!c) return;
    frame_draw(c);
    /* set cursor based on pointer position */
    if (XQueryPointer(dpy, c->frame, &rw, &cr, &rx, &ry, &wx, &wy, &mask)) {
        edge = wm_edge_detect(c, wx, wy);
        wm_set_frame_cursor(c, edge);
    }
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
