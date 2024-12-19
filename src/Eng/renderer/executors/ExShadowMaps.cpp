#include "ExShadowMaps.h"

#include <Ren/Context.h>

#include "../../utils/ShaderLoader.h"
#include "../Renderer_Structs.h"

void Eng::ExShadowMaps::Execute(FgBuilder &builder) {
    FgAllocBuf &vtx_buf1 = builder.GetReadBuffer(vtx_buf1_);
    FgAllocBuf &vtx_buf2 = builder.GetReadBuffer(vtx_buf2_);
    FgAllocBuf &ndx_buf = builder.GetReadBuffer(ndx_buf_);

    FgAllocTex &shadowmap_tex = builder.GetWriteTexture(shadowmap_tex_);

    LazyInit(builder.ctx(), builder.sh(), vtx_buf1, vtx_buf2, ndx_buf, shadowmap_tex);
    DrawShadowMaps(builder, shadowmap_tex);
}

void Eng::ExShadowMaps::LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh, FgAllocBuf &vtx_buf1, FgAllocBuf &vtx_buf2,
                                 FgAllocBuf &ndx_buf, FgAllocTex &shadowmap_tex) {
    if (!initialized) {
#if defined(REN_GL_BACKEND)
        const bool bindless = ctx.capabilities.bindless_texture;
#else
        const bool bindless = true;
#endif

        const int buf1_stride = 16, buf2_stride = 16;

        Ren::VertexInputRef vi_depth_pass_solid, vi_depth_pass_vege_solid, vi_depth_pass_transp,
            vi_depth_pass_vege_transp;

        { // VertexInput for solid shadow pass (uses position attribute only)
            const Ren::VtxAttribDesc attribs[] = {{vtx_buf1.ref, VTX_POS_LOC, 3, Ren::eType::Float32, buf1_stride, 0}};
            vi_depth_pass_solid = sh.LoadVertexInput(attribs, ndx_buf.ref);
        }

        { // VertexInput for for solid shadow pass of vegetation (uses position and secondary uv attributes)
            const Ren::VtxAttribDesc attribs[] = {
                {vtx_buf1.ref, VTX_POS_LOC, 3, Ren::eType::Float32, buf1_stride, 0},
                {vtx_buf2.ref, VTX_AUX_LOC, 1, Ren::eType::Uint32, buf2_stride, 6 * sizeof(uint16_t)}};
            vi_depth_pass_vege_solid = sh.LoadVertexInput(attribs, ndx_buf.ref);
        }

        { // VertexInput for for alpha-tested shadow pass (uses position and uv attributes)
            const Ren::VtxAttribDesc attribs[] = {
                {vtx_buf1.ref, VTX_POS_LOC, 3, Ren::eType::Float32, buf1_stride, 0},
                {vtx_buf1.ref, VTX_UV1_LOC, 2, Ren::eType::Float16, buf1_stride, 3 * sizeof(float)}};
            vi_depth_pass_transp = sh.LoadVertexInput(attribs, ndx_buf.ref);
        }

        { // VertexInput for for transparent shadow pass of vegetation (uses position, primary and
          // secondary uv attributes)
            const Ren::VtxAttribDesc attribs[] = {
                {vtx_buf1.ref, VTX_POS_LOC, 3, Ren::eType::Float32, buf1_stride, 0},
                {vtx_buf1.ref, VTX_UV1_LOC, 2, Ren::eType::Float16, buf1_stride, 3 * sizeof(float)},
                {vtx_buf2.ref, VTX_AUX_LOC, 1, Ren::eType::Uint32, buf2_stride, 6 * sizeof(uint16_t)}};
            vi_depth_pass_vege_transp = sh.LoadVertexInput(attribs, ndx_buf.ref);
        }

        Ren::ProgramRef shadow_solid_prog = sh.LoadProgram("internal/shadow.vert.glsl", "internal/shadow.frag.glsl");
        Ren::ProgramRef shadow_vege_solid_prog =
            sh.LoadProgram(bindless ? "internal/shadow_vege.vert.glsl" : "internal/shadow_vege@NO_BINDLESS.vert.glsl",
                           "internal/shadow.frag.glsl");
        Ren::ProgramRef shadow_transp_prog = sh.LoadProgram(
            bindless ? "internal/shadow@ALPHATEST.vert.glsl" : "internal/shadow@ALPHATEST;NO_BINDLESS.vert.glsl",
            bindless ? "internal/shadow@ALPHATEST.frag.glsl" : "internal/shadow@ALPHATEST;NO_BINDLESS.frag.glsl");
        Ren::ProgramRef shadow_vege_transp_prog = sh.LoadProgram(
            bindless ? "internal/shadow_vege@ALPHATEST.vert.glsl"
                     : "internal/shadow_vege@ALPHATEST;NO_BINDLESS.vert.glsl",
            bindless ? "internal/shadow@ALPHATEST.frag.glsl" : "internal/shadow@ALPHATEST;NO_BINDLESS.frag.glsl");

        const Ren::RenderTarget depth_target = {shadowmap_tex.ref, Ren::eLoadOp::Load, Ren::eStoreOp::Store};

        Ren::RenderPassRef rp_depth_only = sh.LoadRenderPass(depth_target, {});

        { // solid/transp
            Ren::RastState rast_state;
            rast_state.poly.cull = uint8_t(Ren::eCullFace::None);
            rast_state.poly.depth_bias_mode = uint8_t(Ren::eDepthBiasMode::Dynamic);

            rast_state.depth.test_enabled = true;
            rast_state.depth.compare_op = unsigned(Ren::eCompareOp::Greater);
            rast_state.scissor.enabled = true;

            pi_solid_[2] = sh.LoadPipeline(rast_state, shadow_solid_prog, vi_depth_pass_solid, rp_depth_only, 0);
            pi_transp_[2] = sh.LoadPipeline(rast_state, shadow_transp_prog, vi_depth_pass_transp, rp_depth_only, 0);

            pi_vege_solid_ =
                sh.LoadPipeline(rast_state, shadow_vege_solid_prog, vi_depth_pass_vege_solid, rp_depth_only, 0);
            pi_vege_transp_ =
                sh.LoadPipeline(rast_state, shadow_vege_transp_prog, vi_depth_pass_vege_transp, rp_depth_only, 0);

            rast_state.poly.cull = uint8_t(Ren::eCullFace::Front);

            pi_solid_[0] = sh.LoadPipeline(rast_state, shadow_solid_prog, vi_depth_pass_solid, rp_depth_only, 0);
            pi_transp_[0] = sh.LoadPipeline(rast_state, shadow_transp_prog, vi_depth_pass_transp, rp_depth_only, 0);

            rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

            pi_solid_[1] = sh.LoadPipeline(rast_state, shadow_solid_prog, vi_depth_pass_solid, rp_depth_only, 0);
            pi_transp_[1] = sh.LoadPipeline(rast_state, shadow_transp_prog, vi_depth_pass_transp, rp_depth_only, 0);
        }
        initialized = true;
    }

    if (!shadow_fb_.Setup(ctx.api_ctx(), *pi_solid_[0]->render_pass(), w_, h_, shadowmap_tex.ref, {},
                          Ren::Span<const Ren::WeakTex2DRef>{}, false, ctx.log())) {
        ctx.log()->Error("ExShadowMaps: shadow_fb_ init failed!");
    }
}
