#include "ExRTGICache.h"

#include <Ren/Context.h>
#include <Ren/DrawCall.h>

#include "../../utils/ShaderLoader.h"
#include "../shaders/rt_gi_cache_interface.h"

void Eng::ExRTGICache::Execute_HWRT(FgBuilder &builder) { assert(false && "Not implemented!"); }

void Eng::ExRTGICache::Execute_SWRT(FgBuilder &builder) {
    FgAllocBuf &geo_data_buf = builder.GetReadBuffer(args_->geo_data);
    FgAllocBuf &materials_buf = builder.GetReadBuffer(args_->materials);
    FgAllocBuf &vtx_buf1 = builder.GetReadBuffer(args_->vtx_buf1);
    FgAllocBuf &ndx_buf = builder.GetReadBuffer(args_->ndx_buf);
    FgAllocBuf &rt_blas_buf = builder.GetReadBuffer(args_->swrt.rt_blas_buf);
    FgAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(args_->shared_data);
    FgAllocTex &env_tex = builder.GetReadTexture(args_->env_tex);
    FgAllocBuf &rt_tlas_buf = builder.GetReadBuffer(args_->tlas_buf);
    FgAllocBuf &prim_ndx_buf = builder.GetReadBuffer(args_->swrt.prim_ndx_buf);
    FgAllocBuf &mesh_instances_buf = builder.GetReadBuffer(args_->swrt.mesh_instances_buf);
    FgAllocBuf &textures_buf = builder.GetReadBuffer(args_->swrt.textures_buf);
    FgAllocBuf &lights_buf = builder.GetReadBuffer(args_->lights_buf);
    FgAllocTex &shadow_depth_tex = builder.GetReadTexture(args_->shadow_depth_tex);
    FgAllocTex &shadow_color_tex = builder.GetReadTexture(args_->shadow_color_tex);
    FgAllocTex &ltc_luts_tex = builder.GetReadTexture(args_->ltc_luts_tex);
    FgAllocBuf &cells_buf = builder.GetReadBuffer(args_->cells_buf);
    FgAllocBuf &items_buf = builder.GetReadBuffer(args_->items_buf);
    FgAllocTex &irr_tex = builder.GetReadTexture(args_->irradiance_tex);
    FgAllocTex &dist_tex = builder.GetReadTexture(args_->distance_tex);
    FgAllocTex &off_tex = builder.GetReadTexture(args_->offset_tex);

    FgAllocBuf *random_seq_buf = nullptr, *stoch_lights_buf = nullptr, *light_nodes_buf = nullptr;
    if (args_->stoch_lights_buf) {
        random_seq_buf = &builder.GetReadBuffer(args_->random_seq);
        stoch_lights_buf = &builder.GetReadBuffer(args_->stoch_lights_buf);
        light_nodes_buf = &builder.GetReadBuffer(args_->light_nodes_buf);
    }

    FgAllocTex &out_gi_tex = builder.GetWriteTexture(args_->out_ray_data_tex);

    Ren::Context &ctx = builder.ctx();
    Ren::ApiContext *api_ctx = ctx.api_ctx();

    Ren::SmallVector<Ren::Binding, 16> bindings = {
        {Ren::eBindTarget::SBufRO, BIND_BINDLESS_TEX, *textures_buf.ref},
        {Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, *unif_sh_data_buf.ref},
        {Ren::eBindTarget::TexSampled, RTGICache::ENV_TEX_SLOT, *env_tex.ref},
        {Ren::eBindTarget::UTBuf, RTGICache::BLAS_BUF_SLOT, *rt_blas_buf.ref},
        {Ren::eBindTarget::UTBuf, RTGICache::TLAS_BUF_SLOT, *rt_tlas_buf.ref},
        {Ren::eBindTarget::UTBuf, RTGICache::PRIM_NDX_BUF_SLOT, *prim_ndx_buf.ref},
        {Ren::eBindTarget::UTBuf, RTGICache::MESH_INSTANCES_BUF_SLOT, *mesh_instances_buf.ref},
        {Ren::eBindTarget::SBufRO, RTGICache::GEO_DATA_BUF_SLOT, *geo_data_buf.ref},
        {Ren::eBindTarget::SBufRO, RTGICache::MATERIAL_BUF_SLOT, *materials_buf.ref},
        {Ren::eBindTarget::UTBuf, RTGICache::VTX_BUF1_SLOT, *vtx_buf1.ref},
        {Ren::eBindTarget::UTBuf, RTGICache::NDX_BUF_SLOT, *ndx_buf.ref},
        {Ren::eBindTarget::SBufRO, RTGICache::LIGHTS_BUF_SLOT, *lights_buf.ref},
        {Ren::eBindTarget::TexSampled, RTGICache::SHADOW_DEPTH_TEX_SLOT, *shadow_depth_tex.ref},
        {Ren::eBindTarget::TexSampled, RTGICache::SHADOW_COLOR_TEX_SLOT, *shadow_color_tex.ref},
        {Ren::eBindTarget::TexSampled, RTGICache::LTC_LUTS_TEX_SLOT, *ltc_luts_tex.ref},
        {Ren::eBindTarget::UTBuf, RTGICache::CELLS_BUF_SLOT, *cells_buf.ref},
        {Ren::eBindTarget::UTBuf, RTGICache::ITEMS_BUF_SLOT, *items_buf.ref},
        {Ren::eBindTarget::TexSampled, RTGICache::IRRADIANCE_TEX_SLOT, *irr_tex.ref},
        {Ren::eBindTarget::TexSampled, RTGICache::DISTANCE_TEX_SLOT, *dist_tex.ref},
        {Ren::eBindTarget::TexSampled, RTGICache::OFFSET_TEX_SLOT, *off_tex.ref},
        {Ren::eBindTarget::ImageRW, RTGICache::OUT_RAY_DATA_IMG_SLOT, *out_gi_tex.ref}};
    if (stoch_lights_buf) {
        bindings.emplace_back(Ren::eBindTarget::UTBuf, RTGICache::RANDOM_SEQ_BUF_SLOT, *random_seq_buf->ref);
        bindings.emplace_back(Ren::eBindTarget::UTBuf, RTGICache::STOCH_LIGHTS_BUF_SLOT, *stoch_lights_buf->ref);
        bindings.emplace_back(Ren::eBindTarget::UTBuf, RTGICache::LIGHT_NODES_BUF_SLOT, *light_nodes_buf->ref);
    }

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

    DispatchCompute(*pi_rt_gi_cache_[stoch_lights_buf != nullptr][args_->partial_update],
                    Ren::Vec3u{(PROBE_TOTAL_RAYS_COUNT / RTGICache::LOCAL_GROUP_SIZE_X),
                               PROBE_VOLUME_RES_X * PROBE_VOLUME_RES_Z, PROBE_VOLUME_RES_Y},
                    bindings, &uniform_params, sizeof(uniform_params), ctx.default_descr_alloc(), ctx.log());
}
