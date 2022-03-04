#include "RpRTReflections.h"

#include <Ren/Context.h>
#include <Ren/RastState.h>
#include <Ren/Texture.h>

#include "../../Utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"

#include "../assets/shaders/internal/rt_reflections_interface.glsl"

void RpRTReflections::ExecuteRTPipeline(RpBuilder &builder) {
    LazyInit(builder.ctx(), builder.sh());

    RpAllocBuf &sobol_buf = builder.GetReadBuffer(sobol_buf_);
    RpAllocBuf &scrambling_tile_buf = builder.GetReadBuffer(scrambling_tile_buf_);
    RpAllocBuf &ranking_tile_buf = builder.GetReadBuffer(ranking_tile_buf_);
    RpAllocBuf &geo_data_buf = builder.GetReadBuffer(geo_data_buf_);
    RpAllocBuf &materials_buf = builder.GetReadBuffer(materials_buf_);
    RpAllocBuf &vtx_buf1 = builder.GetReadBuffer(vtx_buf1_);
    RpAllocBuf &vtx_buf2 = builder.GetReadBuffer(vtx_buf2_);
    RpAllocBuf &ndx_buf = builder.GetReadBuffer(ndx_buf_);
    RpAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(shared_data_buf_);
    RpAllocTex &depth_tex = builder.GetReadTexture(depth_tex_);
    RpAllocTex &normal_tex = builder.GetReadTexture(normal_tex_);
    RpAllocTex &env_tex = builder.GetReadTexture(env_tex_);
    RpAllocBuf &ray_counter_buf = builder.GetReadBuffer(ray_counter_buf_);
    RpAllocBuf &ray_list_buf = builder.GetReadBuffer(ray_list_buf_);
    RpAllocBuf &indir_args_buf = builder.GetReadBuffer(indir_args_buf_);
    RpAllocTex &dummy_black = builder.GetReadTexture(dummy_black_);
    RpAllocTex *lm_tex[5];
    for (int i = 0; i < 5; ++i) {
        if (lm_tex_[i]) {
            lm_tex[i] = &builder.GetReadTexture(lm_tex_[i]);
        } else {
            lm_tex[i] = &dummy_black;
        }
    }

    RpAllocTex &out_refl_tex = builder.GetWriteTexture(out_refl_tex_);
    RpAllocTex &out_raylen_tex = builder.GetWriteTexture(out_raylen_tex_);

    Ren::Context &ctx = builder.ctx();
    Ren::ApiContext *api_ctx = ctx.api_ctx();

    Ren::_SubmitCurrentCommandsWaitForCompletionAndResume(api_ctx);

    // Initialize texel buffers if needed
    if (!sobol_buf.tbos[0]) {
        sobol_buf.tbos[0] =
            ctx.CreateTexture1D("SobolSequenceTex", sobol_buf.ref, Ren::eTexFormat::RawR32UI, 0, sobol_buf.ref->size());
    }
    if (!scrambling_tile_buf.tbos[0]) {
        scrambling_tile_buf.tbos[0] =
            ctx.CreateTexture1D("ScramblingTile32SppTex", scrambling_tile_buf.ref, Ren::eTexFormat::RawR32UI, 0,
                                scrambling_tile_buf.ref->size());
    }
    if (!ranking_tile_buf.tbos[0]) {
        ranking_tile_buf.tbos[0] = ctx.CreateTexture1D("RankingTile32SppTex", ranking_tile_buf.ref,
                                                       Ren::eTexFormat::RawR32UI, 0, ranking_tile_buf.ref->size());
    }

    auto *acc_struct = static_cast<Ren::AccStructureVK *>(acc_struct_data_->rt_tlas);

    VkCommandBuffer cmd_buf = api_ctx->draw_cmd_buf[api_ctx->backend_frame];

    const Ren::Binding bindings[] = {
        {Ren::eBindTarget::UBuf, REN_UB_SHARED_DATA_LOC, *unif_sh_data_buf.ref},
        {Ren::eBindTarget::Tex2D, RTReflections::DEPTH_TEX_SLOT, *depth_tex.ref},
        {Ren::eBindTarget::Tex2D, RTReflections::NORM_TEX_SLOT, *normal_tex.ref},
        {Ren::eBindTarget::TBuf, RTReflections::SOBOL_BUF_SLOT, *sobol_buf.tbos[0]},
        {Ren::eBindTarget::TBuf, RTReflections::SCRAMLING_TILE_BUF_SLOT, *scrambling_tile_buf.tbos[0]},
        {Ren::eBindTarget::TBuf, RTReflections::RANKING_TILE_BUF_SLOT, *ranking_tile_buf.tbos[0]},
        //{Ren::eBindTarget::SBuf, RTReflections::RAY_COUNTER_SLOT, *ray_counter_buf.ref},
        {Ren::eBindTarget::SBuf, RTReflections::RAY_LIST_SLOT, *ray_list_buf.ref},
        {Ren::eBindTarget::Tex2D, RTReflections::ENV_TEX_SLOT, *env_tex.ref},
        {Ren::eBindTarget::AccStruct, RTReflections::TLAS_SLOT, *acc_struct},
        {Ren::eBindTarget::SBuf, RTReflections::GEO_DATA_BUF_SLOT, *geo_data_buf.ref},
        {Ren::eBindTarget::SBuf, RTReflections::MATERIAL_BUF_SLOT, *materials_buf.ref},
        {Ren::eBindTarget::SBuf, RTReflections::VTX_BUF1_SLOT, *vtx_buf1.ref},
        {Ren::eBindTarget::SBuf, RTReflections::VTX_BUF2_SLOT, *vtx_buf2.ref},
        {Ren::eBindTarget::SBuf, RTReflections::NDX_BUF_SLOT, *ndx_buf.ref},
        {Ren::eBindTarget::Tex2D, RTReflections::LMAP_TEX_SLOTS, 0, *lm_tex[0]->ref},
        {Ren::eBindTarget::Tex2D, RTReflections::LMAP_TEX_SLOTS, 1, *lm_tex[1]->ref},
        {Ren::eBindTarget::Tex2D, RTReflections::LMAP_TEX_SLOTS, 2, *lm_tex[2]->ref},
        {Ren::eBindTarget::Tex2D, RTReflections::LMAP_TEX_SLOTS, 3, *lm_tex[3]->ref},
        {Ren::eBindTarget::Tex2D, RTReflections::LMAP_TEX_SLOTS, 4, *lm_tex[4]->ref},
        {Ren::eBindTarget::Image, RTReflections::OUT_REFL_IMG_SLOT, *out_refl_tex.ref},
        {Ren::eBindTarget::Image, RTReflections::OUT_RAYLEN_IMG_SLOT, *out_raylen_tex.ref}};

    VkDescriptorSet descr_sets[2];
    descr_sets[0] = Ren::PrepareDescriptorSet(api_ctx, pi_rt_reflections_.prog()->descr_set_layouts()[0], bindings,
                                              COUNT_OF(bindings), ctx.default_descr_alloc(), ctx.log());
    descr_sets[1] = bindless_tex_->rt_textures_descr_set;

    vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pi_rt_reflections_.handle());
    vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pi_rt_reflections_.layout(), 0, 2,
                            descr_sets, 0, nullptr);

    RTReflections::Params uniform_params;
    uniform_params.img_size = Ren::Vec2u{uint32_t(view_state_->act_res[0]), uint32_t(view_state_->act_res[1])};
    uniform_params.pixel_spread_angle = std::atan(
        2.0f * std::tan(0.5f * view_state_->vertical_fov * Ren::Pi<float>() / 180.0f) / float(view_state_->scr_res[1]));

    vkCmdPushConstants(cmd_buf, pi_rt_reflections_.layout(),
                       VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, 0, sizeof(uniform_params),
                       &uniform_params);

    vkCmdTraceRaysIndirectKHR(cmd_buf, pi_rt_reflections_.rgen_table(), pi_rt_reflections_.miss_table(),
                              pi_rt_reflections_.hit_table(), pi_rt_reflections_.call_table(),
                              indir_args_buf.ref->vk_device_address());
}

