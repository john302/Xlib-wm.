/* Emacs style mode select: -*- linux-c++ -*- */
#ifndef XLIB_WM_H_
#define XLIB_WM_H_

#define MAX_MENU_ITEMS 32
#define MAX_CMD_LENGTH 256

typedef struct {
    char name[64];
    char command[MAX_CMD_LENGTH];
} MenuItem;

// Update Client struct to store client window
typedef struct {
    Window window;      // Frame window
    Window client;      // Client window
    int x, y;
    int width, height;
    int is_mapped;
} Client;


// Global variables
Display *display;
Window root;
int screen;
int menu_item_count = 0;
MenuItem menu_items[MAX_MENU_ITEMS];
Client *clients = NULL;
int client_count = 0;
Window menu_window = None;
int menu_height = 20;
int menu_width = 400;
int menu_visible = 0;
int dragging = 0;
int drag_start_x, drag_start_y;
Window drag_window;
int resizing = 0;
int resize_start_x, resize_start_y;
Window resize_window;
GC menu_gc = NULL;
int i;
XEvent e;

// Function prototypes
void init_x(void);
void cleanup_x(void);
void handle_events(void);
void create_root_menu(void);
void load_config(void);
void draw_frame();
void manage_window(Window w);
void unmanage_window(Window w);
void handle_button_press(XButtonEvent *e);
void handle_key_press(XKeyEvent *e);
void handle_map_request(XMapRequestEvent *e);
void handle_unmap_notify(XUnmapEvent *e);
void handle_destroy_notify(XDestroyWindowEvent *e);
void handle_configure_request(XConfigureRequestEvent *e);
void show_menu(int x, int y);
void handle_motion_notify(XMotionEvent *e);
void handle_button_release(XButtonEvent *e);

// Add root_weave bitmap data (example, you can find this in X11 bitmap files)
#define root_weave_width 16
#define root_weave_height 16
static unsigned char root_weave_bits[] = {
   0x88, 0x88, 0x00, 0x22, 0x22, 0x00, 0x88, 0x88,
   0x00, 0x22, 0x22, 0x00, 0x88, 0x88, 0x00, 0x22,
   0x22, 0x00, 0x88, 0x88, 0x00, 0x22, 0x22, 0x00,
   0x88, 0x88, 0x00, 0x22, 0x22, 0x00, 0x88, 0x88};

void init_x(void) {
    display = XOpenDisplay(NULL);
    if (!display) {
        fprintf(stderr, "Cannot open display\n");
        exit(1);
    }

    screen = DefaultScreen(display);
    root = RootWindow(display, screen);

    // Set root window background to root_weave
    Pixmap bg_pixmap = XCreatePixmapFromBitmapData(display, root,
        (char *)root_weave_bits, root_weave_width, root_weave_height,
        WhitePixel(display, screen), BlackPixel(display, screen),
        DefaultDepth(display, screen));
    XSetWindowBackgroundPixmap(display, root, bg_pixmap);
    XFreePixmap(display, bg_pixmap);  // Free after setting
    XClearWindow(display, root);  // Refresh root window to apply background

    // Select events we want to handle
    XSelectInput(display, root,
        SubstructureRedirectMask | SubstructureNotifyMask |
        ButtonPressMask | KeyPressMask);
}

void cleanup_x(void) {
    if (display) {
        XCloseDisplay(display);
    }
    if (clients) {
        free(clients);
    }
}

/* New function to close the window properly.*/

void close_window(Window frame) {
    printf("close_window: Attempting to close frame %lu\n", frame);
    for (i = 0; i < client_count; i++) {
        if (clients[i].window == frame) {
            printf("close_window: Found frame %lu, client %lu at index %d\n", frame, clients[i].client, i);
            //Window root_ret, parent_ret;
            //Window *children;
            //unsigned int n;
            /*
            if (XQueryTree(display, frame, &root_ret, &parent_ret, &children, &n)) {
                for (unsigned int j = 0; j < n; j++) {
                    printf("close_window: Destroying child %lu\n", children[j]);
                    XDestroyWindow(display, children[j]);
                }
                XFree(children);
            } */
            XDestroyWindow(display, frame);
            //printf("close_window: Frame %lu destroyed\n", frame);
            unmanage_window(frame);
            //printf("close_window: unmanage_window called for frame %lu\n", frame);
            break;
        }
    }
    if (i == client_count) {
        printf("close_window: Frame %lu not found in clients array\n", frame);
    }
}

