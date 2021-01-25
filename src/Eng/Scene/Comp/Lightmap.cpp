#include "Lightmap.h"

#include <Sys/Json.h>

void Lightmap::Read(const JsObject &js_in, Lightmap &lm) {
    const JsNumber &js_res = js_in.at("res").as_num();
    lm.size[0] = lm.size[1] = int(js_res.val);
}

void Lightmap::Write(const Lightmap &lm, JsObject &js_out) {
    js_out.Push("res", JsNumber{ (double)lm.size[0] });
}