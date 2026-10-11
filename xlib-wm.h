/*
 * xlib_wm.c
 * Minimal Xlib Window Manager with frame decorations, move, resize, close, and menu.
 * Integrated fixes: reparenting, correct move offsets, resize, WM_DELETE handling, visible menu draw.
 * Compile: gcc -o xlib_wm xlib_wm.c -lX11
 */

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BORDER 2
#define TITLEBAR 20
#define CLOSE_W 18
#define CLOSE_H 16
#define RESIZE_ZONE 12
#define MIN_WIDTH 80
#define MIN_HEIGHT 40

#define MAX_MENU_ITEMS 32
#define MAX_CMD_LENGTH 256

typedef struct {
    char name[64];
    char command[MAX_CMD_LENGTH];
} MenuItem;

typedef struct {
    Window frame;   /* frame window (we draw decorations here) */
    Window client;  /* client window (reparented into frame) */
    int x, y;       /* frame top-left */
    int width, height; /* frame size */
    int mapped;
} Client;

/* Globals */
Display *dpy;
int scr;
Window root;
GC gc;
XFontStruct *fontinfo;

Client *clients = NULL;
int client_count = 0;

/* menu */
MenuItem menu_items[MAX_MENU_ITEMS];
int menu_item_count = 0;
Window menu_win = None;
int menu_visible = 0;
GC menu_gc = 0;
int menu_w = 240;
int menu_h = 20;

/* transient state for move/resize */
int moving = 0;
int resizing = 0;
int start_x, start_y;   /* pointer start (root coords) */
int orig_x, orig_y;     /* original frame pos */
int orig_w, orig_h;     /* original frame size */
Window action_window = None; /* frame being moved/resized */

Atom WM_DELETE_WINDOW;

/* helpers */
void die(const char *msg) { perror(msg); exit(1); }


Client *find_client_by_frame(Window frame) {
    for (int i = 0; i < client_count; ++i) {
        if (clients[i].frame == frame) return &clients[i];
    }
    return NULL;
}

Client *find_client_by_client(Window clientw) {
    for (int i = 0; i < client_count; ++i) {
        if (clients[i].client == clientw) return &clients[i];
    }
    return NULL;
}

void draw_frame(Client *c) {
    if (!c) return;
    Window f = c->frame;
    /* draw titlebar, close button, and resize hint */
    XClearWindow(dpy, f);
    XSetForeground(dpy, gc, BlackPixel(dpy, scr));
    XFillRectangle(dpy, f, gc, 0, 0, c->width, TITLEBAR);

    /* title text */
    char *title = get_window_title(dpy, c->client);
    if (!title) title = strdup("Untitled");
    XSetForeground(dpy, gc, WhitePixel(dpy, scr));
    XDrawString(dpy, f, gc, 6, 14, title, strlen(title));
    free(title);

    /* close button (top-right) */
    int cx = c->width - CLOSE_W - 4;
    XSetForeground(dpy, gc, 0x880000); /* dark red, may not map exactly */
    XFillRectangle(dpy, f, gc, cx, 2, CLOSE_W, CLOSE_H);
    XSetForeground(dpy, gc, WhitePixel(dpy, scr));
    XDrawLine(dpy, f, gc, cx+3, 5, cx + CLOSE_W - 4, 5 + CLOSE_H - 6);
    XDrawLine(dpy, f, gc, cx + CLOSE_W - 4, 5, cx+3, 5 + CLOSE_H - 6);

    /* resize corner */
    XSetForeground(dpy, gc, BlackPixel(dpy, scr));
    XDrawLine(dpy, f, gc, c->width - RESIZE_ZONE, c->height - 2, c->width - 2, c->height - RESIZE_ZONE);
}

void unmanage(Client *c) {
    if (!c) return;
    /* reparent client back to root at the frame's position (excluding titlebar) */
    XReparentWindow(dpy, c->client, root, c->x, c->y + TITLEBAR);
    XRemoveFromSaveSet(dpy, c->client);
    XMapWindow(dpy, c->client);
    XDestroyWindow(dpy, c->frame);
}

void remove_client_by_index(int idx) {
    if (idx < 0 || idx >= client_count) return;
    /* unmanage and remove */
    Client c = clients[idx];
    unmanage(&c);

    /* shift array */
    memmove(&clients[idx], &clients[idx+1], (client_count - idx - 1) * sizeof(Client));
    client_count--;
    if (client_count == 0) {
        free(clients); clients = NULL;
    } else {
        clients = realloc(clients, client_count * sizeof(Client));
    }
}

