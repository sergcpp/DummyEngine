#include "ExDebugOIT.h"

#include <Ren/Context.h>
#include <Ren/DrawCall.h>

#include "../../utils/ShaderLoader.h"
#include "../shaders/oit_debug_interface.h"

void Eng::ExDebugOIT::Execute(FgBuilder &builder) {
    LazyInit(builder.ctx(), builder.sh());

    FgAllocBuf &oit_depth_buf = builder.GetReadBuffer(args_->oit_depth_buf);
    FgAllocTex &output_tex = builder.GetWriteTexture(args_->output_tex);

    const Ren::Binding bindings[] = {{Ren::eBindTarget::UTBuf, OITDebug::OIT_DEPTH_BUF_SLOT, *oit_depth_buf.tbos[0]},
                                     {Ren::eBindTarget::Image2D, OITDebug::OUT_IMG_SLOT, *output_tex.ref}};

    const Ren::Vec3u grp_count =
        Ren::Vec3u{(view_state_->act_res[0] + OITDebug::LOCAL_GROUP_SIZE_X - 1u) / OITDebug::LOCAL_GROUP_SIZE_X,
                   (view_state_->act_res[1] + OITDebug::LOCAL_GROUP_SIZE_Y - 1u) / OITDebug::LOCAL_GROUP_SIZE_Y, 1u};

    OITDebug::Params uniform_params = {};
    uniform_params.img_size[0] = view_state_->act_res[0];
    uniform_params.img_size[1] = view_state_->act_res[1];
    uniform_params.layer_index = args_->layer_index;

    DispatchCompute(pi_debug_oit_, grp_count, bindings, &uniform_params, sizeof(uniform_params),
                    builder.ctx().default_descr_alloc(), builder.ctx().log());
}

void Eng::ExDebugOIT::LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh) {
    if (!initialized) {
        Ren::ProgramRef debug_prog = sh.LoadProgram(ctx, "internal/oit_debug.comp.glsl");
        assert(debug_prog->ready());

        if (!pi_debug_oit_.Init(ctx.api_ctx(), debug_prog, ctx.log())) {
            ctx.log()->Error("ExDebugOIT: Failed to initialize pipeline!");
        }

        initialized = true;
    }
}
