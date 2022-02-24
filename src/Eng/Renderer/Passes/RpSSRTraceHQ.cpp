#include "RpSSRTraceHQ.h"

#include <Ren/Context.h>
#include <Ren/ProbeStorage.h>
#include <Ren/Program.h>
#include <Ren/RastState.h>

#include "../../Utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"

#include "../assets/shaders/internal/ssr_trace_hq_interface.glsl"

void RpSSRTraceHQ::Setup(RpBuilder &builder, const ViewState *view_state, Ren::WeakBufferRef sobol_buf,
                         Ren::WeakBufferRef scrambling_tile_buf, Ren::WeakBufferRef ranking_tile_buf,
                         const char shared_data_buf_name[], const char color_tex_name[], const char normal_tex_name[],
                         const char depth_hierarchy_name[], const char ray_counter_name[],
                         const char in_ray_list_name[], const char indir_args_name[], const char out_refl_tex_name[],
                         const char out_raylen_name[], const char out_ray_list_name[]) {
    view_state_ = view_state;

    sobol_buf_ = builder.ReadBuffer(sobol_buf, Ren::eResState::ShaderResource, Ren::eStageBits::ComputeShader, *this);
    scrambling_tile_buf_ =
        builder.ReadBuffer(scrambling_tile_buf, Ren::eResState::ShaderResource, Ren::eStageBits::ComputeShader, *this);
    ranking_tile_buf_ =
        builder.ReadBuffer(ranking_tile_buf, Ren::eResState::ShaderResource, Ren::eStageBits::ComputeShader, *this);
    shared_data_buf_ =
        builder.ReadBuffer(shared_data_buf_name, Ren::eResState::UniformBuffer, Ren::eStageBits::ComputeShader, *this);
    color_tex_ =
        builder.ReadTexture(color_tex_name, Ren::eResState::ShaderResource, Ren::eStageBits::ComputeShader, *this);
    normal_tex_ =
        builder.ReadTexture(normal_tex_name, Ren::eResState::ShaderResource, Ren::eStageBits::ComputeShader, *this);
    depth_hierarchy_tex_ = builder.ReadTexture(depth_hierarchy_name, Ren::eResState::ShaderResource,
                                               Ren::eStageBits::ComputeShader, *this);

    in_ray_list_buf_ =
        builder.ReadBuffer(in_ray_list_name, Ren::eResState::ShaderResource, Ren::eStageBits::ComputeShader, *this);
    indir_args_buf_ =
        builder.ReadBuffer(indir_args_name, Ren::eResState::IndirectArgument, Ren::eStageBits::DrawIndirect, *this);

    out_ray_counter_buf_ =
        builder.WriteBuffer(ray_counter_name, Ren::eResState::UnorderedAccess, Ren::eStageBits::ComputeShader, *this);
    out_refl_tex_ =
        builder.WriteTexture(out_refl_tex_name, Ren::eResState::UnorderedAccess, Ren::eStageBits::ComputeShader, *this);

    { // Ray length texture
        Ren::Tex2DParams params;
        params.w = view_state->scr_res[0];
        params.h = view_state->scr_res[1];
        params.format = Ren::eTexFormat::RawR16F;
        params.usage = (Ren::eTexUsage::Transfer | Ren::eTexUsage::Sampled | Ren::eTexUsage::Storage);
        params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
        params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

        out_raylen_tex_ = builder.WriteTexture(out_raylen_name, params, Ren::eResState::UnorderedAccess,
                                               Ren::eStageBits::ComputeShader, *this);
    }
    { // packed ray list
        RpBufDesc desc;
        desc.type = Ren::eBufType::Storage;
        desc.size = view_state->scr_res[0] * view_state->scr_res[1] * sizeof(uint32_t);

        out_ray_list_buf_ = builder.WriteBuffer(out_ray_list_name, desc, Ren::eResState::UnorderedAccess,
                                                Ren::eStageBits::ComputeShader, *this);
    }
}

