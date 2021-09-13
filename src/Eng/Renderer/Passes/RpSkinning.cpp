#include "RpSkinning.h"

#include "../../Utils/ShaderLoader.h"
#include "../Renderer_Structs.h"

void RpSkinning::Setup(RpBuilder &builder, const DrawList &list, Ren::BufferRef vtx_buf1, Ren::BufferRef vtx_buf2,
                       Ren::BufferRef delta_buf, Ren::BufferRef skin_vtx_buf, const char skin_transforms_buf[],
                       const char shape_keys_buf[]) {
    skin_regions_ = list.skin_regions;

    skin_vtx_buf_ = builder.ReadBuffer(std::move(skin_vtx_buf), Ren::eResState::ShaderResource,
                                       Ren::eStageBits::ComputeShader, *this);
    skin_transforms_buf_ =
        builder.ReadBuffer(skin_transforms_buf, Ren::eResState::ShaderResource, Ren::eStageBits::ComputeShader, *this);
    shape_keys_buf_ =
        builder.ReadBuffer(shape_keys_buf, Ren::eResState::ShaderResource, Ren::eStageBits::ComputeShader, *this);
    delta_buf_ =
        builder.ReadBuffer(std::move(delta_buf), Ren::eResState::ShaderResource, Ren::eStageBits::ComputeShader, *this);

    vtx_buf1_ = builder.WriteBuffer(std::move(vtx_buf1), Ren::eResState::UnorderedAccess,
                                    Ren::eStageBits::ComputeShader, *this);
    vtx_buf2_ = builder.WriteBuffer(std::move(vtx_buf2), Ren::eResState::UnorderedAccess,
                                    Ren::eStageBits::ComputeShader, *this);
}

void RpSkinning::LazyInit(Ren::Context &ctx, ShaderLoader &sh) {
    if (initialized) {
        return;
    }

    Ren::ProgramRef skinning_prog = sh.LoadProgram(ctx, "skinning_prog", "internal/skinning.comp.glsl");
    assert(skinning_prog->ready());

    if (!pi_skinning_.Init(ctx.api_ctx(), std::move(skinning_prog), ctx.log())) {
        ctx.log()->Error("RpSkinning: failed to initialize pipeline!");
    }

    // InitPipeline(ctx);

    initialized = true;
}
