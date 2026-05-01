#include "ExGBufferFill.h"

#include <Ren/Context.h>

#include "../../utils/ShaderLoader.h"
#include "../Renderer_Structs.h"
#include "../framegraph/FgBuilder.h"

void Eng::ExGBufferFill::Execute(const FgContext &fg) {
    const Ren::ImageRWHandle albedo = fg.AccessRWImage(args_->out_albedo);
    const Ren::ImageRWHandle normal = fg.AccessRWImage(args_->out_normal);
    const Ren::ImageRWHandle spec = fg.AccessRWImage(args_->out_spec);
    const Ren::ImageRWHandle depth = fg.AccessRWImage(args_->out_depth);

    LazyInit(fg, albedo, normal, spec, depth);
    DrawOpaque(fg, albedo, normal, spec, depth);
}

void Eng::ExGBufferFill::LazyInit(const FgContext &fg, const Ren::ImageRWHandle albedo,
                                  const Ren::ImageRWHandle normal, const Ren::ImageRWHandle spec,
                                  const Ren::ImageRWHandle depth) {
    const Ren::RenderTarget color_targets[] = {{albedo, Ren::eLoadOp::Clear, Ren::eStoreOp::Store},
                                               {normal, Ren::eLoadOp::Clear, Ren::eStoreOp::Store},
                                               {spec, Ren::eLoadOp::Clear, Ren::eStoreOp::Store}};
    const Ren::RenderTarget depth_target = {depth, Ren::eLoadOp::Load, Ren::eStoreOp::Store, Ren::eLoadOp::Load,
                                            Ren::eStoreOp::Store};

    if (!initialized_) {
        auto &ctx = fg.ren_ctx();
        auto &sh = fg.sh();
#if defined(REN_GL_BACKEND)
        const bool bindless = ctx.capabilities.bindless_texture;
#else
        const bool bindless = true;
#endif

        const int buf1_stride = 16, buf2_stride = 16;

        Ren::VertexInputHandle vi_simple, vi_vegetation;

        { // VertexInput for simple and skinned meshes
            const Ren::VtxAttribDesc attribs[] = {
                {0, VTX_POS_LOC, 3, Ren::eType::Float32, buf1_stride, 0, 0},
                {0, VTX_UV1_LOC, 2, Ren::eType::Float16, buf1_stride, 0, 3 * sizeof(float)},
                {1, VTX_NOR_LOC, 4, Ren::eType::Int16_snorm, buf2_stride, 0, 0},
                {1, VTX_TAN_LOC, 2, Ren::eType::Int16_snorm, buf2_stride, 0, 4 * sizeof(uint16_t)}};
            vi_simple = sh.FindOrCreateVertexInput(attribs);
        }
        { // VertexInput for vegetation meshes (uses additional vertex color attribute)
            const Ren::VtxAttribDesc attribs[] = {
                {0, VTX_POS_LOC, 3, Ren::eType::Float32, buf1_stride, 0, 0},
                {0, VTX_UV1_LOC, 2, Ren::eType::Float16, buf1_stride, 0, 3 * sizeof(float)},
                {1, VTX_NOR_LOC, 4, Ren::eType::Int16_snorm, buf2_stride, 0, 0},
                {1, VTX_TAN_LOC, 2, Ren::eType::Int16_snorm, buf2_stride, 0, 4 * sizeof(uint16_t)},
                {1, VTX_AUX_LOC, 1, Ren::eType::Uint32, buf2_stride, 0, 6 * sizeof(uint16_t)}};
            vi_vegetation = sh.FindOrCreateVertexInput(attribs);
        }

        const Ren::ProgramHandle gbuf_simple_prog = sh.FindOrCreateProgram(
            bindless ? "internal/gbuffer_fill.vert.glsl" : "internal/gbuffer_fill@NO_BINDLESS.vert.glsl",
            bindless ? "internal/gbuffer_fill.frag.glsl" : "internal/gbuffer_fill@NO_BINDLESS.frag.glsl");
        const Ren::ProgramHandle gbuf_vegetation_prog = sh.FindOrCreateProgram(
            bindless ? "internal/gbuffer_fill@VEGETATION.vert.glsl"
                     : "internal/gbuffer_fill@VEGETATION;NO_BINDLESS.vert.glsl",
            bindless ? "internal/gbuffer_fill.frag.glsl" : "internal/gbuffer_fill@NO_BINDLESS.frag.glsl");

        const Ren::RenderPassHandle rp_main_draw = sh.FindOrCreateRenderPass(depth_target, color_targets);

        { // simple and skinned
            Ren::RastState rast_state;
            rast_state.depth.test_enabled = true;
            rast_state.depth.compare_op = uint8_t(Ren::eCompareOp::Equal);

            pi_simple_[2] = sh.FindOrCreatePipeline(rast_state, gbuf_simple_prog, vi_simple, rp_main_draw, 0);

            rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

            pi_simple_[0] = sh.FindOrCreatePipeline(rast_state, gbuf_simple_prog, vi_simple, rp_main_draw, 0);

            rast_state.poly.cull = uint8_t(Ren::eCullFace::Front);

            pi_simple_[1] = sh.FindOrCreatePipeline(rast_state, gbuf_simple_prog, vi_simple, rp_main_draw, 0);
        }
        { // vegetation
            Ren::RastState rast_state;
            rast_state.depth.test_enabled = true;
            rast_state.depth.compare_op = uint8_t(Ren::eCompareOp::Equal);

            pi_vegetation_[1] =
                sh.FindOrCreatePipeline(rast_state, gbuf_vegetation_prog, vi_vegetation, rp_main_draw, 0);

            rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

            pi_vegetation_[0] =
                sh.FindOrCreatePipeline(rast_state, gbuf_vegetation_prog, vi_vegetation, rp_main_draw, 0);
        }

        initialized_ = true;
    }
}