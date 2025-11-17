#include "ExDebugOIT.h"

#include <Ren/Context.h>
#include <Ren/DrawCall.h>

#include "../../utils/ShaderLoader.h"
#include "../shaders/oit_debug_interface.h"

Eng::ExDebugOIT::ExDebugOIT(FgContext &fg, const view_state_t *view_state, const Args *pass_data) {
    view_state_ = view_state;
    args_ = pass_data;
    pi_debug_oit_ = fg.sh().LoadPipeline("internal/oit_debug.comp.glsl");
}

void Eng::ExDebugOIT::Execute(FgContext &fg) {
    const Ren::Buffer &oit_depth_buf = fg.AccessROBuffer(args_->oit_depth_buf);
    Ren::Texture &output_tex = fg.AccessRWTexture(args_->output_tex);

    const Ren::Binding bindings[] = {{Ren::eBindTarget::UTBuf, OITDebug::OIT_DEPTH_BUF_SLOT, oit_depth_buf},
                                     {Ren::eBindTarget::ImageRW, OITDebug::OUT_IMG_SLOT, output_tex}};

    const Ren::Vec3u grp_count =
        Ren::Vec3u{(view_state_->ren_res[0] + OITDebug::GRP_SIZE_X - 1u) / OITDebug::GRP_SIZE_X,
                   (view_state_->ren_res[1] + OITDebug::GRP_SIZE_Y - 1u) / OITDebug::GRP_SIZE_Y, 1u};

    OITDebug::Params uniform_params = {};
    uniform_params.img_size[0] = view_state_->ren_res[0];
    uniform_params.img_size[1] = view_state_->ren_res[1];
    uniform_params.layer_index = args_->layer_index;

    DispatchCompute(*pi_debug_oit_, grp_count, bindings, &uniform_params, sizeof(uniform_params), fg.descr_alloc(),
                    fg.log());
}
