#include "ExOITScheduleRays.h"

#include <Ren/Context.h>

#include "../../utils/ShaderLoader.h"

void Eng::ExOITScheduleRays::Execute(FgBuilder &builder) {
    FgAllocBuf &vtx_buf1 = builder.GetReadBuffer(vtx_buf1_);
    FgAllocBuf &vtx_buf2 = builder.GetReadBuffer(vtx_buf2_);
    FgAllocBuf &ndx_buf = builder.GetReadBuffer(ndx_buf_);
    FgAllocTex &depth_tex = builder.GetWriteTexture(depth_tex_);

    LazyInit(builder.ctx(), builder.sh(), vtx_buf1, vtx_buf2, ndx_buf, depth_tex);
    DrawTransparent(builder, depth_tex);
}

void Eng::ExOITScheduleRays::LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh, FgAllocBuf &vtx_buf1,
                                      FgAllocBuf &vtx_buf2, FgAllocBuf &ndx_buf, FgAllocTex &depth_tex) {
    const Ren::RenderTarget depth_target = {depth_tex.ref, Ren::eLoadOp::Load, Ren::eStoreOp::Store, Ren::eLoadOp::Load,
                                            Ren::eStoreOp::Store};

    const int buf1_stride = 16, buf2_stride = 16;

    { // VAO for simple and skinned meshes
        const Ren::VtxAttribDesc attribs[] = {
            // Attributes from buffer 1
            {vtx_buf1.ref, VTX_POS_LOC, 3, Ren::eType::Float32, buf1_stride, 0},
            {vtx_buf1.ref, VTX_UV1_LOC, 2, Ren::eType::Float16, buf1_stride, 3 * sizeof(float)},
            // Attributes from buffer 2
            {vtx_buf2.ref, VTX_NOR_LOC, 4, Ren::eType::Int16SNorm, buf2_stride, 0},
            {vtx_buf2.ref, VTX_TAN_LOC, 2, Ren::eType::Int16SNorm, buf2_stride, 4 * sizeof(uint16_t)}};
        if (!vi_simple_.Setup(attribs, ndx_buf.ref)) {
            ctx.log()->Error("[ExOITScheduleRays::LazyInit]: vi_simple_ init failed!");
        }
    }

    { // VAO for vegetation meshes (uses additional vertex color attribute)
        const Ren::VtxAttribDesc attribs[] = {
            // Attributes from buffer 1
            {vtx_buf1.ref, VTX_POS_LOC, 3, Ren::eType::Float32, buf1_stride, 0},
            {vtx_buf1.ref, VTX_UV1_LOC, 2, Ren::eType::Float16, buf1_stride, 3 * sizeof(float)},
            // Attributes from buffer 2
            {vtx_buf2.ref, VTX_NOR_LOC, 4, Ren::eType::Int16SNorm, buf2_stride, 0},
            {vtx_buf2.ref, VTX_TAN_LOC, 2, Ren::eType::Int16SNorm, buf2_stride, 4 * sizeof(uint16_t)},
            {vtx_buf2.ref, VTX_AUX_LOC, 1, Ren::eType::Uint32, buf2_stride, 6 * sizeof(uint16_t)}};
        if (!vi_vegetation_.Setup(attribs, ndx_buf.ref)) {
            ctx.log()->Error("[ExOITScheduleRays::LazyInit]: vi_vegetation_ init failed!");
        }
    }

    if (!initialized) {
#if defined(REN_GL_BACKEND)
        const bool bindless = ctx.capabilities.bindless_texture;
#else
        const bool bindless = true;
#endif

        Ren::ProgramRef oit_blend_simple_prog = sh.LoadProgram(
            bindless ? "internal/oit_schedule_rays.vert.glsl" : "internal/oit_schedule_rays@NO_BINDLESS.vert.glsl",
            bindless ? "internal/oit_schedule_rays.frag.glsl" : "internal/oit_schedule_rays@NO_BINDLESS.frag.glsl");
        Ren::ProgramRef oit_blend_vegetation_prog = sh.LoadProgram(
            bindless ? "internal/oit_schedule_rays@VEGETATION.vert.glsl"
                     : "internal/oit_schedule_rays@VEGETATION;NO_BINDLESS.vert.glsl",
            bindless ? "internal/oit_schedule_rays.frag.glsl" : "internal/oit_schedule_rays@NO_BINDLESS.frag.glsl");
        if (!rp_oit_schedule_rays_.Setup(ctx.api_ctx(), {}, depth_target, ctx.log())) {
            ctx.log()->Error("[ExOITScheduleRays::LazyInit]: Failed to init render pass!");
        }

        { // simple and skinned
            Ren::RastState rast_state;
            rast_state.depth.test_enabled = true;
            rast_state.depth.write_enabled = false;
            rast_state.depth.compare_op = uint8_t(Ren::eCompareOp::Greater);

            if (!pi_simple_[2].Init(ctx.api_ctx(), rast_state, oit_blend_simple_prog, &vi_simple_,
                                    &rp_oit_schedule_rays_, 0, ctx.log())) {
                ctx.log()->Error("[ExOITScheduleRays::LazyInit]: Failed to initialize pipeline!");
            }

            rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

            if (!pi_simple_[0].Init(ctx.api_ctx(), rast_state, oit_blend_simple_prog, &vi_simple_,
                                    &rp_oit_schedule_rays_, 0, ctx.log())) {
                ctx.log()->Error("[ExOITScheduleRays::LazyInit]: Failed to initialize pipeline!");
            }

            rast_state.poly.cull = uint8_t(Ren::eCullFace::Front);

            if (!pi_simple_[1].Init(ctx.api_ctx(), rast_state, oit_blend_simple_prog, &vi_simple_,
                                    &rp_oit_schedule_rays_, 0, ctx.log())) {
                ctx.log()->Error("[ExOITScheduleRays::LazyInit]: Failed to initialize pipeline!");
            }
        }

        { // vegetation
            Ren::RastState rast_state;
            rast_state.depth.test_enabled = true;
            rast_state.depth.write_enabled = false;
            rast_state.depth.compare_op = uint8_t(Ren::eCompareOp::Greater);

            if (!pi_vegetation_[1].Init(ctx.api_ctx(), rast_state, oit_blend_vegetation_prog, &vi_vegetation_,
                                        &rp_oit_schedule_rays_, 0, ctx.log())) {
                ctx.log()->Error("[ExOITScheduleRays::LazyInit]: Failed to initialize pipeline!");
            }

            rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

            if (!pi_vegetation_[0].Init(ctx.api_ctx(), rast_state, oit_blend_vegetation_prog, &vi_vegetation_,
                                        &rp_oit_schedule_rays_, 0, ctx.log())) {
                ctx.log()->Error("[ExOITScheduleRays::LazyInit]: Failed to initialize pipeline!");
            }
        }

        initialized = true;
    }

    fb_to_use_ = (fb_to_use_ + 1) % 2;

    if (!main_draw_fb_[ctx.backend_frame()][fb_to_use_].Setup(ctx.api_ctx(), rp_oit_schedule_rays_, depth_tex.desc.w,
                                                              depth_tex.desc.h, depth_target, depth_target, {},
                                                              ctx.log())) {
        ctx.log()->Error("[ExOITScheduleRays::LazyInit]: main_draw_fb_ init failed!");
    }
}