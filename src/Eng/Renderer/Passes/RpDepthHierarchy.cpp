#include "RpDepthHierarchy.h"

#include <Ren/Context.h>
#include <Ren/Program.h>

#include "../../Utils/ShaderLoader.h"

void RpDepthHierarchy::Setup(RpBuilder &builder, const ViewState *view_state, const char depth_tex[],
                             const char output_tex[]) {
    view_state_ = view_state;

    input_tex_ = builder.ReadTexture(depth_tex, Ren::eResState::ShaderResource, Ren::eStageBits::ComputeShader, *this);
    { // 32-bit linear depth hierarchy
        Ren::Tex2DParams params;
        params.w = view_state->scr_res[0];
        params.h = view_state->scr_res[1];
        params.format = Ren::eTexFormat::RawR32F;
        params.mip_count = 6;
        params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

        output_tex_ = builder.WriteTexture(output_tex, params, Ren::eResState::UnorderedAccess,
                                           Ren::eStageBits::ComputeShader, *this);
    }
}

void RpDepthHierarchy::LazyInit(Ren::Context &ctx, ShaderLoader &sh) {
    if (!initialized) {
        Ren::ProgramRef depth_hierarchy_prog =
            sh.LoadProgram(ctx, "depth_hierarchy", "internal/depth_hierarchy.comp.glsl");
        assert(depth_hierarchy_prog->ready());

        if (!pi_depth_hierarchy_.Init(ctx.api_ctx(), std::move(depth_hierarchy_prog), ctx.log())) {
            ctx.log()->Error("RpDepthHierarchy: failed to initialize pipeline!");
        }

        initialized = true;
    }
}
