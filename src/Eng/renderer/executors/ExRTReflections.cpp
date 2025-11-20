#include "ExRTReflections.h"

#include <Ren/Context.h>
#include <Ren/DrawCall.h>
#include <Ren/RastState.h>

#include "../../utils/ShaderLoader.h"
#include "../shaders/rt_reflections_interface.h"

void Eng::ExRTReflections::Execute(FgContext &fg) {
    LazyInit(fg.ren_ctx(), fg.sh());
    if (fg.ren_ctx().capabilities.hwrt) {
        Execute_HWRT(fg);
    } else {
        Execute_SWRT(fg);
    }
}

void Eng::ExRTReflections::LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh) {
    if (!initialized_) {
        auto hwrt_select = [&ctx](std::string_view hwrt_shader, std::string_view swrt_shader) {
            return ctx.capabilities.hwrt ? hwrt_shader : swrt_shader;
        };
        auto subgroup_select = [&ctx](std::string_view subgroup_shader, std::string_view nosubgroup_shader) {
            return ctx.capabilities.subgroup ? subgroup_shader : nosubgroup_shader;
        };

        if (args_->four_bounces) {
            assert(!args_->layered);
            if (args_->irradiance_tex) {
                pi_rt_reflections_[0] = sh.LoadPipeline(hwrt_select(
                    subgroup_select("internal/rt_reflections_hwrt@FOUR_BOUNCES;GI_CACHE.comp.glsl",
                                    "internal/rt_reflections_hwrt@FOUR_BOUNCES;GI_CACHE;NO_SUBGROUP.comp.glsl"),
                    subgroup_select("internal/rt_reflections_swrt@FOUR_BOUNCES;GI_CACHE.comp.glsl",
                                    "internal/rt_reflections_swrt@FOUR_BOUNCES;GI_CACHE;NO_SUBGROUP.comp.glsl")));
            } else {
                pi_rt_reflections_[0] = sh.LoadPipeline(
                    hwrt_select(subgroup_select("internal/rt_reflections_hwrt@FOUR_BOUNCES.comp.glsl",
                                                "internal/rt_reflections_hwrt@FOUR_BOUNCES;NO_SUBGROUP.comp.glsl"),
                                subgroup_select("internal/rt_reflections_swrt@FOUR_BOUNCES.comp.glsl",
                                                "internal/rt_reflections_swrt@FOUR_BOUNCES;NO_SUBGROUP.comp.glsl")));
            }

            pi_rt_reflections_[1] = sh.LoadPipeline(hwrt_select(
                subgroup_select(
                    "internal/rt_reflections_hwrt@STOCH_LIGHTS;FOUR_BOUNCES;GI_CACHE.comp.glsl",
                    "internal/rt_reflections_hwrt@STOCH_LIGHTS;FOUR_BOUNCES;GI_CACHE;NO_SUBGROUP.comp.glsl"),
                subgroup_select(
                    "internal/rt_reflections_swrt@STOCH_LIGHTS;FOUR_BOUNCES;GI_CACHE.comp.glsl",
                    "internal/rt_reflections_swrt@STOCH_LIGHTS;FOUR_BOUNCES;GI_CACHE;NO_SUBGROUP.comp.glsl")));
        } else {
            if (args_->layered) {
                if (args_->irradiance_tex) {
                    pi_rt_reflections_[0] = sh.LoadPipeline(hwrt_select(
                        subgroup_select("internal/rt_reflections_hwrt@LAYERED;GI_CACHE.comp.glsl",
                                        "internal/rt_reflections_hwrt@LAYERED;GI_CACHE;NO_SUBGROUP.comp.glsl"),
                        subgroup_select("internal/rt_reflections_swrt@LAYERED;GI_CACHE.comp.glsl",
                                        "internal/rt_reflections_swrt@LAYERED;GI_CACHE;NO_SUBGROUP.comp.glsl")));
                } else {
                    pi_rt_reflections_[0] = sh.LoadPipeline(
                        hwrt_select(subgroup_select("internal/rt_reflections_hwrt@LAYERED.comp.glsl",
                                                    "internal/rt_reflections_hwrt@LAYERED;NO_SUBGROUP.comp.glsl"),
                                    subgroup_select("internal/rt_reflections_swrt@LAYERED.comp.glsl",
                                                    "internal/rt_reflections_swrt@LAYERED;NO_SUBGROUP.comp.glsl")));
                }
            } else {
                if (args_->irradiance_tex) {
                    pi_rt_reflections_[0] = sh.LoadPipeline(
                        hwrt_select(subgroup_select("internal/rt_reflections_hwrt@GI_CACHE.comp.glsl",
                                                    "internal/rt_reflections_hwrt@GI_CACHE;NO_SUBGROUP.comp.glsl"),
                                    subgroup_select("internal/rt_reflections_swrt@GI_CACHE.comp.glsl",
                                                    "internal/rt_reflections_swrt@GI_CACHE;NO_SUBGROUP.comp.glsl")));
                } else {
                    pi_rt_reflections_[0] = sh.LoadPipeline(
                        hwrt_select(subgroup_select("internal/rt_reflections_hwrt.comp.glsl",
                                                    "internal/rt_reflections_hwrt@NO_SUBGROUP.comp.glsl"),
                                    subgroup_select("internal/rt_reflections_swrt.comp.glsl",
                                                    "internal/rt_reflections_swrt@NO_SUBGROUP.comp.glsl")));
                }

                pi_rt_reflections_[1] = sh.LoadPipeline(hwrt_select(
                    subgroup_select("internal/rt_reflections_hwrt@STOCH_LIGHTS;GI_CACHE.comp.glsl",
                                    "internal/rt_reflections_hwrt@STOCH_LIGHTS;GI_CACHE;NO_SUBGROUP.comp.glsl"),
                    subgroup_select("internal/rt_reflections_swrt@STOCH_LIGHTS;GI_CACHE.comp.glsl",
                                    "internal/rt_reflections_swrt@STOCH_LIGHTS;GI_CACHE;NO_SUBGROUP.comp.glsl")));
            }
        }
        initialized_ = true;
    }
}

