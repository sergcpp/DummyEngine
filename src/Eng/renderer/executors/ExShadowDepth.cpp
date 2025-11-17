#include "ExShadowDepth.h"

#include <Ren/Context.h>

#include "../../utils/ShaderLoader.h"
#include "../Renderer_Structs.h"

void Eng::ExShadowDepth::Execute(FgContext &fg) {
    Ren::WeakBufRef vtx_buf1 = fg.AccessROBufferRef(vtx_buf1_);
    Ren::WeakBufRef vtx_buf2 = fg.AccessROBufferRef(vtx_buf2_);
    Ren::WeakBufRef ndx_buf = fg.AccessROBufferRef(ndx_buf_);

    Ren::WeakTexRef shadow_depth_tex = fg.AccessRWTextureRef(shadow_depth_tex_);

    LazyInit(fg.ren_ctx(), fg.sh(), vtx_buf1, vtx_buf2, ndx_buf, shadow_depth_tex);
    DrawShadowMaps(fg);
}

void Eng::ExShadowDepth::LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh, const Ren::WeakBufRef &vtx_buf1,
                                  const Ren::WeakBufRef &vtx_buf2, const Ren::WeakBufRef &ndx_buf,
                                  const Ren::WeakTexRef &shadow_depth_tex) {
    if (!initialized) {
#if defined(REN_GL_BACKEND)
        const bool bindless = ctx.capabilities.bindless_texture;
#else
        const bool bindless = true;
#endif

        const int buf1_stride = 16, buf2_stride = 16;

        Ren::VertexInputRef vi_depth_pass_solid, vi_depth_pass_vege_solid, vi_depth_pass_alpha,
            vi_depth_pass_vege_alpha;

        { // VertexInput for solid shadow pass (uses position attribute only)
            const Ren::VtxAttribDesc attribs[] = {{vtx_buf1, VTX_POS_LOC, 3, Ren::eType::Float32, buf1_stride, 0}};
            vi_depth_pass_solid = sh.LoadVertexInput(attribs, ndx_buf);
        }

        { // VertexInput for for solid shadow pass of vegetation (uses position and secondary uv attributes)
            const Ren::VtxAttribDesc attribs[] = {
                {vtx_buf1, VTX_POS_LOC, 3, Ren::eType::Float32, buf1_stride, 0},
                {vtx_buf2, VTX_AUX_LOC, 1, Ren::eType::Uint32, buf2_stride, 6 * sizeof(uint16_t)}};
            vi_depth_pass_vege_solid = sh.LoadVertexInput(attribs, ndx_buf);
        }

        { // VertexInput for for alpha-tested shadow pass (uses position and uv attributes)
            const Ren::VtxAttribDesc attribs[] = {
                {vtx_buf1, VTX_POS_LOC, 3, Ren::eType::Float32, buf1_stride, 0},
                {vtx_buf1, VTX_UV1_LOC, 2, Ren::eType::Float16, buf1_stride, 3 * sizeof(float)}};
            vi_depth_pass_alpha = sh.LoadVertexInput(attribs, ndx_buf);
        }

        { // VertexInput for for alpha-tested shadow pass of vegetation (uses position, primary and
          // secondary uv attributes)
            const Ren::VtxAttribDesc attribs[] = {
                {vtx_buf1, VTX_POS_LOC, 3, Ren::eType::Float32, buf1_stride, 0},
                {vtx_buf1, VTX_UV1_LOC, 2, Ren::eType::Float16, buf1_stride, 3 * sizeof(float)},
                {vtx_buf2, VTX_AUX_LOC, 1, Ren::eType::Uint32, buf2_stride, 6 * sizeof(uint16_t)}};
            vi_depth_pass_vege_alpha = sh.LoadVertexInput(attribs, ndx_buf);
        }

        Ren::ProgramRef shadow_solid_prog =
            sh.LoadProgram("internal/shadow_depth.vert.glsl", "internal/shadow_depth.frag.glsl");
        Ren::ProgramRef shadow_vege_solid_prog = sh.LoadProgram(
            bindless ? "internal/shadow_depth_vege.vert.glsl" : "internal/shadow_depth_vege@NO_BINDLESS.vert.glsl",
            "internal/shadow_depth.frag.glsl");
        Ren::ProgramRef shadow_alpha_prog =
            sh.LoadProgram(bindless ? "internal/shadow_depth@ALPHATEST.vert.glsl"
                                    : "internal/shadow_depth@ALPHATEST;NO_BINDLESS.vert.glsl",
                           bindless ? "internal/shadow_depth@ALPHATEST.frag.glsl"
                                    : "internal/shadow_depth@ALPHATEST;NO_BINDLESS.frag.glsl");
        Ren::ProgramRef shadow_vege_alpha_prog =
            sh.LoadProgram(bindless ? "internal/shadow_depth_vege@ALPHATEST.vert.glsl"
                                    : "internal/shadow_depth_vege@ALPHATEST;NO_BINDLESS.vert.glsl",
                           bindless ? "internal/shadow_depth@ALPHATEST.frag.glsl"
                                    : "internal/shadow_depth@ALPHATEST;NO_BINDLESS.frag.glsl");

        const Ren::RenderTarget depth_target = {shadow_depth_tex, Ren::eLoadOp::Load, Ren::eStoreOp::Store};

        Ren::RenderPassRef rp_depth_only = sh.LoadRenderPass(depth_target, {});

        { // solid/alpha-tested
            Ren::RastState rast_state;
            rast_state.poly.cull = uint8_t(Ren::eCullFace::None);
            rast_state.poly.depth_bias_mode = uint8_t(Ren::eDepthBiasMode::Dynamic);

            rast_state.depth.test_enabled = true;
            rast_state.depth.compare_op = unsigned(Ren::eCompareOp::Greater);
            rast_state.scissor.enabled = true;

            pi_solid_[2] = sh.LoadPipeline(rast_state, shadow_solid_prog, vi_depth_pass_solid, rp_depth_only, 0);
            pi_alpha_[2] = sh.LoadPipeline(rast_state, shadow_alpha_prog, vi_depth_pass_alpha, rp_depth_only, 0);

            pi_vege_solid_ =
                sh.LoadPipeline(rast_state, shadow_vege_solid_prog, vi_depth_pass_vege_solid, rp_depth_only, 0);
            pi_vege_alpha_ =
                sh.LoadPipeline(rast_state, shadow_vege_alpha_prog, vi_depth_pass_vege_alpha, rp_depth_only, 0);

            rast_state.poly.cull = uint8_t(Ren::eCullFace::Front);

            pi_solid_[0] = sh.LoadPipeline(rast_state, shadow_solid_prog, vi_depth_pass_solid, rp_depth_only, 0);
            pi_alpha_[0] = sh.LoadPipeline(rast_state, shadow_alpha_prog, vi_depth_pass_alpha, rp_depth_only, 0);

            rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

            pi_solid_[1] = sh.LoadPipeline(rast_state, shadow_solid_prog, vi_depth_pass_solid, rp_depth_only, 0);
            pi_alpha_[1] = sh.LoadPipeline(rast_state, shadow_alpha_prog, vi_depth_pass_alpha, rp_depth_only, 0);
        }
        initialized = true;
    }

    if (!shadow_fb_.Setup(ctx.api_ctx(), *pi_solid_[0]->render_pass(), w_, h_, shadow_depth_tex, {},
                          Ren::Span<const Ren::WeakTexRef>{}, false, ctx.log())) {
        ctx.log()->Error("ExShadowMaps: shadow_fb_ init failed!");
    }
}
