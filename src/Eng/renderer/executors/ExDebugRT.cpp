#include "ExDebugRT.h"

#include <Ren/Context.h>
#include <Ren/DrawCall.h>

#include "../../utils/ShaderLoader.h"
#include "../shaders/rt_debug_interface.h"

Eng::ExDebugRT::ExDebugRT(FgContext &fg, const view_state_t *view_state, const BindlessTextureData *bindless_tex,
                          const Args *args) {
    view_state_ = view_state;
    bindless_tex_ = bindless_tex;
    args_ = args;
#if defined(REN_VK_BACKEND)
    if (fg.ren_ctx().capabilities.hwrt) {
        Ren::ProgramRef debug_hwrt_prog =
            fg.sh().LoadProgram2("internal/rt_debug.rgen.glsl", "internal/rt_debug@GI_CACHE.rchit.glsl",
                                 "internal/rt_debug.rahit.glsl", "internal/rt_debug.rmiss.glsl", {});
        pi_debug_ = fg.sh().LoadPipeline(debug_hwrt_prog);
    } else
#endif
    {
        pi_debug_ = fg.sh().LoadPipeline("internal/rt_debug_swrt@GI_CACHE.comp.glsl");
    }
}

void Eng::ExDebugRT::Execute(FgContext &fg) {
    if (fg.ren_ctx().capabilities.hwrt) {
        Execute_HWRT(fg);
    } else {
        Execute_SWRT(fg);
    }
}

