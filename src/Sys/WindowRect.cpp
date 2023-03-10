#include "WindowRect.h"

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

#include <X11/Xlib.h>

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

    x = (xw_attrs.width / 2) - (w / 2);
    y = (xw_attrs.height / 2) - (h / 2);
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
