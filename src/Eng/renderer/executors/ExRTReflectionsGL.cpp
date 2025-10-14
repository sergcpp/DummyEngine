#include "ExRTReflections.h"

#include <Ren/Context.h>
#include <Ren/RastState.h>
#include <Ren/Texture.h>

#include "../../utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"

#include "../shaders/rt_reflections_interface.h"

void Eng::ExRTReflections::Execute_HWRT(FgContext &ctx) { assert(false && "Not implemented!"); }

void Eng::ExRTReflections::Execute_SWRT(FgContext &ctx) {
    FgAllocBuf &geo_data_buf = ctx.AccessROBuffer(args_->geo_data);
    FgAllocBuf &materials_buf = ctx.AccessROBuffer(args_->materials);
    FgAllocBuf &vtx_buf1 = ctx.AccessROBuffer(args_->vtx_buf1);
    FgAllocBuf &vtx_buf2 = ctx.AccessROBuffer(args_->vtx_buf2);
    FgAllocBuf &ndx_buf = ctx.AccessROBuffer(args_->ndx_buf);
    FgAllocBuf &lights_buf = ctx.AccessROBuffer(args_->lights_buf);
    FgAllocBuf &rt_blas_buf = ctx.AccessROBuffer(args_->swrt.rt_blas_buf);
    FgAllocBuf &unif_sh_data_buf = ctx.AccessROBuffer(args_->shared_data);
    FgAllocTex &depth_tex = ctx.AccessROTexture(args_->depth_tex);
    FgAllocTex &normal_tex = ctx.AccessROTexture(args_->normal_tex);
    FgAllocTex &env_tex = ctx.AccessROTexture(args_->env_tex);
    FgAllocBuf &ray_counter_buf = ctx.AccessROBuffer(args_->ray_counter);
    FgAllocBuf &ray_list_buf = ctx.AccessROBuffer(args_->ray_list);
    FgAllocBuf &indir_args_buf = ctx.AccessROBuffer(args_->indir_args);
    FgAllocBuf &rt_tlas_buf = ctx.AccessROBuffer(args_->tlas_buf);
    FgAllocBuf &prim_ndx_buf = ctx.AccessROBuffer(args_->swrt.prim_ndx_buf);
    FgAllocBuf &mesh_instances_buf = ctx.AccessROBuffer(args_->swrt.mesh_instances_buf);
    FgAllocBuf &textures_buf = ctx.AccessROBuffer(args_->swrt.textures_buf);
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

    FgAllocBuf *stoch_lights_buf = nullptr, *light_nodes_buf = nullptr;
    if (args_->stoch_lights_buf) {
        stoch_lights_buf = &ctx.AccessROBuffer(args_->stoch_lights_buf);
        light_nodes_buf = &ctx.AccessROBuffer(args_->light_nodes_buf);
    }

    FgAllocBuf *oit_depth_buf = nullptr;
    FgAllocTex *noise_tex = nullptr;
    if (args_->oit_depth_buf) {
        oit_depth_buf = &ctx.AccessROBuffer(args_->oit_depth_buf);
    } else {
        noise_tex = &ctx.AccessROTexture(args_->noise_tex);
    }

    Ren::SmallVector<Ren::Binding, 24> bindings = {
        {Ren::eBindTarget::SBufRO, BIND_BINDLESS_TEX, *textures_buf.ref},
        {Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, *unif_sh_data_buf.ref},
        {Ren::eBindTarget::TexSampled, RTReflections::DEPTH_TEX_SLOT, {*depth_tex.ref, 1}},
        {Ren::eBindTarget::TexSampled, RTReflections::NORM_TEX_SLOT, *normal_tex.ref},
        {Ren::eBindTarget::SBufRO, RTReflections::RAY_COUNTER_SLOT, *ray_counter_buf.ref},
        {Ren::eBindTarget::SBufRO, RTReflections::RAY_LIST_SLOT, *ray_list_buf.ref},
        {Ren::eBindTarget::TexSampled, RTReflections::ENV_TEX_SLOT, *env_tex.ref},
        {Ren::eBindTarget::UTBuf, RTReflections::BLAS_BUF_SLOT, *rt_blas_buf.ref},
        {Ren::eBindTarget::UTBuf, RTReflections::TLAS_BUF_SLOT, *rt_tlas_buf.ref},
        {Ren::eBindTarget::UTBuf, RTReflections::PRIM_NDX_BUF_SLOT, *prim_ndx_buf.ref},
        {Ren::eBindTarget::UTBuf, RTReflections::MESH_INSTANCES_BUF_SLOT, *mesh_instances_buf.ref},
        {Ren::eBindTarget::SBufRO, RTReflections::GEO_DATA_BUF_SLOT, *geo_data_buf.ref},
        {Ren::eBindTarget::SBufRO, RTReflections::MATERIAL_BUF_SLOT, *materials_buf.ref},
        {Ren::eBindTarget::UTBuf, RTReflections::VTX_BUF1_SLOT, *vtx_buf1.ref},
        {Ren::eBindTarget::UTBuf, RTReflections::VTX_BUF2_SLOT, *vtx_buf2.ref},
        {Ren::eBindTarget::UTBuf, RTReflections::NDX_BUF_SLOT, *ndx_buf.ref},
        {Ren::eBindTarget::SBufRO, RTReflections::LIGHTS_BUF_SLOT, *lights_buf.ref},
        {Ren::eBindTarget::TexSampled, RTReflections::SHADOW_DEPTH_TEX_SLOT, *shadow_depth_tex.ref},
        {Ren::eBindTarget::TexSampled, RTReflections::SHADOW_COLOR_TEX_SLOT, *shadow_color_tex.ref},
        {Ren::eBindTarget::TexSampled, RTReflections::LTC_LUTS_TEX_SLOT, *ltc_luts_tex.ref},
        {Ren::eBindTarget::UTBuf, RTReflections::CELLS_BUF_SLOT, *cells_buf.ref},
        {Ren::eBindTarget::UTBuf, RTReflections::ITEMS_BUF_SLOT, *items_buf.ref}};
    if (irr_tex) {
        bindings.emplace_back(Ren::eBindTarget::TexSampled, RTReflections::IRRADIANCE_TEX_SLOT, *irr_tex->ref);
        bindings.emplace_back(Ren::eBindTarget::TexSampled, RTReflections::DISTANCE_TEX_SLOT, *dist_tex->ref);
        bindings.emplace_back(Ren::eBindTarget::TexSampled, RTReflections::OFFSET_TEX_SLOT, *off_tex->ref);
    }
    if (stoch_lights_buf) {
        bindings.emplace_back(Ren::eBindTarget::UTBuf, RTReflections::STOCH_LIGHTS_BUF_SLOT, *stoch_lights_buf->ref);
        bindings.emplace_back(Ren::eBindTarget::UTBuf, RTReflections::LIGHT_NODES_BUF_SLOT, *light_nodes_buf->ref);
    }
    if (noise_tex) {
        bindings.emplace_back(Ren::eBindTarget::TexSampled, RTReflections::NOISE_TEX_SLOT, *noise_tex->ref);
    }
    if (oit_depth_buf) {
        bindings.emplace_back(Ren::eBindTarget::UTBuf, RTReflections::OIT_DEPTH_BUF_SLOT, *oit_depth_buf->ref);
    }
    for (int i = 0; i < OIT_REFLECTION_LAYERS && args_->out_refl_tex[i]; ++i) {
        FgAllocTex &out_refl_tex = ctx.AccessRWTexture(args_->out_refl_tex[i]);
        bindings.emplace_back(Ren::eBindTarget::ImageRW, RTReflections::OUT_REFL_IMG_SLOT, i, 1, *out_refl_tex.ref);
    }

    const Ren::Pipeline *pi = nullptr;
    if (stoch_lights_buf) {
        pi = pi_rt_reflections_[1].get();
    } else {
        pi = pi_rt_reflections_[0].get();
    }

    RTReflections::Params uniform_params;
    uniform_params.img_size = Ren::Vec2u{view_state_->ren_res};
    uniform_params.pixel_spread_angle = view_state_->pixel_spread_angle;
    if (oit_depth_buf) {
        // Expected to be half resolution
        uniform_params.pixel_spread_angle *= 2.0f;
    }
    uniform_params.lights_count = view_state_->stochastic_lights_count;

    DispatchComputeIndirect(*pi, *indir_args_buf.ref, sizeof(VkTraceRaysIndirectCommandKHR), bindings, &uniform_params,
                            sizeof(uniform_params), ctx.descr_alloc(), ctx.log());
}
