#include "Lightmap.h"

#include <Sys/Json.h>

void Eng::Lightmap::Read(const JsObjectP &js_in, Lightmap &lm) {
    const JsNumber &js_res = js_in.at("res").as_num();
    lm.size[0] = lm.size[1] = int(js_res.val);
}

void Eng::Lightmap::Write(const Lightmap &lm, JsObjectP &js_out) { js_out.Insert("res", JsNumber{lm.size[0]}); }