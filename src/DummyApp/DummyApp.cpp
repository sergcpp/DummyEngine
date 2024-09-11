#include "DummyApp.h"

#include <Eng/ViewerBase.h>
#include <Eng/input/InputManager.h>
#include <Sys/Time_.h>

const char *DummyApp::Version() const { return "v0.1.0-unknown-commit"; }


void DummyApp::Frame() {
    if (!minimized_) {
        viewer_->Frame();
    }
}

void DummyApp::Resize(const int w, const int h) {
    minimized_ = (w == 0 || h == 0);
    if (viewer_ && !minimized_) {
        viewer_->Resize(w, h);
    }
}

void DummyApp::AddEvent(const Eng::eInputEvent type, const uint32_t key_code, const float x, const float y,
                        const float dx, const float dy) {
    if (!input_manager_) {
        return;
    }

    Eng::input_event_t evt;
    evt.type = type;
    evt.key_code = key_code;
    evt.point[0] = x;
    evt.point[1] = viewer_->height - 1 - y;
    evt.move[0] = dx;
    evt.move[1] = -dy;
    evt.time_stamp = Sys::GetTimeUs();

    input_manager_->AddRawInputEvent(evt);
}