void Eng::ExRTReflections::Execute_SWRT(FgContext &fg) {
    const Ren::Buffer &geo_data_buf = fg.AccessROBuffer(args_->geo_data);
    const Ren::Buffer &materials_buf = fg.AccessROBuffer(args_->materials);
    const Ren::Buffer &vtx_buf1 = fg.AccessROBuffer(args_->vtx_buf1);
    const Ren::Buffer &vtx_buf2 = fg.AccessROBuffer(args_->vtx_buf2);
    const Ren::Buffer &ndx_buf = fg.AccessROBuffer(args_->ndx_buf);
    const Ren::Buffer &rt_blas_buf = fg.AccessROBuffer(args_->swrt.rt_blas_buf);
    const Ren::Buffer &unif_sh_data_buf = fg.AccessROBuffer(args_->shared_data);
    const Ren::Image &depth_tex = fg.AccessROImage(args_->depth_tex);
    const Ren::Image &normal_tex = fg.AccessROImage(args_->normal_tex);
    const Ren::Image &env_tex = fg.AccessROImage(args_->env_tex);
    const Ren::Buffer &ray_counter_buf = fg.AccessROBuffer(args_->ray_counter);
    const Ren::Buffer &ray_list_buf = fg.AccessROBuffer(args_->ray_list);
    const Ren::Buffer &indir_args_buf = fg.AccessROBuffer(args_->indir_args);
    const Ren::Buffer &rt_tlas_buf = fg.AccessROBuffer(args_->tlas_buf);
    const Ren::Buffer &prim_ndx_buf = fg.AccessROBuffer(args_->swrt.prim_ndx_buf);
    const Ren::Buffer &mesh_instances_buf = fg.AccessROBuffer(args_->swrt.mesh_instances_buf);
    const Ren::Buffer &lights_buf = fg.AccessROBuffer(args_->lights_buf);
    const Ren::Image &shadow_depth_tex = fg.AccessROImage(args_->shadow_depth_tex);
    const Ren::Image &shadow_color_tex = fg.AccessROImage(args_->shadow_color_tex);
    const Ren::Image &ltc_luts_tex = fg.AccessROImage(args_->ltc_luts_tex);
    const Ren::Buffer &cells_buf = fg.AccessROBuffer(args_->cells_buf);
    const Ren::Buffer &items_buf = fg.AccessROBuffer(args_->items_buf);

    const Ren::Image *irr_tex = nullptr, *dist_tex = nullptr, *off_tex = nullptr;
    if (args_->irradiance_tex) {
        irr_tex = &fg.AccessROImage(args_->irradiance_tex);
        dist_tex = &fg.AccessROImage(args_->distance_tex);
        off_tex = &fg.AccessROImage(args_->offset_tex);
    }

    const Ren::Buffer *stoch_lights_buf = nullptr, *light_nodes_buf = nullptr;
    if (args_->stoch_lights_buf) {
        stoch_lights_buf = &fg.AccessROBuffer(args_->stoch_lights_buf);
        light_nodes_buf = &fg.AccessROBuffer(args_->light_nodes_buf);
    }

    const Ren::Buffer *oit_depth_buf = nullptr;
    const Ren::Image *noise_tex = nullptr;
    if (args_->oit_depth_buf) {
        oit_depth_buf = &fg.AccessROBuffer(args_->oit_depth_buf);
    } else {
        noise_tex = &fg.AccessROImage(args_->noise_tex);
    }

    Ren::ApiContext *api_ctx = fg.ren_ctx().api_ctx();

    Ren::SmallVector<Ren::Binding, 24> bindings = {
        {Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, unif_sh_data_buf},
        {Ren::eBindTarget::BindlessDescriptors, BIND_BINDLESS_TEX, bindless_tex_->rt_inline_textures},
        {Ren::eBindTarget::TexSampled, RTReflections::DEPTH_TEX_SLOT, {depth_tex, 1}},
        {Ren::eBindTarget::TexSampled, RTReflections::NORM_TEX_SLOT, normal_tex},
        {Ren::eBindTarget::SBufRO, RTReflections::RAY_COUNTER_SLOT, ray_counter_buf},
        {Ren::eBindTarget::SBufRO, RTReflections::RAY_LIST_SLOT, ray_list_buf},
        {Ren::eBindTarget::TexSampled, RTReflections::ENV_TEX_SLOT, env_tex},
        {Ren::eBindTarget::UTBuf, RTReflections::BLAS_BUF_SLOT, rt_blas_buf},
        {Ren::eBindTarget::UTBuf, RTReflections::TLAS_BUF_SLOT, rt_tlas_buf},
        {Ren::eBindTarget::UTBuf, RTReflections::PRIM_NDX_BUF_SLOT, prim_ndx_buf},
        {Ren::eBindTarget::UTBuf, RTReflections::MESH_INSTANCES_BUF_SLOT, mesh_instances_buf},
        {Ren::eBindTarget::SBufRO, RTReflections::GEO_DATA_BUF_SLOT, geo_data_buf},
        {Ren::eBindTarget::SBufRO, RTReflections::MATERIAL_BUF_SLOT, materials_buf},
        {Ren::eBindTarget::UTBuf, RTReflections::VTX_BUF1_SLOT, vtx_buf1},
        {Ren::eBindTarget::UTBuf, RTReflections::VTX_BUF2_SLOT, vtx_buf2},
        {Ren::eBindTarget::UTBuf, RTReflections::NDX_BUF_SLOT, ndx_buf},
        {Ren::eBindTarget::SBufRO, RTReflections::LIGHTS_BUF_SLOT, lights_buf},
        {Ren::eBindTarget::TexSampled, RTReflections::SHADOW_DEPTH_TEX_SLOT, shadow_depth_tex},
        {Ren::eBindTarget::TexSampled, RTReflections::SHADOW_COLOR_TEX_SLOT, shadow_color_tex},
        {Ren::eBindTarget::TexSampled, RTReflections::LTC_LUTS_TEX_SLOT, ltc_luts_tex},
        {Ren::eBindTarget::UTBuf, RTReflections::CELLS_BUF_SLOT, cells_buf},
        {Ren::eBindTarget::UTBuf, RTReflections::ITEMS_BUF_SLOT, items_buf}};
    if (irr_tex) {
        bindings.emplace_back(Ren::eBindTarget::TexSampled, RTReflections::IRRADIANCE_TEX_SLOT, *irr_tex);
        bindings.emplace_back(Ren::eBindTarget::TexSampled, RTReflections::DISTANCE_TEX_SLOT, *dist_tex);
        bindings.emplace_back(Ren::eBindTarget::TexSampled, RTReflections::OFFSET_TEX_SLOT, *off_tex);
    }
    if (stoch_lights_buf) {
        bindings.emplace_back(Ren::eBindTarget::UTBuf, RTReflections::STOCH_LIGHTS_BUF_SLOT, *stoch_lights_buf);
        bindings.emplace_back(Ren::eBindTarget::UTBuf, RTReflections::LIGHT_NODES_BUF_SLOT, *light_nodes_buf);
    }
    if (noise_tex) {
        bindings.emplace_back(Ren::eBindTarget::TexSampled, RTReflections::NOISE_TEX_SLOT, *noise_tex);
    }
    if (oit_depth_buf) {
        bindings.emplace_back(Ren::eBindTarget::UTBuf, RTReflections::OIT_DEPTH_BUF_SLOT, *oit_depth_buf);
    }
    for (int i = 0; i < OIT_REFLECTION_LAYERS && args_->out_refl_tex[i]; ++i) {
        Ren::Image &out_refl_tex = fg.AccessRWImage(args_->out_refl_tex[i]);
        bindings.emplace_back(Ren::eBindTarget::ImageRW, RTReflections::OUT_REFL_IMG_SLOT, i, 1, out_refl_tex);
    }

    const Ren::Pipeline &pi = *pi_rt_reflections_[stoch_lights_buf != nullptr];

    RTReflections::Params uniform_params;
    uniform_params.img_size = Ren::Vec2u{view_state_->ren_res};
    uniform_params.pixel_spread_angle = view_state_->pixel_spread_angle;
    if (oit_depth_buf) {
        // Expected to be half resolution
        uniform_params.pixel_spread_angle *= 2.0f;
    }
    uniform_params.lights_count = view_state_->stochastic_lights_count;

    DispatchComputeIndirect(fg.cmd_buf(), pi, indir_args_buf, sizeof(VkTraceRaysIndirectCommandKHR), bindings,
                            &uniform_params, sizeof(uniform_params), fg.descr_alloc(), fg.log());
}

#if defined(REN_GL_BACKEND)
void Eng::ExRTReflections::Execute_HWRT(FgContext &fg) { assert(false && "Not implemented!"); }
#endif