// New function to draw menu
void draw_menu(Window w) {
    if (w != menu_window) return;
    for (int i = 0; i < menu_item_count; i++) {
        XDrawString(display, menu_window, menu_gc, 10, (i + 1) * menu_height - 5,
                    menu_items[i].name, strlen(menu_items[i].name));
    }
}

void handle_events(void) {

    while (1) {
        XNextEvent(display, &e);
        printf("handle_events: Received event type %d for window %lu\n", e.type, e.xany.window);
        switch (e.type) {
            case ButtonPress:
                handle_button_press(&e.xbutton);
                break;
            case ButtonRelease:
                handle_button_release(&e.xbutton);
                break;
            case MotionNotify:
                handle_motion_notify(&e.xmotion);
                break;
            case KeyPress:
                handle_key_press(&e.xkey);
                break;
            case MapRequest:
                handle_map_request(&e.xmaprequest);
                break;
            case Expose:
                if (e.xexpose.window == menu_window) {
                    draw_menu(e.xexpose.window);
                } else {
                    draw_frame(e.xexpose.window);
                }
                break;
            case UnmapNotify:
                handle_unmap_notify(&e.xunmap);
                break;
            case DestroyNotify:
                handle_destroy_notify(&e.xdestroywindow);
                break;
            case ConfigureRequest:
                handle_configure_request(&e.xconfigurerequest);
                break;
            default:
                printf("handle_events: Unhandled event type %d\n", e.type);
                break;
        }
    }
}

void load_config(void) {
    FILE *config = fopen("/etc/wm.conf", "r");
    if (!config) {
        fprintf(stderr, "Could not open config file\n");
        return;
    }

    char line[512];
    while (fgets(line, sizeof(line), config) && menu_item_count < MAX_MENU_ITEMS) {
        char name[64], cmd[MAX_CMD_LENGTH];
        if (sscanf(line, "%63[^:]:%255[^\n]", name, cmd) == 2) {
            strncpy(menu_items[menu_item_count].name, name, sizeof(menu_items[menu_item_count].name) - 1);
            strncpy(menu_items[menu_item_count].command, cmd, sizeof(menu_items[menu_item_count].command) - 1);
            menu_item_count++;
        }
    }
    fclose(config);
}

// Modify create_root_menu
void create_root_menu(void) {
    XSetWindowAttributes attrs;
    attrs.override_redirect = True;
    attrs.background_pixel = 0x222222;  // Dark background
    attrs.border_pixel = 0x000000;
    attrs.event_mask = ExposureMask | ButtonPressMask;  // Add ExposureMask and ButtonPressMask

    menu_window = XCreateWindow(display, root,
        0, 0, menu_width, menu_height * menu_item_count,
        1, CopyFromParent, InputOutput, CopyFromParent,
        CWOverrideRedirect | CWBackPixel | CWBorderPixel | CWEventMask,
        &attrs);

    // Create GC for menu
    menu_gc = XCreateGC(display, menu_window, 0, NULL);
    XSetForeground(display, menu_gc, WhitePixel(display, screen));  // White text
}


void show_menu(int x, int y) {
    if (menu_visible) {
        XUnmapWindow(display, menu_window);
        menu_visible = 0;
        return;
    }

    XMoveWindow(display, menu_window, x, y);
    XMapWindow(display, menu_window);
    XRaiseWindow(display, menu_window);
    XSetInputFocus(display, menu_window, RevertToParent, CurrentTime);  // Add focus
    menu_visible = 1;
}

void handle_motion_notify(XMotionEvent *e) {
    if (dragging) {
        int new_x = e->x_root - drag_start_x;
        int new_y = e->y_root - drag_start_y;
        XMoveWindow(display, drag_window, new_x, new_y);
    }
    else if (resizing) {
        int new_width = e->x_root - resize_start_x;
        int new_height = e->y_root - resize_start_y;
        if (new_width > 50 && new_height > 50) {  // Minimum size
            XResizeWindow(display, resize_window, new_width, new_height);
        }
    }
}

