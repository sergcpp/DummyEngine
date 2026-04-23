#include "ExRTGICache.h"

#include <Ren/Context.h>
#include <Ren/DrawCall.h>

#include "../../utils/ShaderLoader.h"
#include "../Renderer_DrawList.h"
#include "../framegraph/FgBuilder.h"
#include "../shaders/rt_gi_cache_interface.h"

void Eng::ExRTGICache::Execute(const FgContext &fg) {
    LazyInit(fg.ren_ctx(), fg.sh());
    if (fg.ren_ctx().capabilities.hwrt) {
        Execute_HWRT(fg);
    } else {
        Execute_SWRT(fg);
    }
}

void Eng::ExRTGICache::LazyInit(Ren::Context &ctx, ShaderLoader &sh) {
    if (!initialized_) {
        auto hwrt_select = [&ctx](std::string_view hwrt_shader, std::string_view swrt_shader) {
            return ctx.capabilities.hwrt ? hwrt_shader : swrt_shader;
        };
        auto subgroup_select = [&ctx](std::string_view subgroup_shader, std::string_view nosubgroup_shader) {
            return ctx.capabilities.subgroup ? subgroup_shader : nosubgroup_shader;
        };

        pi_rt_gi_cache_[0][0] = sh.FindOrCreatePipeline(hwrt_select(
            subgroup_select("internal/rt_gi_cache_hwrt.comp.glsl", "internal/rt_gi_cache_hwrt@NO_SUBGROUP.comp.glsl"),
            subgroup_select("internal/rt_gi_cache_swrt.comp.glsl", "internal/rt_gi_cache_swrt@NO_SUBGROUP.comp.glsl")));
        if (!pi_rt_gi_cache_[0][0]) {
            ctx.log()->Error("ExRTGICache: Failed to initialize pipeline!");
        }

        pi_rt_gi_cache_[0][1] = sh.FindOrCreatePipeline(
            hwrt_select(subgroup_select("internal/rt_gi_cache_hwrt@PARTIAL.comp.glsl",
                                        "internal/rt_gi_cache_hwrt@PARTIAL;NO_SUBGROUP.comp.glsl"),
                        subgroup_select("internal/rt_gi_cache_swrt@PARTIAL.comp.glsl",
                                        "internal/rt_gi_cache_swrt@PARTIAL;NO_SUBGROUP.comp.glsl")));
        if (!pi_rt_gi_cache_[0][1]) {
            ctx.log()->Error("ExRTGICache: Failed to initialize pipeline!");
        }

        pi_rt_gi_cache_[1][0] = sh.FindOrCreatePipeline(
            hwrt_select(subgroup_select("internal/rt_gi_cache_hwrt@STOCH_LIGHTS.comp.glsl",
                                        "internal/rt_gi_cache_hwrt@STOCH_LIGHTS;NO_SUBGROUP.comp.glsl"),
                        subgroup_select("internal/rt_gi_cache_swrt@STOCH_LIGHTS.comp.glsl",
                                        "internal/rt_gi_cache_swrt@STOCH_LIGHTS;NO_SUBGROUP.comp.glsl")));
        if (!pi_rt_gi_cache_[1][0]) {
            ctx.log()->Error("ExRTGICache: Failed to initialize pipeline!");
        }

        pi_rt_gi_cache_[1][1] = sh.FindOrCreatePipeline(
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

void Eng::ExRTGICache::Execute_SWRT(const FgContext &fg) {
    const Ren::BufferROHandle geo_data = fg.AccessROBuffer(args_->geo_data);
    const Ren::BufferROHandle materials = fg.AccessROBuffer(args_->materials);
    const Ren::BufferROHandle vtx_buf1 = fg.AccessROBuffer(args_->vtx_buf1);
    const Ren::BufferROHandle ndx_buf = fg.AccessROBuffer(args_->ndx_buf);
    const Ren::BufferROHandle rt_blas = fg.AccessROBuffer(args_->swrt.rt_blas);
    const Ren::BufferROHandle unif_sh_data = fg.AccessROBuffer(args_->shared_data);
    const Ren::ImageROHandle env = fg.AccessROImage(args_->env);
    const Ren::BufferROHandle rt_tlas = fg.AccessROBuffer(args_->tlas_buf);
    const Ren::BufferROHandle prim_ndx = fg.AccessROBuffer(args_->swrt.prim_ndx);
    const Ren::BufferROHandle mesh_instances = fg.AccessROBuffer(args_->swrt.mesh_instances);
    const Ren::BufferROHandle lights = fg.AccessROBuffer(args_->lights);
    const Ren::ImageROHandle shadow_depth = fg.AccessROImage(args_->shadow_depth);
    const Ren::ImageROHandle shadow_color = fg.AccessROImage(args_->shadow_color);
    const Ren::ImageROHandle ltc_luts = fg.AccessROImage(args_->ltc_luts);
    const Ren::BufferROHandle cells = fg.AccessROBuffer(args_->cells);
    const Ren::BufferROHandle items = fg.AccessROBuffer(args_->items);
    const Ren::ImageROHandle irr = fg.AccessROImage(args_->irradiance);
    const Ren::ImageROHandle dist = fg.AccessROImage(args_->distance);
    const Ren::ImageROHandle off = fg.AccessROImage(args_->offset);

    Ren::BufferROHandle random_seq = {}, stoch_lights = {}, light_nodes = {};
    if (args_->stoch_lights) {
        random_seq = fg.AccessROBuffer(args_->random_seq);
        stoch_lights = fg.AccessROBuffer(args_->stoch_lights);
        light_nodes = fg.AccessROBuffer(args_->light_nodes);
    }

    const Ren::ImageRWHandle out_ray_data = fg.AccessRWImage(args_->out_ray_data);

    Ren::SmallVector<Ren::Binding, 16> bindings = {
        {Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, unif_sh_data},
        {Ren::eBindTarget::BindlessDescriptors, BIND_BINDLESS_TEX, bindless_tex_->rt_inline_textures},
        {Ren::eBindTarget::TexSampled, RTGICache::ENV_TEX_SLOT, env},
        {Ren::eBindTarget::UTBuf, RTGICache::BLAS_BUF_SLOT, rt_blas},
        {Ren::eBindTarget::UTBuf, RTGICache::TLAS_BUF_SLOT, rt_tlas},
        {Ren::eBindTarget::UTBuf, RTGICache::PRIM_NDX_BUF_SLOT, prim_ndx},
        {Ren::eBindTarget::UTBuf, RTGICache::MESH_INSTANCES_BUF_SLOT, mesh_instances},
        {Ren::eBindTarget::SBufRO, RTGICache::GEO_DATA_BUF_SLOT, geo_data},
        {Ren::eBindTarget::SBufRO, RTGICache::MATERIAL_BUF_SLOT, materials},
        {Ren::eBindTarget::UTBuf, RTGICache::VTX_BUF1_SLOT, vtx_buf1},
        {Ren::eBindTarget::UTBuf, RTGICache::NDX_BUF_SLOT, ndx_buf},
        {Ren::eBindTarget::SBufRO, RTGICache::LIGHTS_BUF_SLOT, lights},
        {Ren::eBindTarget::TexSampled, RTGICache::SHADOW_DEPTH_TEX_SLOT, shadow_depth},
        {Ren::eBindTarget::TexSampled, RTGICache::SHADOW_COLOR_TEX_SLOT, shadow_color},
        {Ren::eBindTarget::TexSampled, RTGICache::LTC_LUTS_TEX_SLOT, ltc_luts},
        {Ren::eBindTarget::UTBuf, RTGICache::CELLS_BUF_SLOT, cells},
        {Ren::eBindTarget::UTBuf, RTGICache::ITEMS_BUF_SLOT, items},
        {Ren::eBindTarget::TexSampled, RTGICache::IRRADIANCE_TEX_SLOT, irr},
        {Ren::eBindTarget::TexSampled, RTGICache::DISTANCE_TEX_SLOT, dist},
        {Ren::eBindTarget::TexSampled, RTGICache::OFFSET_TEX_SLOT, off},
        {Ren::eBindTarget::ImageRW, RTGICache::OUT_RAY_DATA_IMG_SLOT, out_ray_data}};
    if (stoch_lights) {
        bindings.emplace_back(Ren::eBindTarget::UTBuf, RTGICache::RANDOM_SEQ_BUF_SLOT, random_seq);
        bindings.emplace_back(Ren::eBindTarget::UTBuf, RTGICache::STOCH_LIGHTS_BUF_SLOT, stoch_lights);
        bindings.emplace_back(Ren::eBindTarget::UTBuf, RTGICache::LIGHT_NODES_BUF_SLOT, light_nodes);
    }

    const Ren::PipelineHandle pi = pi_rt_gi_cache_[bool(stoch_lights)][args_->partial_update];

    RTGICache::Params uniform_params = {};
    uniform_params.volume_index = view_state_->volume_to_update;
    uniform_params.stoch_lights_count = view_state_->stochastic_lights_count_cache;
    uniform_params.pass_hash = view_state_->probe_ray_hash;
    uniform_params.oct_index = (args_->probe_volumes[view_state_->volume_to_update].updates_count - 1) % 8;
    uniform_params.grid_origin = Ren::Vec4f{args_->probe_volumes[view_state_->volume_to_update].origin, 0.0f};
    uniform_params.grid_scroll = Ren::Vec4i{args_->probe_volumes[view_state_->volume_to_update].scroll, 0};
    uniform_params.grid_scroll_diff = Ren::Vec4i{args_->probe_volumes[view_state_->volume_to_update].scroll_diff, 0};
    uniform_params.grid_spacing = Ren::Vec4f{args_->probe_volumes[view_state_->volume_to_update].spacing, 0.0f};
    uniform_params.quat_rot = view_state_->probe_ray_rotator;

    const auto grp_count = Ren::Vec3u{(PROBE_TOTAL_RAYS_COUNT / RTGICache::GRP_SIZE_X),
                                      PROBE_VOLUME_RES_X * PROBE_VOLUME_RES_Z, PROBE_VOLUME_RES_Y};

    DispatchCompute(fg.cmd_buf(), pi, fg.storages(), grp_count, bindings, &uniform_params, sizeof(uniform_params),
                    fg.descr_alloc(), fg.log());
}

#if defined(REN_GL_BACKEND)
void Eng::ExRTGICache::Execute_HWRT(const FgContext &fg) { assert(false && "Not implemented!"); }
#endif