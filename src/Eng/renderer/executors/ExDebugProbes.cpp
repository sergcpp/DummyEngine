#include "ExDebugProbes.h"

#include <Ren/Context.h>
#include <Ren/RastState.h>

#include "../../utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../shaders/probe_debug_interface.h"

Eng::ExDebugProbes::ExDebugProbes(PrimDraw &prim_draw, FgContext &fg, const DrawList &list,
                                  const view_state_t *view_state, const Args *args)
    : prim_draw_(prim_draw), view_state_(view_state), args_(args) {
    prog_probe_debug_ = fg.sh().LoadProgram("internal/probe_debug.vert.glsl", "internal/probe_debug.frag.glsl");
}

void Eng::ExDebugProbes::Execute(FgContext &fg) {
    FgAllocBuf &unif_sh_data_buf = fg.AccessROBuffer(args_->shared_data);
    FgAllocTex &off_tex = fg.AccessROTexture(args_->offset_tex);
    FgAllocTex &irr_tex = fg.AccessROTexture(args_->irradiance_tex);
    [[maybe_unused]] FgAllocTex &dist_tex = fg.AccessROTexture(args_->distance_tex);
    FgAllocTex &depth_tex = fg.AccessRWTexture(args_->depth_tex);
    FgAllocTex &output_tex = fg.AccessRWTexture(args_->output_tex);

    Ren::RastState rast_state;
    rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);
    rast_state.viewport[2] = view_state_->ren_res[0];
    rast_state.viewport[3] = view_state_->ren_res[1];

    rast_state.depth.test_enabled = true;
    rast_state.depth.compare_op = unsigned(Ren::eCompareOp::Greater);

    const Ren::Binding bindings[] = {{Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, *unif_sh_data_buf.ref},
                                     {Ren::eBindTarget::TexSampled, ProbeDebug::OFFSET_TEX_SLOT, *off_tex.ref},
                                     {Ren::eBindTarget::TexSampled, ProbeDebug::IRRADIANCE_TEX_SLOT, *irr_tex.ref}};

    const probe_volume_t &volume = args_->probe_volumes[args_->volume_to_debug];

    ProbeDebug::Params uniform_params = {};
    uniform_params.volume_index = args_->volume_to_debug;
    uniform_params.grid_origin = Ren::Vec4f(volume.origin[0], volume.origin[1], volume.origin[2], 0.0f);
    uniform_params.grid_scroll = Ren::Vec4i(volume.scroll[0], volume.scroll[1], volume.scroll[2], 0);
    uniform_params.grid_scroll_diff =
        Ren::Vec4i(volume.scroll_diff[0], volume.scroll_diff[1], volume.scroll_diff[2], 0);
    uniform_params.grid_spacing = Ren::Vec4f(volume.spacing[0], volume.spacing[1], volume.spacing[2], 0.0f);

    const Ren::RenderTarget render_targets[] = {{output_tex.ref, Ren::eLoadOp::Load, Ren::eStoreOp::Store}};
    const Ren::RenderTarget depth_target = {depth_tex.ref, Ren::eLoadOp::Load, Ren::eStoreOp::Store};

    prim_draw_.DrawPrim(PrimDraw::ePrim::Sphere, prog_probe_debug_, depth_target, render_targets, rast_state,
                        fg.rast_state(), bindings, &uniform_params, sizeof(uniform_params), 0,
                        PROBE_VOLUME_RES_X * PROBE_VOLUME_RES_Y * PROBE_VOLUME_RES_Z);
}