void handle_button_press(XButtonEvent *e) {
    // Existing client handling
    for (int i = 0; i < client_count; i++) {
        if (clients[i].window == e->window) {
            if (e->y < 20 && e->x > (clients[i].width - 20)) {
                printf("handle_button_press: Close button clicked on frame %lu\n", e->window);
                close_window(e->window);
                return;
            }
            if (e->y < 20) {
                dragging = 1;
                drag_start_x = e->x_root - clients[i].x;
                drag_start_y = e->y_root - clients[i].y;
                drag_window = e->window;
                return;
            }
            if (e->x > (clients[i].width - 10) && e->y > (clients[i].height - 10)) {
                resizing = 1;
                resize_start_x = e->x_root - clients[i].width;
                resize_start_y = e->y_root - clients[i].height;
                resize_window = e->window;
                return;
            }
        }
    }

    // Menu and desktop clicks (unchanged)
    if (e->button == Button3) {
        printf("Right-click at (%d, %d), showing menu\n", e->x_root, e->y_root);
        show_menu(e->x_root, e->y_root);
    } else if (e->button == Button1) {
        if (e->window == menu_window && menu_visible) {
            int menu_y = e->y;
            int item_index = menu_y / menu_height;
            printf("Left-click on menu at y=%d, item_index=%d, menu_visible=%d\n", menu_y, item_index, menu_visible);

            if (item_index >= 0 && item_index < menu_item_count) {
                char cmd[MAX_CMD_LENGTH + 10];
                snprintf(cmd, sizeof(cmd), "%s &", menu_items[item_index].command);
                printf("Executing command: %s\n", cmd);

                if (strcmp(menu_items[item_index].name, "Logout") == 0) {
                    cleanup_x();
                    system(cmd);
                    exit(0);
                } else {
                    int ret = system(cmd);
                    if (ret != 0) {
                        fprintf(stderr, "Command failed: %s (return code %d)\n", cmd, ret);
                    }
                }

                XUnmapWindow(display, menu_window);
                menu_visible = 0;
            }
        } else if (e->window == root) {
            printf("Left-click on desktop, opening xterm\n");
            system("xterm &");
        }
    }
}

void handle_button_release(XButtonEvent *e) {
    dragging = 0;
    resizing = 0;
}

void handle_key_press(XKeyEvent *e) {
    if (e->keycode == XKeysymToKeycode(display, XK_F1)) {
        show_menu(0, 0);
    }
}

void draw_frame(Window frame) {
    GC gc = XCreateGC(display, frame, 0, NULL);
    XSetForeground(display, gc, BlackPixel(display, screen));

    // Get frame dimensions
    XWindowAttributes attrs;
    XGetWindowAttributes(display, frame, &attrs);
    int frame_width = attrs.width;

    // Draw title bar (full width of frame)
    XDrawRectangle(display, frame, gc, 0, 0, frame_width, 20);

    // Find client window for this frame
    char *title = NULL;
    for (int i = 0; i < client_count; i++) {
        if (clients[i].window == frame) {
            title = get_window_title(display, clients[i].client);
            break;
        }
    }

    // Draw title, no fixed length limit but truncate if too long for frame
    if (!title) title = strdup("Untitled");
    int title_len = strlen(title);
    int max_len = (frame_width - 30) / XTextWidth(XQueryFont(display, XGContextFromGC(gc)), "a", 1);
    if (title_len > max_len) title_len = max_len;
    XDrawString(display, frame, gc, 5, 15, title, title_len);
    free(title);

    // Draw close button (adjusted for wider frame)
    XDrawRectangle(display, frame, gc, frame_width - 20, 2, 16, 16);
    XDrawLine(display, frame, gc, frame_width - 18, 4, frame_width - 4, 18);
    XDrawLine(display, frame, gc, frame_width - 4, 4, frame_width - 18, 18);

    XFreeGC(display, gc);
}

