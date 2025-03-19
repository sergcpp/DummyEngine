#include "ExRTReflections.h"

#include <Ren/Context.h>
#include <Ren/RastState.h>
#include <Ren/Texture.h>

#include "../../utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"

#include "../shaders/rt_reflections_interface.h"

void Eng::ExRTReflections::Execute_HWRT(FgBuilder &builder) { assert(false && "Not implemented!"); }

void Eng::ExRTReflections::Execute_SWRT(FgBuilder &builder) {
    FgAllocBuf &geo_data_buf = builder.GetReadBuffer(args_->geo_data);
    FgAllocBuf &materials_buf = builder.GetReadBuffer(args_->materials);
    FgAllocBuf &vtx_buf1 = builder.GetReadBuffer(args_->vtx_buf1);
    FgAllocBuf &vtx_buf2 = builder.GetReadBuffer(args_->vtx_buf2);
    FgAllocBuf &ndx_buf = builder.GetReadBuffer(args_->ndx_buf);
    FgAllocBuf &lights_buf = builder.GetReadBuffer(args_->lights_buf);
    FgAllocBuf &rt_blas_buf = builder.GetReadBuffer(args_->swrt.rt_blas_buf);
    FgAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(args_->shared_data);
    FgAllocTex &depth_tex = builder.GetReadTexture(args_->depth_tex);
    FgAllocTex &normal_tex = builder.GetReadTexture(args_->normal_tex);
    FgAllocTex &env_tex = builder.GetReadTexture(args_->env_tex);
    FgAllocBuf &ray_counter_buf = builder.GetReadBuffer(args_->ray_counter);
    FgAllocBuf &ray_list_buf = builder.GetReadBuffer(args_->ray_list);
    FgAllocBuf &indir_args_buf = builder.GetReadBuffer(args_->indir_args);
    FgAllocBuf &rt_tlas_buf = builder.GetReadBuffer(args_->tlas_buf);
    FgAllocBuf &prim_ndx_buf = builder.GetReadBuffer(args_->swrt.prim_ndx_buf);
    FgAllocBuf &mesh_instances_buf = builder.GetReadBuffer(args_->swrt.mesh_instances_buf);
    FgAllocBuf &textures_buf = builder.GetReadBuffer(args_->swrt.textures_buf);
    FgAllocTex &shadow_depth_tex = builder.GetReadTexture(args_->shadow_depth_tex);
    FgAllocTex &shadow_color_tex = builder.GetReadTexture(args_->shadow_color_tex);
    FgAllocTex &ltc_luts_tex = builder.GetReadTexture(args_->ltc_luts_tex);
    FgAllocBuf &cells_buf = builder.GetReadBuffer(args_->cells_buf);
    FgAllocBuf &items_buf = builder.GetReadBuffer(args_->items_buf);

    FgAllocTex *irr_tex = nullptr, *dist_tex = nullptr, *off_tex = nullptr;
    if (args_->irradiance_tex) {
        irr_tex = &builder.GetReadTexture(args_->irradiance_tex);
        dist_tex = &builder.GetReadTexture(args_->distance_tex);
        off_tex = &builder.GetReadTexture(args_->offset_tex);
    }

    FgAllocBuf *stoch_lights_buf = nullptr, *light_nodes_buf = nullptr;
    if (args_->stoch_lights_buf) {
        stoch_lights_buf = &builder.GetReadBuffer(args_->stoch_lights_buf);
        light_nodes_buf = &builder.GetReadBuffer(args_->light_nodes_buf);
    }

    FgAllocBuf *oit_depth_buf = nullptr;
    FgAllocTex *noise_tex = nullptr;
    if (args_->oit_depth_buf) {
        oit_depth_buf = &builder.GetReadBuffer(args_->oit_depth_buf);
    } else {
        noise_tex = &builder.GetReadTexture(args_->noise_tex);
    }

    Ren::Context &ctx = builder.ctx();
    Ren::ApiContext *api_ctx = ctx.api_ctx();

    Ren::SmallVector<Ren::Binding, 24> bindings = {
        {Ren::eBindTarget::SBufRO, BIND_BINDLESS_TEX, *textures_buf.ref},
        {Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, *unif_sh_data_buf.ref},
        {Ren::eBindTarget::Tex2DSampled, RTReflections::DEPTH_TEX_SLOT, {*depth_tex.ref, 1}},
        {Ren::eBindTarget::Tex2DSampled, RTReflections::NORM_TEX_SLOT, *normal_tex.ref},
        {Ren::eBindTarget::SBufRO, RTReflections::RAY_COUNTER_SLOT, *ray_counter_buf.ref},
        {Ren::eBindTarget::SBufRO, RTReflections::RAY_LIST_SLOT, *ray_list_buf.ref},
        {Ren::eBindTarget::Tex2DSampled, RTReflections::ENV_TEX_SLOT, *env_tex.ref},
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
        {Ren::eBindTarget::Tex2DSampled, RTReflections::SHADOW_DEPTH_TEX_SLOT, *shadow_depth_tex.ref},
        {Ren::eBindTarget::Tex2DSampled, RTReflections::SHADOW_COLOR_TEX_SLOT, *shadow_color_tex.ref},
        {Ren::eBindTarget::Tex2DSampled, RTReflections::LTC_LUTS_TEX_SLOT, *ltc_luts_tex.ref},
        {Ren::eBindTarget::UTBuf, RTReflections::CELLS_BUF_SLOT, *cells_buf.ref},
        {Ren::eBindTarget::UTBuf, RTReflections::ITEMS_BUF_SLOT, *items_buf.ref}};
    if (irr_tex) {
        bindings.emplace_back(Ren::eBindTarget::Tex2DArraySampled, RTReflections::IRRADIANCE_TEX_SLOT, *irr_tex->ref);
        bindings.emplace_back(Ren::eBindTarget::Tex2DArraySampled, RTReflections::DISTANCE_TEX_SLOT, *dist_tex->ref);
        bindings.emplace_back(Ren::eBindTarget::Tex2DArraySampled, RTReflections::OFFSET_TEX_SLOT, *off_tex->ref);
    }
    if (stoch_lights_buf) {
        bindings.emplace_back(Ren::eBindTarget::UTBuf, RTReflections::STOCH_LIGHTS_BUF_SLOT, *stoch_lights_buf->ref);
        bindings.emplace_back(Ren::eBindTarget::UTBuf, RTReflections::LIGHT_NODES_BUF_SLOT, *light_nodes_buf->ref);
    }
    if (noise_tex) {
        bindings.emplace_back(Ren::eBindTarget::Tex2DSampled, RTReflections::NOISE_TEX_SLOT, *noise_tex->ref);
    }
    if (oit_depth_buf) {
        bindings.emplace_back(Ren::eBindTarget::UTBuf, RTReflections::OIT_DEPTH_BUF_SLOT, *oit_depth_buf->ref);
    }
    for (int i = 0; i < OIT_REFLECTION_LAYERS; ++i) {
        if (!args_->out_refl_tex[i]) {
            break;
        }
        FgAllocTex &out_refl_tex = builder.GetWriteTexture(args_->out_refl_tex[i]);
        bindings.emplace_back(Ren::eBindTarget::Image2D, RTReflections::OUT_REFL_IMG_SLOT, i, 1, *out_refl_tex.ref);
    }

    const Ren::Pipeline *pi = nullptr;
    if (args_->four_bounces) {
        if (stoch_lights_buf) {
            pi = pi_rt_reflections_4bounce_[2].get();
        } else if (irr_tex) {
            pi = pi_rt_reflections_4bounce_[1].get();
        } else {
            pi = pi_rt_reflections_4bounce_[0].get();
        }
    } else {
        if (stoch_lights_buf) {
            pi = pi_rt_reflections_[2].get();
        } else if (irr_tex) {
            pi = pi_rt_reflections_[1].get();
        } else {
            pi = pi_rt_reflections_[0].get();
        }
    }

    RTReflections::Params uniform_params;
    uniform_params.img_size = Ren::Vec2u{uint32_t(view_state_->act_res[0]), uint32_t(view_state_->act_res[1])};
    uniform_params.pixel_spread_angle = view_state_->pixel_spread_angle;
    if (oit_depth_buf) {
        // Expected to be half resolution
        uniform_params.pixel_spread_angle *= 2.0f;
    }
    uniform_params.lights_count = view_state_->stochastic_lights_count;

    DispatchComputeIndirect(*pi, *indir_args_buf.ref, sizeof(VkTraceRaysIndirectCommandKHR), bindings, &uniform_params,
                            sizeof(uniform_params), nullptr, ctx.log());
}
