#include "RpEmissive.h"

#include <Ren/Context.h>

#include "../../utils/ShaderLoader.h"
#include "../Renderer_Structs.h"

void Eng::RpEmissive::Execute(RpBuilder &builder) {
    RpAllocBuf &vtx_buf1 = builder.GetReadBuffer(vtx_buf1_);
    RpAllocBuf &vtx_buf2 = builder.GetReadBuffer(vtx_buf2_);
    RpAllocBuf &ndx_buf = builder.GetReadBuffer(ndx_buf_);

    RpAllocTex &color_tex = builder.GetWriteTexture(out_color_tex_);
    RpAllocTex &depth_tex = builder.GetWriteTexture(out_depth_tex_);

    LazyInit(builder.ctx(), builder.sh(), vtx_buf1, vtx_buf2, ndx_buf, color_tex, depth_tex);
    DrawOpaque(builder);
}

void Eng::RpEmissive::LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh, RpAllocBuf &vtx_buf1, RpAllocBuf &vtx_buf2,
                               RpAllocBuf &ndx_buf, RpAllocTex &color_tex, RpAllocTex &depth_tex) {
    const Ren::RenderTarget color_targets[] = {{color_tex.ref, Ren::eLoadOp::Load, Ren::eStoreOp::Store}};
    const Ren::RenderTarget depth_target = {depth_tex.ref, Ren::eLoadOp::Load, Ren::eStoreOp::Store, Ren::eLoadOp::Load,
                                            Ren::eStoreOp::Store};

    if (!initialized) {
#if defined(USE_GL_RENDER)
        const bool bindless = ctx.capabilities.bindless_texture;
#else
        const bool bindless = true;
#endif

        Ren::ProgramRef emissive_simple_prog =
            sh.LoadProgram(ctx, bindless ? "internal/emissive.vert.glsl" : "internal/emissive.vert.glsl@NO_BINDLESS",
                           bindless ? "internal/emissive.frag.glsl" : "internal/emissive.frag.glsl@NO_BINDLESS", {}, {},
                           bindless ? "internal/emissive.geom.glsl" : "internal/emissive.geom.glsl@NO_BINDLESS");
        assert(emissive_simple_prog->ready());
        Ren::ProgramRef emissive_vegetation_prog = sh.LoadProgram(
            ctx,
            bindless ? "internal/emissive.vert.glsl@VEGETATION" : "internal/emissive.vert.glsl@VEGETATION;NO_BINDLESS",
            bindless ? "internal/emissive.frag.glsl" : "internal/emissive.frag.glsl@NO_BINDLESS", {}, {},
            bindless ? "internal/emissive.geom.glsl" : "internal/emissive.geom.glsl@NO_BINDLESS");
        assert(emissive_vegetation_prog->ready());

        const bool res = rp_main_draw_.Setup(ctx.api_ctx(), color_targets, depth_target, ctx.log());
        if (!res) {
            ctx.log()->Error("[RpEmissive::LazyInit]: Failed to initialize render pass!");
        }

        const int buf1_stride = 16, buf2_stride = 16;

        { // VAO for simple and skinned meshes
            const Ren::VtxAttribDesc attribs[] = {
                // Attributes from buffer 1
                {vtx_buf1.ref, VTX_POS_LOC, 3, Ren::eType::Float32, buf1_stride, 0},
                {vtx_buf1.ref, VTX_UV1_LOC, 2, Ren::eType::Float16, buf1_stride, 3 * sizeof(float)}};
            if (!vi_simple_.Setup(attribs, ndx_buf.ref)) {
                ctx.log()->Error("[RpEmissive::LazyInit]: vi_simple_ init failed!");
            }
        }

        { // VAO for vegetation meshes (uses additional vertex color attribute)
            const Ren::VtxAttribDesc attribs[] = {
                // Attributes from buffer 1
                {vtx_buf1.ref, VTX_POS_LOC, 3, Ren::eType::Float32, buf1_stride, 0},
                {vtx_buf1.ref, VTX_UV1_LOC, 2, Ren::eType::Float16, buf1_stride, 3 * sizeof(float)},
                // Attributes from buffer 2
                {vtx_buf2.ref, VTX_AUX_LOC, 1, Ren::eType::Uint32, buf2_stride, 6 * sizeof(uint16_t)}};
            if (!vi_vegetation_.Setup(attribs, ndx_buf.ref)) {
                ctx.log()->Error("[RpEmissive::LazyInit]: vi_vegetation_ init failed!");
            }
        }

        { // simple and skinned
            Ren::RastState rast_state;
            rast_state.depth.test_enabled = true;
            rast_state.depth.compare_op = uint8_t(Ren::eCompareOp::Equal);
            rast_state.blend.enabled = true;
            rast_state.blend.src_color = rast_state.blend.src_alpha = uint8_t(Ren::eBlendFactor::One);
            rast_state.blend.dst_color = rast_state.blend.dst_alpha = uint8_t(Ren::eBlendFactor::One);

            if (!pi_simple_[2].Init(ctx.api_ctx(), rast_state, emissive_simple_prog, &vi_simple_, &rp_main_draw_, 0,
                                    ctx.log())) {
                ctx.log()->Error("[RpEmissive::LazyInit]: Failed to initialize pipeline!");
            }

            rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

            if (!pi_simple_[0].Init(ctx.api_ctx(), rast_state, emissive_simple_prog, &vi_simple_, &rp_main_draw_, 0,
                                    ctx.log())) {
                ctx.log()->Error("[RpEmissive::LazyInit]: Failed to initialize pipeline!");
            }

            rast_state.poly.cull = uint8_t(Ren::eCullFace::Front);

            if (!pi_simple_[1].Init(ctx.api_ctx(), rast_state, emissive_simple_prog, &vi_simple_, &rp_main_draw_, 0,
                                    ctx.log())) {
                ctx.log()->Error("[RpEmissive::LazyInit]: Failed to initialize pipeline!");
            }
        }

        { // vegetation
            Ren::RastState rast_state;
            rast_state.depth.test_enabled = true;
            rast_state.depth.compare_op = uint8_t(Ren::eCompareOp::Equal);
            rast_state.blend.enabled = true;
            rast_state.blend.src_color = rast_state.blend.src_alpha = uint8_t(Ren::eBlendFactor::One);
            rast_state.blend.dst_color = rast_state.blend.dst_alpha = uint8_t(Ren::eBlendFactor::One);

            if (!pi_vegetation_[1].Init(ctx.api_ctx(), rast_state, emissive_vegetation_prog, &vi_vegetation_,
                                        &rp_main_draw_, 0, ctx.log())) {
                ctx.log()->Error("[RpEmissive::LazyInit]: Failed to initialize pipeline!");
            }

            rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

            if (!pi_vegetation_[0].Init(ctx.api_ctx(), rast_state, emissive_vegetation_prog, &vi_vegetation_,
                                        &rp_main_draw_, 0, ctx.log())) {
                ctx.log()->Error("[RpEmissive::LazyInit]: Failed to initialize pipeline!");
            }
        }

        initialized = true;
    }

    fb_to_use_ = (fb_to_use_ + 1) % 2;

    if (!main_draw_fb_[ctx.backend_frame()][fb_to_use_].Setup(ctx.api_ctx(), rp_main_draw_, depth_tex.desc.w,
                                                              depth_tex.desc.h, depth_target, depth_target,
                                                              color_targets, ctx.log())) {
        ctx.log()->Error("[RpEmissive::LazyInit]: main_draw_fb_ init failed!");
    }
}