#pragma once

#include <Ren/RastState.h>
#include <Ren/RenderPass.h>
#include <Ren/VertexInput.h>
#include <Sys/Json.h>

namespace Eng {
Ren::PolyState ParsePolyState(const Sys::JsObjectP &js_poly_state);
Ren::DepthState ParseDepthState(const Sys::JsObjectP &js_depth_state);
Ren::BlendState ParseBlendState(const Sys::JsObjectP &js_blend_state);
Ren::StencilState ParseStencilState(const Sys::JsObjectP &js_stencil_state);
Ren::DepthBias ParseDepthBias(const Sys::JsObjectP &js_depth_bias);
Ren::RastState ParseRastState(const Sys::JsObjectP &js_rast_state);

Ren::RenderTargetInfo ParseRTInfo(const Sys::JsObjectP &js_rt_info);
} // namespace Eng