#include "ExShadowColor.h"

#include <Ren/Context.h>

#include "../../utils/ShaderLoader.h"
#include "../Renderer_Structs.h"
#include "../framegraph/FgBuilder.h"

void Eng::ExShadowColor::Execute(const FgContext &fg) {
    const Ren::ImageRWHandle shadow_depth = fg.AccessRWImage(args_->shadow_depth);
    const Ren::ImageRWHandle shadow_color = fg.AccessRWImage(args_->shadow_color);

    LazyInit(fg, shadow_depth, shadow_color);
    DrawShadowMaps(fg, shadow_depth, shadow_color);
}

void Eng::ExShadowColor::LazyInit(const FgContext &fg, const Ren::ImageRWHandle shadow_depth,
                                  const Ren::ImageRWHandle shadow_color) {
    const Ren::RenderTarget depth_target = {shadow_depth, Ren::eLoadOp::Load, Ren::eStoreOp::Store};
    const Ren::RenderTarget color_targets[] = {{shadow_color, Ren::eLoadOp::Load, Ren::eStoreOp::Store}};

    if (!initialized_) {
        auto &ctx = fg.ren_ctx();
        auto &sh = fg.sh();
#if defined(REN_GL_BACKEND)
        const bool bindless = ctx.capabilities.bindless_texture;
#else
        const bool bindless = true;
#endif

        const int buf1_stride = 16, buf2_stride = 16;

        Ren::VertexInputHandle vi_depth_pass, vi_depth_pass_vege;
        { // VertexInput for for alpha-tested shadow pass (uses position and uv attributes)
            const Ren::VtxAttribDesc attribs[] = {
                {0, VTX_POS_LOC, 3, Ren::eType::Float32, buf1_stride, 0, 0},
                {0, VTX_UV1_LOC, 2, Ren::eType::Float16, buf1_stride, 0, 3 * sizeof(float)}};
            vi_depth_pass = sh.FindOrCreateVertexInput(attribs);
        }
        { // VertexInput for for alpha-tested shadow pass of vegetation (uses position, primary and
          // secondary uv attributes)
            const Ren::VtxAttribDesc attribs[] = {
                {0, VTX_POS_LOC, 3, Ren::eType::Float32, buf1_stride, 0, 0},
                {0, VTX_UV1_LOC, 2, Ren::eType::Float16, buf1_stride, 0, 3 * sizeof(float)},
                {1, VTX_AUX_LOC, 1, Ren::eType::Uint32, buf2_stride, 0, 6 * sizeof(uint16_t)}};
            vi_depth_pass_vege = sh.FindOrCreateVertexInput(attribs);
        }

        const Ren::ProgramHandle shadow_solid_prog = sh.FindOrCreateProgram(
            bindless ? "internal/shadow_color.vert.glsl" : "internal/shadow_color@NO_BINDLESS.vert.glsl",
            "internal/shadow_color.frag.glsl");
        const Ren::ProgramHandle shadow_alpha_prog = sh.FindOrCreateProgram(
            bindless ? "internal/shadow_color.vert.glsl" : "internal/shadow_color@NO_BINDLESS.vert.glsl",
            bindless ? "internal/shadow_color@ALPHATEST.frag.glsl"
                     : "internal/shadow_color@ALPHATEST;NO_BINDLESS.frag.glsl");

        const Ren::RenderPassHandle rp_depth_only = sh.FindOrCreateRenderPass(depth_target, color_targets);

        { // solid/alpha-tested
            Ren::RastState rast_state;
            rast_state.poly.cull = uint8_t(Ren::eCullFace::None);
            rast_state.poly.depth_bias_mode = uint8_t(Ren::eDepthBiasMode::Dynamic);

            rast_state.blend.enabled = true;
            rast_state.blend.color_op = uint8_t(Ren::eBlendOp::Add);
            rast_state.blend.src_color = uint8_t(Ren::eBlendFactor::Zero);
            rast_state.blend.dst_color = uint8_t(Ren::eBlendFactor::SrcColor);
            rast_state.blend.alpha_op = uint8_t(Ren::eBlendOp::Max);
            rast_state.blend.src_alpha = uint8_t(Ren::eBlendFactor::One);
            rast_state.blend.dst_alpha = uint8_t(Ren::eBlendFactor::One);

            rast_state.depth.write_enabled = false;
            rast_state.depth.test_enabled = true;
            rast_state.depth.compare_op = unsigned(Ren::eCompareOp::Greater);
            rast_state.scissor.enabled = true;

            pi_solid_[2] = sh.FindOrCreatePipeline(rast_state, shadow_solid_prog, vi_depth_pass, rp_depth_only, 0);
            pi_alpha_[2] = sh.FindOrCreatePipeline(rast_state, shadow_alpha_prog, vi_depth_pass, rp_depth_only, 0);

            // pi_vege_solid_ =
            //     sh.FindOrCreatePipeline(rast_state, shadow_vege_solid_prog, vi_depth_pass_vege_solid, rp_depth_only,
            //     0);
            // pi_vege_alpha_ =
            //     sh.FindOrCreatePipeline(rast_state, shadow_vege_alpha_prog, vi_depth_pass_vege_alpha, rp_depth_only,
            //     0);

            rast_state.poly.cull = uint8_t(Ren::eCullFace::Front);

            pi_solid_[0] = sh.FindOrCreatePipeline(rast_state, shadow_solid_prog, vi_depth_pass, rp_depth_only, 0);
            pi_alpha_[0] = sh.FindOrCreatePipeline(rast_state, shadow_alpha_prog, vi_depth_pass, rp_depth_only, 0);

            rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

            pi_solid_[1] = sh.FindOrCreatePipeline(rast_state, shadow_solid_prog, vi_depth_pass, rp_depth_only, 0);
            pi_alpha_[1] = sh.FindOrCreatePipeline(rast_state, shadow_alpha_prog, vi_depth_pass, rp_depth_only, 0);
        }
        initialized_ = true;
    }
}
