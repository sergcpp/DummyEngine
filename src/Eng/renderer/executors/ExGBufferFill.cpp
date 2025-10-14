#include "ExGBufferFill.h"

#include <Ren/Context.h>

#include "../../utils/ShaderLoader.h"
#include "../Renderer_Structs.h"

void Eng::ExGBufferFill::Execute(FgContext &ctx) {
    FgAllocBuf &vtx_buf1 = ctx.AccessROBuffer(vtx_buf1_);
    FgAllocBuf &vtx_buf2 = ctx.AccessROBuffer(vtx_buf2_);
    FgAllocBuf &ndx_buf = ctx.AccessROBuffer(ndx_buf_);

    FgAllocTex &albedo_tex = ctx.AccessRWTexture(out_albedo_tex_);
    FgAllocTex &normal_tex = ctx.AccessRWTexture(out_normal_tex_);
    FgAllocTex &spec_tex = ctx.AccessRWTexture(out_spec_tex_);
    FgAllocTex &depth_tex = ctx.AccessRWTexture(out_depth_tex_);

    LazyInit(ctx.ren_ctx(), ctx.sh(), vtx_buf1, vtx_buf2, ndx_buf, albedo_tex, normal_tex, spec_tex, depth_tex);
    DrawOpaque(ctx);
}

void Eng::ExGBufferFill::LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh, FgAllocBuf &vtx_buf1, FgAllocBuf &vtx_buf2,
                                  FgAllocBuf &ndx_buf, FgAllocTex &albedo_tex, FgAllocTex &normal_tex,
                                  FgAllocTex &spec_tex, FgAllocTex &depth_tex) {
    const Ren::RenderTarget color_targets[] = {{albedo_tex.ref, Ren::eLoadOp::Clear, Ren::eStoreOp::Store},
                                               {normal_tex.ref, Ren::eLoadOp::Clear, Ren::eStoreOp::Store},
                                               {spec_tex.ref, Ren::eLoadOp::Clear, Ren::eStoreOp::Store}};
    const Ren::RenderTarget depth_target = {depth_tex.ref, Ren::eLoadOp::Load, Ren::eStoreOp::Store, Ren::eLoadOp::Load,
                                            Ren::eStoreOp::Store};

    if (!initialized) {
#if defined(REN_GL_BACKEND)
        const bool bindless = ctx.capabilities.bindless_texture;
#else
        const bool bindless = true;
#endif

        const int buf1_stride = 16, buf2_stride = 16;

        Ren::VertexInputRef vi_simple, vi_vegetation;

        { // VertexInput for simple and skinned meshes
            const Ren::VtxAttribDesc attribs[] = {
                // Attributes from buffer 1
                {vtx_buf1.ref, VTX_POS_LOC, 3, Ren::eType::Float32, buf1_stride, 0},
                {vtx_buf1.ref, VTX_UV1_LOC, 2, Ren::eType::Float16, buf1_stride, 3 * sizeof(float)},
                // Attributes from buffer 2
                {vtx_buf2.ref, VTX_NOR_LOC, 4, Ren::eType::Int16_snorm, buf2_stride, 0},
                {vtx_buf2.ref, VTX_TAN_LOC, 2, Ren::eType::Int16_snorm, buf2_stride, 4 * sizeof(uint16_t)}};
            vi_simple = sh.LoadVertexInput(attribs, ndx_buf.ref);
        }
        { // VertexInput for vegetation meshes (uses additional vertex color attribute)
            const Ren::VtxAttribDesc attribs[] = {
                // Attributes from buffer 1
                {vtx_buf1.ref, VTX_POS_LOC, 3, Ren::eType::Float32, buf1_stride, 0},
                {vtx_buf1.ref, VTX_UV1_LOC, 2, Ren::eType::Float16, buf1_stride, 3 * sizeof(float)},
                // Attributes from buffer 2
                {vtx_buf2.ref, VTX_NOR_LOC, 4, Ren::eType::Int16_snorm, buf2_stride, 0},
                {vtx_buf2.ref, VTX_TAN_LOC, 2, Ren::eType::Int16_snorm, buf2_stride, 4 * sizeof(uint16_t)},
                {vtx_buf2.ref, VTX_AUX_LOC, 1, Ren::eType::Uint32, buf2_stride, 6 * sizeof(uint16_t)}};
            vi_vegetation = sh.LoadVertexInput(attribs, ndx_buf.ref);
        }

        Ren::ProgramRef gbuf_simple_prog = sh.LoadProgram(
            bindless ? "internal/gbuffer_fill.vert.glsl" : "internal/gbuffer_fill@NO_BINDLESS.vert.glsl",
            bindless ? "internal/gbuffer_fill.frag.glsl" : "internal/gbuffer_fill@NO_BINDLESS.frag.glsl");
        Ren::ProgramRef gbuf_vegetation_prog = sh.LoadProgram(
            bindless ? "internal/gbuffer_fill@VEGETATION.vert.glsl"
                     : "internal/gbuffer_fill@VEGETATION;NO_BINDLESS.vert.glsl",
            bindless ? "internal/gbuffer_fill.frag.glsl" : "internal/gbuffer_fill@NO_BINDLESS.frag.glsl");

        Ren::RenderPassRef rp_main_draw = sh.LoadRenderPass(depth_target, color_targets);

        { // simple and skinned
            Ren::RastState rast_state;
            rast_state.depth.test_enabled = true;
            rast_state.depth.compare_op = uint8_t(Ren::eCompareOp::Equal);

            pi_simple_[2] = sh.LoadPipeline(rast_state, gbuf_simple_prog, vi_simple, rp_main_draw, 0);

            rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

            pi_simple_[0] = sh.LoadPipeline(rast_state, gbuf_simple_prog, vi_simple, rp_main_draw, 0);

            rast_state.poly.cull = uint8_t(Ren::eCullFace::Front);

            pi_simple_[1] = sh.LoadPipeline(rast_state, gbuf_simple_prog, vi_simple, rp_main_draw, 0);
        }
        { // vegetation
            Ren::RastState rast_state;
            rast_state.depth.test_enabled = true;
            rast_state.depth.compare_op = uint8_t(Ren::eCompareOp::Equal);

            pi_vegetation_[1] = sh.LoadPipeline(rast_state, gbuf_vegetation_prog, vi_vegetation, rp_main_draw, 0);

            rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

            pi_vegetation_[0] = sh.LoadPipeline(rast_state, gbuf_vegetation_prog, vi_vegetation, rp_main_draw, 0);
        }

        initialized = true;
    }

    fb_to_use_ = (fb_to_use_ + 1) % 2;

    if (!main_draw_fb_[ctx.backend_frame()][fb_to_use_].Setup(ctx.api_ctx(), *pi_simple_[0]->render_pass(),
                                                              depth_tex.desc.w, depth_tex.desc.h, depth_target,
                                                              depth_target, color_targets, ctx.log())) {
        ctx.log()->Error("[ExGBufferFill::LazyInit]: main_draw_fb_ init failed!");
    }
}