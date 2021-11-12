#include "RpDepthHierarchy.h"

#include <Ren/Context.h>
#include <Ren/Program.h>

#include "../Renderer_Structs.h"
#include "../../Utils/ShaderLoader.h"

namespace RpDepthHierarchyInternal {
extern const int MipCount = 7;
const int TileSize = 1 << (MipCount - 1);
}

void RpDepthHierarchy::Setup(RpBuilder &builder, const ViewState *view_state, const char depth_tex[],
                             const char atomic_counter[], const char output_tex[]) {
    using namespace RpDepthHierarchyInternal;

    view_state_ = view_state;

    input_tex_ = builder.ReadTexture(depth_tex, Ren::eResState::ShaderResource, Ren::eStageBits::ComputeShader, *this);
    { // global counter used to count active workgroups
        atomic_buf_ =
            builder.WriteBuffer(atomic_counter, Ren::eResState::UnorderedAccess, Ren::eStageBits::ComputeShader, *this);
	}
    { // 32-bit float depth hierarchy
        Ren::Tex2DParams params;
        params.w = ((view_state->scr_res[0] + TileSize - 1) / TileSize) * TileSize;
        params.h = ((view_state->scr_res[1] + TileSize - 1) / TileSize) * TileSize;
        params.format = Ren::eTexFormat::RawR32F;
        params.usage = (Ren::eTexUsage::Sampled | Ren::eTexUsage::Storage);
        params.mip_count = MipCount;
        params.sampling.wrap = Ren::eTexWrap::ClampToEdge;
        params.sampling.filter = Ren::eTexFilter::NearestMipmap;

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
