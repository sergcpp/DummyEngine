#include "ParseJs.h"

Ren::PolyState Eng::ParsePolyState(const Sys::JsObjectP &js_poly_state) {
    Ren::PolyState ret;
    if (const size_t cull_ndx = js_poly_state.IndexOf("cull"); cull_ndx < js_poly_state.Size()) {
        const Sys::JsStringP &js_cull = js_poly_state[cull_ndx].second.as_str();
        ret.cull = uint8_t(Ren::CullFace(js_cull.val));
    }
    if (const size_t mode_ndx = js_poly_state.IndexOf("mode"); mode_ndx < js_poly_state.Size()) {
        const Sys::JsStringP &js_mode = js_poly_state[mode_ndx].second.as_str();
        ret.mode = uint8_t(Ren::PolygonMode(js_mode.val));
    }
    if (const size_t depth_bias_mode_ndx = js_poly_state.IndexOf("depth_bias_mode");
        depth_bias_mode_ndx < js_poly_state.Size()) {
        const Sys::JsStringP &js_depth_bias_mode = js_poly_state[depth_bias_mode_ndx].second.as_str();
        ret.depth_bias_mode = uint8_t(Ren::DepthBiasMode(js_depth_bias_mode.val));
    }
    if (const size_t multisample_ndx = js_poly_state.IndexOf("multisample"); multisample_ndx < js_poly_state.Size()) {
        const Sys::JsLiteral &js_multisample = js_poly_state[multisample_ndx].second.as_lit();
        ret.multisample = uint8_t(js_multisample.val == Sys::JsLiteralType::True);
    }
    return ret;
}

Ren::DepthState Eng::ParseDepthState(const Sys::JsObjectP &js_depth_state) {
    Ren::DepthState ret;
    if (const size_t test_enabled_ndx = js_depth_state.IndexOf("test_enabled");
        test_enabled_ndx < js_depth_state.Size()) {
        ret.test_enabled = uint8_t(js_depth_state[test_enabled_ndx].second.as_lit().val == Sys::JsLiteralType::True);
    }
    if (const size_t write_enabled_ndx = js_depth_state.IndexOf("write_enabled");
        write_enabled_ndx < js_depth_state.Size()) {
        ret.write_enabled = uint8_t(js_depth_state[write_enabled_ndx].second.as_lit().val == Sys::JsLiteralType::True);
    }
    if (const size_t range_mode_ndx = js_depth_state.IndexOf("range_mode"); range_mode_ndx < js_depth_state.Size()) {
        const Sys::JsStringP &js_range_mode = js_depth_state[range_mode_ndx].second.as_str();
        ret.range_mode = uint8_t(Ren::DepthRangeMode(js_range_mode.val));
    }
    if (const size_t compare_op_ndx = js_depth_state.IndexOf("compare_op"); compare_op_ndx < js_depth_state.Size()) {
        ret.compare_op = uint8_t(Ren::CompareOp(js_depth_state[compare_op_ndx].second.as_str().val));
    }
    return ret;
}

Ren::BlendState Eng::ParseBlendState(const Sys::JsObjectP &js_blend_state) {
    Ren::BlendState ret;
    if (const size_t enabled_ndx = js_blend_state.IndexOf("enabled"); enabled_ndx < js_blend_state.Size()) {
        ret.enabled = uint8_t(js_blend_state[enabled_ndx].second.as_lit().val == Sys::JsLiteralType::True);
    }
    if (const size_t src_color_ndx = js_blend_state.IndexOf("src_color"); src_color_ndx < js_blend_state.Size()) {
        ret.src_color = uint8_t(Ren::BlendFactor(js_blend_state[src_color_ndx].second.as_str().val));
    }
    if (const size_t dst_color_ndx = js_blend_state.IndexOf("dst_color"); dst_color_ndx < js_blend_state.Size()) {
        ret.dst_color = uint8_t(Ren::BlendFactor(js_blend_state[dst_color_ndx].second.as_str().val));
    }
    if (const size_t src_alpha_ndx = js_blend_state.IndexOf("src_alpha"); src_alpha_ndx < js_blend_state.Size()) {
        ret.src_alpha = uint8_t(Ren::BlendFactor(js_blend_state[src_alpha_ndx].second.as_str().val));
    }
    if (const size_t dst_alpha_ndx = js_blend_state.IndexOf("dst_alpha"); dst_alpha_ndx < js_blend_state.Size()) {
        ret.dst_alpha = uint8_t(Ren::BlendFactor(js_blend_state[dst_alpha_ndx].second.as_str().val));
    }
    return ret;
}