// Update manage_window
void manage_window(Window w) {
    XWindowAttributes xwa;
    XGetWindowAttributes(display, w, &xwa);

    // Create frame window with wider title bar
    int border = 2;
    int title_height = 20;
    int frame_width = xwa.width > menu_width ? xwa.width : menu_width;  // Use client width or menu_width, whichever is larger

    Window frame = XCreateSimpleWindow(display, root,
        xwa.x, xwa.y,
        frame_width,
        xwa.height + title_height,
        border,
        BlackPixel(display, screen),
        WhitePixel(display, screen));

    // Select events on frame for input
    XSelectInput(display, frame, SubstructureRedirectMask | SubstructureNotifyMask |
                 ExposureMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask);

    // Reparent client into frame
    XAddToSaveSet(display, w);
    XReparentWindow(display, w, frame, 0, title_height);

    // Map both
    XMapWindow(display, w);
    XMapWindow(display, frame);

    // Track client
    clients = realloc(clients, (client_count + 1) * sizeof(Client));
    clients[client_count].window = frame;
    clients[client_count].client = w;
    clients[client_count].x = xwa.x;
    clients[client_count].y = xwa.y;
    clients[client_count].width = frame_width;  // Update width to match frame
    clients[client_count].height = xwa.height + title_height;
    clients[client_count].is_mapped = 1;
    client_count++;
}

void unmanage_window(Window frame) {
    printf("unmanage_window: Attempting to unmanage frame %lu, client_count %d\n", frame, client_count);
    if (client_count == 0) {
        printf("unmanage_window: No clients to unmanage\n");
        return;
    }
    for (int i = 0; i < client_count; i++) {
        if (clients[i].window == frame) {
            printf("unmanage_window: Found frame %lu, client %lu at index %d\n", frame, clients[i].client, i);
            Window client = clients[i].client;
            Window root_ret, parent_ret;
            Window *children;
            unsigned int n;
            if (XQueryTree(display, frame, &root_ret, &parent_ret, &children, &n) && n > 0) {
                printf("unmanage_window: Reparenting client %lu to root\n", client);
                XReparentWindow(display, client, root, clients[i].x, clients[i].y - 20);
                XRemoveFromSaveSet(display, client);
                XFree(children);
            } else {
                printf("unmanage_window: XQueryTree failed or no children for frame %lu\n", frame);
            }

            // Destroy the frame window
            printf("unmanage_window: Destroying frame %lu\n", frame);
            XDestroyWindow(display, frame);

            // Remove from clients array
            memmove(&clients[i], &clients[i + 1], (client_count - i - 1) * sizeof(Client));
            client_count--;
            clients = realloc(clients, client_count * sizeof(Client));
            if (clients == NULL && client_count > 0) {
                printf("unmanage_window: realloc failed, client_count %d\n", client_count);
                exit(1);  // Handle memory failure
            }
            printf("unmanage_window: Frame %lu unmanaged, new client_count %d\n", frame, client_count);
            break;
        }
    }
    if (i == client_count) {
        printf("unmanage_window: Frame %lu not found in clients array\n", frame);
    }
}

void handle_map_request(XMapRequestEvent *e) {
    manage_window(e->window);
}

void handle_unmap_notify(XUnmapEvent *e) {
    if (e->event == root && e->window != root) {
        for (int i = 0; i < client_count; i++) {
            if (clients[i].client == e->window) {  // Check client window
                unmanage_window(clients[i].window);
                break;
            }
        }
    }
}

void handle_destroy_notify(XDestroyWindowEvent *e) {
    for (int i = 0; i < client_count; i++) {
        if (clients[i].client == e->window) {  // Check client window
            unmanage_window(clients[i].window);
            break;
        }
    }
}

void handle_configure_request(XConfigureRequestEvent *e) {
    XWindowChanges wc;
    wc.x = e->x;
    wc.y = e->y;
    wc.width = e->width;
    wc.height = e->height;
    wc.border_width = e->border_width;
    wc.sibling = e->above;
    wc.stack_mode = e->detail;

    XConfigureWindow(display, e->window, e->value_mask, &wc);
}

#endif