void Eng::ExDebugRT::Execute_SWRT(FgContext &fg) {
    const Ren::Buffer &geo_data_buf = fg.AccessROBuffer(args_->geo_data_buf);
    const Ren::Buffer &materials_buf = fg.AccessROBuffer(args_->materials_buf);
    const Ren::Buffer &vtx_buf1 = fg.AccessROBuffer(args_->vtx_buf1);
    const Ren::Buffer &vtx_buf2 = fg.AccessROBuffer(args_->vtx_buf2);
    const Ren::Buffer &ndx_buf = fg.AccessROBuffer(args_->ndx_buf);
    const Ren::Buffer &lights_buf = fg.AccessROBuffer(args_->lights_buf);
    const Ren::Buffer &rt_tlas_buf = fg.AccessROBuffer(args_->tlas_buf);
    const Ren::Buffer &rt_blas_buf = fg.AccessROBuffer(args_->swrt.rt_blas_buf);
    const Ren::Buffer &prim_ndx_buf = fg.AccessROBuffer(args_->swrt.prim_ndx_buf);
    const Ren::Buffer &mesh_instances_buf = fg.AccessROBuffer(args_->swrt.mesh_instances_buf);
    const Ren::Buffer &unif_sh_data_buf = fg.AccessROBuffer(args_->shared_data);
    const Ren::Texture &env_tex = fg.AccessROTexture(args_->env_tex);
    const Ren::Texture &shadow_depth_tex = fg.AccessROTexture(args_->shadow_depth_tex);
    const Ren::Texture &shadow_color_tex = fg.AccessROTexture(args_->shadow_color_tex);
    const Ren::Texture &ltc_luts_tex = fg.AccessROTexture(args_->ltc_luts_tex);
    const Ren::Buffer &cells_buf = fg.AccessROBuffer(args_->cells_buf);
    const Ren::Buffer &items_buf = fg.AccessROBuffer(args_->items_buf);

    const Ren::Texture *irr_tex = nullptr, *dist_tex = nullptr, *off_tex = nullptr;
    if (args_->irradiance_tex) {
        irr_tex = &fg.AccessROTexture(args_->irradiance_tex);
        dist_tex = &fg.AccessROTexture(args_->distance_tex);
        off_tex = &fg.AccessROTexture(args_->offset_tex);
    }

    Ren::Texture &output_tex = fg.AccessRWTexture(args_->output_tex);

    Ren::ApiContext *api_ctx = fg.ren_ctx().api_ctx();

    Ren::SmallVector<Ren::Binding, 24> bindings = {
        {Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, unif_sh_data_buf},
        {Ren::eBindTarget::BindlessDescriptors, BIND_BINDLESS_TEX, bindless_tex_->rt_inline_textures},
        {Ren::eBindTarget::SBufRO, RTDebug::GEO_DATA_BUF_SLOT, geo_data_buf},
        {Ren::eBindTarget::SBufRO, RTDebug::MATERIAL_BUF_SLOT, materials_buf},
        {Ren::eBindTarget::UTBuf, RTDebug::VTX_BUF1_SLOT, vtx_buf1},
        {Ren::eBindTarget::UTBuf, RTDebug::VTX_BUF2_SLOT, vtx_buf2},
        {Ren::eBindTarget::UTBuf, RTDebug::NDX_BUF_SLOT, ndx_buf},
        {Ren::eBindTarget::UTBuf, RTDebug::BLAS_BUF_SLOT, rt_blas_buf},
        {Ren::eBindTarget::UTBuf, RTDebug::TLAS_BUF_SLOT, rt_tlas_buf},
        {Ren::eBindTarget::UTBuf, RTDebug::PRIM_NDX_BUF_SLOT, prim_ndx_buf},
        {Ren::eBindTarget::UTBuf, RTDebug::MESH_INSTANCES_BUF_SLOT, mesh_instances_buf},
        {Ren::eBindTarget::SBufRO, RTDebug::LIGHTS_BUF_SLOT, lights_buf},
        {Ren::eBindTarget::TexSampled, RTDebug::ENV_TEX_SLOT, env_tex},
        {Ren::eBindTarget::TexSampled, RTDebug::SHADOW_DEPTH_TEX_SLOT, shadow_depth_tex},
        {Ren::eBindTarget::TexSampled, RTDebug::SHADOW_COLOR_TEX_SLOT, shadow_color_tex},
        {Ren::eBindTarget::TexSampled, RTDebug::LTC_LUTS_TEX_SLOT, ltc_luts_tex},
        {Ren::eBindTarget::UTBuf, RTDebug::CELLS_BUF_SLOT, cells_buf},
        {Ren::eBindTarget::UTBuf, RTDebug::ITEMS_BUF_SLOT, items_buf},
        {Ren::eBindTarget::ImageRW, RTDebug::OUT_IMG_SLOT, output_tex}};
    if (irr_tex) {
        bindings.emplace_back(Ren::eBindTarget::TexSampled, RTDebug::IRRADIANCE_TEX_SLOT, *irr_tex);
        bindings.emplace_back(Ren::eBindTarget::TexSampled, RTDebug::DISTANCE_TEX_SLOT, *dist_tex);
        bindings.emplace_back(Ren::eBindTarget::TexSampled, RTDebug::OFFSET_TEX_SLOT, *off_tex);
    }

    const auto grp_count = Ren::Vec3u{(view_state_->ren_res[0] + RTDebug::GRP_SIZE_X - 1u) / RTDebug::GRP_SIZE_X,
                                      (view_state_->ren_res[1] + RTDebug::GRP_SIZE_Y - 1u) / RTDebug::GRP_SIZE_Y, 1u};

    RTDebug::Params uniform_params;
    uniform_params.img_size = Ren::Vec2u{view_state_->ren_res};
    uniform_params.pixel_spread_angle = view_state_->pixel_spread_angle;
    uniform_params.root_node = args_->swrt.root_node;
    uniform_params.cull_mask = args_->cull_mask;

    DispatchCompute(fg.cmd_buf(), *pi_debug_, grp_count, bindings, &uniform_params, sizeof(uniform_params),
                    fg.descr_alloc(), fg.log());
}

#if defined(REN_GL_BACKEND)
void Eng::ExDebugRT::Execute_HWRT(FgContext &fg) { assert(false && "Not implemented!"); }
#endif
