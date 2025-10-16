#include "ExShadowColor.h"

#include <Ren/Context.h>

#include "../../utils/ShaderLoader.h"
#include "../Renderer_Structs.h"

void Eng::ExShadowColor::Execute(FgContext &fg) {
    FgAllocBuf &vtx_buf1 = fg.AccessROBuffer(vtx_buf1_);
    FgAllocBuf &vtx_buf2 = fg.AccessROBuffer(vtx_buf2_);
    FgAllocBuf &ndx_buf = fg.AccessROBuffer(ndx_buf_);

    FgAllocTex &shadow_depth_tex = fg.AccessRWTexture(shadow_depth_tex_);
    FgAllocTex &shadow_color_tex = fg.AccessRWTexture(shadow_color_tex_);

    LazyInit(fg.ren_ctx(), fg.sh(), vtx_buf1, vtx_buf2, ndx_buf, shadow_depth_tex, shadow_color_tex);
    DrawShadowMaps(fg);
}

void Eng::ExShadowColor::LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh, FgAllocBuf &vtx_buf1, FgAllocBuf &vtx_buf2,
                                  FgAllocBuf &ndx_buf, FgAllocTex &shadow_depth_tex, FgAllocTex &shadow_color_tex) {
    const Ren::RenderTarget depth_target = {shadow_depth_tex.ref, Ren::eLoadOp::Load, Ren::eStoreOp::Store};
    const Ren::RenderTarget color_targets[] = {{shadow_color_tex.ref, Ren::eLoadOp::Load, Ren::eStoreOp::Store}};

    if (!initialized) {
#if defined(REN_GL_BACKEND)
        const bool bindless = ctx.capabilities.bindless_texture;
#else
        const bool bindless = true;
#endif

        const int buf1_stride = 16, buf2_stride = 16;

        Ren::VertexInputRef vi_depth_pass, vi_depth_pass_vege;

        { // VertexInput for for alpha-tested shadow pass (uses position and uv attributes)
            const Ren::VtxAttribDesc attribs[] = {
                {vtx_buf1.ref, VTX_POS_LOC, 3, Ren::eType::Float32, buf1_stride, 0},
                {vtx_buf1.ref, VTX_UV1_LOC, 2, Ren::eType::Float16, buf1_stride, 3 * sizeof(float)}};
            vi_depth_pass = sh.LoadVertexInput(attribs, ndx_buf.ref);
        }

        { // VertexInput for for alpha-tested shadow pass of vegetation (uses position, primary and
          // secondary uv attributes)
            const Ren::VtxAttribDesc attribs[] = {
                {vtx_buf1.ref, VTX_POS_LOC, 3, Ren::eType::Float32, buf1_stride, 0},
                {vtx_buf1.ref, VTX_UV1_LOC, 2, Ren::eType::Float16, buf1_stride, 3 * sizeof(float)},
                {vtx_buf2.ref, VTX_AUX_LOC, 1, Ren::eType::Uint32, buf2_stride, 6 * sizeof(uint16_t)}};
            vi_depth_pass_vege = sh.LoadVertexInput(attribs, ndx_buf.ref);
        }

        Ren::ProgramRef shadow_solid_prog =
            sh.LoadProgram(bindless ? "internal/shadow_color.vert.glsl" : "internal/shadow_color@NO_BINDLESS.vert.glsl",
                           "internal/shadow_color.frag.glsl");
        Ren::ProgramRef shadow_alpha_prog =
            sh.LoadProgram(bindless ? "internal/shadow_color.vert.glsl" : "internal/shadow_color@NO_BINDLESS.vert.glsl",
                           bindless ? "internal/shadow_color@ALPHATEST.frag.glsl"
                                    : "internal/shadow_color@ALPHATEST;NO_BINDLESS.frag.glsl");

        Ren::RenderPassRef rp_depth_only = sh.LoadRenderPass(depth_target, color_targets);

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

            pi_solid_[2] = sh.LoadPipeline(rast_state, shadow_solid_prog, vi_depth_pass, rp_depth_only, 0);
            pi_alpha_[2] = sh.LoadPipeline(rast_state, shadow_alpha_prog, vi_depth_pass, rp_depth_only, 0);

            // pi_vege_solid_ =
            //     sh.LoadPipeline(rast_state, shadow_vege_solid_prog, vi_depth_pass_vege_solid, rp_depth_only, 0);
            // pi_vege_alpha_ =
            //     sh.LoadPipeline(rast_state, shadow_vege_alpha_prog, vi_depth_pass_vege_alpha, rp_depth_only, 0);

            rast_state.poly.cull = uint8_t(Ren::eCullFace::Front);

            pi_solid_[0] = sh.LoadPipeline(rast_state, shadow_solid_prog, vi_depth_pass, rp_depth_only, 0);
            pi_alpha_[0] = sh.LoadPipeline(rast_state, shadow_alpha_prog, vi_depth_pass, rp_depth_only, 0);

            rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

            pi_solid_[1] = sh.LoadPipeline(rast_state, shadow_solid_prog, vi_depth_pass, rp_depth_only, 0);
            pi_alpha_[1] = sh.LoadPipeline(rast_state, shadow_alpha_prog, vi_depth_pass, rp_depth_only, 0);
        }
        initialized = true;
    }

    if (!shadow_fb_.Setup(ctx.api_ctx(), *pi_solid_[0]->render_pass(), w_, h_, depth_target, {}, color_targets,
                          ctx.log())) {
        ctx.log()->Error("ExShadowMaps: shadow_fb_ init failed!");
    }
}