void handle_map_request(XMapRequestEvent *ev) {
    /* create frame and reparent client */
    XWindowAttributes a;
    if (!XGetWindowAttributes(dpy, ev->window, &a)) return;

    int fw = (a.width < MIN_WIDTH) ? MIN_WIDTH : a.width;
    int fh = a.height + TITLEBAR;

    Window frame = XCreateSimpleWindow(dpy, root, a.x, a.y, fw, fh, BORDER,
                                       BlackPixel(dpy, scr), WhitePixel(dpy, scr));

    XSelectInput(dpy, frame, ExposureMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask);

    XAddToSaveSet(dpy, ev->window);
    XReparentWindow(dpy, ev->window, frame, 0, TITLEBAR);
    XMapWindow(dpy, ev->window);
    XMapWindow(dpy, frame);

    clients = realloc(clients, (client_count + 1) * sizeof(Client));
    clients[client_count].frame = frame;
    clients[client_count].client = ev->window;
    clients[client_count].x = a.x;
    clients[client_count].y = a.y;
    clients[client_count].width = fw;
    clients[client_count].height = fh;
    clients[client_count].mapped = 1;
    client_count++;

    /* set WM_DELETE for client to allow polite close */
    XSetWMProtocols(dpy, ev->window, &WM_DELETE_WINDOW, 1);
}

void handle_configure_request(XConfigureRequestEvent *ev) {
    /* honor configure requests for clients (translate to their client window) */
    Client *c = find_client_by_client(ev->window);
    if (c) {
        /* resize client and frame */
        XWindowChanges wc;
        if (ev->value_mask & CWX) { c->x = ev->x; XMoveWindow(dpy, c->frame, c->x, c->y); }
        if (ev->value_mask & CWY) { c->y = ev->y; XMoveWindow(dpy, c->frame, c->x, c->y); }
        if (ev->value_mask & CWWidth || ev->value_mask & CWHeight) {
            int nw = (ev->width > MIN_WIDTH) ? ev->width : MIN_WIDTH;
            int nh = (ev->height > MIN_HEIGHT) ? ev->height : MIN_HEIGHT;
            c->width = nw;
            c->height = nh + TITLEBAR;
            XResizeWindow(dpy, c->frame, c->width, c->height);
            XResizeWindow(dpy, c->client, nw, nh);
        }
    } else {
        /* not managed: pass through */
        XWindowChanges wc;
        wc.x = ev->x; wc.y = ev->y; wc.width = ev->width; wc.height = ev->height;
        wc.border_width = ev->border_width; wc.sibling = ev->above; wc.stack_mode = ev->detail;
        XConfigureWindow(dpy, ev->window, ev->value_mask, &wc);
    }
}

void do_close(Client *c) {
    if (!c) return;
    /* try polite WM_DELETE */
    XEvent msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = ClientMessage;
    msg.xclient.window = c->client;
    msg.xclient.message_type = XInternAtom(dpy, "WM_PROTOCOLS", True);
    msg.xclient.format = 32;
    msg.xclient.data.l[0] = WM_DELETE_WINDOW;
    msg.xclient.data.l[1] = CurrentTime;
    XSendEvent(dpy, c->client, False, NoEventMask, &msg);
    XFlush(dpy);
    /* If client doesn't close, we'll catch DestroyNotify and unmanage then. */
}

