#include "RpRTGICache.h"

#include <Ren/Context.h>
#include <Ren/DrawCall.h>
#include <Ren/VKCtx.h>

#include "../../utils/ShaderLoader.h"
#include "../shaders/rt_gi_cache_interface.h"

void Eng::RpRTGICache::Execute_HWRT(RpBuilder &builder) {
    Ren::Context &ctx = builder.ctx();
    Ren::ApiContext *api_ctx = ctx.api_ctx();

    RpAllocBuf &geo_data_buf = builder.GetReadBuffer(pass_data_->geo_data);
    RpAllocBuf &materials_buf = builder.GetReadBuffer(pass_data_->materials);
    RpAllocBuf &vtx_buf1 = builder.GetReadBuffer(pass_data_->vtx_buf1);
    RpAllocBuf &vtx_buf2 = builder.GetReadBuffer(pass_data_->vtx_buf2);
    RpAllocBuf &ndx_buf = builder.GetReadBuffer(pass_data_->ndx_buf);
    RpAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(pass_data_->shared_data);
    RpAllocTex &env_tex = builder.GetReadTexture(pass_data_->env_tex);
    RpAllocBuf &tlas_buf = builder.GetReadBuffer(pass_data_->tlas_buf);
    RpAllocBuf &lights_buf = builder.GetReadBuffer(pass_data_->lights_buf);
    RpAllocTex &shadowmap_tex = builder.GetReadTexture(pass_data_->shadowmap_tex);
    RpAllocTex &ltc_luts_tex = builder.GetReadTexture(pass_data_->ltc_luts_tex);
    RpAllocBuf &cells_buf = builder.GetReadBuffer(pass_data_->cells_buf);
    RpAllocBuf &items_buf = builder.GetReadBuffer(pass_data_->items_buf);
    RpAllocTex &irradiance_tex = builder.GetReadTexture(pass_data_->irradiance_tex);
    RpAllocTex &distance_tex = builder.GetReadTexture(pass_data_->distance_tex);
    RpAllocTex &offset_tex = builder.GetReadTexture(pass_data_->offset_tex);

    RpAllocBuf *random_seq_buf = nullptr, *stoch_lights_buf = nullptr;
    if (pass_data_->stoch_lights_buf) {
        random_seq_buf = &builder.GetReadBuffer(pass_data_->random_seq);
        stoch_lights_buf = &builder.GetReadBuffer(pass_data_->stoch_lights_buf);

        if (!random_seq_buf->tbos[0] || random_seq_buf->tbos[0]->params().size != random_seq_buf->ref->size()) {
            random_seq_buf->tbos[0] = builder.ctx().CreateTexture1D(
                "Random Seq Buf TBO", random_seq_buf->ref, Ren::eTexFormat::RawR32UI, 0, random_seq_buf->ref->size());
        }
    }

    RpAllocTex &out_ray_data_tex = builder.GetWriteTexture(pass_data_->out_ray_data_tex);

    auto *acc_struct = static_cast<Ren::AccStructureVK *>(pass_data_->tlas);

    VkCommandBuffer cmd_buf = api_ctx->draw_cmd_buf[api_ctx->backend_frame];

    Ren::SmallVector<Ren::Binding, 16> bindings = {
        {Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, *unif_sh_data_buf.ref},
        {Ren::eBindTarget::Tex2DSampled, RTGICache::ENV_TEX_SLOT, *env_tex.ref},
        {Ren::eBindTarget::AccStruct, RTGICache::TLAS_SLOT, *acc_struct},
        {Ren::eBindTarget::SBufRO, RTGICache::GEO_DATA_BUF_SLOT, *geo_data_buf.ref},
        {Ren::eBindTarget::SBufRO, RTGICache::MATERIAL_BUF_SLOT, *materials_buf.ref},
        {Ren::eBindTarget::SBufRO, RTGICache::VTX_BUF1_SLOT, *vtx_buf1.ref},
        {Ren::eBindTarget::SBufRO, RTGICache::VTX_BUF2_SLOT, *vtx_buf2.ref},
        {Ren::eBindTarget::SBufRO, RTGICache::NDX_BUF_SLOT, *ndx_buf.ref},
        {Ren::eBindTarget::SBufRO, RTGICache::LIGHTS_BUF_SLOT, *lights_buf.ref},
        {Ren::eBindTarget::Tex2DSampled, RTGICache::SHADOW_TEX_SLOT, *shadowmap_tex.ref},
        {Ren::eBindTarget::Tex2DSampled, RTGICache::LTC_LUTS_TEX_SLOT, *ltc_luts_tex.ref},
        {Ren::eBindTarget::UTBuf, RTGICache::CELLS_BUF_SLOT, *cells_buf.tbos[0]},
        {Ren::eBindTarget::UTBuf, RTGICache::ITEMS_BUF_SLOT, *items_buf.tbos[0]},
        {Ren::eBindTarget::Tex2DArraySampled, RTGICache::IRRADIANCE_TEX_SLOT, *irradiance_tex.arr},
        {Ren::eBindTarget::Tex2DArraySampled, RTGICache::DISTANCE_TEX_SLOT, *distance_tex.arr},
        {Ren::eBindTarget::Tex2DArraySampled, RTGICache::OFFSET_TEX_SLOT, *offset_tex.arr},
        {Ren::eBindTarget::Image2DArray, RTGICache::OUT_RAY_DATA_IMG_SLOT, *out_ray_data_tex.arr}};
    if (stoch_lights_buf) {
        bindings.emplace_back(Ren::eBindTarget::UTBuf, RTGICache::RANDOM_SEQ_BUF_SLOT, *random_seq_buf->tbos[0]);
        bindings.emplace_back(Ren::eBindTarget::SBufRO, RTGICache::STOCH_LIGHTS_BUF_SLOT, *stoch_lights_buf->ref);
    }

    VkDescriptorSet descr_sets[2];
    descr_sets[0] = Ren::PrepareDescriptorSet(
        api_ctx, pi_rt_gi_cache_hwrt_[stoch_lights_buf != nullptr].prog()->descr_set_layouts()[0], bindings,
        ctx.default_descr_alloc(), ctx.log());
    descr_sets[1] = bindless_tex_->rt_inline_textures_descr_set;

    api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE,
                               pi_rt_gi_cache_hwrt_[stoch_lights_buf != nullptr].handle());
    api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE,
                                     pi_rt_gi_cache_hwrt_[stoch_lights_buf != nullptr].layout(), 0, 2, descr_sets, 0,
                                     nullptr);

    RTGICache::Params uniform_params = {};
    uniform_params.volume_index = view_state_->volume_to_update;
    uniform_params.stoch_lights_count = view_state_->stochastic_lights_count_cache;
    uniform_params.grid_origin = Ren::Vec4f(pass_data_->probe_volumes[view_state_->volume_to_update].origin[0],
                                            pass_data_->probe_volumes[view_state_->volume_to_update].origin[1],
                                            pass_data_->probe_volumes[view_state_->volume_to_update].origin[2], 0.0f);
    uniform_params.grid_scroll = Ren::Vec4i(pass_data_->probe_volumes[view_state_->volume_to_update].scroll[0],
                                            pass_data_->probe_volumes[view_state_->volume_to_update].scroll[1],
                                            pass_data_->probe_volumes[view_state_->volume_to_update].scroll[2], 0.0f);
    uniform_params.grid_scroll_diff =
        Ren::Vec4i(pass_data_->probe_volumes[view_state_->volume_to_update].scroll_diff[0],
                   pass_data_->probe_volumes[view_state_->volume_to_update].scroll_diff[1],
                   pass_data_->probe_volumes[view_state_->volume_to_update].scroll_diff[2], 0.0f);
    uniform_params.grid_spacing = Ren::Vec4f(pass_data_->probe_volumes[view_state_->volume_to_update].spacing[0],
                                             pass_data_->probe_volumes[view_state_->volume_to_update].spacing[1],
                                             pass_data_->probe_volumes[view_state_->volume_to_update].spacing[2], 0.0f);
    uniform_params.quat_rot = view_state_->probe_ray_rotator;

    api_ctx->vkCmdPushConstants(cmd_buf, pi_rt_gi_cache_hwrt_[stoch_lights_buf != nullptr].layout(),
                                VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uniform_params), &uniform_params);

    api_ctx->vkCmdDispatch(cmd_buf, (PROBE_TOTAL_RAYS_COUNT / RTGICache::LOCAL_GROUP_SIZE_X),
                           PROBE_VOLUME_RES * PROBE_VOLUME_RES, PROBE_VOLUME_RES);
}

