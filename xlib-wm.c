/*
*  This program is free software: you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation, either version 3 of the License, or
*  (at your option) any later version.
*
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/********************************************************************
* Name: FastWM.
* Description: A simple X11 window manager using Xlib.
* Author: John Cartwright <>
* Created at: Mon May 19 10:44:03 AEST 2025
* Computer: buddhism
* System: Linux 5.14.0-503.40.1.el9_5.x86_64 on x86_64
*
* Copyright (c) 2025 John Cartwright  All rights reserved.
*
********************************************************************/

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include "xlib-windows.h"
#include "xlib-wm.h"

int main(int argc, char **argv) {
    dpy = XOpenDisplay(NULL);
    if (!dpy) die("XOpenDisplay");
    scr = DefaultScreen(dpy);
    root = RootWindow(dpy, scr);

    /* basic resources */
    gc = XCreateGC(dpy, root, 0, NULL);
    fontinfo = XLoadQueryFont(dpy, "fixed");
    if (fontinfo) XSetFont(dpy, gc, fontinfo->fid);

    WM_DELETE_WINDOW = XInternAtom(dpy, "WM_DELETE_WINDOW", False);

    /* set root background (optional) - don't free pixmap until exit if used) */

    /* select root events */
    XSelectInput(dpy, root, SubstructureRedirectMask | SubstructureNotifyMask | ButtonPressMask);

    /* sample menu */
    load_dummy_menu();
    create_menu();

    handle_events();

    XCloseDisplay(dpy);
    return 0;
}