void handle_button_press(XButtonEvent *ev) {
    /* If menu visible and click outside menu, hide it */
    if (menu_visible && ev->window != menu_win) {
        XUnmapWindow(dpy, menu_win); menu_visible = 0;
    }

    /* Right-click on root shows menu */
    if (ev->window == root && ev->button == Button3) {
        XMoveWindow(dpy, menu_win, ev->x_root, ev->y_root);
        XMapRaised(dpy, menu_win);
        menu_visible = 1;
        return;
    }

    /* Are we clicking a frame? */
    Client *c = find_client_by_frame(ev->window);
    if (!c) return;

    /* check close button */
    int cx = c->width - CLOSE_W - 4;
    if (ev->y < TITLEBAR && ev->x >= cx && ev->x <= cx + CLOSE_W) {
        do_close(c);
        return;
    }

    /* check resize zone (bottom-right) */
    if (ev->x >= c->width - RESIZE_ZONE && ev->y >= c->height - RESIZE_ZONE) {
        resizing = 1;
        action_window = c->frame;
        start_x = ev->x_root; start_y = ev->y_root;
        orig_w = c->width; orig_h = c->height;
        /* grab pointer so we get motion and release events */
        XGrabPointer(dpy, c->frame, True, PointerMotionMask|ButtonReleaseMask, GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
        return;
    }

    /* titlebar -> move */
    if (ev->y < TITLEBAR) {
        moving = 1;
        action_window = c->frame;
        start_x = ev->x_root; start_y = ev->y_root;
        orig_x = c->x; orig_y = c->y;
        XGrabPointer(dpy, c->frame, True, PointerMotionMask|ButtonReleaseMask, GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
        return;
    }
}

void handle_button_release(XButtonEvent *ev) {
    if (moving || resizing) {
        XUngrabPointer(dpy, CurrentTime);
        moving = resizing = 0;
        action_window = None;
    }
    /* menu click handling */
    if (ev->window == menu_win && menu_visible && ev->button == Button1) {
        int idx = ev->y / menu_h;
        if (idx >= 0 && idx < menu_item_count) {
            char cmd[512];
            snprintf(cmd, sizeof(cmd), "%s &", menu_items[idx].command);
            system(cmd);
        }
        XUnmapWindow(dpy, menu_win);
        menu_visible = 0;
    }
}

void handle_motion(XMotionEvent *ev) {
    if (moving && action_window) {
        Client *c = find_client_by_frame(action_window);
        if (!c) return;
        int nx = orig_x + (ev->x_root - start_x);
        int ny = orig_y + (ev->y_root - start_y);
        c->x = nx; c->y = ny;
        XMoveWindow(dpy, c->frame, c->x, c->y);
    } else if (resizing && action_window) {
        Client *c = find_client_by_frame(action_window);
        if (!c) return;
        int nw = orig_w + (ev->x_root - start_x);
        int nh = orig_h + (ev->y_root - start_y);
        if (nw < MIN_WIDTH) nw = MIN_WIDTH;
        if (nh < MIN_HEIGHT + TITLEBAR) nh = MIN_HEIGHT + TITLEBAR;
        c->width = nw; c->height = nh;
        XResizeWindow(dpy, c->frame, c->width, c->height);
        XResizeWindow(dpy, c->client, c->width, c->height - TITLEBAR);
    }
}

void handle_unmap(XUnmapEvent *ev) {
    /* client could have unmapped itself; unmanage when client == unmapped window */
    Client *c = find_client_by_client(ev->window);
    if (c) {
        /* find index */
        for (int i = 0; i < client_count; ++i) if (clients[i].client == ev->window) { remove_client_by_index(i); break; }
    }
}

void handle_destroy(XDestroyWindowEvent *ev) {
    Client *c = find_client_by_client(ev->window);
    if (c) {
        for (int i = 0; i < client_count; ++i) if (clients[i].client == ev->window) { remove_client_by_index(i); break; }
    } else {
        /* maybe frame destroyed directly? */
        for (int i = 0; i < client_count; ++i) if (clients[i].frame == ev->window) { remove_client_by_index(i); break; }
    }
}

void create_menu(void) {
    menu_win = XCreateSimpleWindow(dpy, root, 0, 0, menu_w, menu_h * (menu_item_count>0?menu_item_count:1), 1, BlackPixel(dpy,scr), WhitePixel(dpy,scr));
    XSelectInput(dpy, menu_win, ExposureMask | ButtonPressMask);
    menu_gc = XCreateGC(dpy, menu_win, 0, NULL);
    XSetForeground(dpy, menu_gc, BlackPixel(dpy, scr));
}

void handle_events(void) {
    XEvent ev;
    while (1) {
        XNextEvent(dpy, &ev);
        switch (ev.type) {
            case MapRequest: handle_map_request(&ev.xmaprequest); break;
            case ConfigureRequest: handle_configure_request(&ev.xconfigurerequest); break;
            case ButtonPress: handle_button_press(&ev.xbutton); break;
            case ButtonRelease: handle_button_release(&ev.xbutton); break;
            case MotionNotify: handle_motion(&ev.xmotion); break;
            case UnmapNotify: handle_unmap(&ev.xunmap); break;
            case DestroyNotify: handle_destroy(&ev.xdestroywindow); break;
            case Expose:
                if (ev.xexpose.window == menu_win) {
                    /* draw menu items */
                    for (int i = 0; i < menu_item_count; ++i) {
                        XDrawString(dpy, menu_win, menu_gc, 6, (i+1)*menu_h - 6, menu_items[i].name, strlen(menu_items[i].name));
                    }
                } else {
                    Client *c = find_client_by_frame(ev.xexpose.window);
                    if (c) draw_frame(c);
                }
                break;
            case ClientMessage:
                /* handle WM_DELETE from client to unmanage */
                if ((Atom)ev.xclient.data.l[0] == WM_DELETE_WINDOW) {
                    /* client requested close */
                    for (int i = 0; i < client_count; ++i) if (clients[i].client == ev.xclient.window) { remove_client_by_index(i); break; }
                }
                break;
            default: break;
        }
    }
}

void load_dummy_menu(void) {
    /* sample menu */
    strncpy(menu_items[0].name, "xterm", sizeof(menu_items[0].name));
    strncpy(menu_items[0].command, "xterm", sizeof(menu_items[0].command));
    strncpy(menu_items[1].name, "xclock", sizeof(menu_items[1].name));
    strncpy(menu_items[1].command, "xclock", sizeof(menu_items[1].command));
    menu_item_count = 2;
}



