#include "RpGBufferFill.h"

#include <Ren/Context.h>

#include "../../Utils/ShaderLoader.h"
#include "../Renderer_Structs.h"

void RpGBufferFill::Execute(RpBuilder &builder) {
    RpAllocBuf &vtx_buf1 = builder.GetReadBuffer(vtx_buf1_);
    RpAllocBuf &vtx_buf2 = builder.GetReadBuffer(vtx_buf2_);
    RpAllocBuf &ndx_buf = builder.GetReadBuffer(ndx_buf_);

    RpAllocTex &albedo_tex = builder.GetWriteTexture(out_albedo_tex_);
    RpAllocTex &normal_tex = builder.GetWriteTexture(out_normal_tex_);
    RpAllocTex &spec_tex = builder.GetWriteTexture(out_spec_tex_);
    RpAllocTex &depth_tex = builder.GetWriteTexture(out_depth_tex_);

    LazyInit(builder.ctx(), builder.sh(), vtx_buf1, vtx_buf2, ndx_buf, albedo_tex, normal_tex, spec_tex, depth_tex);
    DrawOpaque(builder);
}

void RpGBufferFill::LazyInit(Ren::Context &ctx, ShaderLoader &sh, RpAllocBuf &vtx_buf1, RpAllocBuf &vtx_buf2,
                             RpAllocBuf &ndx_buf, RpAllocTex &albedo_tex, RpAllocTex &normal_tex, RpAllocTex &spec_tex,
                             RpAllocTex &depth_tex) {
    const Ren::RenderTarget color_targets[] = {{albedo_tex.ref, Ren::eLoadOp::Load, Ren::eStoreOp::Store},
                                               {normal_tex.ref, Ren::eLoadOp::Load, Ren::eStoreOp::Store},
                                               {spec_tex.ref, Ren::eLoadOp::Load, Ren::eStoreOp::Store}};
    const Ren::RenderTarget depth_target = {depth_tex.ref, Ren::eLoadOp::Load, Ren::eStoreOp::Store, Ren::eLoadOp::Load,
                                            Ren::eStoreOp::Store};

    if (!initialized) {
        Ren::ProgramRef gbuf_simple_prog =
            sh.LoadProgram(ctx, "gbuffer_fill", "internal/gbuffer_fill.vert.glsl", "internal/gbuffer_fill.frag.glsl");
        assert(gbuf_simple_prog->ready());
        Ren::ProgramRef gbuf_vegetation_prog = sh.LoadProgram(
            ctx, "gbuffer_fill_vege", "internal/gbuffer_fill.vert.glsl@VEGETATION", "internal/gbuffer_fill.frag.glsl");
        assert(gbuf_vegetation_prog->ready());

        const bool res =
            rp_main_draw_.Setup(ctx.api_ctx(), color_targets, COUNT_OF(color_targets), depth_target, ctx.log());
        if (!res) {
            ctx.log()->Error("[RpGBufferFill::LazyInit]: Failed to initialize render pass!");
        }

        const int buf1_stride = 16, buf2_stride = 16;

        { // VAO for simple and skinned meshes
            const Ren::VtxAttribDesc attribs[] = {
                // Attributes from buffer 1
                {vtx_buf1.ref, REN_VTX_POS_LOC, 3, Ren::eType::Float32, buf1_stride, 0},
                {vtx_buf1.ref, REN_VTX_UV1_LOC, 2, Ren::eType::Float16, buf1_stride, 3 * sizeof(float)},
                // Attributes from buffer 2
                {vtx_buf2.ref, REN_VTX_NOR_LOC, 4, Ren::eType::Int16SNorm, buf2_stride, 0},
                {vtx_buf2.ref, REN_VTX_TAN_LOC, 2, Ren::eType::Int16SNorm, buf2_stride, 4 * sizeof(uint16_t)}};
            if (!vi_simple_.Setup(attribs, COUNT_OF(attribs), ndx_buf.ref)) {
                ctx.log()->Error("[RpGBufferFill::LazyInit]: vi_simple_ init failed!");
            }
        }

        { // VAO for vegetation meshes (uses additional vertex color attribute)
            const Ren::VtxAttribDesc attribs[] = {
                // Attributes from buffer 1
                {vtx_buf1.ref, REN_VTX_POS_LOC, 3, Ren::eType::Float32, buf1_stride, 0},
                {vtx_buf1.ref, REN_VTX_UV1_LOC, 2, Ren::eType::Float16, buf1_stride, 3 * sizeof(float)},
                // Attributes from buffer 2
                {vtx_buf2.ref, REN_VTX_NOR_LOC, 4, Ren::eType::Int16SNorm, buf2_stride, 0},
                {vtx_buf2.ref, REN_VTX_TAN_LOC, 2, Ren::eType::Int16SNorm, buf2_stride, 4 * sizeof(uint16_t)},
                {vtx_buf2.ref, REN_VTX_AUX_LOC, 1, Ren::eType::Uint32, buf2_stride, 6 * sizeof(uint16_t)}};
            if (!vi_vegetation_.Setup(attribs, COUNT_OF(attribs), ndx_buf.ref)) {
                ctx.log()->Error("[RpGBufferFill::LazyInit]: vi_vegetation_ init failed!");
            }
        }

        { // simple and skinned
            Ren::RastState rast_state;
            rast_state.depth.test_enabled = true;
            rast_state.depth.compare_op = uint8_t(Ren::eCompareOp::Equal);

            if (!pi_simple_[1].Init(ctx.api_ctx(), rast_state, gbuf_simple_prog, &vi_simple_, &rp_main_draw_,
                                    ctx.log())) {
                ctx.log()->Error("[RpGBufferFill::LazyInit]: Failed to initialize pipeline!");
            }

            rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

            if (!pi_simple_[0].Init(ctx.api_ctx(), rast_state, gbuf_simple_prog, &vi_simple_, &rp_main_draw_,
                                    ctx.log())) {
                ctx.log()->Error("[RpGBufferFill::LazyInit]: Failed to initialize pipeline!");
            }
        }

        { // vegetation
            Ren::RastState rast_state;
            rast_state.depth.test_enabled = true;
            rast_state.depth.compare_op = uint8_t(Ren::eCompareOp::Equal);

            if (!pi_vegetation_[1].Init(ctx.api_ctx(), rast_state, gbuf_vegetation_prog, &vi_vegetation_,
                                        &rp_main_draw_, ctx.log())) {
                ctx.log()->Error("[RpGBufferFill::LazyInit]: Failed to initialize pipeline!");
            }

            rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

            if (!pi_vegetation_[0].Init(ctx.api_ctx(), rast_state, gbuf_vegetation_prog, &vi_vegetation_,
                                        &rp_main_draw_, ctx.log())) {
                ctx.log()->Error("[RpGBufferFill::LazyInit]: Failed to initialize pipeline!");
            }
        }

        api_ctx_ = ctx.api_ctx();
#if defined(USE_VK_RENDER)
        InitDescrSetLayout();
#endif

        initialized = true;
    }

    if (!main_draw_fb_[ctx.backend_frame()].Setup(ctx.api_ctx(), rp_main_draw_, depth_tex.desc.w, depth_tex.desc.h,
                                                  depth_target, depth_target, color_targets, COUNT_OF(color_targets))) {
        ctx.log()->Error("[RpGBufferFill::LazyInit]: main_draw_fb_ init failed!");
    }
}