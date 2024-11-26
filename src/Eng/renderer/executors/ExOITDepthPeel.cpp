#include "ExOITDepthPeel.h"

#include <Ren/Context.h>

#include "../../utils/ShaderLoader.h"

void Eng::ExOITDepthPeel::Execute(FgBuilder &builder) {
    FgAllocBuf &vtx_buf1 = builder.GetReadBuffer(vtx_buf1_);
    FgAllocBuf &vtx_buf2 = builder.GetReadBuffer(vtx_buf2_);
    FgAllocBuf &ndx_buf = builder.GetReadBuffer(ndx_buf_);
    FgAllocTex &depth_tex = builder.GetWriteTexture(depth_tex_);

    LazyInit(builder.ctx(), builder.sh(), vtx_buf1, vtx_buf2, ndx_buf, depth_tex);
    DrawTransparent(builder);
}

void Eng::ExOITDepthPeel::LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh, FgAllocBuf &vtx_buf1, FgAllocBuf &vtx_buf2,
                                   FgAllocBuf &ndx_buf, FgAllocTex &depth_tex) {
    const Ren::RenderTarget depth_target = {depth_tex.ref, Ren::eLoadOp::Load, Ren::eStoreOp::Store, Ren::eLoadOp::Load,
                                            Ren::eStoreOp::Store};

    [[maybe_unused]] const int buf1_stride = 16, buf2_stride = 16;

    { // VAO for simple and skinned meshes
        const Ren::VtxAttribDesc attribs[] = {{vtx_buf1.ref, VTX_POS_LOC, 3, Ren::eType::Float32, buf1_stride, 0}};
        if (!vi_simple_.Setup(attribs, ndx_buf.ref)) {
            ctx.log()->Error("[ExOITDepthPeel::LazyInit]: vi_simple_ init failed!");
        }
    }

    if (!initialized) {
#if defined(REN_GL_BACKEND)
        const bool bindless = ctx.capabilities.bindless_texture;
#else
        const bool bindless = true;
#endif

        Ren::ProgramRef depth_peel_simple_high_prog = sh.LoadProgram(
            ctx, bindless ? "internal/depth_peel.vert.glsl" : "internal/depth_peel@NO_BINDLESS.vert.glsl",
            bindless ? "internal/depth_peel@HIGH.frag.glsl" : "internal/depth_peel@HIGH;NO_BINDLESS.frag.glsl");
        assert(depth_peel_simple_high_prog->ready());

        Ren::ProgramRef depth_peel_simple_ultra_prog = sh.LoadProgram(
            ctx, bindless ? "internal/depth_peel.vert.glsl" : "internal/depth_peel@NO_BINDLESS.vert.glsl",
            bindless ? "internal/depth_peel@ULTRA.frag.glsl" : "internal/depth_peel@ULTRA;NO_BINDLESS.frag.glsl");
        assert(depth_peel_simple_ultra_prog->ready());

        if (!rp_depth_peel_.Setup(ctx.api_ctx(), {}, depth_target, ctx.log())) {
            ctx.log()->Error("[ExOITDepthPeel::LazyInit]: Failed to init render pass!");
        }

        { // simple and skinned
            Ren::RastState rast_state;
            rast_state.depth.test_enabled = true;
            rast_state.depth.write_enabled = false;
            rast_state.depth.compare_op = uint8_t(Ren::eCompareOp::Greater);

            if (!pi_simple_[0][2].Init(ctx.api_ctx(), rast_state, depth_peel_simple_high_prog, &vi_simple_,
                                       &rp_depth_peel_, 0, ctx.log())) {
                ctx.log()->Error("[ExOITDepthPeel::LazyInit]: Failed to initialize pipeline!");
            }
            if (!pi_simple_[1][2].Init(ctx.api_ctx(), rast_state, depth_peel_simple_ultra_prog, &vi_simple_,
                                       &rp_depth_peel_, 0, ctx.log())) {
                ctx.log()->Error("[ExOITDepthPeel::LazyInit]: Failed to initialize pipeline!");
            }

            rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

            if (!pi_simple_[0][0].Init(ctx.api_ctx(), rast_state, depth_peel_simple_high_prog, &vi_simple_,
                                       &rp_depth_peel_, 0, ctx.log())) {
                ctx.log()->Error("[ExOITDepthPeel::LazyInit]: Failed to initialize pipeline!");
            }
            if (!pi_simple_[1][0].Init(ctx.api_ctx(), rast_state, depth_peel_simple_ultra_prog, &vi_simple_,
                                       &rp_depth_peel_, 0, ctx.log())) {
                ctx.log()->Error("[ExOITDepthPeel::LazyInit]: Failed to initialize pipeline!");
            }

            rast_state.poly.cull = uint8_t(Ren::eCullFace::Front);

            if (!pi_simple_[0][1].Init(ctx.api_ctx(), rast_state, depth_peel_simple_high_prog, &vi_simple_,
                                       &rp_depth_peel_, 0, ctx.log())) {
                ctx.log()->Error("[ExOITDepthPeel::LazyInit]: Failed to initialize pipeline!");
            }
            if (!pi_simple_[1][1].Init(ctx.api_ctx(), rast_state, depth_peel_simple_ultra_prog, &vi_simple_,
                                       &rp_depth_peel_, 0, ctx.log())) {
                ctx.log()->Error("[ExOITDepthPeel::LazyInit]: Failed to initialize pipeline!");
            }
        }
        initialized = true;
    }

    fb_to_use_ = (fb_to_use_ + 1) % 2;

    if (!main_draw_fb_[ctx.backend_frame()][fb_to_use_].Setup(ctx.api_ctx(), rp_depth_peel_, depth_tex.desc.w,
                                                              depth_tex.desc.h, depth_target, depth_target, {},
                                                              ctx.log())) {
        ctx.log()->Error("[ExOITDepthPeel::LazyInit]: main_draw_fb_ init failed!");
    }
}