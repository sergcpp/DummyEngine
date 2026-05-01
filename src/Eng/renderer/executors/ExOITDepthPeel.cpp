#include "ExOITDepthPeel.h"

#include <Ren/Context.h>

#include "../../utils/ShaderLoader.h"
#include "../Renderer_DrawList.h"
#include "../framegraph/FgBuilder.h"

Eng::ExOITDepthPeel::ExOITDepthPeel(const DrawList **p_list, const view_state_t *view_state,
                                    const FgBufROHandle vtx_buf1, const FgBufROHandle vtx_buf2,
                                    const FgBufROHandle ndx_buf, const FgBufROHandle materials,
                                    const BindlessTextureData *bindless_tex, const FgImgROHandle dummy_white,
                                    const FgBufROHandle instances, const FgBufROHandle instance_indices,
                                    const FgBufROHandle shared_data, const FgImgRWHandle depth,
                                    const FgBufRWHandle out_depth) {
    view_state_ = view_state;
    bindless_tex_ = bindless_tex;

    p_list_ = p_list;

    vtx_buf1_ = vtx_buf1;
    vtx_buf2_ = vtx_buf2;
    ndx_buf_ = ndx_buf;
    instances_ = instances;
    instance_indices_ = instance_indices;
    shared_data_ = shared_data;
    materials_ = materials;
    dummy_white_ = dummy_white;

    depth_ = depth;
    out_depth_ = out_depth;
}

void Eng::ExOITDepthPeel::Execute(const FgContext &fg) {
    const Ren::ImageRWHandle depth = fg.AccessRWImage(depth_);

    LazyInit(fg, depth);
    DrawTransparent(fg, depth);
}

void Eng::ExOITDepthPeel::LazyInit(const FgContext &fg, const Ren::ImageRWHandle depth) {
    const Ren::RenderTarget depth_target = {depth, Ren::eLoadOp::Load, Ren::eStoreOp::Store, Ren::eLoadOp::Load,
                                            Ren::eStoreOp::Store};
    if (!pi_simple_[0]) {
        auto &ctx = fg.ren_ctx();
        auto &sh = fg.sh();
#if defined(REN_GL_BACKEND)
        const bool bindless = ctx.capabilities.bindless_texture;
#else
        const bool bindless = true;
#endif

        const int buf1_stride = 16, buf2_stride = 16;

        Ren::VertexInputHandle vi_simple;

        { // VertexInput for simple and skinned meshes
            const Ren::VtxAttribDesc attribs[] = {{0, VTX_POS_LOC, 3, Ren::eType::Float32, buf1_stride, 0, 0}};
            vi_simple = sh.FindOrCreateVertexInput(attribs);
        }

        Ren::ProgramHandle depth_peel_simple;
        if ((*p_list_)->render_settings.transparency_quality == eTransparencyQuality::Ultra) {
            depth_peel_simple = sh.FindOrCreateProgram(
                bindless ? "internal/depth_peel.vert.glsl" : "internal/depth_peel@NO_BINDLESS.vert.glsl",
                bindless ? "internal/depth_peel@ULTRA.frag.glsl" : "internal/depth_peel@ULTRA;NO_BINDLESS.frag.glsl");
        } else {
            depth_peel_simple = sh.FindOrCreateProgram(
                bindless ? "internal/depth_peel.vert.glsl" : "internal/depth_peel@NO_BINDLESS.vert.glsl",
                bindless ? "internal/depth_peel@HIGH.frag.glsl" : "internal/depth_peel@HIGH;NO_BINDLESS.frag.glsl");
        }

        const Ren::RenderPassHandle rp_depth_peel = sh.FindOrCreateRenderPass(depth_target, {});

        { // simple and skinned
            Ren::RastState rast_state;
            rast_state.depth.test_enabled = true;
            rast_state.depth.write_enabled = false;
            rast_state.depth.compare_op = uint8_t(Ren::eCompareOp::Greater);

            pi_simple_[2] = sh.FindOrCreatePipeline(rast_state, depth_peel_simple, vi_simple, rp_depth_peel, 0);

            rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

            pi_simple_[0] = sh.FindOrCreatePipeline(rast_state, depth_peel_simple, vi_simple, rp_depth_peel, 0);

            rast_state.poly.cull = uint8_t(Ren::eCullFace::Front);

            pi_simple_[1] = sh.FindOrCreatePipeline(rast_state, depth_peel_simple, vi_simple, rp_depth_peel, 0);
        }
    }
}