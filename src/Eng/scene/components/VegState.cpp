#include "VegState.h"

#include <Sys/Json.h>

void Eng::VegState::Read(const Sys::JsObjectP &js_in, VegState &vs) {
    if (const size_t movement_scale_ndx = js_in.IndexOf("movement_scale"); movement_scale_ndx < js_in.Size()) {
        vs.movement_scale = float(js_in[movement_scale_ndx].second.as_num().val);
    } else {
        vs.movement_scale = 1.0f;
    }

    if (const size_t tree_mode_ndx = js_in.IndexOf("tree_mode"); tree_mode_ndx < js_in.Size()) {
        vs.tree_mode = float(js_in[tree_mode_ndx].second.as_num().val);
    } else {
        vs.tree_mode = 1.0f;
    }

    if (const size_t bend_scale_ndx = js_in.IndexOf("bend_scale"); bend_scale_ndx < js_in.Size()) {
        vs.bend_scale = float(js_in[bend_scale_ndx].second.as_num().val);
    } else {
        vs.bend_scale = 1.0f;
    }

    if (const size_t stretch_ndx = js_in.IndexOf("stretch"); stretch_ndx < js_in.Size()) {
        vs.stretch = float(js_in[stretch_ndx].second.as_num().val);
    } else {
        vs.stretch = 0.0f;
    }
}

void Eng::VegState::Write(const VegState &vs, Sys::JsObjectP &js_out) {
    if (vs.movement_scale != 1.0f) {
        js_out["movement_scale"] = Sys::JsNumber{vs.movement_scale};
    }

    if (vs.tree_mode != 1.0f) {
        js_out["tree_mode"] = Sys::JsNumber{vs.tree_mode};
    }

    if (vs.bend_scale != 1.0f) {
        js_out["bend_scale"] = Sys::JsNumber{vs.bend_scale};
    }

    if (vs.stretch != 0.0f) {
        js_out["stretch"] = Sys::JsNumber{vs.stretch};
    }
}
