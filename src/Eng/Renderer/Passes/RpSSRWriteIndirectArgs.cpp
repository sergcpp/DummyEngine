#include "RpSSRWriteIndirectArgs.h"

#include <Ren/Context.h>
#include <Ren/DrawCall.h>

#include "../../Utils/ShaderLoader.h"
#include "../Renderer_Structs.h"

#include "../assets/shaders/internal/ssr_write_indirect_args_interface.glsl"

void RpSSRWriteIndirectArgs::Setup(RpBuilder &builder, const ViewState *view_state, const char ray_counter_name[],
                                   const char indir_disp_name[]) {
    view_state_ = view_state;

    ray_counter_buf_ =
        builder.WriteBuffer(ray_counter_name, Ren::eResState::UnorderedAccess, Ren::eStageBits::ComputeShader, *this);
    { //
        struct DispatchIndirectCommand {
            uint32_t num_groups_x;
            uint32_t num_groups_y;
            uint32_t num_groups_z;
        };

        RpBufDesc desc = {};
        desc.type = Ren::eBufType::Indirect;
        desc.size = sizeof(DispatchIndirectCommand);

        indir_disp_buf_ = builder.WriteBuffer(indir_disp_name, desc, Ren::eResState::UnorderedAccess,
                                              Ren::eStageBits::ComputeShader, *this);
    }
}

void RpSSRWriteIndirectArgs::LazyInit(Ren::Context &ctx, ShaderLoader &sh) {
    if (initialized) {
        return;
    }

    Ren::ProgramRef write_indirect_prog = sh.LoadProgram(ctx, "ssr_write_indirect_args", "internal/ssr_write_indirect_args.comp.glsl");
    assert(write_indirect_prog->ready());

    if (!pi_write_indirect_.Init(ctx.api_ctx(), std::move(write_indirect_prog), ctx.log())) {
        ctx.log()->Error("RpSSRWriteIndirectArgs: failed to initialize pipeline!");
    }

    initialized = true;
}

void RpSSRWriteIndirectArgs::Execute(RpBuilder &builder) {
    LazyInit(builder.ctx(), builder.sh());

    RpAllocBuf &ray_counter_buf = builder.GetWriteBuffer(ray_counter_buf_);
    RpAllocBuf &indir_disp_buf = builder.GetWriteBuffer(indir_disp_buf_);

    const Ren::Binding bindings[] = {
        {Ren::eBindTarget::SBuf, SSRWriteIndirectArgs::RAY_COUNTER_SLOT, *ray_counter_buf.ref},
        {Ren::eBindTarget::SBuf, SSRWriteIndirectArgs::INDIR_ARGS_SLOT, *indir_disp_buf.ref}};

    Ren::DispatchCompute(pi_write_indirect_, Ren::Vec3u{1u, 1u, 1u}, bindings, COUNT_OF(bindings), nullptr, 0,
                         builder.ctx().default_descr_alloc(), builder.ctx().log());
}
