#include "Lightmap.h"

void Lightmap::Read(const JsObject &js_in, Lightmap &lm) {
    const JsNumber &js_res = (const JsNumber &)js_in.at("res");

    lm.size[0] = lm.size[1] = (int)js_res.val;
}