void RpRTReflections::ExecuteRTInline(RpBuilder &builder) {
    LazyInit(builder.ctx(), builder.sh());

    RpAllocBuf &sobol_buf = builder.GetReadBuffer(sobol_buf_);
    RpAllocBuf &scrambling_tile_buf = builder.GetReadBuffer(scrambling_tile_buf_);
    RpAllocBuf &ranking_tile_buf = builder.GetReadBuffer(ranking_tile_buf_);
    RpAllocBuf &geo_data_buf = builder.GetReadBuffer(geo_data_buf_);
    RpAllocBuf &materials_buf = builder.GetReadBuffer(materials_buf_);
    RpAllocBuf &vtx_buf1 = builder.GetReadBuffer(vtx_buf1_);
    RpAllocBuf &vtx_buf2 = builder.GetReadBuffer(vtx_buf2_);
    RpAllocBuf &ndx_buf = builder.GetReadBuffer(ndx_buf_);
    RpAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(shared_data_buf_);
    RpAllocTex &depth_tex = builder.GetReadTexture(depth_tex_);
    RpAllocTex &normal_tex = builder.GetReadTexture(normal_tex_);
    RpAllocTex &env_tex = builder.GetReadTexture(env_tex_);
    RpAllocBuf &ray_counter_buf = builder.GetReadBuffer(ray_counter_buf_);
    RpAllocBuf &ray_list_buf = builder.GetReadBuffer(ray_list_buf_);
    RpAllocBuf &indir_args_buf = builder.GetReadBuffer(indir_args_buf_);
    RpAllocTex &dummy_black = builder.GetReadTexture(dummy_black_);
    RpAllocTex *lm_tex[5];
    for (int i = 0; i < 5; ++i) {
        if (lm_tex_[i]) {
            lm_tex[i] = &builder.GetReadTexture(lm_tex_[i]);
        } else {
            lm_tex[i] = &dummy_black;
        }
    }

    RpAllocTex &out_refl_tex = builder.GetWriteTexture(out_refl_tex_);
    RpAllocTex &out_raylen_tex = builder.GetWriteTexture(out_raylen_tex_);

    Ren::Context &ctx = builder.ctx();
    Ren::ApiContext *api_ctx = ctx.api_ctx();

    Ren::_SubmitCurrentCommandsWaitForCompletionAndResume(api_ctx);

    // Initialize texel buffers if needed
    if (!sobol_buf.tbos[0]) {
        sobol_buf.tbos[0] =
            ctx.CreateTexture1D("SobolSequenceTex", sobol_buf.ref, Ren::eTexFormat::RawR32UI, 0, sobol_buf.ref->size());
    }
    if (!scrambling_tile_buf.tbos[0]) {
        scrambling_tile_buf.tbos[0] =
            ctx.CreateTexture1D("ScramblingTile32SppTex", scrambling_tile_buf.ref, Ren::eTexFormat::RawR32UI, 0,
                                scrambling_tile_buf.ref->size());
    }
    if (!ranking_tile_buf.tbos[0]) {
        ranking_tile_buf.tbos[0] = ctx.CreateTexture1D("RankingTile32SppTex", ranking_tile_buf.ref,
                                                       Ren::eTexFormat::RawR32UI, 0, ranking_tile_buf.ref->size());
    }

    auto *acc_struct = static_cast<Ren::AccStructureVK *>(acc_struct_data_->rt_tlas);

    VkCommandBuffer cmd_buf = api_ctx->draw_cmd_buf[api_ctx->backend_frame];

    const Ren::Binding bindings[] = {
        {Ren::eBindTarget::UBuf, REN_UB_SHARED_DATA_LOC, *unif_sh_data_buf.ref},
        {Ren::eBindTarget::Tex2D, RTReflections::DEPTH_TEX_SLOT, *depth_tex.ref},
        {Ren::eBindTarget::Tex2D, RTReflections::NORM_TEX_SLOT, *normal_tex.ref},
        {Ren::eBindTarget::TBuf, RTReflections::SOBOL_BUF_SLOT, *sobol_buf.tbos[0]},
        {Ren::eBindTarget::TBuf, RTReflections::SCRAMLING_TILE_BUF_SLOT, *scrambling_tile_buf.tbos[0]},
        {Ren::eBindTarget::TBuf, RTReflections::RANKING_TILE_BUF_SLOT, *ranking_tile_buf.tbos[0]},
        {Ren::eBindTarget::SBuf, RTReflections::RAY_COUNTER_SLOT, *ray_counter_buf.ref},
        {Ren::eBindTarget::SBuf, RTReflections::RAY_LIST_SLOT, *ray_list_buf.ref},
        {Ren::eBindTarget::Tex2D, RTReflections::ENV_TEX_SLOT, *env_tex.ref},
        {Ren::eBindTarget::AccStruct, RTReflections::TLAS_SLOT, *acc_struct},
        {Ren::eBindTarget::SBuf, RTReflections::GEO_DATA_BUF_SLOT, *geo_data_buf.ref},
        {Ren::eBindTarget::SBuf, RTReflections::MATERIAL_BUF_SLOT, *materials_buf.ref},
        {Ren::eBindTarget::SBuf, RTReflections::VTX_BUF1_SLOT, *vtx_buf1.ref},
        {Ren::eBindTarget::SBuf, RTReflections::VTX_BUF2_SLOT, *vtx_buf2.ref},
        {Ren::eBindTarget::SBuf, RTReflections::NDX_BUF_SLOT, *ndx_buf.ref},
        {Ren::eBindTarget::Tex2D, RTReflections::LMAP_TEX_SLOTS, 0, *lm_tex[0]->ref},
        {Ren::eBindTarget::Tex2D, RTReflections::LMAP_TEX_SLOTS, 1, *lm_tex[1]->ref},
        {Ren::eBindTarget::Tex2D, RTReflections::LMAP_TEX_SLOTS, 2, *lm_tex[2]->ref},
        {Ren::eBindTarget::Tex2D, RTReflections::LMAP_TEX_SLOTS, 3, *lm_tex[3]->ref},
        {Ren::eBindTarget::Tex2D, RTReflections::LMAP_TEX_SLOTS, 4, *lm_tex[4]->ref},
        {Ren::eBindTarget::Image, RTReflections::OUT_REFL_IMG_SLOT, *out_refl_tex.ref},
        {Ren::eBindTarget::Image, RTReflections::OUT_RAYLEN_IMG_SLOT, *out_raylen_tex.ref}};

    VkDescriptorSet descr_sets[2];
    descr_sets[0] = Ren::PrepareDescriptorSet(api_ctx, pi_rt_reflections_inline_.prog()->descr_set_layouts()[0],
                                              bindings, COUNT_OF(bindings), ctx.default_descr_alloc(), ctx.log());
    descr_sets[1] = bindless_tex_->rt_inline_textures_descr_set;

    vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, pi_rt_reflections_inline_.handle());
    vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, pi_rt_reflections_inline_.layout(), 0, 2,
                            descr_sets, 0, nullptr);

    RTReflections::Params uniform_params;
    uniform_params.img_size = Ren::Vec2u{uint32_t(view_state_->act_res[0]), uint32_t(view_state_->act_res[1])};
    uniform_params.pixel_spread_angle = std::atan(
        2.0f * std::tan(0.5f * view_state_->vertical_fov * Ren::Pi<float>() / 180.0f) / float(view_state_->scr_res[1]));

    vkCmdPushConstants(cmd_buf, pi_rt_reflections_inline_.layout(), VK_SHADER_STAGE_COMPUTE_BIT, 0,
                       sizeof(uniform_params), &uniform_params);

    vkCmdDispatchIndirect(cmd_buf, indir_args_buf.ref->vk_handle(),
                          VkDeviceSize(sizeof(VkTraceRaysIndirectCommandKHR)));
}

void RpRTReflections::LazyInit(Ren::Context &ctx, ShaderLoader &sh) {
    if (!initialized) {
        Ren::ProgramRef rt_reflections_prog = sh.LoadProgram(
            ctx, "rt_reflections", "internal/rt_reflections.rgen.glsl", "internal/rt_reflections.rchit.glsl",
            "internal/rt_reflections.rahit.glsl", "internal/rt_reflections.rmiss.glsl", nullptr);
        assert(rt_reflections_prog->ready());

        if (!pi_rt_reflections_.Init(ctx.api_ctx(), std::move(rt_reflections_prog), ctx.log())) {
            ctx.log()->Error("RpRTReflections: Failed to initialize pipeline!");
        }

        if (ctx.capabilities.ray_query) {
            Ren::ProgramRef rt_reflections_inline_prog =
                sh.LoadProgram(ctx, "rt_reflections_inline", "internal/rt_reflections_spirv14.comp.glsl");
            assert(rt_reflections_inline_prog->ready());

            if (!pi_rt_reflections_inline_.Init(ctx.api_ctx(), std::move(rt_reflections_inline_prog), ctx.log())) {
                ctx.log()->Error("RpRTReflections: Failed to initialize pipeline!");
            }
        }

        initialized = true;
    }
}