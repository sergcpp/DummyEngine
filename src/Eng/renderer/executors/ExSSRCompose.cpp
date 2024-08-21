#include "ExSSRCompose.h"

#include <Ren/Context.h>
#include <Ren/ProbeStorage.h>
#include <Ren/Program.h>
#include <Ren/RastState.h>

#include "../../utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"
#include "../shaders/blit_ssr_compose_interface.h"

void Eng::ExSSRCompose::Execute(FgBuilder &builder) {
    FgAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(args_->shared_data);
    FgAllocBuf &cells_buf = builder.GetReadBuffer(args_->cells_buf);
    FgAllocBuf &items_buf = builder.GetReadBuffer(args_->items_buf);
    FgAllocTex &depth_tex = builder.GetReadTexture(args_->depth_tex);
    FgAllocTex &normal_tex = builder.GetReadTexture(args_->normal_tex);
    FgAllocTex &spec_tex = builder.GetReadTexture(args_->spec_tex);
    FgAllocTex &depth_down_2x_tex = builder.GetReadTexture(args_->depth_down_2x_tex);
    FgAllocTex &down_buf_4x_tex = builder.GetReadTexture(args_->down_buf_4x_tex);
    FgAllocTex &ssr_tex = builder.GetReadTexture(args_->ssr_tex);
    FgAllocTex &brdf_lut = builder.GetReadTexture(args_->brdf_lut);

    FgAllocTex &output_tex = builder.GetWriteTexture(args_->output_tex);

    LazyInit(builder.ctx(), builder.sh(), output_tex);

    if (!probe_storage_) {
        return;
    }

    Ren::RastState rast_state;
    rast_state.depth.test_enabled = false;
    rast_state.depth.write_enabled = false;
    rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

    rast_state.blend.enabled = true;
    rast_state.blend.src_color = rast_state.blend.src_alpha = unsigned(Ren::eBlendFactor::One);
    rast_state.blend.dst_color = rast_state.blend.dst_alpha = unsigned(Ren::eBlendFactor::One);

    rast_state.viewport[2] = view_state_->scr_res[0];
    rast_state.viewport[3] = view_state_->scr_res[1];

    const Ren::eBindTarget clean_buf_bind_target =
        view_state_->is_multisampled ? Ren::eBindTarget::Tex2DMs : Ren::eBindTarget::Tex2DSampled;

    { // compose reflections on top of clean buffer
        Ren::ProgramRef blit_ssr_compose_prog =
            view_state_->is_multisampled ? blit_ssr_compose_ms_prog_ : blit_ssr_compose_prog_;
        if (ssr_tex.desc.w == output_tex.desc.w) {
            blit_ssr_compose_prog = blit_ssr_compose_hq_prog_;
        }

        const Ren::RenderTarget render_targets[] = {{output_tex.ref, Ren::eLoadOp::Load, Ren::eStoreOp::Store}};

        // TODO: get rid of global binding slots
        const Ren::Binding bindings[] = {
            {clean_buf_bind_target, BIND_REFL_SPEC_TEX, *spec_tex.ref},
            {clean_buf_bind_target, BIND_REFL_DEPTH_TEX, {*depth_tex.ref, 1}},
            {clean_buf_bind_target, BIND_REFL_NORM_TEX, *normal_tex.ref},
            //
            {Ren::eBindTarget::Tex2DSampled, BIND_REFL_DEPTH_LOW_TEX, *depth_down_2x_tex.ref},
            {Ren::eBindTarget::Tex2DSampled, BIND_REFL_SSR_TEX, *ssr_tex.ref},
            //
            {Ren::eBindTarget::Tex2DSampled, BIND_REFL_PREV_TEX, *down_buf_4x_tex.ref},
            {Ren::eBindTarget::Tex2DSampled, BIND_REFL_BRDF_TEX, *brdf_lut.ref},
            //
            {Ren::eBindTarget::UTBuf, BIND_CELLS_BUF, *cells_buf.tbos[0]},
            {Ren::eBindTarget::UTBuf, BIND_ITEMS_BUF, *items_buf.tbos[0]},
            {Ren::eBindTarget::TexCubeArray, BIND_ENV_TEX, *probe_storage_},
            {Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, 0, sizeof(SharedDataBlock), *unif_sh_data_buf.ref}};

        SSRCompose::Params uniform_params;
        uniform_params.transform = Ren::Vec4f{0.0f, 0.0f, 1.0f, 1.0f};

        prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, blit_ssr_compose_prog, render_targets, {}, rast_state,
                            builder.rast_state(), bindings, &uniform_params, sizeof(SSRCompose::Params), 0);
    }
}

void Eng::ExSSRCompose::LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh, FgAllocTex &output_tex) {
    if (!initialized) {
        blit_ssr_compose_prog_ =
            sh.LoadProgram(ctx, "internal/blit_ssr_compose.vert.glsl", "internal/blit_ssr_compose.frag.glsl@HALFRES");
        assert(blit_ssr_compose_prog_->ready());
        blit_ssr_compose_ms_prog_ =
            sh.LoadProgram(ctx, "internal/blit.vert.glsl", "internal/blit_ssr_compose.frag.glsl@HALFRES;MSAA_4");
        assert(blit_ssr_compose_ms_prog_->ready());

        blit_ssr_compose_hq_prog_ =
            sh.LoadProgram(ctx, "internal/blit_ssr_compose.vert.glsl", "internal/blit_ssr_compose.frag.glsl");
        assert(blit_ssr_compose_hq_prog_->ready());

        initialized = true;
    }
}