Ren::StencilState Eng::ParseStencilState(const Sys::JsObjectP &js_stencil_state) {
    Ren::StencilState ret;
    if (const size_t enabled_ndx = js_stencil_state.IndexOf("enabled"); enabled_ndx < js_stencil_state.Size()) {
        ret.enabled = uint16_t(js_stencil_state[enabled_ndx].second.as_lit().val == Sys::JsLiteralType::True);
    }
    if (const size_t stencil_fail_ndx = js_stencil_state.IndexOf("stencil_fail");
        stencil_fail_ndx < js_stencil_state.Size()) {
        ret.stencil_fail = uint8_t(Ren::StencilOp(js_stencil_state[stencil_fail_ndx].second.as_str().val));
    }
    if (const size_t depth_fail_ndx = js_stencil_state.IndexOf("depth_fail");
        depth_fail_ndx < js_stencil_state.Size()) {
        ret.depth_fail = uint8_t(Ren::StencilOp(js_stencil_state[depth_fail_ndx].second.as_str().val));
    }
    if (const size_t pass_ndx = js_stencil_state.IndexOf("pass"); pass_ndx < js_stencil_state.Size()) {
        ret.pass = uint8_t(Ren::StencilOp(js_stencil_state[pass_ndx].second.as_str().val));
    }
    if (const size_t compare_op_ndx = js_stencil_state.IndexOf("compare_op");
        compare_op_ndx < js_stencil_state.Size()) {
        ret.compare_op = uint8_t(Ren::CompareOp(js_stencil_state[compare_op_ndx].second.as_str().val));
    }
    if (const size_t reference_ndx = js_stencil_state.IndexOf("reference"); reference_ndx < js_stencil_state.Size()) {
        ret.reference = uint8_t(js_stencil_state[reference_ndx].second.as_num().val);
    }
    if (const size_t write_mask_ndx = js_stencil_state.IndexOf("write_mask");
        write_mask_ndx < js_stencil_state.Size()) {
        ret.write_mask = uint8_t(js_stencil_state[write_mask_ndx].second.as_num().val);
    }
    if (const size_t compare_mask_ndx = js_stencil_state.IndexOf("compare_mask");
        compare_mask_ndx < js_stencil_state.Size()) {
        ret.compare_mask = uint8_t(js_stencil_state[compare_mask_ndx].second.as_num().val);
    }
    return ret;
}

Ren::DepthBias Eng::ParseDepthBias(const Sys::JsObjectP &js_depth_bias) {
    Ren::DepthBias ret;
    if (const size_t slope_factor_ndx = js_depth_bias.IndexOf("slope_factor");
        slope_factor_ndx < js_depth_bias.Size()) {
        ret.slope_factor = float(js_depth_bias[slope_factor_ndx].second.as_num().val);
    }
    if (const size_t constant_offset_ndx = js_depth_bias.IndexOf("constant_offset");
        constant_offset_ndx < js_depth_bias.Size()) {
        ret.constant_offset = float(js_depth_bias[constant_offset_ndx].second.as_num().val);
    }
    return ret;
}

Ren::RastState Eng::ParseRastState(const Sys::JsObjectP &js_rast_state) {
    Ren::RastState ret;
    if (const size_t poly_ndx = js_rast_state.IndexOf("poly"); poly_ndx < js_rast_state.Size()) {
        ret.poly = ParsePolyState(js_rast_state[poly_ndx].second.as_obj());
    }
    if (const size_t depth_ndx = js_rast_state.IndexOf("depth"); depth_ndx < js_rast_state.Size()) {
        ret.depth = ParseDepthState(js_rast_state[depth_ndx].second.as_obj());
    }
    if (const size_t blend_ndx = js_rast_state.IndexOf("blend"); blend_ndx < js_rast_state.Size()) {
        ret.blend = ParseBlendState(js_rast_state[blend_ndx].second.as_obj());
    }
    if (const size_t stencil_ndx = js_rast_state.IndexOf("stencil"); stencil_ndx < js_rast_state.Size()) {
        ret.stencil = ParseStencilState(js_rast_state[stencil_ndx].second.as_obj());
    }
    if (const size_t depth_bias_ndx = js_rast_state.IndexOf("depth_bias"); depth_bias_ndx < js_rast_state.Size()) {
        ret.depth_bias = ParseDepthBias(js_rast_state[depth_bias_ndx].second.as_obj());
    }
    if (const size_t scissor_ndx = js_rast_state.IndexOf("scissor"); scissor_ndx < js_rast_state.Size()) {
        const Sys::JsObjectP &js_scissor = js_rast_state[scissor_ndx].second.as_obj();
        if (const size_t enabled_ndx = js_scissor.IndexOf("enabled"); enabled_ndx < js_scissor.Size()) {
            ret.scissor.enabled = (js_scissor[enabled_ndx].second.as_lit().val == Sys::JsLiteralType::True);
        }
    }
    return ret;
}

Ren::RenderTargetInfo Eng::ParseRTInfo(const Sys::JsObjectP &js_rt_info) {
    Ren::RenderTargetInfo ret;
    ret.format = Ren::TexFormat(js_rt_info.at("format").as_str().val);
    ret.load = Ren::LoadOp(js_rt_info.at("load").as_str().val);
    ret.store = Ren::StoreOp(js_rt_info.at("store").as_str().val);
    if (const size_t stencil_load_ndx = js_rt_info.IndexOf("stencil_load"); stencil_load_ndx < js_rt_info.Size()) {
        ret.stencil_load = Ren::LoadOp(js_rt_info[stencil_load_ndx].second.as_str().val);
    }
    if (const size_t stencil_store_ndx = js_rt_info.IndexOf("stencil_store"); stencil_store_ndx < js_rt_info.Size()) {
        ret.stencil_store = Ren::StoreOp(js_rt_info[stencil_store_ndx].second.as_str().val);
    }
    return ret;
}
