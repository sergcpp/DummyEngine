#include "ExShadowDepth.h"

#include <Ren/Context.h>

#include "../../utils/ShaderLoader.h"
#include "../Renderer_Structs.h"
#include "../framegraph/FgBuilder.h"

void Eng::ExShadowDepth::Execute(const FgContext &fg) {
    const Ren::ImageRWHandle shadow_depth = fg.AccessRWImage(args_->shadow_depth);

    LazyInit(fg, shadow_depth);
    DrawShadowMaps(fg, shadow_depth);
}

void Eng::ExShadowDepth::LazyInit(const FgContext &fg, const Ren::ImageRWHandle shadow_depth) {
    if (!initialized_) {
        auto &ctx = fg.ren_ctx();
        auto &sh = fg.sh();
#if defined(REN_GL_BACKEND)
        const bool bindless = ctx.capabilities.bindless_texture;
#else
        const bool bindless = true;
#endif

        const int buf1_stride = 16, buf2_stride = 16;

        Ren::VertexInputHandle vi_depth_pass_solid, vi_depth_pass_vege_solid, vi_depth_pass_alpha,
            vi_depth_pass_vege_alpha;
        { // VertexInput for solid shadow pass (uses position attribute only)
            const Ren::VtxAttribDesc attribs[] = {{0, VTX_POS_LOC, 3, Ren::eType::Float32, buf1_stride, 0, 0}};
            vi_depth_pass_solid = sh.FindOrCreateVertexInput(attribs);
        }
        { // VertexInput for for solid shadow pass of vegetation (uses position and secondary uv attributes)
            const Ren::VtxAttribDesc attribs[] = {
                {0, VTX_POS_LOC, 3, Ren::eType::Float32, buf1_stride, 0, 0},
                {1, VTX_AUX_LOC, 1, Ren::eType::Uint32, buf2_stride, 0, 6 * sizeof(uint16_t)}};
            vi_depth_pass_vege_solid = sh.FindOrCreateVertexInput(attribs);
        }
        { // VertexInput for for alpha-tested shadow pass (uses position and uv attributes)
            const Ren::VtxAttribDesc attribs[] = {
                {0, VTX_POS_LOC, 3, Ren::eType::Float32, buf1_stride, 0, 0},
                {0, VTX_UV1_LOC, 2, Ren::eType::Float16, buf1_stride, 0, 3 * sizeof(float)}};
            vi_depth_pass_alpha = sh.FindOrCreateVertexInput(attribs);
        }
        { // VertexInput for for alpha-tested shadow pass of vegetation (uses position, primary and
          // secondary uv attributes)
            const Ren::VtxAttribDesc attribs[] = {
                {0, VTX_POS_LOC, 3, Ren::eType::Float32, buf1_stride, 0, 0},
                {0, VTX_UV1_LOC, 2, Ren::eType::Float16, buf1_stride, 0, 3 * sizeof(float)},
                {1, VTX_AUX_LOC, 1, Ren::eType::Uint32, buf2_stride, 0, 6 * sizeof(uint16_t)}};
            vi_depth_pass_vege_alpha = sh.FindOrCreateVertexInput(attribs);
        }

        const Ren::ProgramHandle shadow_solid_prog =
            sh.FindOrCreateProgram("internal/shadow_depth.vert.glsl", "internal/shadow_depth.frag.glsl");
        const Ren::ProgramHandle shadow_vege_solid_prog = sh.FindOrCreateProgram(
            bindless ? "internal/shadow_depth_vege.vert.glsl" : "internal/shadow_depth_vege@NO_BINDLESS.vert.glsl",
            "internal/shadow_depth.frag.glsl");
        const Ren::ProgramHandle shadow_alpha_prog =
            sh.FindOrCreateProgram(bindless ? "internal/shadow_depth@ALPHATEST.vert.glsl"
                                            : "internal/shadow_depth@ALPHATEST;NO_BINDLESS.vert.glsl",
                                   bindless ? "internal/shadow_depth@ALPHATEST.frag.glsl"
                                            : "internal/shadow_depth@ALPHATEST;NO_BINDLESS.frag.glsl");
        const Ren::ProgramHandle shadow_vege_alpha_prog =
            sh.FindOrCreateProgram(bindless ? "internal/shadow_depth_vege@ALPHATEST.vert.glsl"
                                            : "internal/shadow_depth_vege@ALPHATEST;NO_BINDLESS.vert.glsl",
                                   bindless ? "internal/shadow_depth@ALPHATEST.frag.glsl"
                                            : "internal/shadow_depth@ALPHATEST;NO_BINDLESS.frag.glsl");

        const Ren::RenderTarget depth_target = {shadow_depth, Ren::eLoadOp::Load, Ren::eStoreOp::Store};

        const Ren::RenderPassHandle rp_depth_only = sh.FindOrCreateRenderPass(depth_target, {});

        { // solid/alpha-tested
            Ren::RastState rast_state;
            rast_state.poly.cull = uint8_t(Ren::eCullFace::None);
            rast_state.poly.depth_bias_mode = uint8_t(Ren::eDepthBiasMode::Dynamic);

            rast_state.depth.test_enabled = true;
            rast_state.depth.compare_op = unsigned(Ren::eCompareOp::Greater);
            rast_state.scissor.enabled = true;

            pi_solid_[2] =
                sh.FindOrCreatePipeline(rast_state, shadow_solid_prog, vi_depth_pass_solid, rp_depth_only, 0);
            pi_alpha_[2] =
                sh.FindOrCreatePipeline(rast_state, shadow_alpha_prog, vi_depth_pass_alpha, rp_depth_only, 0);

            pi_vege_solid_ =
                sh.FindOrCreatePipeline(rast_state, shadow_vege_solid_prog, vi_depth_pass_vege_solid, rp_depth_only, 0);
            pi_vege_alpha_ =
                sh.FindOrCreatePipeline(rast_state, shadow_vege_alpha_prog, vi_depth_pass_vege_alpha, rp_depth_only, 0);

            rast_state.poly.cull = uint8_t(Ren::eCullFace::Front);

            pi_solid_[0] =
                sh.FindOrCreatePipeline(rast_state, shadow_solid_prog, vi_depth_pass_solid, rp_depth_only, 0);
            pi_alpha_[0] =
                sh.FindOrCreatePipeline(rast_state, shadow_alpha_prog, vi_depth_pass_alpha, rp_depth_only, 0);

            rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

            pi_solid_[1] =
                sh.FindOrCreatePipeline(rast_state, shadow_solid_prog, vi_depth_pass_solid, rp_depth_only, 0);
            pi_alpha_[1] =
                sh.FindOrCreatePipeline(rast_state, shadow_alpha_prog, vi_depth_pass_alpha, rp_depth_only, 0);
        }
        initialized_ = true;
    }
}
