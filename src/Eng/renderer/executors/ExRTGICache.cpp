#include "ExRTGICache.h"

#include <Ren/Context.h>
#include <Ren/DrawCall.h>

#include "../../utils/ShaderLoader.h"
#include "../shaders/rt_gi_cache_interface.h"

void Eng::ExRTGICache::Execute(FgContext &fg) {
    LazyInit(fg.ren_ctx(), fg.sh());
    if (fg.ren_ctx().capabilities.hwrt) {
        Execute_HWRT(fg);
    } else {
        Execute_SWRT(fg);
    }
}

void Eng::ExRTGICache::LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh) {
    if (!initialized_) {
        auto hwrt_select = [&ctx](std::string_view hwrt_shader, std::string_view swrt_shader) {
            return ctx.capabilities.hwrt ? hwrt_shader : swrt_shader;
        };
        auto subgroup_select = [&ctx](std::string_view subgroup_shader, std::string_view nosubgroup_shader) {
            return ctx.capabilities.subgroup ? subgroup_shader : nosubgroup_shader;
        };

        pi_rt_gi_cache_[0][0] = sh.LoadPipeline(hwrt_select(
            subgroup_select("internal/rt_gi_cache_hwrt.comp.glsl", "internal/rt_gi_cache_hwrt@NO_SUBGROUP.comp.glsl"),
            subgroup_select("internal/rt_gi_cache_swrt.comp.glsl", "internal/rt_gi_cache_swrt@NO_SUBGROUP.comp.glsl")));
        if (!pi_rt_gi_cache_[0][0]) {
            ctx.log()->Error("ExRTGICache: Failed to initialize pipeline!");
        }

        pi_rt_gi_cache_[0][1] =
            sh.LoadPipeline(hwrt_select(subgroup_select("internal/rt_gi_cache_hwrt@PARTIAL.comp.glsl",
                                                        "internal/rt_gi_cache_hwrt@PARTIAL;NO_SUBGROUP.comp.glsl"),
                                        subgroup_select("internal/rt_gi_cache_swrt@PARTIAL.comp.glsl",
                                                        "internal/rt_gi_cache_swrt@PARTIAL;NO_SUBGROUP.comp.glsl")));
        if (!pi_rt_gi_cache_[0][1]) {
            ctx.log()->Error("ExRTGICache: Failed to initialize pipeline!");
        }

        pi_rt_gi_cache_[1][0] = sh.LoadPipeline(
            hwrt_select(subgroup_select("internal/rt_gi_cache_hwrt@STOCH_LIGHTS.comp.glsl",
                                        "internal/rt_gi_cache_hwrt@STOCH_LIGHTS;NO_SUBGROUP.comp.glsl"),
                        subgroup_select("internal/rt_gi_cache_swrt@STOCH_LIGHTS.comp.glsl",
                                        "internal/rt_gi_cache_swrt@STOCH_LIGHTS;NO_SUBGROUP.comp.glsl")));
        if (!pi_rt_gi_cache_[1][0]) {
            ctx.log()->Error("ExRTGICache: Failed to initialize pipeline!");
        }

        pi_rt_gi_cache_[1][1] = sh.LoadPipeline(
            hwrt_select(subgroup_select("internal/rt_gi_cache_hwrt@STOCH_LIGHTS;PARTIAL.comp.glsl",
                                        "internal/rt_gi_cache_hwrt@STOCH_LIGHTS;PARTIAL;NO_SUBGROUP.comp.glsl"),
                        subgroup_select("internal/rt_gi_cache_swrt@STOCH_LIGHTS;PARTIAL.comp.glsl",
                                        "internal/rt_gi_cache_swrt@STOCH_LIGHTS;PARTIAL;NO_SUBGROUP.comp.glsl")));
        if (!pi_rt_gi_cache_[1][1]) {
            ctx.log()->Error("ExRTGICache: Failed to initialize pipeline!");
        }

        initialized_ = true;
    }
}

