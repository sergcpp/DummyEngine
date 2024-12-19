#include "ParseJs.h"

Ren::PolyState Eng::ParsePolyState(const Sys::JsObjectP &js_poly_state) {
    Ren::PolyState ret;
    if (js_poly_state.Has("cull")) {
        const Sys::JsStringP &js_cull = js_poly_state.at("cull").as_str();
        ret.cull = uint8_t(Ren::CullFace(js_cull.val));
    }
    if (js_poly_state.Has("mode")) {
        const Sys::JsStringP &js_mode = js_poly_state.at("mode").as_str();
        ret.mode = uint8_t(Ren::PolygonMode(js_mode.val));
    }
    if (js_poly_state.Has("depth_bias_mode")) {
        const Sys::JsStringP &js_depth_bias_mode = js_poly_state.at("depth_bias_mode").as_str();
        ret.depth_bias_mode = uint8_t(Ren::DepthBiasMode(js_depth_bias_mode.val));
    }
    if (js_poly_state.Has("multisample")) {
        const Sys::JsLiteral &js_multisample = js_poly_state.at("multisample").as_lit();
        ret.multisample = uint8_t(js_multisample.val == Sys::JsLiteralType::True);
    }
    return ret;
}

Ren::DepthState Eng::ParseDepthState(const Sys::JsObjectP &js_depth_state) {
    Ren::DepthState ret;
    if (js_depth_state.Has("test_enabled")) {
        ret.test_enabled = uint8_t(js_depth_state.at("test_enabled").as_lit().val == Sys::JsLiteralType::True);
    }
    if (js_depth_state.Has("write_enabled")) {
        ret.write_enabled = uint8_t(js_depth_state.at("write_enabled").as_lit().val == Sys::JsLiteralType::True);
    }
    if (js_depth_state.Has("range_mode")) {
        const Sys::JsStringP &js_range_mode = js_depth_state.at("range_mode").as_str();
        ret.range_mode = uint8_t(Ren::DepthRangeMode(js_range_mode.val));
    }
    if (js_depth_state.Has("compare_op")) {
        ret.compare_op = uint8_t(Ren::CompareOp(js_depth_state.at("compare_op").as_str().val));
    }
    return ret;
}

Ren::BlendState Eng::ParseBlendState(const Sys::JsObjectP &js_blend_state) {
    Ren::BlendState ret;
    if (js_blend_state.Has("enabled")) {
        ret.enabled = uint8_t(js_blend_state.at("enabled").as_lit().val == Sys::JsLiteralType::True);
    }
    if (js_blend_state.Has("src_color")) {
        ret.src_color = uint8_t(Ren::BlendFactor(js_blend_state.at("src_color").as_str().val));
    }
    if (js_blend_state.Has("dst_color")) {
        ret.dst_color = uint8_t(Ren::BlendFactor(js_blend_state.at("dst_color").as_str().val));
    }
    if (js_blend_state.Has("src_alpha")) {
        ret.src_alpha = uint8_t(Ren::BlendFactor(js_blend_state.at("src_alpha").as_str().val));
    }
    if (js_blend_state.Has("dst_alpha")) {
        ret.dst_alpha = uint8_t(Ren::BlendFactor(js_blend_state.at("dst_alpha").as_str().val));
    }
    return ret;
}

Ren::StencilState Eng::ParseStencilState(const Sys::JsObjectP &js_stencil_state) {
    Ren::StencilState ret;
    if (js_stencil_state.Has("enabled")) {
        ret.enabled = uint16_t(js_stencil_state.at("enabled").as_lit().val == Sys::JsLiteralType::True);
    }
    if (js_stencil_state.Has("stencil_fail")) {
        ret.stencil_fail = uint8_t(Ren::StencilOp(js_stencil_state.at("stencil_fail").as_str().val));
    }
    if (js_stencil_state.Has("depth_fail")) {
        ret.depth_fail = uint8_t(Ren::StencilOp(js_stencil_state.at("depth_fail").as_str().val));
    }
    if (js_stencil_state.Has("pass")) {
        ret.pass = uint8_t(Ren::StencilOp(js_stencil_state.at("pass").as_str().val));
    }
    if (js_stencil_state.Has("compare_op")) {
        ret.compare_op = uint8_t(Ren::CompareOp(js_stencil_state.at("compare_op").as_str().val));
    }
    if (js_stencil_state.Has("reference")) {
        ret.reference = uint8_t(js_stencil_state.at("reference").as_num().val);
    }
    if (js_stencil_state.Has("write_mask")) {
        ret.write_mask = uint8_t(js_stencil_state.at("write_mask").as_num().val);
    }
    if (js_stencil_state.Has("compare_mask")) {
        ret.compare_mask = uint8_t(js_stencil_state.at("compare_mask").as_num().val);
    }
    return ret;
}

Ren::DepthBias Eng::ParseDepthBias(const Sys::JsObjectP &js_depth_bias) {
    Ren::DepthBias ret;
    if (js_depth_bias.Has("slope_factor")) {
        ret.slope_factor = float(js_depth_bias.at("slope_factor").as_num().val);
    }
    if (js_depth_bias.Has("constant_offset")) {
        ret.constant_offset = float(js_depth_bias.at("constant_offset").as_num().val);
    }
    return ret;
}

Ren::RastState Eng::ParseRastState(const Sys::JsObjectP &js_rast_state) {
    Ren::RastState ret;
    if (js_rast_state.Has("poly")) {
        ret.poly = ParsePolyState(js_rast_state.at("poly").as_obj());
    }
    if (js_rast_state.Has("depth")) {
        ret.depth = ParseDepthState(js_rast_state.at("depth").as_obj());
    }
    if (js_rast_state.Has("blend")) {
        ret.blend = ParseBlendState(js_rast_state.at("blend").as_obj());
    }
    if (js_rast_state.Has("stencil")) {
        ret.stencil = ParseStencilState(js_rast_state.at("stencil").as_obj());
    }
    if (js_rast_state.Has("depth_bias")) {
        ret.depth_bias = ParseDepthBias(js_rast_state.at("depth_bias").as_obj());
    }
    if (js_rast_state.Has("scissor")) {
        const Sys::JsObjectP &js_scissor = js_rast_state.at("scissor").as_obj();
        if (js_scissor.Has("enabled")) {
            ret.scissor.enabled = (js_scissor.at("enabled").as_lit().val == Sys::JsLiteralType::True);
        }
    }
    return ret;
}

Ren::RenderTargetInfo Eng::ParseRTInfo(const Sys::JsObjectP &js_rt_info) {
    Ren::RenderTargetInfo ret;
    ret.format = Ren::TexFormat(js_rt_info.at("format").as_str().val);
    ret.load = Ren::LoadOp(js_rt_info.at("load").as_str().val);
    ret.store = Ren::StoreOp(js_rt_info.at("store").as_str().val);
    if (js_rt_info.Has("stencil_load")) {
        ret.stencil_load = Ren::LoadOp(js_rt_info.at("stencil_load").as_str().val);
    }
    if (js_rt_info.Has("stencil_store")) {
        ret.stencil_store = Ren::StoreOp(js_rt_info.at("stencil_store").as_str().val);
    }
    return ret;
}
