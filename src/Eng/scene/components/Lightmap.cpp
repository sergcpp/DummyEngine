#include "Lightmap.h"

#include <Sys/Json.h>

void Eng::Lightmap::Read(const Sys::JsObjectP &js_in, Lightmap &lm) {
    const Sys::JsNumber &js_res = js_in.at("res").as_num();
    lm.size[0] = lm.size[1] = int(js_res.val);
}

void Eng::Lightmap::Write(const Lightmap &lm, Sys::JsObjectP &js_out) {
    js_out.Insert("res", Sys::JsNumber{lm.size[0]});
}