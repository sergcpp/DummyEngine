#include "LightProbe.h"

#include <Sys/Json.h>

void LightProbe::Read(const JsObject &js_in, LightProbe &pr) {
    if (js_in.Has("offset")) {
        const JsArray &js_offset = (const JsArray &)js_in.at("offset");

        pr.offset = { (float)((const JsNumber &)js_offset.at(0)).val,
                      (float)((const JsNumber &)js_offset.at(1)).val,
                      (float)((const JsNumber &)js_offset.at(2)).val };
    }

    if (js_in.Has("radius")) {
        const JsNumber &js_radius = (const JsNumber &)js_in.at("radius");

        pr.radius = (float)js_radius.val;
    }
}

void LightProbe::Write(const LightProbe &pr, JsObject &js_out) {

}