#include "ExDebugProbes.h"

#include <Ren/Context.h>
#include <Ren/RastState.h>

#include "../../utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_DrawList.h"
#include "../framegraph/FgBuilder.h"
#include "../shaders/probe_debug_interface.h"

Eng::ExDebugProbes::ExDebugProbes(PrimDraw &prim_draw, ShaderLoader &sh, const view_state_t *view_state,
                                  const Args *args)
    : prim_draw_(prim_draw), view_state_(view_state), args_(args) {
    prog_probe_debug_ = sh.FindOrCreateProgram("internal/probe_debug.vert.glsl", "internal/probe_debug.frag.glsl");
}

void Eng::ExDebugProbes::Execute(const FgContext &fg) {
    const Ren::BufferROHandle unif_sh_data = fg.AccessROBuffer(args_->shared_data);
    const Ren::ImageROHandle irr = fg.AccessROImage(args_->irradiance);
    [[maybe_unused]] const Ren::ImageROHandle dist = fg.AccessROImage(args_->distance);
    const Ren::ImageROHandle off = fg.AccessROImage(args_->offset);

    const Ren::ImageRWHandle depth = fg.AccessRWImage(args_->depth);
    const Ren::ImageRWHandle output = fg.AccessRWImage(args_->output);

    Ren::RastState rast_state;
    rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);
    rast_state.viewport[2] = view_state_->ren_res[0];
    rast_state.viewport[3] = view_state_->ren_res[1];

    rast_state.depth.test_enabled = true;
    rast_state.depth.compare_op = unsigned(Ren::eCompareOp::Greater);

    const Ren::Binding bindings[] = {{Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, unif_sh_data},
                                     {Ren::eBindTarget::TexSampled, ProbeDebug::OFFSET_TEX_SLOT, off},
                                     {Ren::eBindTarget::TexSampled, ProbeDebug::IRRADIANCE_TEX_SLOT, irr}};

    const probe_volume_t &volume = args_->probe_volumes[args_->volume_to_debug];

    ProbeDebug::Params uniform_params = {};
    uniform_params.volume_index = args_->volume_to_debug;
    uniform_params.grid_origin = Ren::Vec4f{volume.origin, 0.0f};
    uniform_params.grid_scroll = Ren::Vec4i{volume.scroll, 0};
    uniform_params.grid_scroll_diff = Ren::Vec4i{volume.scroll_diff, 0};
    uniform_params.grid_spacing = Ren::Vec4f{volume.spacing, 0.0f};

    const Ren::RenderTarget render_targets[] = {{output, Ren::eLoadOp::Load, Ren::eStoreOp::Store}};
    const Ren::RenderTarget depth_target = {depth, Ren::eLoadOp::Load, Ren::eStoreOp::Store};

    prim_draw_.DrawPrim(fg.cmd_buf(), PrimDraw::ePrim::Sphere, prog_probe_debug_, depth_target, render_targets,
                        rast_state, fg.rast_state(), bindings, &uniform_params, sizeof(uniform_params), 0,
                        fg.framebuffers(), PROBE_VOLUME_RES_X * PROBE_VOLUME_RES_Y * PROBE_VOLUME_RES_Z);
}
