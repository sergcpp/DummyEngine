#include "LightProbe.h"

#include <Sys/Json.h>

void Eng::LightProbe::Read(const Sys::JsObjectP &js_in, LightProbe &pr) {
    if (js_in.Has("offset")) {
        const Sys::JsArrayP &js_offset = js_in.at("offset").as_arr();

        pr.offset = Ren::Vec3f{float(js_offset.at(0).as_num().val),
                               float(js_offset.at(1).as_num().val),
                               float(js_offset.at(2).as_num().val)};
    }

    if (js_in.Has("radius")) {
        const Sys::JsNumber &js_radius = js_in.at("radius").as_num();
        pr.radius = float(js_radius.val);
    }

    if (js_in.Has("sh_coeffs")) {
        const Sys::JsArrayP &js_sh_coeffs = js_in.at("sh_coeffs").as_arr();

        for (int i = 0; i < 4; i++) {
            const Sys::JsArrayP &js_sh_coeff = js_sh_coeffs.at(i).as_arr();

            pr.sh_coeffs[i] = Ren::Vec3f{float(js_sh_coeff.at(0).as_num().val),
                                         float(js_sh_coeff.at(1).as_num().val),
                                         float(js_sh_coeff.at(2).as_num().val)};
        }
    }
}

void Eng::LightProbe::Write(const LightProbe &pr, Sys::JsObjectP &js_out) {
    const auto &alloc = js_out.elements.get_allocator();

    { // write offset
        Sys::JsArrayP js_offset(alloc);

        js_offset.Push(Sys::JsNumber(pr.offset[0]));
        js_offset.Push(Sys::JsNumber(pr.offset[1]));
        js_offset.Push(Sys::JsNumber(pr.offset[2]));

        js_out.Insert("offset", std::move(js_offset));
    }

    { // write radius
        js_out.Insert("radius", Sys::JsNumber(pr.radius));
    }

    { // write sh coefficients
        Sys::JsArrayP js_coeffs(alloc);

        for (int i = 0; i < 4; i++) {
            Sys::JsArrayP js_coeff(alloc);

            js_coeff.Push(Sys::JsNumber(pr.sh_coeffs[i][0]));
            js_coeff.Push(Sys::JsNumber(pr.sh_coeffs[i][1]));
            js_coeff.Push(Sys::JsNumber(pr.sh_coeffs[i][2]));

            js_coeffs.Push(std::move(js_coeff));
        }

        js_out.Insert("sh_coeffs", std::move(js_coeffs));
    }
}