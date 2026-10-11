/* Emacs style mode select: -*- linux-c++ -*- */
#ifndef XLIB_WINDOWS_H_
#define XLIB_WINDOWS_H_

// New function to get window title
char *get_window_title(Display *display, Window client) {
    Atom net_wm_name = XInternAtom(display, "_NET_WM_NAME", False);
    Atom utf8_string = XInternAtom(display, "UTF8_STRING", False);
    Atom wm_name = XInternAtom(display, "WM_NAME", False);
    char *title = NULL;
    XTextProperty prop;
    unsigned char *data = NULL;
    int n;
    Atom type;
    int format;

    // Try _NET_WM_NAME first
    if (XGetWindowProperty(display, client, net_wm_name, 0, 1024, False,
                           utf8_string, &type, &format, &n, &n, &data) == Success && data) {
        title = strdup((char *)data);
        XFree(data);
        return title;
    }

    // Fallback to WM_NAME
    if (XGetTextProperty(display, client, &prop, wm_name) && prop.nitems) {
        title = strdup((char *)prop.value);
        XFree(prop.value);
        return title;
    }

    // Default if no title found
    return strdup("Untitled");
}

#endif
