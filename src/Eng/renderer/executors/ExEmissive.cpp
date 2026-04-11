#include "ExEmissive.h"

#include <Ren/Context.h>

#include "../../utils/ShaderLoader.h"
#include "../Renderer_DrawList.h"
#include "../framegraph/FgBuilder.h"

void Eng::ExEmissive::Execute(const FgContext &fg) {
    const Ren::ImageRWHandle color = fg.AccessRWImage(out_color_);
    const Ren::ImageRWHandle depth = fg.AccessRWImage(out_depth_);

    LazyInit(fg.ren_ctx(), fg.sh(), color, depth);
    DrawOpaque(fg, color, depth);
}

void Eng::ExEmissive::LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh, const Ren::ImageRWHandle color,
                               const Ren::ImageRWHandle depth) {
    const Ren::RenderTarget color_targets[] = {{color, Ren::eLoadOp::Load, Ren::eStoreOp::Store}};
    const Ren::RenderTarget depth_target = {depth, Ren::eLoadOp::Load, Ren::eStoreOp::Store, Ren::eLoadOp::Load,
                                            Ren::eStoreOp::Store};

    if (!initialized) {
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
                {0, VTX_UV1_LOC, 2, Ren::eType::Float16, buf1_stride, 0, 3 * sizeof(float)}};
            vi_simple = sh.FindOrCreateVertexInput(attribs);
        }

        { // VertexInput for vegetation meshes (uses additional vertex color attribute)
            const Ren::VtxAttribDesc attribs[] = {
                {0, VTX_POS_LOC, 3, Ren::eType::Float32, buf1_stride, 0, 0},
                {0, VTX_UV1_LOC, 2, Ren::eType::Float16, buf1_stride, 0, 3 * sizeof(float)},
                {1, VTX_AUX_LOC, 1, Ren::eType::Uint32, buf2_stride, 0, 6 * sizeof(uint16_t)}};
            vi_vegetation = sh.FindOrCreateVertexInput(attribs);
        }

        const Ren::ProgramHandle emissive_simple_prog = sh.FindOrCreateProgram(
            bindless ? "internal/emissive.vert.glsl" : "internal/emissive@NO_BINDLESS.vert.glsl",
            bindless ? "internal/emissive.frag.glsl" : "internal/emissive@NO_BINDLESS.frag.glsl");
        const Ren::ProgramHandle emissive_vegetation_prog = sh.FindOrCreateProgram(
            bindless ? "internal/emissive@VEGETATION.vert.glsl" : "internal/emissive@VEGETATION;NO_BINDLESS.vert.glsl",
            bindless ? "internal/emissive.frag.glsl" : "internal/emissive@NO_BINDLESS.frag.glsl");

        const Ren::RenderPassHandle rp_main_draw = sh.FindOrCreateRenderPass(depth_target, color_targets);

        { // simple and skinned
            Ren::RastState rast_state;
            rast_state.depth.test_enabled = true;
            rast_state.depth.compare_op = uint8_t(Ren::eCompareOp::Equal);
            rast_state.blend.enabled = true;
            rast_state.blend.src_color = rast_state.blend.src_alpha = uint8_t(Ren::eBlendFactor::One);
            rast_state.blend.dst_color = rast_state.blend.dst_alpha = uint8_t(Ren::eBlendFactor::One);
            rast_state.stencil.enabled = true;
            rast_state.stencil.pass = uint8_t(Ren::eStencilOp::Replace);
            rast_state.stencil.compare_mask = rast_state.stencil.write_mask = STENCIL_EMISSIVE_BIT;
            rast_state.stencil.reference = STENCIL_EMISSIVE_BIT;

            pi_simple_[2] = sh.FindOrCreatePipeline(rast_state, emissive_simple_prog, vi_simple, rp_main_draw, 0);

            rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

            pi_simple_[0] = sh.FindOrCreatePipeline(rast_state, emissive_simple_prog, vi_simple, rp_main_draw, 0);

            rast_state.poly.cull = uint8_t(Ren::eCullFace::Front);

            pi_simple_[1] = sh.FindOrCreatePipeline(rast_state, emissive_simple_prog, vi_simple, rp_main_draw, 0);
        }
        { // vegetation
            Ren::RastState rast_state;
            rast_state.depth.test_enabled = true;
            rast_state.depth.compare_op = uint8_t(Ren::eCompareOp::Equal);
            rast_state.blend.enabled = true;
            rast_state.blend.src_color = rast_state.blend.src_alpha = uint8_t(Ren::eBlendFactor::One);
            rast_state.blend.dst_color = rast_state.blend.dst_alpha = uint8_t(Ren::eBlendFactor::One);
            rast_state.stencil.enabled = true;
            rast_state.stencil.pass = uint8_t(Ren::eStencilOp::Replace);
            rast_state.stencil.compare_mask = rast_state.stencil.write_mask = STENCIL_EMISSIVE_BIT;
            rast_state.stencil.reference = STENCIL_EMISSIVE_BIT;

            pi_vegetation_[1] =
                sh.FindOrCreatePipeline(rast_state, emissive_vegetation_prog, vi_vegetation, rp_main_draw, 0);

            rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

            pi_vegetation_[0] =
                sh.FindOrCreatePipeline(rast_state, emissive_vegetation_prog, vi_vegetation, rp_main_draw, 0);
        }

        initialized = true;
    }
}