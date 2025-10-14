#include "ExDebugRT.h"

#include <Ren/Context.h>
#include <Ren/RastState.h>
#include <Ren/Texture.h>

#include "../../utils/ShaderLoader.h"
#include "../PrimDraw.h"

#include "../shaders/rt_debug_interface.h"

void Eng::ExDebugRT::Execute_SWRT(FgContext &ctx) {
    FgAllocBuf &geo_data_buf = ctx.AccessROBuffer(args_->geo_data_buf);
    FgAllocBuf &materials_buf = ctx.AccessROBuffer(args_->materials_buf);
    FgAllocBuf &vtx_buf1 = ctx.AccessROBuffer(args_->vtx_buf1);
    FgAllocBuf &vtx_buf2 = ctx.AccessROBuffer(args_->vtx_buf2);
    FgAllocBuf &ndx_buf = ctx.AccessROBuffer(args_->ndx_buf);
    FgAllocBuf &lights_buf = ctx.AccessROBuffer(args_->lights_buf);
    FgAllocBuf &rt_tlas_buf = ctx.AccessROBuffer(args_->tlas_buf);
    FgAllocBuf &rt_blas_buf = ctx.AccessROBuffer(args_->swrt.rt_blas_buf);
    FgAllocBuf &prim_ndx_buf = ctx.AccessROBuffer(args_->swrt.prim_ndx_buf);
    FgAllocBuf &mesh_instances_buf = ctx.AccessROBuffer(args_->swrt.mesh_instances_buf);
    FgAllocBuf &textures_buf = ctx.AccessROBuffer(args_->swrt.textures_buf);
    FgAllocBuf &unif_sh_data_buf = ctx.AccessROBuffer(args_->shared_data);
    FgAllocTex &env_tex = ctx.AccessROTexture(args_->env_tex);
    FgAllocTex &shadow_depth_tex = ctx.AccessROTexture(args_->shadow_depth_tex);
    FgAllocTex &shadow_color_tex = ctx.AccessROTexture(args_->shadow_color_tex);
    FgAllocTex &ltc_luts_tex = ctx.AccessROTexture(args_->ltc_luts_tex);
    FgAllocBuf &cells_buf = ctx.AccessROBuffer(args_->cells_buf);
    FgAllocBuf &items_buf = ctx.AccessROBuffer(args_->items_buf);

    FgAllocTex *irr_tex = nullptr, *dist_tex = nullptr, *off_tex = nullptr;
    if (args_->irradiance_tex) {
        irr_tex = &ctx.AccessROTexture(args_->irradiance_tex);
        dist_tex = &ctx.AccessROTexture(args_->distance_tex);
        off_tex = &ctx.AccessROTexture(args_->offset_tex);
    }

    FgAllocTex *output_tex = &ctx.AccessRWTexture(args_->output_tex);

    Ren::SmallVector<Ren::Binding, 24> bindings = {
        {Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, *unif_sh_data_buf.ref},
        {Ren::eBindTarget::SBufRO, BIND_BINDLESS_TEX, *textures_buf.ref},
        {Ren::eBindTarget::SBufRO, RTDebug::GEO_DATA_BUF_SLOT, *geo_data_buf.ref},
        {Ren::eBindTarget::SBufRO, RTDebug::MATERIAL_BUF_SLOT, *materials_buf.ref},
        {Ren::eBindTarget::UTBuf, RTDebug::VTX_BUF1_SLOT, *vtx_buf1.ref},
        {Ren::eBindTarget::UTBuf, RTDebug::VTX_BUF2_SLOT, *vtx_buf2.ref},
        {Ren::eBindTarget::UTBuf, RTDebug::NDX_BUF_SLOT, *ndx_buf.ref},
        {Ren::eBindTarget::UTBuf, RTDebug::BLAS_BUF_SLOT, *rt_blas_buf.ref},
        {Ren::eBindTarget::UTBuf, RTDebug::TLAS_BUF_SLOT, *rt_tlas_buf.ref},
        {Ren::eBindTarget::UTBuf, RTDebug::PRIM_NDX_BUF_SLOT, *prim_ndx_buf.ref},
        {Ren::eBindTarget::UTBuf, RTDebug::MESH_INSTANCES_BUF_SLOT, *mesh_instances_buf.ref},
        {Ren::eBindTarget::SBufRO, RTDebug::LIGHTS_BUF_SLOT, *lights_buf.ref},
        {Ren::eBindTarget::TexSampled, RTDebug::SHADOW_DEPTH_TEX_SLOT, *shadow_depth_tex.ref},
        {Ren::eBindTarget::TexSampled, RTDebug::SHADOW_COLOR_TEX_SLOT, *shadow_color_tex.ref},
        {Ren::eBindTarget::TexSampled, RTDebug::LTC_LUTS_TEX_SLOT, *ltc_luts_tex.ref},
        {Ren::eBindTarget::UTBuf, RTDebug::CELLS_BUF_SLOT, *cells_buf.ref},
        {Ren::eBindTarget::UTBuf, RTDebug::ITEMS_BUF_SLOT, *items_buf.ref},
        {Ren::eBindTarget::TexSampled, RTDebug::ENV_TEX_SLOT, *env_tex.ref},
        {Ren::eBindTarget::ImageRW, RTDebug::OUT_IMG_SLOT, *output_tex->ref}};
    if (irr_tex) {
        bindings.emplace_back(Ren::eBindTarget::TexSampled, RTDebug::IRRADIANCE_TEX_SLOT, *irr_tex->ref);
        bindings.emplace_back(Ren::eBindTarget::TexSampled, RTDebug::DISTANCE_TEX_SLOT, *dist_tex->ref);
        bindings.emplace_back(Ren::eBindTarget::TexSampled, RTDebug::OFFSET_TEX_SLOT, *off_tex->ref);
    }

    const auto grp_count = Ren::Vec3u{(view_state_->ren_res[0] + RTDebug::GRP_SIZE_X - 1u) / RTDebug::GRP_SIZE_X,
                                      (view_state_->ren_res[1] + RTDebug::GRP_SIZE_Y - 1u) / RTDebug::GRP_SIZE_Y, 1u};

    RTDebug::Params uniform_params;
    uniform_params.img_size = Ren::Vec2u{view_state_->ren_res};
    uniform_params.pixel_spread_angle = view_state_->pixel_spread_angle;
    uniform_params.root_node = args_->swrt.root_node;
    uniform_params.cull_mask = args_->cull_mask;

    DispatchCompute(*pi_debug_, grp_count, bindings, &uniform_params, sizeof(uniform_params), ctx.descr_alloc(),
                    ctx.log());
}
