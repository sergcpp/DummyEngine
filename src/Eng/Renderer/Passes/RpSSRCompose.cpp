#include "RpSSRCompose.h"

#include <Ren/Context.h>
#include <Ren/Program.h>
#include <Ren/RastState.h>

#include "../../Scene/ProbeStorage.h"
#include "../../Utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"

#include "../assets/shaders/internal/blit_ssr_compose_interface.glsl"

void RpSSRCompose::Setup(RpBuilder &builder, const ViewState *view_state, const ProbeStorage *probe_storage,
                         Ren::WeakTex2DRef down_buf_4x_tex, Ren::Tex2DRef brdf_lut, const char shared_data_buf[],
                         const char cells_buf[], const char items_buf[], const char depth_tex[],
                         const char normal_tex[], const char spec_tex[], const char depth_down_2x[],
                         const char ssr_tex_name[], const char output_tex_name[]) {
    view_state_ = view_state;

    probe_storage_ = probe_storage;

    shared_data_buf_ = builder.ReadBuffer(shared_data_buf, Ren::eResState::UniformBuffer,
                                          Ren::eStageBits::VertexShader | Ren::eStageBits::FragmentShader, *this);
    cells_buf_ = builder.ReadBuffer(cells_buf, Ren::eResState::ShaderResource, Ren::eStageBits::FragmentShader, *this);
    items_buf_ = builder.ReadBuffer(items_buf, Ren::eResState::ShaderResource, Ren::eStageBits::FragmentShader, *this);

    depth_tex_ = builder.ReadTexture(depth_tex, Ren::eResState::ShaderResource, Ren::eStageBits::FragmentShader, *this);
    normal_tex_ =
        builder.ReadTexture(normal_tex, Ren::eResState::ShaderResource, Ren::eStageBits::FragmentShader, *this);
    spec_tex_ = builder.ReadTexture(spec_tex, Ren::eResState::ShaderResource, Ren::eStageBits::FragmentShader, *this);
    depth_down_2x_tex_ =
        builder.ReadTexture(depth_down_2x, Ren::eResState::ShaderResource, Ren::eStageBits::FragmentShader, *this);
    down_buf_4x_tex_ =
        builder.ReadTexture(down_buf_4x_tex, Ren::eResState::ShaderResource, Ren::eStageBits::FragmentShader, *this);
    ssr_tex_ =
        builder.ReadTexture(ssr_tex_name, Ren::eResState::ShaderResource, Ren::eStageBits::FragmentShader, *this);
    brdf_lut_ = builder.ReadTexture(brdf_lut, Ren::eResState::ShaderResource, Ren::eStageBits::FragmentShader, *this);

    output_tex_ =
        builder.WriteTexture(output_tex_name, Ren::eResState::RenderTarget, Ren::eStageBits::ColorAttachment, *this);
}

void RpSSRCompose::Execute(RpBuilder &builder) {
    RpAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(shared_data_buf_);
    RpAllocBuf &cells_buf = builder.GetReadBuffer(cells_buf_);
    RpAllocBuf &items_buf = builder.GetReadBuffer(items_buf_);
    RpAllocTex &depth_tex = builder.GetReadTexture(depth_tex_);
    RpAllocTex &normal_tex = builder.GetReadTexture(normal_tex_);
    RpAllocTex &spec_tex = builder.GetReadTexture(spec_tex_);
    RpAllocTex &depth_down_2x_tex = builder.GetReadTexture(depth_down_2x_tex_);
    RpAllocTex &down_buf_4x_tex = builder.GetReadTexture(down_buf_4x_tex_);
    RpAllocTex &ssr_tex = builder.GetReadTexture(ssr_tex_);
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

    const Ren::eBindTarget clean_buf_bind_target =
        view_state_->is_multisampled ? Ren::eBindTarget::Tex2DMs : Ren::eBindTarget::Tex2D;

    { // compose reflections on top of clean buffer
        Ren::Program *blit_ssr_compose_prog =
            view_state_->is_multisampled ? blit_ssr_compose_ms_prog_.get() : blit_ssr_compose_prog_.get();

        // TODO: get rid of global binding slots
        const PrimDraw::Binding bindings[] = {
            {clean_buf_bind_target, REN_REFL_SPEC_TEX_SLOT, *spec_tex.ref},
            {clean_buf_bind_target, REN_REFL_DEPTH_TEX_SLOT, *depth_tex.ref},
            {clean_buf_bind_target, REN_REFL_NORM_TEX_SLOT, *normal_tex.ref},
            //
            {Ren::eBindTarget::Tex2D, REN_REFL_DEPTH_LOW_TEX_SLOT, *depth_down_2x_tex.ref},
            {Ren::eBindTarget::Tex2D, REN_REFL_SSR_TEX_SLOT, *ssr_tex.ref},
            //
            {Ren::eBindTarget::Tex2D, REN_REFL_PREV_TEX_SLOT, *down_buf_4x_tex.ref},
            {Ren::eBindTarget::Tex2D, REN_REFL_BRDF_TEX_SLOT, *brdf_lut.ref},
            //
            {Ren::eBindTarget::TexBuf, REN_CELLS_BUF_SLOT, *cells_buf.tbos[0]},
            {Ren::eBindTarget::TexBuf, REN_ITEMS_BUF_SLOT, *items_buf.tbos[0]},
            {Ren::eBindTarget::TexCubeArray, REN_ENV_TEX_SLOT, *probe_storage_},
            {Ren::eBindTarget::UBuf, REN_UB_SHARED_DATA_LOC, 0, sizeof(SharedDataBlock), *unif_sh_data_buf.ref}};

        SSRCompose::Params uniform_params;
        uniform_params.transform = Ren::Vec4f{0.0f, 0.0f, 1.0f, 1.0f};

        prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, blit_ssr_compose_prog_, output_fb_, render_pass_, rast_state,
                            builder.rast_state(), bindings, COUNT_OF(bindings), &uniform_params,
                            sizeof(SSRCompose::Params), 0);
    }
}

void RpSSRCompose::LazyInit(Ren::Context &ctx, ShaderLoader &sh, RpAllocTex &output_tex) {
    if (!initialized) {
        blit_ssr_compose_prog_ = sh.LoadProgram(ctx, "blit_ssr_compose", "internal/blit_ssr_compose.vert.glsl",
                                                "internal/blit_ssr_compose.frag.glsl");
        assert(blit_ssr_compose_prog_->ready());
        blit_ssr_compose_ms_prog_ = sh.LoadProgram(ctx, "blit_ssr_compose_ms", "internal/blit.vert.glsl",
                                                   "internal/blit_ssr_compose.frag.glsl@MSAA_4");
        assert(blit_ssr_compose_ms_prog_->ready());

        initialized = true;
    }

    const Ren::RenderTarget render_targets[] = {{output_tex.ref, Ren::eLoadOp::Load, Ren::eStoreOp::Store}};

    if (!render_pass_.Setup(ctx.api_ctx(), render_targets, 1, {}, ctx.log())) {
        ctx.log()->Error("RpSSRDilate: render_pass_ init failed!");
    }

    if (!output_fb_.Setup(ctx.api_ctx(), render_pass_, output_tex.desc.w, output_tex.desc.h, render_targets, 1, {},
                          {})) {
        ctx.log()->Error("RpSSRDilate: output_fb_ init failed!");
    }
}
