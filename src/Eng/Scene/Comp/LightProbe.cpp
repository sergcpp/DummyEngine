#include "LightProbe.h"

#include <Sys/Json.h>

void LightProbe::Read(const JsObject &js_in, LightProbe &pr) {
    if (js_in.Has("offset")) {
        const JsArray &js_offset = (const JsArray &)js_in.at("offset");

        pr.offset = Ren::Vec3f{
            (float)((const JsNumber &)js_offset.at(0)).val,
            (float)((const JsNumber &)js_offset.at(1)).val,
            (float)((const JsNumber &)js_offset.at(2)).val
        };
    }

    if (js_in.Has("radius")) {
        const JsNumber &js_radius = (const JsNumber &)js_in.at("radius");

        pr.radius = (float)js_radius.val;
    }

    if (js_in.Has("sh_coeffs")) {
        const JsArray &js_sh_coeffs = (const JsArray &)js_in.at("sh_coeffs");

        for (int i = 0; i < 4; i++) {
            const auto &js_sh_coeff = (const JsArray &)js_sh_coeffs.at(i);

            pr.sh_coeffs[i] = Ren::Vec3f{
                (float)((const JsNumber &)js_sh_coeff.at(0)),
                (float)((const JsNumber &)js_sh_coeff.at(1)),
                (float)((const JsNumber &)js_sh_coeff.at(2))
            };
        }
    }
}

void LightProbe::Write(const LightProbe &pr, JsObject &js_out) {
    {   // write offset
        JsArray js_offset;

        js_offset.Push(JsNumber((double)pr.offset[0]));
        js_offset.Push(JsNumber((double)pr.offset[1]));
        js_offset.Push(JsNumber((double)pr.offset[2]));

        js_out.Push("offset", std::move(js_offset));
    }

    {   // write radius
        js_out.Push("radius", JsNumber((double)pr.radius));
    }

    {   // write sh coefficients
        JsArray js_coeffs;

        for (int i = 0; i < 4; i++) {
            JsArray js_coeff;

            js_coeff.Push(JsNumber((double)pr.sh_coeffs[i][0]));
            js_coeff.Push(JsNumber((double)pr.sh_coeffs[i][1]));
            js_coeff.Push(JsNumber((double)pr.sh_coeffs[i][2]));

            js_coeffs.Push(std::move(js_coeff));
        }

        js_out.Push("sh_coeffs", std::move(js_coeffs));
    }
}