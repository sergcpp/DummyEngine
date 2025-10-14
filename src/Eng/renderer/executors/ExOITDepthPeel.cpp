#include "ExOITDepthPeel.h"

#include <Ren/Context.h>

#include "../../utils/ShaderLoader.h"

Eng::ExOITDepthPeel::ExOITDepthPeel(const DrawList **p_list, const view_state_t *view_state, const FgResRef vtx_buf1,
                                    const FgResRef vtx_buf2, const FgResRef ndx_buf, const FgResRef materials_buf,
                                    const FgResRef textures_buf, const BindlessTextureData *bindless_tex,
                                    const FgResRef dummy_white, const FgResRef instances_buf,
                                    const FgResRef instance_indices_buf, const FgResRef shared_data_buf,
                                    const FgResRef depth_tex, const FgResRef out_depth_buf) {
    view_state_ = view_state;
    bindless_tex_ = bindless_tex;

    p_list_ = p_list;

    vtx_buf1_ = vtx_buf1;
    vtx_buf2_ = vtx_buf2;
    ndx_buf_ = ndx_buf;
    instances_buf_ = instances_buf;
    instance_indices_buf_ = instance_indices_buf;
    shared_data_buf_ = shared_data_buf;
    materials_buf_ = materials_buf;
    textures_buf_ = textures_buf;
    dummy_white_ = dummy_white;

    depth_tex_ = depth_tex;
    out_depth_buf_ = out_depth_buf;
}

void Eng::ExOITDepthPeel::Execute(FgContext &ctx) {
    FgAllocBuf &vtx_buf1 = ctx.AccessROBuffer(vtx_buf1_);
    FgAllocBuf &vtx_buf2 = ctx.AccessROBuffer(vtx_buf2_);
    FgAllocBuf &ndx_buf = ctx.AccessROBuffer(ndx_buf_);
    FgAllocTex &depth_tex = ctx.AccessRWTexture(depth_tex_);

    LazyInit(ctx.ren_ctx(), ctx.sh(), vtx_buf1, vtx_buf2, ndx_buf, depth_tex);
    DrawTransparent(ctx);
}

void Eng::ExOITDepthPeel::LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh, FgAllocBuf &vtx_buf1, FgAllocBuf &vtx_buf2,
                                   FgAllocBuf &ndx_buf, FgAllocTex &depth_tex) {
    const Ren::RenderTarget depth_target = {depth_tex.ref, Ren::eLoadOp::Load, Ren::eStoreOp::Store, Ren::eLoadOp::Load,
                                            Ren::eStoreOp::Store};
    if (!pi_simple_[0]) {
#if defined(REN_GL_BACKEND)
        const bool bindless = ctx.capabilities.bindless_texture;
#else
        const bool bindless = true;
#endif

        [[maybe_unused]] const int buf1_stride = 16, buf2_stride = 16;

        Ren::VertexInputRef vi_simple;

        { // VertexInput for simple and skinned meshes
            const Ren::VtxAttribDesc attribs[] = {{vtx_buf1.ref, VTX_POS_LOC, 3, Ren::eType::Float32, buf1_stride, 0}};
            vi_simple = sh.LoadVertexInput(attribs, ndx_buf.ref);
        }

        Ren::ProgramRef depth_peel_simple;
        if ((*p_list_)->render_settings.transparency_quality == eTransparencyQuality::Ultra) {
            depth_peel_simple = sh.LoadProgram(
                bindless ? "internal/depth_peel.vert.glsl" : "internal/depth_peel@NO_BINDLESS.vert.glsl",
                bindless ? "internal/depth_peel@ULTRA.frag.glsl" : "internal/depth_peel@ULTRA;NO_BINDLESS.frag.glsl");
        } else {
            depth_peel_simple = sh.LoadProgram(
                bindless ? "internal/depth_peel.vert.glsl" : "internal/depth_peel@NO_BINDLESS.vert.glsl",
                bindless ? "internal/depth_peel@HIGH.frag.glsl" : "internal/depth_peel@HIGH;NO_BINDLESS.frag.glsl");
        }

        Ren::RenderPassRef rp_depth_peel = sh.LoadRenderPass(depth_target, {});

        { // simple and skinned
            Ren::RastState rast_state;
            rast_state.depth.test_enabled = true;
            rast_state.depth.write_enabled = false;
            rast_state.depth.compare_op = uint8_t(Ren::eCompareOp::Greater);

            pi_simple_[2] = sh.LoadPipeline(rast_state, depth_peel_simple, vi_simple, rp_depth_peel, 0);

            rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

            pi_simple_[0] = sh.LoadPipeline(rast_state, depth_peel_simple, vi_simple, rp_depth_peel, 0);

            rast_state.poly.cull = uint8_t(Ren::eCullFace::Front);

            pi_simple_[1] = sh.LoadPipeline(rast_state, depth_peel_simple, vi_simple, rp_depth_peel, 0);
        }
    }

    fb_to_use_ = (fb_to_use_ + 1) % 2;

    if (!main_draw_fb_[ctx.backend_frame()][fb_to_use_].Setup(ctx.api_ctx(), *pi_simple_[0]->render_pass(),
                                                              depth_tex.desc.w, depth_tex.desc.h, depth_target,
                                                              depth_target, {}, ctx.log())) {
        ctx.log()->Error("[ExOITDepthPeel::LazyInit]: main_draw_fb_ init failed!");
    }
}