void RpSSRTraceHQ::Setup(RpBuilder &builder, const ViewState *view_state, Ren::WeakBufferRef sobol_buf,
                         Ren::WeakBufferRef scrambling_tile_buf, Ren::WeakBufferRef ranking_tile_buf,
                         const char shared_data_buf_name[], const char color_tex_name[], const char normal_tex_name[],
                         const char depth_hierarchy_name[], const char ray_counter_name[],
                         const char in_ray_list_name[], const char indir_args_name[], Ren::WeakTex2DRef out_refl_tex,
                         const char out_raylen_name[], const char out_ray_list_name[]) {
    view_state_ = view_state;

    sobol_buf_ = builder.ReadBuffer(sobol_buf, Ren::eResState::ShaderResource, Ren::eStageBits::ComputeShader, *this);
    scrambling_tile_buf_ =
        builder.ReadBuffer(scrambling_tile_buf, Ren::eResState::ShaderResource, Ren::eStageBits::ComputeShader, *this);
    ranking_tile_buf_ =
        builder.ReadBuffer(ranking_tile_buf, Ren::eResState::ShaderResource, Ren::eStageBits::ComputeShader, *this);
    shared_data_buf_ =
        builder.ReadBuffer(shared_data_buf_name, Ren::eResState::UniformBuffer, Ren::eStageBits::ComputeShader, *this);
    color_tex_ =
        builder.ReadTexture(color_tex_name, Ren::eResState::ShaderResource, Ren::eStageBits::ComputeShader, *this);
    normal_tex_ =
        builder.ReadTexture(normal_tex_name, Ren::eResState::ShaderResource, Ren::eStageBits::ComputeShader, *this);
    depth_hierarchy_tex_ = builder.ReadTexture(depth_hierarchy_name, Ren::eResState::ShaderResource,
                                               Ren::eStageBits::ComputeShader, *this);

    in_ray_list_buf_ =
        builder.ReadBuffer(in_ray_list_name, Ren::eResState::ShaderResource, Ren::eStageBits::ComputeShader, *this);
    indir_args_buf_ =
        builder.ReadBuffer(indir_args_name, Ren::eResState::IndirectArgument, Ren::eStageBits::DrawIndirect, *this);

    out_ray_counter_buf_ =
        builder.WriteBuffer(ray_counter_name, Ren::eResState::UnorderedAccess, Ren::eStageBits::ComputeShader, *this);
    out_refl_tex_ =
        builder.WriteTexture(out_refl_tex, Ren::eResState::UnorderedAccess, Ren::eStageBits::ComputeShader, *this);

    { // Ray length texture
        Ren::Tex2DParams params;
        params.w = view_state->scr_res[0];
        params.h = view_state->scr_res[1];
        params.format = Ren::eTexFormat::RawR16F;
        params.usage = (Ren::eTexUsage::Transfer | Ren::eTexUsage::Sampled | Ren::eTexUsage::Storage);
        params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
        params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

        out_raylen_tex_ = builder.WriteTexture(out_raylen_name, params, Ren::eResState::UnorderedAccess,
                                               Ren::eStageBits::ComputeShader, *this);
    }
    { // packed ray list
        RpBufDesc desc;
        desc.type = Ren::eBufType::Storage;
        desc.size = view_state->scr_res[0] * view_state->scr_res[1] * sizeof(uint32_t);

        out_ray_list_buf_ = builder.WriteBuffer(out_ray_list_name, desc, Ren::eResState::UnorderedAccess,
                                                Ren::eStageBits::ComputeShader, *this);
    }
}

void RpSSRTraceHQ::LazyInit(Ren::Context &ctx, ShaderLoader &sh) {
    if (!initialized) {
        Ren::ProgramRef ssr_trace_hq_prog = sh.LoadProgram(ctx, "ssr_trace_hq", "internal/ssr_trace_hq.comp.glsl");
        assert(ssr_trace_hq_prog->ready());

        if (!pi_ssr_trace_hq_.Init(ctx.api_ctx(), std::move(ssr_trace_hq_prog), ctx.log())) {
            ctx.log()->Error("RpSSRTraceHQ: failed to initialize pipeline!");
        }

        initialized = true;
    }
}

