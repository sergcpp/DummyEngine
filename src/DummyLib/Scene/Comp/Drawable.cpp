#include "Drawable.h"

void Drawable::Read(const JsObject &js_in, Drawable &dr) {
    dr.flags = DrVisibleToShadow;

    if (js_in.Has("visible_to_shadow")) {
        auto v = (JsLiteral)js_in.at("visible_to_shadow");
        if (v.val == JS_FALSE) {
            dr.flags &= ~DrVisibleToShadow;
        }
    }
}