void Eng::ExRTGICache::Execute_SWRT(FgContext &fg) {
    Ren::ApiContext *api_ctx = fg.ren_ctx().api_ctx();

    const Ren::Buffer &geo_data_buf = fg.AccessROBuffer(args_->geo_data);
    const Ren::Buffer &materials_buf = fg.AccessROBuffer(args_->materials);
    const Ren::Buffer &vtx_buf1 = fg.AccessROBuffer(args_->vtx_buf1);
    const Ren::Buffer &ndx_buf = fg.AccessROBuffer(args_->ndx_buf);
    const Ren::Buffer &rt_blas_buf = fg.AccessROBuffer(args_->swrt.rt_blas_buf);
    const Ren::Buffer &unif_sh_data_buf = fg.AccessROBuffer(args_->shared_data);
    const Ren::Texture &env_tex = fg.AccessROTexture(args_->env_tex);
    const Ren::Buffer &rt_tlas_buf = fg.AccessROBuffer(args_->tlas_buf);
    const Ren::Buffer &prim_ndx_buf = fg.AccessROBuffer(args_->swrt.prim_ndx_buf);
    const Ren::Buffer &mesh_instances_buf = fg.AccessROBuffer(args_->swrt.mesh_instances_buf);
    const Ren::Buffer &lights_buf = fg.AccessROBuffer(args_->lights_buf);
    const Ren::Texture &shadow_depth_tex = fg.AccessROTexture(args_->shadow_depth_tex);
    const Ren::Texture &shadow_color_tex = fg.AccessROTexture(args_->shadow_color_tex);
    const Ren::Texture &ltc_luts_tex = fg.AccessROTexture(args_->ltc_luts_tex);
    const Ren::Buffer &cells_buf = fg.AccessROBuffer(args_->cells_buf);
    const Ren::Buffer &items_buf = fg.AccessROBuffer(args_->items_buf);
    const Ren::Texture &irr_tex = fg.AccessROTexture(args_->irradiance_tex);
    const Ren::Texture &dist_tex = fg.AccessROTexture(args_->distance_tex);
    const Ren::Texture &off_tex = fg.AccessROTexture(args_->offset_tex);

    const Ren::Buffer *random_seq_buf = nullptr, *stoch_lights_buf = nullptr, *light_nodes_buf = nullptr;
    if (args_->stoch_lights_buf) {
        random_seq_buf = &fg.AccessROBuffer(args_->random_seq);
        stoch_lights_buf = &fg.AccessROBuffer(args_->stoch_lights_buf);
        light_nodes_buf = &fg.AccessROBuffer(args_->light_nodes_buf);
    }

    Ren::Texture &out_ray_data_tex = fg.AccessRWTexture(args_->out_ray_data_tex);

    Ren::SmallVector<Ren::Binding, 16> bindings = {
        {Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, unif_sh_data_buf},
        {Ren::eBindTarget::BindlessDescriptors, BIND_BINDLESS_TEX, bindless_tex_->rt_inline_textures},
        {Ren::eBindTarget::TexSampled, RTGICache::ENV_TEX_SLOT, env_tex},
        {Ren::eBindTarget::UTBuf, RTGICache::BLAS_BUF_SLOT, rt_blas_buf},
        {Ren::eBindTarget::UTBuf, RTGICache::TLAS_BUF_SLOT, rt_tlas_buf},
        {Ren::eBindTarget::UTBuf, RTGICache::PRIM_NDX_BUF_SLOT, prim_ndx_buf},
        {Ren::eBindTarget::UTBuf, RTGICache::MESH_INSTANCES_BUF_SLOT, mesh_instances_buf},
        {Ren::eBindTarget::SBufRO, RTGICache::GEO_DATA_BUF_SLOT, geo_data_buf},
        {Ren::eBindTarget::SBufRO, RTGICache::MATERIAL_BUF_SLOT, materials_buf},
        {Ren::eBindTarget::UTBuf, RTGICache::VTX_BUF1_SLOT, vtx_buf1},
        {Ren::eBindTarget::UTBuf, RTGICache::NDX_BUF_SLOT, ndx_buf},
        {Ren::eBindTarget::SBufRO, RTGICache::LIGHTS_BUF_SLOT, lights_buf},
        {Ren::eBindTarget::TexSampled, RTGICache::SHADOW_DEPTH_TEX_SLOT, shadow_depth_tex},
        {Ren::eBindTarget::TexSampled, RTGICache::SHADOW_COLOR_TEX_SLOT, shadow_color_tex},
        {Ren::eBindTarget::TexSampled, RTGICache::LTC_LUTS_TEX_SLOT, ltc_luts_tex},
        {Ren::eBindTarget::UTBuf, RTGICache::CELLS_BUF_SLOT, cells_buf},
        {Ren::eBindTarget::UTBuf, RTGICache::ITEMS_BUF_SLOT, items_buf},
        {Ren::eBindTarget::TexSampled, RTGICache::IRRADIANCE_TEX_SLOT, irr_tex},
        {Ren::eBindTarget::TexSampled, RTGICache::DISTANCE_TEX_SLOT, dist_tex},
        {Ren::eBindTarget::TexSampled, RTGICache::OFFSET_TEX_SLOT, off_tex},
        {Ren::eBindTarget::ImageRW, RTGICache::OUT_RAY_DATA_IMG_SLOT, out_ray_data_tex}};
    if (stoch_lights_buf) {
        bindings.emplace_back(Ren::eBindTarget::UTBuf, RTGICache::RANDOM_SEQ_BUF_SLOT, *random_seq_buf);
        bindings.emplace_back(Ren::eBindTarget::UTBuf, RTGICache::STOCH_LIGHTS_BUF_SLOT, *stoch_lights_buf);
        bindings.emplace_back(Ren::eBindTarget::UTBuf, RTGICache::LIGHT_NODES_BUF_SLOT, *light_nodes_buf);
    }

    const Ren::Pipeline &pi = *pi_rt_gi_cache_[stoch_lights_buf != nullptr][args_->partial_update];

    RTGICache::Params uniform_params = {};
    uniform_params.volume_index = view_state_->volume_to_update;
    uniform_params.stoch_lights_count = view_state_->stochastic_lights_count_cache;
    uniform_params.pass_hash = view_state_->probe_ray_hash;
    uniform_params.oct_index = (args_->probe_volumes[view_state_->volume_to_update].updates_count - 1) % 8;
    uniform_params.grid_origin = Ren::Vec4f(args_->probe_volumes[view_state_->volume_to_update].origin[0],
                                            args_->probe_volumes[view_state_->volume_to_update].origin[1],
                                            args_->probe_volumes[view_state_->volume_to_update].origin[2], 0.0f);
    uniform_params.grid_scroll = Ren::Vec4i(args_->probe_volumes[view_state_->volume_to_update].scroll[0],
                                            args_->probe_volumes[view_state_->volume_to_update].scroll[1],
                                            args_->probe_volumes[view_state_->volume_to_update].scroll[2], 0.0f);
    uniform_params.grid_scroll_diff = Ren::Vec4i(args_->probe_volumes[view_state_->volume_to_update].scroll_diff[0],
                                                 args_->probe_volumes[view_state_->volume_to_update].scroll_diff[1],
                                                 args_->probe_volumes[view_state_->volume_to_update].scroll_diff[2], 0);
    uniform_params.grid_spacing = Ren::Vec4f(args_->probe_volumes[view_state_->volume_to_update].spacing[0],
                                             args_->probe_volumes[view_state_->volume_to_update].spacing[1],
                                             args_->probe_volumes[view_state_->volume_to_update].spacing[2], 0.0f);
    uniform_params.quat_rot = view_state_->probe_ray_rotator;

    const auto grp_count = Ren::Vec3u{(PROBE_TOTAL_RAYS_COUNT / RTGICache::GRP_SIZE_X),
                                      PROBE_VOLUME_RES_X * PROBE_VOLUME_RES_Z, PROBE_VOLUME_RES_Y};

    DispatchCompute(fg.cmd_buf(), pi, grp_count, bindings, &uniform_params, sizeof(uniform_params), fg.descr_alloc(),
                    fg.log());
}

#if defined(REN_GL_BACKEND)
void Eng::ExRTGICache::Execute_HWRT(FgContext &fg) { assert(false && "Not implemented!"); }
#endif