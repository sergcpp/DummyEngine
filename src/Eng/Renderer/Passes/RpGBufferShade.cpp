#include "RpGBufferShade.h"
#if 0
#include <Ren/Context.h>
#include <Ren/ProbeStorage.h>
#include <Ren/Program.h>
#include <Ren/RastState.h>

#include "../../Utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"

#include "../assets/shaders/internal/gbuffer_shade_interface.glsl"

void RpGBufferShade::Setup(RpBuilder &builder, const ViewState *view_state, const char shared_data_buf[],
                           const char cells_buf[], const char items_buf[], const char lights_buf[],
                           const char decals_buf[], const char depth_tex_name[], const char albedo_tex_name[],
                           const char normal_tex_name[], const char spec_tex_name[], const char shadowmap_tex[],
                           const char ssao_tex[], const char out_color_img_name[]) {
    view_state_ = view_state;

    shared_data_buf_ =
        builder.ReadBuffer(shared_data_buf, Ren::eResState::UniformBuffer, Ren::eStageBits::ComputeShader, *this);
    cells_buf_ = builder.ReadBuffer(cells_buf, Ren::eResState::ShaderResource, Ren::eStageBits::ComputeShader, *this);
    items_buf_ = builder.ReadBuffer(items_buf, Ren::eResState::ShaderResource, Ren::eStageBits::ComputeShader, *this);
    lights_buf_ = builder.ReadBuffer(lights_buf, Ren::eResState::ShaderResource, Ren::eStageBits::ComputeShader, *this);
    decals_buf_ = builder.ReadBuffer(decals_buf, Ren::eResState::ShaderResource, Ren::eStageBits::ComputeShader, *this);

    depth_tex_ =
        builder.ReadTexture(depth_tex_name, Ren::eResState::ShaderResource, Ren::eStageBits::ComputeShader, *this);
    albedo_tex_ =
        builder.ReadTexture(albedo_tex_name, Ren::eResState::ShaderResource, Ren::eStageBits::ComputeShader, *this);
    normal_tex_ =
        builder.ReadTexture(normal_tex_name, Ren::eResState::ShaderResource, Ren::eStageBits::ComputeShader, *this);
    spec_tex_ =
        builder.ReadTexture(spec_tex_name, Ren::eResState::ShaderResource, Ren::eStageBits::ComputeShader, *this);

    shad_tex_ =
        builder.ReadTexture(shadowmap_tex, Ren::eResState::ShaderResource, Ren::eStageBits::ComputeShader, *this);
    ssao_tex_ = builder.ReadTexture(ssao_tex, Ren::eResState::ShaderResource, Ren::eStageBits::FragmentShader, *this);

    { // Output color texture
        Ren::Tex2DParams params;
        params.w = view_state->scr_res[0];
        params.h = view_state->scr_res[1];
        params.format = Ren::eTexFormat::RawRG11F_B10F;
        params.usage = (Ren::eTexUsage::Sampled | Ren::eTexUsage::Storage | Ren::eTexUsage::RenderTarget);
        params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
        params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

        out_color_tex_ = builder.WriteTexture(out_color_img_name, params, Ren::eResState::UnorderedAccess,
                                              Ren::eStageBits::ComputeShader, *this);
    }
}

void RpGBufferShade::LazyInit(Ren::Context &ctx, ShaderLoader &sh) {
    if (!initialized) {
        Ren::ProgramRef gbuf_shade_prog = sh.LoadProgram(ctx, "gbuffer_shade", "internal/gbuffer_shade.comp.glsl");
        assert(gbuf_shade_prog->ready());

        if (!pi_gbuf_shade_.Init(ctx.api_ctx(), std::move(gbuf_shade_prog), ctx.log())) {
            ctx.log()->Error("RpGBufferShade: failed to initialize pipeline!");
        }

        initialized = true;
    }
}

void RpGBufferShade::Execute(RpBuilder &builder) {
    RpAllocBuf &unif_shared_data_buf = builder.GetReadBuffer(shared_data_buf_);
    RpAllocBuf &cells_buf = builder.GetReadBuffer(cells_buf_);
    RpAllocBuf &items_buf = builder.GetReadBuffer(items_buf_);
    RpAllocBuf &lights_buf = builder.GetReadBuffer(lights_buf_);
    RpAllocBuf &decals_buf = builder.GetReadBuffer(decals_buf_);

    RpAllocTex &depth_tex = builder.GetReadTexture(depth_tex_);
    RpAllocTex &albedo_tex = builder.GetReadTexture(albedo_tex_);
    RpAllocTex &normal_tex = builder.GetReadTexture(normal_tex_);
    RpAllocTex &spec_tex = builder.GetReadTexture(spec_tex_);

    RpAllocTex &shad_tex = builder.GetReadTexture(shad_tex_);
    RpAllocTex &ssao_tex = builder.GetReadTexture(ssao_tex_);

    RpAllocTex &out_color_tex = builder.GetWriteTexture(out_color_tex_);

    LazyInit(builder.ctx(), builder.sh());

    const Ren::Binding bindings[] = {{Ren::eBindTarget::UBuf, REN_UB_SHARED_DATA_LOC, *unif_shared_data_buf.ref},
                                     {Ren::eBindTarget::TBuf, GBufferShade::CELLS_BUF_SLOT, *cells_buf.tbos[0]},
                                     {Ren::eBindTarget::TBuf, GBufferShade::ITEMS_BUF_SLOT, *items_buf.tbos[0]},
                                     {Ren::eBindTarget::TBuf, GBufferShade::LIGHT_BUF_SLOT, *lights_buf.tbos[0]},
                                     {Ren::eBindTarget::TBuf, GBufferShade::DECAL_BUF_SLOT, *decals_buf.tbos[0]},
                                     {Ren::eBindTarget::Tex2D, GBufferShade::DEPTH_TEX_SLOT, *depth_tex.ref},
                                     {Ren::eBindTarget::Tex2D, GBufferShade::ALBEDO_TEX_SLOT, *albedo_tex.ref},
                                     {Ren::eBindTarget::Tex2D, GBufferShade::NORMAL_TEX_SLOT, *normal_tex.ref},
                                     {Ren::eBindTarget::Tex2D, GBufferShade::SPECULAR_TEX_SLOT, *spec_tex.ref},
                                     {Ren::eBindTarget::Tex2D, GBufferShade::SHADOW_TEX_SLOT, *shad_tex.ref},
                                     {Ren::eBindTarget::Tex2D, GBufferShade::SSAO_TEX_SLOT, *ssao_tex.ref},
                                     {Ren::eBindTarget::Image, GBufferShade::OUT_COLOR_IMG_SLOT, *out_color_tex.ref}};

    const Ren::Vec3u grp_count = Ren::Vec3u{
        (view_state_->act_res[0] + GBufferShade::LOCAL_GROUP_SIZE_X - 1u) / GBufferShade::LOCAL_GROUP_SIZE_X,
        (view_state_->act_res[1] + GBufferShade::LOCAL_GROUP_SIZE_Y - 1u) / GBufferShade::LOCAL_GROUP_SIZE_Y, 1u};

    GBufferShade::Params uniform_params;
    uniform_params.img_size = Ren::Vec2u{uint32_t(view_state_->act_res[0]), uint32_t(view_state_->act_res[1])};

    Ren::DispatchCompute(pi_gbuf_shade_, grp_count, bindings, COUNT_OF(bindings), &uniform_params,
                         sizeof(uniform_params), builder.ctx().default_descr_alloc(), builder.ctx().log());
}
#endif