void Eng::RpRTGICache::Execute_SWRT(RpBuilder &builder) {
    Ren::Context &ctx = builder.ctx();
    Ren::ApiContext *api_ctx = ctx.api_ctx();

    RpAllocBuf &geo_data_buf = builder.GetReadBuffer(pass_data_->geo_data);
    RpAllocBuf &materials_buf = builder.GetReadBuffer(pass_data_->materials);
    RpAllocBuf &vtx_buf1 = builder.GetReadBuffer(pass_data_->vtx_buf1);
    RpAllocBuf &vtx_buf2 = builder.GetReadBuffer(pass_data_->vtx_buf2);
    RpAllocBuf &ndx_buf = builder.GetReadBuffer(pass_data_->ndx_buf);
    RpAllocBuf &rt_blas_buf = builder.GetReadBuffer(pass_data_->swrt.rt_blas_buf);
    RpAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(pass_data_->shared_data);
    RpAllocTex &env_tex = builder.GetReadTexture(pass_data_->env_tex);
    RpAllocBuf &rt_tlas_buf = builder.GetReadBuffer(pass_data_->tlas_buf);
    RpAllocBuf &prim_ndx_buf = builder.GetReadBuffer(pass_data_->swrt.prim_ndx_buf);
    RpAllocBuf &meshes_buf = builder.GetReadBuffer(pass_data_->swrt.meshes_buf);
    RpAllocBuf &mesh_instances_buf = builder.GetReadBuffer(pass_data_->swrt.mesh_instances_buf);
    RpAllocBuf &lights_buf = builder.GetReadBuffer(pass_data_->lights_buf);
    RpAllocTex &shadowmap_tex = builder.GetReadTexture(pass_data_->shadowmap_tex);
    RpAllocTex &ltc_luts_tex = builder.GetReadTexture(pass_data_->ltc_luts_tex);
    RpAllocBuf &cells_buf = builder.GetReadBuffer(pass_data_->cells_buf);
    RpAllocBuf &items_buf = builder.GetReadBuffer(pass_data_->items_buf);
    RpAllocTex &irradiance_tex = builder.GetReadTexture(pass_data_->irradiance_tex);
    RpAllocTex &distance_tex = builder.GetReadTexture(pass_data_->distance_tex);
    RpAllocTex &offset_tex = builder.GetReadTexture(pass_data_->offset_tex);

    RpAllocBuf *random_seq_buf = nullptr, *stoch_lights_buf = nullptr;
    if (pass_data_->stoch_lights_buf) {
        random_seq_buf = &builder.GetReadBuffer(pass_data_->random_seq);
        stoch_lights_buf = &builder.GetReadBuffer(pass_data_->stoch_lights_buf);

        if (!random_seq_buf->tbos[0] || random_seq_buf->tbos[0]->params().size != random_seq_buf->ref->size()) {
            random_seq_buf->tbos[0] = builder.ctx().CreateTexture1D(
                "Random Seq Buf TBO", random_seq_buf->ref, Ren::eTexFormat::RawR32UI, 0, random_seq_buf->ref->size());
        }
    }

    RpAllocTex &out_ray_data_tex = builder.GetWriteTexture(pass_data_->out_ray_data_tex);

    if (!vtx_buf1.tbos[0] || vtx_buf1.tbos[0]->params().size != vtx_buf1.ref->size()) {
        vtx_buf1.tbos[0] =
            ctx.CreateTexture1D("Vertex Buf 1 TBO", vtx_buf1.ref, Ren::eTexFormat::RawRGBA32F, 0, vtx_buf1.ref->size());
    }

    if (!vtx_buf2.tbos[0] || vtx_buf2.tbos[0]->params().size != vtx_buf2.ref->size()) {
        vtx_buf2.tbos[0] = ctx.CreateTexture1D("Vertex Buf 2 TBO", vtx_buf2.ref, Ren::eTexFormat::RawRGBA32UI, 0,
                                               vtx_buf2.ref->size());
    }

    if (!ndx_buf.tbos[0] || ndx_buf.tbos[0]->params().size != ndx_buf.ref->size()) {
        ndx_buf.tbos[0] =
            ctx.CreateTexture1D("Index Buf TBO", ndx_buf.ref, Ren::eTexFormat::RawR32UI, 0, ndx_buf.ref->size());
    }

    if (!prim_ndx_buf.tbos[0] || prim_ndx_buf.tbos[0]->params().size != prim_ndx_buf.ref->size()) {
        prim_ndx_buf.tbos[0] = ctx.CreateTexture1D("Prim Ndx TBO", prim_ndx_buf.ref, Ren::eTexFormat::RawR32UI, 0,
                                                   prim_ndx_buf.ref->size());
    }

    if (!rt_blas_buf.tbos[0] || rt_blas_buf.tbos[0]->params().size != rt_blas_buf.ref->size()) {
        rt_blas_buf.tbos[0] = ctx.CreateTexture1D("RT BLAS TBO", rt_blas_buf.ref, Ren::eTexFormat::RawRGBA32F, 0,
                                                  rt_blas_buf.ref->size());
    }

    if (!rt_tlas_buf.tbos[0] || rt_tlas_buf.tbos[0]->params().size != rt_tlas_buf.ref->size()) {
        rt_tlas_buf.tbos[0] = ctx.CreateTexture1D("RT TLAS TBO", rt_tlas_buf.ref, Ren::eTexFormat::RawRGBA32F, 0,
                                                  rt_tlas_buf.ref->size());
    }

    if (!mesh_instances_buf.tbos[0] || mesh_instances_buf.tbos[0]->params().size != mesh_instances_buf.ref->size()) {
        mesh_instances_buf.tbos[0] =
            ctx.CreateTexture1D("Mesh Instances TBO", mesh_instances_buf.ref, Ren::eTexFormat::RawRGBA32F, 0,
                                mesh_instances_buf.ref->size());
    }

    if (!meshes_buf.tbos[0] || meshes_buf.tbos[0]->params().size != meshes_buf.ref->size()) {
        meshes_buf.tbos[0] =
            ctx.CreateTexture1D("Meshes TBO", meshes_buf.ref, Ren::eTexFormat::RawRG32UI, 0, meshes_buf.ref->size());
    }

    VkCommandBuffer cmd_buf = api_ctx->draw_cmd_buf[api_ctx->backend_frame];

    Ren::SmallVector<Ren::Binding, 16> bindings = {
        {Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, *unif_sh_data_buf.ref},
        {Ren::eBindTarget::Tex2DSampled, RTGICache::ENV_TEX_SLOT, *env_tex.ref},
        {Ren::eBindTarget::UTBuf, RTGICache::BLAS_BUF_SLOT, *rt_blas_buf.tbos[0]},
        {Ren::eBindTarget::UTBuf, RTGICache::TLAS_BUF_SLOT, *rt_tlas_buf.tbos[0]},
        {Ren::eBindTarget::UTBuf, RTGICache::PRIM_NDX_BUF_SLOT, *prim_ndx_buf.tbos[0]},
        {Ren::eBindTarget::UTBuf, RTGICache::MESHES_BUF_SLOT, *meshes_buf.tbos[0]},
        {Ren::eBindTarget::UTBuf, RTGICache::MESH_INSTANCES_BUF_SLOT, *mesh_instances_buf.tbos[0]},
        {Ren::eBindTarget::SBufRO, RTGICache::GEO_DATA_BUF_SLOT, *geo_data_buf.ref},
        {Ren::eBindTarget::SBufRO, RTGICache::MATERIAL_BUF_SLOT, *materials_buf.ref},
        {Ren::eBindTarget::UTBuf, RTGICache::VTX_BUF1_SLOT, *vtx_buf1.tbos[0]},
        {Ren::eBindTarget::UTBuf, RTGICache::VTX_BUF2_SLOT, *vtx_buf2.tbos[0]},
        {Ren::eBindTarget::UTBuf, RTGICache::NDX_BUF_SLOT, *ndx_buf.tbos[0]},
        {Ren::eBindTarget::SBufRO, RTGICache::LIGHTS_BUF_SLOT, *lights_buf.ref},
        {Ren::eBindTarget::Tex2DSampled, RTGICache::SHADOW_TEX_SLOT, *shadowmap_tex.ref},
        {Ren::eBindTarget::Tex2DSampled, RTGICache::LTC_LUTS_TEX_SLOT, *ltc_luts_tex.ref},
        {Ren::eBindTarget::UTBuf, RTGICache::CELLS_BUF_SLOT, *cells_buf.tbos[0]},
        {Ren::eBindTarget::UTBuf, RTGICache::ITEMS_BUF_SLOT, *items_buf.tbos[0]},
        {Ren::eBindTarget::Tex2DArraySampled, RTGICache::IRRADIANCE_TEX_SLOT, *irradiance_tex.arr},
        {Ren::eBindTarget::Tex2DArraySampled, RTGICache::DISTANCE_TEX_SLOT, *distance_tex.arr},
        {Ren::eBindTarget::Tex2DArraySampled, RTGICache::OFFSET_TEX_SLOT, *offset_tex.arr},
        {Ren::eBindTarget::Image2DArray, RTGICache::OUT_RAY_DATA_IMG_SLOT, *out_ray_data_tex.arr}};
    if (stoch_lights_buf) {
        bindings.emplace_back(Ren::eBindTarget::UTBuf, RTGICache::RANDOM_SEQ_BUF_SLOT, *random_seq_buf->tbos[0]);
        bindings.emplace_back(Ren::eBindTarget::SBufRO, RTGICache::STOCH_LIGHTS_BUF_SLOT, *stoch_lights_buf->ref);
    }

    VkDescriptorSet descr_sets[2];
    descr_sets[0] = Ren::PrepareDescriptorSet(
        api_ctx, pi_rt_gi_cache_swrt_[stoch_lights_buf != nullptr].prog()->descr_set_layouts()[0], bindings,
        ctx.default_descr_alloc(), ctx.log());
    descr_sets[1] = bindless_tex_->rt_inline_textures_descr_set;

    api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE,
                               pi_rt_gi_cache_swrt_[stoch_lights_buf != nullptr].handle());
    api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE,
                                     pi_rt_gi_cache_swrt_[stoch_lights_buf != nullptr].layout(), 0, 2, descr_sets, 0,
                                     nullptr);

    RTGICache::Params uniform_params = {};
    uniform_params.volume_index = view_state_->volume_to_update;
    uniform_params.stoch_lights_count = view_state_->stochastic_lights_count_cache;
    uniform_params.grid_origin = Ren::Vec4f(pass_data_->probe_volumes[view_state_->volume_to_update].origin[0],
                                            pass_data_->probe_volumes[view_state_->volume_to_update].origin[1],
                                            pass_data_->probe_volumes[view_state_->volume_to_update].origin[2], 0.0f);
    uniform_params.grid_scroll = Ren::Vec4i(pass_data_->probe_volumes[view_state_->volume_to_update].scroll[0],
                                            pass_data_->probe_volumes[view_state_->volume_to_update].scroll[1],
                                            pass_data_->probe_volumes[view_state_->volume_to_update].scroll[2], 0.0f);
    uniform_params.grid_scroll_diff =
        Ren::Vec4i(pass_data_->probe_volumes[view_state_->volume_to_update].scroll_diff[0],
                   pass_data_->probe_volumes[view_state_->volume_to_update].scroll_diff[1],
                   pass_data_->probe_volumes[view_state_->volume_to_update].scroll_diff[2], 0.0f);
    uniform_params.grid_spacing = Ren::Vec4f(pass_data_->probe_volumes[view_state_->volume_to_update].spacing[0],
                                             pass_data_->probe_volumes[view_state_->volume_to_update].spacing[1],
                                             pass_data_->probe_volumes[view_state_->volume_to_update].spacing[2], 0.0f);
    uniform_params.quat_rot = view_state_->probe_ray_rotator;

    api_ctx->vkCmdPushConstants(cmd_buf, pi_rt_gi_cache_swrt_[stoch_lights_buf != nullptr].layout(),
                                VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uniform_params), &uniform_params);

    api_ctx->vkCmdDispatch(cmd_buf, (PROBE_TOTAL_RAYS_COUNT / RTGICache::LOCAL_GROUP_SIZE_X),
                           PROBE_VOLUME_RES * PROBE_VOLUME_RES, PROBE_VOLUME_RES);
}

