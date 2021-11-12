#include "RpSSRVSDepth.h"

#include <Ren/Context.h>
#include <Ren/Program.h>

#include "../../Utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"

#include "../assets/shaders/internal/ssr_vs_depth_interface.glsl"

void RpSSRVSDepth::Setup(RpBuilder &builder, const ViewState *view_state, const char depth_tex_name[],
                         const char shared_data_buf_name[], const char out_vs_depth_img_name[]) {
    view_state_ = view_state;

    depth_tex_ =
        builder.ReadTexture(depth_tex_name, Ren::eResState::ShaderResource, Ren::eStageBits::ComputeShader, *this);
    shared_data_buf_ =
        builder.ReadBuffer(shared_data_buf_name, Ren::eResState::UniformBuffer, Ren::eStageBits::ComputeShader, *this);

    { // View-space depth
        Ren::Tex2DParams params;
        params.w = view_state->scr_res[0];
        params.h = view_state->scr_res[1];
        params.format = Ren::eTexFormat::RawR32F;
        params.usage = (Ren::eTexUsage::Sampled | Ren::eTexUsage::Storage);
        params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
        params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

        out_vs_depth_img_ = builder.WriteTexture(out_vs_depth_img_name, params, Ren::eResState::UnorderedAccess,
                                                 Ren::eStageBits::ComputeShader, *this);
    }
}

void RpSSRVSDepth::LazyInit(Ren::Context &ctx, ShaderLoader &sh) {
    if (!initialized) {
        Ren::ProgramRef ssr_vs_depth_prog = sh.LoadProgram(ctx, "ssr_vs_depth", "internal/ssr_vs_depth.comp.glsl");
        assert(ssr_vs_depth_prog->ready());

        if (!pi_ssr_vs_depth_.Init(ctx.api_ctx(), std::move(ssr_vs_depth_prog), ctx.log())) {
            ctx.log()->Error("RpSSRVSDepth: failed to initialize pipeline!");
        }

        initialized = true;
    }
}

void RpSSRVSDepth::Execute(RpBuilder &builder) {
    RpAllocTex &depth_tex = builder.GetReadTexture(depth_tex_);
    RpAllocBuf &shared_data_buf = builder.GetReadBuffer(shared_data_buf_);

    RpAllocTex &out_vs_depth_img = builder.GetWriteTexture(out_vs_depth_img_);

    LazyInit(builder.ctx(), builder.sh());

    const Ren::Binding bindings[] = {
        {Ren::eBindTarget::Tex2D, SSRVSDepth::DEPTH_TEX_SLOT, *depth_tex.ref},
        {Ren::eBindTarget::UBuf, REN_UB_SHARED_DATA_LOC, *shared_data_buf.ref},
        {Ren::eBindTarget::Image, SSRVSDepth::OUT_VS_DEPTH_IMG_SLOT, *out_vs_depth_img.ref}};

    const auto grp_count = Ren::Vec3u{
        (view_state_->act_res[0] + SSRVSDepth::LOCAL_GROUP_SIZE_X - 1u) / SSRVSDepth::LOCAL_GROUP_SIZE_X,
        (view_state_->act_res[1] + SSRVSDepth::LOCAL_GROUP_SIZE_Y - 1u) / SSRVSDepth::LOCAL_GROUP_SIZE_Y, 1u};

    SSRVSDepth::Params uniform_params;
    uniform_params.resolution = view_state_->act_res;

    Ren::DispatchCompute(pi_ssr_vs_depth_, grp_count, bindings, COUNT_OF(bindings), &uniform_params,
                         sizeof(uniform_params), builder.ctx().default_descr_alloc(), builder.ctx().log());
}
