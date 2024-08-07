#include "RpSSRCompose2.h"

#include <Ren/Context.h>
#include <Ren/ProbeStorage.h>
#include <Ren/Program.h>
#include <Ren/RastState.h>

#include "../../utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"
#include "../shaders/blit_ssr_compose2_interface.h"

void Eng::RpSSRCompose2::Execute(RpBuilder &builder) {
    RpAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(pass_data_->shared_data);
    RpAllocTex &depth_tex = builder.GetReadTexture(pass_data_->depth_tex);
    RpAllocTex &normal_tex = builder.GetReadTexture(pass_data_->normal_tex);
    RpAllocTex &spec_tex = builder.GetReadTexture(pass_data_->spec_tex);
    RpAllocTex &refl_tex = builder.GetReadTexture(pass_data_->refl_tex);
    RpAllocTex &brdf_lut = builder.GetReadTexture(pass_data_->brdf_lut);

    RpAllocTex &output_tex = builder.GetWriteTexture(pass_data_->output_tex);

    LazyInit(builder.ctx(), builder.sh(), output_tex);

    if (!probe_storage_) {
        return;
    }

    Ren::RastState rast_state;
    rast_state.depth.test_enabled = false;
    rast_state.depth.write_enabled = false;
    rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

    rast_state.blend.enabled = true;
    rast_state.blend.src_color = unsigned(Ren::eBlendFactor::One);
    rast_state.blend.dst_color = unsigned(Ren::eBlendFactor::One);
    rast_state.blend.src_alpha = unsigned(Ren::eBlendFactor::Zero);
    rast_state.blend.dst_alpha = unsigned(Ren::eBlendFactor::One);

    rast_state.viewport[2] = view_state_->scr_res[0];
    rast_state.viewport[3] = view_state_->scr_res[1];

    { // compose reflections on top of clean buffer
        const Ren::RenderTarget render_targets[] = {{output_tex.ref, Ren::eLoadOp::Load, Ren::eStoreOp::Store}};

        // TODO: get rid of global binding slots
        const Ren::Binding bindings[] = {
            {Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, 0, sizeof(SharedDataBlock), *unif_sh_data_buf.ref},
            {Ren::eBindTarget::Tex2DSampled, SSRCompose2::SPEC_TEX_SLOT, *spec_tex.ref},
            {Ren::eBindTarget::Tex2DSampled, SSRCompose2::DEPTH_TEX_SLOT, {*depth_tex.ref, 1}},
            {Ren::eBindTarget::Tex2DSampled, SSRCompose2::NORM_TEX_SLOT, *normal_tex.ref},
            {Ren::eBindTarget::Tex2DSampled, SSRCompose2::REFL_TEX_SLOT, *refl_tex.ref},
            {Ren::eBindTarget::Tex2DSampled, SSRCompose2::BRDF_TEX_SLOT, *brdf_lut.ref},
        };

        SSRCompose2::Params uniform_params;
        uniform_params.transform = Ren::Vec4f{0.0f, 0.0f, 1.0f, 1.0f};

        prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, blit_ssr_compose_prog_, render_targets, {}, rast_state,
                            builder.rast_state(), bindings, &uniform_params, sizeof(uniform_params), 0);
    }
}

void Eng::RpSSRCompose2::LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh, RpAllocTex &output_tex) {
    if (!initialized) {
        blit_ssr_compose_prog_ = sh.LoadProgram(ctx, "blit_ssr_compose2", "internal/blit_ssr_compose2.vert.glsl",
                                                "internal/blit_ssr_compose2.frag.glsl");
        assert(blit_ssr_compose_prog_->ready());

        initialized = true;
    }
}
