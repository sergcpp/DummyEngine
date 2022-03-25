#include "RpSSRCompose2.h"

#include <Ren/Context.h>
#include <Ren/ProbeStorage.h>
#include <Ren/Program.h>
#include <Ren/RastState.h>

#include "../../Utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"

#include "../assets/shaders/internal/blit_ssr_compose2_interface.glsl"

void RpSSRCompose2::Setup(RpBuilder &builder, const ViewState *view_state, const Ren::ProbeStorage *probe_storage,
                          Ren::Tex2DRef brdf_lut, const char shared_data_buf[], const char depth_tex_name[],
                          const char normal_tex_name[], const char spec_tex_name[], const char refl_tex_name[],
                          const char output_tex_name[]) {
    view_state_ = view_state;

    probe_storage_ = probe_storage;

    shared_data_buf_ = builder.ReadBuffer(shared_data_buf, Ren::eResState::UniformBuffer,
                                          Ren::eStageBits::VertexShader | Ren::eStageBits::FragmentShader, *this);
    depth_tex_ =
        builder.ReadTexture(depth_tex_name, Ren::eResState::ShaderResource, Ren::eStageBits::FragmentShader, *this);
    normal_tex_ =
        builder.ReadTexture(normal_tex_name, Ren::eResState::ShaderResource, Ren::eStageBits::FragmentShader, *this);
    spec_tex_ =
        builder.ReadTexture(spec_tex_name, Ren::eResState::ShaderResource, Ren::eStageBits::FragmentShader, *this);
    refl_tex_ =
        builder.ReadTexture(refl_tex_name, Ren::eResState::ShaderResource, Ren::eStageBits::FragmentShader, *this);
    brdf_lut_ = builder.ReadTexture(brdf_lut, Ren::eResState::ShaderResource, Ren::eStageBits::FragmentShader, *this);

    output_tex_ =
        builder.WriteTexture(output_tex_name, Ren::eResState::RenderTarget, Ren::eStageBits::ColorAttachment, *this);
}

void RpSSRCompose2::Setup(RpBuilder &builder, const ViewState *view_state, const Ren::ProbeStorage *probe_storage,
                          Ren::Tex2DRef brdf_lut, const char shared_data_buf[], const char depth_tex_name[],
                          const char normal_tex_name[], const char spec_tex_name[], Ren::WeakTex2DRef refl_tex,
                          const char output_tex_name[]) {
    view_state_ = view_state;

    probe_storage_ = probe_storage;

    shared_data_buf_ = builder.ReadBuffer(shared_data_buf, Ren::eResState::UniformBuffer,
                                          Ren::eStageBits::VertexShader | Ren::eStageBits::FragmentShader, *this);
    depth_tex_ =
        builder.ReadTexture(depth_tex_name, Ren::eResState::ShaderResource, Ren::eStageBits::FragmentShader, *this);
    normal_tex_ =
        builder.ReadTexture(normal_tex_name, Ren::eResState::ShaderResource, Ren::eStageBits::FragmentShader, *this);
    spec_tex_ =
        builder.ReadTexture(spec_tex_name, Ren::eResState::ShaderResource, Ren::eStageBits::FragmentShader, *this);
    refl_tex_ =
        builder.ReadTexture(refl_tex, Ren::eResState::ShaderResource, Ren::eStageBits::FragmentShader, *this);
    brdf_lut_ = builder.ReadTexture(brdf_lut, Ren::eResState::ShaderResource, Ren::eStageBits::FragmentShader, *this);

    output_tex_ =
        builder.WriteTexture(output_tex_name, Ren::eResState::RenderTarget, Ren::eStageBits::ColorAttachment, *this);
}

void RpSSRCompose2::Execute(RpBuilder &builder) {
    RpAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(shared_data_buf_);
    RpAllocTex &depth_tex = builder.GetReadTexture(depth_tex_);
    RpAllocTex &normal_tex = builder.GetReadTexture(normal_tex_);
    RpAllocTex &spec_tex = builder.GetReadTexture(spec_tex_);
    RpAllocTex &refl_tex = builder.GetReadTexture(refl_tex_);
    RpAllocTex &brdf_lut = builder.GetReadTexture(brdf_lut_);

    RpAllocTex &output_tex = builder.GetWriteTexture(output_tex_);

    LazyInit(builder.ctx(), builder.sh(), output_tex);

    if (!probe_storage_) {
        return;
    }

    Ren::RastState rast_state;
    rast_state.depth.test_enabled = false;
    rast_state.depth.write_enabled = false;
    rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

    rast_state.blend.enabled = true;
    rast_state.blend.src = unsigned(Ren::eBlendFactor::One);
    rast_state.blend.dst = unsigned(Ren::eBlendFactor::One);

    rast_state.viewport[2] = view_state_->scr_res[0];
    rast_state.viewport[3] = view_state_->scr_res[1];

    { // compose reflections on top of clean buffer

        // TODO: get rid of global binding slots
        const Ren::Binding bindings[] = {
            {Ren::eBindTarget::UBuf, REN_UB_SHARED_DATA_LOC, 0, sizeof(SharedDataBlock), *unif_sh_data_buf.ref},
            {Ren::eBindTarget::Tex2D, SSRCompose2::SPEC_TEX_SLOT, *spec_tex.ref},
            {Ren::eBindTarget::Tex2D, SSRCompose2::DEPTH_TEX_SLOT, *depth_tex.ref},
            {Ren::eBindTarget::Tex2D, SSRCompose2::NORM_TEX_SLOT, *normal_tex.ref},
            {Ren::eBindTarget::Tex2D, SSRCompose2::REFL_TEX_SLOT, *refl_tex.ref},
            {Ren::eBindTarget::Tex2D, SSRCompose2::BRDF_TEX_SLOT, *brdf_lut.ref},
        };

        SSRCompose2::Params uniform_params;
        uniform_params.transform = Ren::Vec4f{0.0f, 0.0f, 1.0f, 1.0f};

        prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, blit_ssr_compose_prog_, output_fb_, render_pass_, rast_state,
                            builder.rast_state(), bindings, COUNT_OF(bindings), &uniform_params, sizeof(uniform_params),
                            0);
    }
}

void RpSSRCompose2::LazyInit(Ren::Context &ctx, ShaderLoader &sh, RpAllocTex &output_tex) {
    if (!initialized) {
        blit_ssr_compose_prog_ = sh.LoadProgram(ctx, "blit_ssr_compose2", "internal/blit_ssr_compose2.vert.glsl",
                                                "internal/blit_ssr_compose2.frag.glsl");
        assert(blit_ssr_compose_prog_->ready());

        initialized = true;
    }

    const Ren::RenderTarget render_targets[] = {{output_tex.ref, Ren::eLoadOp::Load, Ren::eStoreOp::Store}};

    if (!render_pass_.Setup(ctx.api_ctx(), render_targets, 1, {}, ctx.log())) {
        ctx.log()->Error("RpSSRDilate: render_pass_ init failed!");
    }

    if (!output_fb_.Setup(ctx.api_ctx(), render_pass_, output_tex.desc.w, output_tex.desc.h, {}, {}, render_targets,
                          1)) {
        ctx.log()->Error("RpSSRDilate: output_fb_ init failed!");
    }
}
