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

    if (!initialized) {
#if defined(USE_GL_RENDER)
        const bool bindless = ctx.capabilities.bindless_texture;
#else
        const bool bindless = true;
#endif

        Ren::ProgramRef depth_peel_simple_prog = sh.LoadProgram(
            ctx, bindless ? "internal/depth_peel.vert.glsl" : "internal/depth_peel@NO_BINDLESS.vert.glsl",
            bindless ? "internal/depth_peel.frag.glsl" : "internal/depth_peel@NO_BINDLESS.frag.glsl");
        assert(depth_peel_simple_prog->ready());
        Ren::ProgramRef depth_peel_vegetation_prog =
            sh.LoadProgram(ctx,
                           bindless ? "internal/depth_peel@VEGETATION.vert.glsl"
                                    : "internal/depth_peel@VEGETATION;NO_BINDLESS.vert.glsl",
                           bindless ? "internal/depth_peel.frag.glsl" : "internal/depth_peel@NO_BINDLESS.frag.glsl");
        assert(depth_peel_vegetation_prog->ready());

        if (!rp_depth_peel_.Setup(ctx.api_ctx(), {}, depth_target, ctx.log())) {
            ctx.log()->Error("[ExOITDepthPeel::LazyInit]: Failed to init render pass!");
        }

        const int buf1_stride = 16, buf2_stride = 16;

        { // VAO for simple and skinned meshes
            const Ren::VtxAttribDesc attribs[] = {{vtx_buf1.ref, VTX_POS_LOC, 3, Ren::eType::Float32, buf1_stride, 0}};
            if (!vi_simple_.Setup(attribs, ndx_buf.ref)) {
                ctx.log()->Error("[ExOITDepthPeel::LazyInit]: vi_simple_ init failed!");
            }
        }

        { // VAO for vegetation meshes (uses additional vertex color attribute)
            const Ren::VtxAttribDesc attribs[] = {
                // Attributes from buffer 1
                {vtx_buf1.ref, VTX_POS_LOC, 3, Ren::eType::Float32, buf1_stride, 0},
                // Attributes from buffer 2
                {vtx_buf2.ref, VTX_AUX_LOC, 1, Ren::eType::Uint32, buf2_stride, 6 * sizeof(uint16_t)}};
            if (!vi_vegetation_.Setup(attribs, ndx_buf.ref)) {
                ctx.log()->Error("[ExOITDepthPeel::LazyInit]: vi_vegetation_ init failed!");
            }
        }

        { // simple and skinned
            Ren::RastState rast_state;
            rast_state.depth.test_enabled = true;
            rast_state.depth.write_enabled = false;
            rast_state.depth.compare_op = uint8_t(Ren::eCompareOp::Greater);

            if (!pi_simple_[2].Init(ctx.api_ctx(), rast_state, depth_peel_simple_prog, &vi_simple_, &rp_depth_peel_, 0,
                                    ctx.log())) {
                ctx.log()->Error("[ExOITDepthPeel::LazyInit]: Failed to initialize pipeline!");
            }

            rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

            if (!pi_simple_[0].Init(ctx.api_ctx(), rast_state, depth_peel_simple_prog, &vi_simple_, &rp_depth_peel_, 0,
                                    ctx.log())) {
                ctx.log()->Error("[ExOITDepthPeel::LazyInit]: Failed to initialize pipeline!");
            }

            rast_state.poly.cull = uint8_t(Ren::eCullFace::Front);

            if (!pi_simple_[1].Init(ctx.api_ctx(), rast_state, depth_peel_simple_prog, &vi_simple_, &rp_depth_peel_, 0,
                                    ctx.log())) {
                ctx.log()->Error("[ExOITDepthPeel::LazyInit]: Failed to initialize pipeline!");
            }
        }

        { // vegetation
            Ren::RastState rast_state;
            rast_state.depth.test_enabled = true;
            rast_state.depth.write_enabled = false;
            rast_state.depth.compare_op = uint8_t(Ren::eCompareOp::Greater);

            if (!pi_vegetation_[1].Init(ctx.api_ctx(), rast_state, depth_peel_vegetation_prog, &vi_vegetation_,
                                        &rp_depth_peel_, 0, ctx.log())) {
                ctx.log()->Error("[ExOITDepthPeel::LazyInit]: Failed to initialize pipeline!");
            }

            rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

            if (!pi_vegetation_[0].Init(ctx.api_ctx(), rast_state, depth_peel_vegetation_prog, &vi_vegetation_,
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