void Eng::RpRTGICache::LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh) {
    if (!initialized) {
        if (ctx.capabilities.hwrt) {
            Ren::ProgramRef rt_gi_cache_inline_prog = sh.LoadProgram(ctx, "internal/rt_gi_cache_hwrt.comp.glsl");
            assert(rt_gi_cache_inline_prog->ready());

            if (!pi_rt_gi_cache_hwrt_[0].Init(ctx.api_ctx(), std::move(rt_gi_cache_inline_prog), ctx.log())) {
                ctx.log()->Error("RpRTGICache: Failed to initialize pipeline!");
            }

            rt_gi_cache_inline_prog = sh.LoadProgram(ctx, "internal/rt_gi_cache_hwrt.comp.glsl@STOCH_LIGHTS");
            assert(rt_gi_cache_inline_prog->ready());

            if (!pi_rt_gi_cache_hwrt_[1].Init(ctx.api_ctx(), std::move(rt_gi_cache_inline_prog), ctx.log())) {
                ctx.log()->Error("RpRTGICache: Failed to initialize pipeline!");
            }
        }

        Ren::ProgramRef rt_gi_cache_swrt_prog = sh.LoadProgram(ctx, "internal/rt_gi_cache_swrt.comp.glsl");
        assert(rt_gi_cache_swrt_prog->ready());

        if (!pi_rt_gi_cache_swrt_[0].Init(ctx.api_ctx(), std::move(rt_gi_cache_swrt_prog), ctx.log())) {
            ctx.log()->Error("RpRTGICache: Failed to initialize pipeline!");
        }

        rt_gi_cache_swrt_prog = sh.LoadProgram(ctx, "internal/rt_gi_cache_swrt.comp.glsl@STOCH_LIGHTS");
        assert(rt_gi_cache_swrt_prog->ready());

        if (!pi_rt_gi_cache_swrt_[1].Init(ctx.api_ctx(), std::move(rt_gi_cache_swrt_prog), ctx.log())) {
            ctx.log()->Error("RpRTGICache: Failed to initialize pipeline!");
        }

        initialized = true;
    }
}