#include "WindowRect.h"

#include <cstdio>
#include <cstdlib>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

void Sys::CenterWindowRect(int &x, int &y, int &w, int &h) {
    RECT rect;
    rect.left = rect.top = 0;
    rect.right = w;
    rect.bottom = h;

    ::AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW | WS_VISIBLE, false);

    const int display_width = ::GetSystemMetrics(SM_CXSCREEN);
    const int display_height = ::GetSystemMetrics(SM_CYSCREEN);

    x = (display_width / 2) - (w / 2);
    y = (display_height / 2) - (h / 2);
    w = int(rect.right - rect.left);
    h = int(rect.bottom - rect.top);
}

#elif defined(__linux__) && !defined(__ANDROID__)

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xresource.h>

double GetMonitorDPI(Display *dpy) {
    char *resourceString = XResourceManagerString(dpy);
    XrmDatabase db;
    XrmValue value;
    char *type = nullptr;
    double dpi = 0.0;

    XrmInitialize(); /* Need to initialize the DB before calling Xrm* functions */

    db = XrmGetStringDatabase(resourceString);

    if (resourceString) {
        printf("Entire DB:\n%s\n", resourceString);
        if (XrmGetResource(db, "Xft.dpi", "String", &type, &value) == True) {
            if (value.addr) {
                dpi = atof(value.addr);
            }
        }
    }

    return dpi;
}

void Sys::CenterWindowRect(int &x, int &y, int &w, int &h) {
    Display *dpy = XOpenDisplay(nullptr);
    if (!dpy) {
        return;
    }

    Window window = DefaultRootWindow(dpy);
    if (!window) {
        return;
    }

    XWindowAttributes xw_attrs;
    XGetWindowAttributes(dpy, window, &xw_attrs);

    const double dpi = GetMonitorDPI(dpy);
    if (dpi > 0.0) {
        const double dpmm = dpi / 25.4;

        const double w_mm = double(w) / dpmm;
        const double h_mm = double(h) / dpmm;

        const double x_mm = double(xw_attrs.screen->mwidth) / 2.0 - (w_mm / 2.0);
        const double y_mm = double(xw_attrs.screen->mheight) / 2.0 - (h_mm / 2.0);

        x = int(x_mm * dpmm);
        y = int(y_mm * dpmm);
    } else {
        x = (xw_attrs.width / 2) - (w / 2);
        y = (xw_attrs.height / 2) - (h / 2);
    }
}

#elif defined(__APPLE__)

#include <CoreGraphics/CGDisplayConfiguration.h>

void Sys::CenterWindowRect(int &x, int &y, int &w, int &h) {
    auto mainDisplayId = CGMainDisplayID();

    const int display_width = int(CGDisplayPixelsWide(mainDisplayId));
    const int display_height = int(CGDisplayPixelsHigh(mainDisplayId));

    x = (display_width / 2) - (w / 2);
    y = (display_height / 2) - (h / 2);
}

#else

void Sys::CenterWindowRect(int &x, int &y, int &w, int &h) {}

#endif
