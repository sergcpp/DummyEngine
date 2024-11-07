#include "ExOITScheduleRays.h"

#include <Ren/Context.h>
#include <Ren/DebugMarker.h>
#include <Ren/RastState.h>
#include <Ren/VKCtx.h>

#include "../PrimDraw.h"
#include "../shaders/oit_schedule_rays_interface.h"

namespace ExSharedInternal {
uint32_t _draw_range_ext(Ren::ApiContext *api_ctx, VkCommandBuffer cmd_buf, const Ren::Pipeline &pipeline,
                         Ren::Span<const uint32_t> batch_indices, Ren::Span<const Eng::BasicDrawBatch> batches,
                         uint32_t i, uint64_t mask, uint32_t materials_per_descriptor,
                         Ren::Span<const VkDescriptorSet> descr_sets, int *draws_count);
}

void Eng::ExOITScheduleRays::DrawTransparent(FgBuilder &builder, FgAllocTex &depth_tex) {
    using namespace ExSharedInternal;

    auto &ctx = builder.ctx();
    auto *api_ctx = ctx.api_ctx();

    FgAllocTex &noise_tex = builder.GetReadTexture(noise_tex_);
    FgAllocBuf &instances_buf = builder.GetReadBuffer(instances_buf_);
    FgAllocBuf &instance_indices_buf = builder.GetReadBuffer(instance_indices_buf_);
    FgAllocBuf &unif_shared_data_buf = builder.GetReadBuffer(shared_data_buf_);
    FgAllocBuf &materials_buf = builder.GetReadBuffer(materials_buf_);
    FgAllocBuf &oit_depth_buf = builder.GetReadBuffer(oit_depth_buf_);
    FgAllocBuf &ray_counter_buf = builder.GetWriteBuffer(ray_counter_);
    FgAllocBuf &ray_list_buf = builder.GetWriteBuffer(ray_list_);

    if ((*p_list_)->alpha_blend_start_index == -1) {
        return;
    }

    const Ren::Binding bindings[] = {
        {Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, *unif_shared_data_buf.ref},
        {Ren::eBindTarget::UTBuf, BIND_INST_BUF, *instances_buf.tbos[0]},
        {Ren::eBindTarget::UTBuf, OITScheduleRays::OIT_DEPTH_BUF_SLOT, *oit_depth_buf.tbos[0]},
        {Ren::eBindTarget::SBufRO, BIND_INST_NDX_BUF, *instance_indices_buf.ref},
        {Ren::eBindTarget::SBufRO, BIND_MATERIALS_BUF, *materials_buf.ref},
        {Ren::eBindTarget::SBufRO, OITScheduleRays::RAY_COUNTER_SLOT, *ray_counter_buf.ref},
        {Ren::eBindTarget::SBufRO, OITScheduleRays::RAY_LIST_SLOT, *ray_list_buf.ref},
        {Ren::eBindTarget::Tex2DSampled, BIND_NOISE_TEX, *noise_tex.ref}};

    VkDescriptorSet descr_sets[2];
    descr_sets[0] = PrepareDescriptorSet(api_ctx, pi_vegetation_[0].prog()->descr_set_layouts()[0], bindings,
                                              ctx.default_descr_alloc(), ctx.log());
    descr_sets[1] = bindless_tex_->textures_descr_sets[0];

    using BDB = BasicDrawBatch;

    const uint32_t materials_per_descriptor = api_ctx->max_combined_image_samplers / MAX_TEX_PER_MATERIAL;

    VkCommandBuffer cmd_buf = api_ctx->draw_cmd_buf[api_ctx->backend_frame];

    const VkViewport viewport = {0.0f, 0.0f, float(view_state_->act_res[0]), float(view_state_->act_res[1]),
                                 0.0f, 1.0f};
    api_ctx->vkCmdSetViewport(cmd_buf, 0, 1, &viewport);
    const VkRect2D scissor = {{0, 0}, {uint32_t(view_state_->act_res[0]), uint32_t(view_state_->act_res[1])}};
    api_ctx->vkCmdSetScissor(cmd_buf, 0, 1, &scissor);

    const Ren::Span<const BasicDrawBatch> batches = {(*p_list_)->basic_batches};
    const Ren::Span<const uint32_t> batch_indices = {(*p_list_)->basic_batch_indices};

    int draws_count = 0;
    uint32_t i = (*p_list_)->alpha_blend_start_index;

    { // solid meshes
        VkRenderPassBeginInfo rp_begin_info = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        rp_begin_info.renderPass = rp_oit_schedule_rays_.handle();
        rp_begin_info.framebuffer = main_draw_fb_[api_ctx->backend_frame][fb_to_use_].handle();
        rp_begin_info.renderArea = {{0, 0}, {uint32_t(view_state_->act_res[0]), uint32_t(view_state_->act_res[1])}};
        const VkClearValue clear_values[4] = {{}, {}, {}, {}};
        rp_begin_info.pClearValues = clear_values;
        rp_begin_info.clearValueCount = 4;
        api_ctx->vkCmdBeginRenderPass(cmd_buf, &rp_begin_info, VK_SUBPASS_CONTENTS_INLINE);

        { // Simple meshes
            Ren::DebugMarker _m(api_ctx, cmd_buf, "SIMPLE");

            vi_simple_.BindBuffers(api_ctx, cmd_buf, 0, VK_INDEX_TYPE_UINT32);

            { // solid one-sided
                Ren::DebugMarker _mm(api_ctx, cmd_buf, "SOLID-ONE-SIDED");
                api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple_[0].handle());
                api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple_[0].layout(), 0, 2,
                                                 descr_sets, 0, nullptr);
                i = _draw_range_ext(api_ctx, cmd_buf, pi_simple_[0], batch_indices, batches, i, BDB::BitAlphaBlend,
                                    materials_per_descriptor, bindless_tex_->textures_descr_sets, &draws_count);
                api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple_[1].handle());
                i = _draw_range_ext(api_ctx, cmd_buf, pi_simple_[1], batch_indices, batches, i,
                                    BDB::BitAlphaBlend | BDB::BitBackSided, materials_per_descriptor,
                                    bindless_tex_->textures_descr_sets, &draws_count);
            }

            { // solid two-sided
                Ren::DebugMarker _mm(api_ctx, cmd_buf, "SOLID-TWO-SIDED");
                api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple_[2].handle());
                api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple_[2].layout(), 0, 2,
                                                 descr_sets, 0, nullptr);
                i = _draw_range_ext(api_ctx, cmd_buf, pi_simple_[2], batch_indices, batches, i,
                                    BDB::BitAlphaBlend | BDB::BitTwoSided, materials_per_descriptor,
                                    bindless_tex_->textures_descr_sets, &draws_count);
            }

            { // moving solid one-sided
                Ren::DebugMarker _mm(api_ctx, cmd_buf, "MOVING-SOLID-ONE-SIDED");
                api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple_[0].handle());
                api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple_[0].layout(), 0, 2,
                                                 descr_sets, 0, nullptr);
                i = _draw_range_ext(api_ctx, cmd_buf, pi_simple_[0], batch_indices, batches, i,
                                    BDB::BitAlphaBlend | BDB::BitMoving, materials_per_descriptor,
                                    bindless_tex_->textures_descr_sets, &draws_count);
            }

            { // moving solid two-sided
                Ren::DebugMarker _mm(api_ctx, cmd_buf, "MOVING-SOLID-TWO-SIDED");
                api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple_[2].handle());
                api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple_[2].layout(), 0, 2,
                                                 descr_sets, 0, nullptr);
                i = _draw_range_ext(api_ctx, cmd_buf, pi_simple_[2], batch_indices, batches, i,
                                    BDB::BitAlphaBlend | BDB::BitMoving | BDB::BitTwoSided, materials_per_descriptor,
                                    bindless_tex_->textures_descr_sets, &draws_count);
            }

            { // alpha-tested one-sided
                Ren::DebugMarker _mm(api_ctx, cmd_buf, "ALPHA-ONE-SIDED");
                api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple_[0].handle());
                api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple_[0].layout(), 0, 2,
                                                 descr_sets, 0, nullptr);
                i = _draw_range_ext(api_ctx, cmd_buf, pi_simple_[0], batch_indices, batches, i,
                                    BDB::BitAlphaBlend | BDB::BitAlphaTest, materials_per_descriptor,
                                    bindless_tex_->textures_descr_sets, &draws_count);
                api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple_[1].handle());
                i = _draw_range_ext(api_ctx, cmd_buf, pi_simple_[1], batch_indices, batches, i,
                                    BDB::BitAlphaBlend | BDB::BitAlphaTest | BDB::BitBackSided,
                                    materials_per_descriptor, bindless_tex_->textures_descr_sets, &draws_count);
            }

            { // alpha-tested two-sided
                Ren::DebugMarker _mm(api_ctx, cmd_buf, "ALPHA-TWO-SIDED");
                api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple_[2].handle());
                api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple_[2].layout(), 0, 2,
                                                 descr_sets, 0, nullptr);
                i = _draw_range_ext(api_ctx, cmd_buf, pi_simple_[2], batch_indices, batches, i,
                                    BDB::BitAlphaBlend | BDB::BitAlphaTest | BDB::BitTwoSided, materials_per_descriptor,
                                    bindless_tex_->textures_descr_sets, &draws_count);
            }

            { // moving alpha-tested one-sided
                Ren::DebugMarker _mm(api_ctx, cmd_buf, "MOVING-ALPHA-ONE-SIDED");
                api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple_[0].handle());
                api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple_[0].layout(), 0, 2,
                                                 descr_sets, 0, nullptr);
                i = _draw_range_ext(api_ctx, cmd_buf, pi_simple_[0], batch_indices, batches, i,
                                    BDB::BitAlphaBlend | BDB::BitMoving | BDB::BitAlphaTest, materials_per_descriptor,
                                    bindless_tex_->textures_descr_sets, &draws_count);
                api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple_[1].handle());
                i = _draw_range_ext(api_ctx, cmd_buf, pi_simple_[1], batch_indices, batches, i,
                                    BDB::BitAlphaBlend | BDB::BitMoving | BDB::BitAlphaTest | BDB::BitBackSided,
                                    materials_per_descriptor, bindless_tex_->textures_descr_sets, &draws_count);
            }

            { // moving alpha-tested two-sided
                Ren::DebugMarker _mm(api_ctx, cmd_buf, "MOVING-ALPHA-TWO-SIDED");
                api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple_[2].handle());
                api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_simple_[2].layout(), 0, 2,
                                                 descr_sets, 0, nullptr);
                i = _draw_range_ext(api_ctx, cmd_buf, pi_simple_[2], batch_indices, batches, i,
                                    BDB::BitAlphaBlend | BDB::BitMoving | BDB::BitAlphaTest | BDB::BitTwoSided,
                                    materials_per_descriptor, bindless_tex_->textures_descr_sets, &draws_count);
            }
        }

        api_ctx->vkCmdEndRenderPass(cmd_buf);
    }
}