void RpSSRTraceHQ::Execute(RpBuilder &builder) {
    RpAllocBuf &sobol_buf = builder.GetReadBuffer(sobol_buf_);
    RpAllocBuf &scrambling_tile_buf = builder.GetReadBuffer(scrambling_tile_buf_);
    RpAllocBuf &ranking_tile_buf = builder.GetReadBuffer(ranking_tile_buf_);
    RpAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(shared_data_buf_);
    RpAllocTex &color_tex = builder.GetReadTexture(color_tex_);
    RpAllocTex &normal_tex = builder.GetReadTexture(normal_tex_);
    RpAllocTex &depth_hierarchy_tex = builder.GetReadTexture(depth_hierarchy_tex_);
    RpAllocBuf &in_ray_list_buf = builder.GetReadBuffer(in_ray_list_buf_);
    RpAllocBuf &indir_args_buf = builder.GetReadBuffer(indir_args_buf_);

    RpAllocTex &out_refl_tex = builder.GetWriteTexture(out_refl_tex_);
    RpAllocTex &out_raylen_tex = builder.GetWriteTexture(out_raylen_tex_);
    RpAllocBuf &out_ray_counter_buf = builder.GetWriteBuffer(out_ray_counter_buf_);
    RpAllocBuf &out_ray_list_buf = builder.GetWriteBuffer(out_ray_list_buf_);

    LazyInit(builder.ctx(), builder.sh());

    Ren::Context &ctx = builder.ctx();
    Ren::ApiContext *api_ctx = ctx.api_ctx();

    // Initialize texel buffers
    if (!sobol_buf.tbos[0]) {
        sobol_buf.tbos[0] =
            ctx.CreateTexture1D("SobolSequenceTex", sobol_buf.ref, Ren::eTexFormat::RawR32UI, 0, sobol_buf.ref->size());
    }
    if (!scrambling_tile_buf.tbos[0]) {
        scrambling_tile_buf.tbos[0] =
            ctx.CreateTexture1D("ScramblingTile32SppTex", scrambling_tile_buf.ref, Ren::eTexFormat::RawR32UI, 0,
                                scrambling_tile_buf.ref->size());
    }
    if (!ranking_tile_buf.tbos[0]) {
        ranking_tile_buf.tbos[0] = ctx.CreateTexture1D("RankingTile32SppTex", ranking_tile_buf.ref,
                                                       Ren::eTexFormat::RawR32UI, 0, ranking_tile_buf.ref->size());
    }

    const Ren::Binding bindings[] = {
        {Ren::eBindTarget::UBuf, REN_UB_SHARED_DATA_LOC, *unif_sh_data_buf.ref},
        {Ren::eBindTarget::Tex2D, SSRTraceHQ::DEPTH_TEX_SLOT, *depth_hierarchy_tex.ref},
        {Ren::eBindTarget::Tex2D, SSRTraceHQ::COLOR_TEX_SLOT, *color_tex.ref},
        {Ren::eBindTarget::Tex2D, SSRTraceHQ::NORM_TEX_SLOT, *normal_tex.ref},
        {Ren::eBindTarget::SBuf, SSRTraceHQ::IN_RAY_LIST_SLOT, *in_ray_list_buf.ref},
        {Ren::eBindTarget::TBuf, SSRTraceHQ::SOBOL_BUF_SLOT, *sobol_buf.tbos[0]},
        {Ren::eBindTarget::TBuf, SSRTraceHQ::SCRAMLING_TILE_BUF_SLOT, *scrambling_tile_buf.tbos[0]},
        {Ren::eBindTarget::TBuf, SSRTraceHQ::RANKING_TILE_BUF_SLOT, *ranking_tile_buf.tbos[0]},
        {Ren::eBindTarget::Image, SSRTraceHQ::OUT_REFL_IMG_SLOT, *out_refl_tex.ref},
        {Ren::eBindTarget::Image, SSRTraceHQ::OUT_RAYLEN_IMG_SLOT, *out_raylen_tex.ref},
        {Ren::eBindTarget::SBuf, SSRTraceHQ::RAY_COUNTER_SLOT, *out_ray_counter_buf.ref},
        {Ren::eBindTarget::SBuf, SSRTraceHQ::OUT_RAY_LIST_SLOT, *out_ray_list_buf.ref}};

    SSRTraceHQ::Params uniform_params;
    uniform_params.resolution = Ren::Vec4u{uint32_t(view_state_->act_res[0]), uint32_t(view_state_->act_res[1]), 0, 0};

    Ren::DispatchComputeIndirect(pi_ssr_trace_hq_, *indir_args_buf.ref, 0, bindings, COUNT_OF(bindings),
                                 &uniform_params, sizeof(uniform_params), builder.ctx().default_descr_alloc(),
                                 builder.ctx().log());
}
