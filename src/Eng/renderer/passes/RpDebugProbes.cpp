#include "RpDebugProbes.h"

#include <Ren/Context.h>
#include <Ren/RastState.h>

#include "../../utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"
#include "../shaders/probe_debug_interface.h"

void Eng::RpDebugProbes::Execute(RpBuilder &builder) {
    LazyInit(builder.ctx(), builder.sh());

    RpAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(pass_data_->shared_data);
    RpAllocTex &offset_tex = builder.GetReadTexture(pass_data_->offset_tex);
    RpAllocTex &irradiance_tex = builder.GetReadTexture(pass_data_->irradiance_tex);
    RpAllocTex &distance_tex = builder.GetReadTexture(pass_data_->distance_tex);
    RpAllocTex &depth_tex = builder.GetWriteTexture(pass_data_->depth_tex);
    RpAllocTex &output_tex = builder.GetWriteTexture(pass_data_->output_tex);

    Ren::RastState rast_state;
    rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);
    rast_state.viewport[2] = view_state_->act_res[0];
    rast_state.viewport[3] = view_state_->act_res[1];

    rast_state.depth.test_enabled = true;
    rast_state.depth.compare_op = unsigned(Ren::eCompareOp::Less);

    const Ren::Binding bindings[] = {
        {Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, *unif_sh_data_buf.ref},
        {Ren::eBindTarget::Tex2DArray, ProbeDebug::OFFSET_TEX_SLOT, *offset_tex.arr},
        {Ren::eBindTarget::Tex2DArray, ProbeDebug::IRRADIANCE_TEX_SLOT, *irradiance_tex.arr}};

    ProbeDebug::Params uniform_params = {};
    uniform_params.grid_origin =
        Ren::Vec4f(pass_data_->grid_origin[0], pass_data_->grid_origin[1], pass_data_->grid_origin[2], 0.0f);
    uniform_params.grid_scroll =
        Ren::Vec4i(pass_data_->grid_scroll[0], pass_data_->grid_scroll[1], pass_data_->grid_scroll[2], 0.0f);
    uniform_params.grid_spacing =
        Ren::Vec4f(pass_data_->grid_spacing[0], pass_data_->grid_spacing[1], pass_data_->grid_spacing[2], 0.0f);

    const Ren::RenderTarget render_targets[] = {{output_tex.ref, Ren::eLoadOp::Load, Ren::eStoreOp::Store}};
    const Ren::RenderTarget depth_target = {depth_tex.ref, Ren::eLoadOp::Load, Ren::eStoreOp::Store};

    prim_draw_.DrawPrim(PrimDraw::ePrim::Sphere, prog_probe_debug_, render_targets, depth_target, rast_state,
                        builder.rast_state(), bindings, &uniform_params, sizeof(uniform_params), 0,
                        PROBE_VOLUME_RES * PROBE_VOLUME_RES * PROBE_VOLUME_RES);
}

void Eng::RpDebugProbes::LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh) {
    if (!initialized) {
        prog_probe_debug_ =
            sh.LoadProgram(ctx, "probe_debug", "internal/probe_debug.vert.glsl", "internal/probe_debug.frag.glsl");
        assert(prog_probe_debug_->ready());

        initialized = true;
    }
}
