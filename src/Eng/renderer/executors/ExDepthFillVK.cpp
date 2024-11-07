#include "ExDepthFill.h"

#include <Ren/Context.h>
#include <Ren/DebugMarker.h>
#include <Ren/DrawCall.h>
#include <Ren/Span.h>
#include <Ren/VKCtx.h>

namespace ExSharedInternal {
uint32_t _draw_range(Ren::ApiContext *api_ctx, VkCommandBuffer cmd_buf, Ren::Span<const uint32_t> batch_indices,
                     Ren::Span<const Eng::BasicDrawBatch> batches, uint32_t i, const uint64_t mask, int *draws_count) {
    for (; i < batch_indices.size(); i++) {
        const auto &batch = batches[batch_indices[i]];
        if ((batch.sort_key & Eng::BasicDrawBatch::FlagBits) != mask) {
            break;
        }

        if (!batch.instance_count) {
            continue;
        }

        api_ctx->vkCmdDrawIndexed(cmd_buf, batch.indices_count, // index count
                                  batch.instance_count,         // instance count
                                  batch.indices_offset,         // first index
                                  batch.base_vertex,            // vertex offset
                                  batch.instance_start);        // first instance
        ++(*draws_count);
    }
    return i;
}

uint32_t _draw_range_ext(Ren::ApiContext *api_ctx, VkCommandBuffer cmd_buf, const Ren::Pipeline &pipeline,
                         Ren::Span<const uint32_t> batch_indices, Ren::Span<const Eng::BasicDrawBatch> batches,
                         uint32_t i, const uint64_t mask, const uint32_t materials_per_descriptor,
                         Ren::Span<const VkDescriptorSet> descr_sets, int *draws_count) {
    uint32_t bound_descr_id = 0;
    for (; i < batch_indices.size(); i++) {
        const auto &batch = batches[batch_indices[i]];
        if ((batch.sort_key & Eng::BasicDrawBatch::FlagBits) != mask) {
            break;
        }

        if (!batch.instance_count) {
            continue;
        }

        const uint32_t descr_id = batch.material_index / materials_per_descriptor;
        if (descr_id != bound_descr_id) {
            api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.layout(), 1, 1,
                                             &descr_sets[descr_id], 0, nullptr);
            bound_descr_id = descr_id;
        }

        api_ctx->vkCmdDrawIndexed(cmd_buf, batch.indices_count, // index count
                                  batch.instance_count,         // instance count
                                  batch.indices_offset,         // first index
                                  batch.base_vertex,            // vertex offset
                                  batch.instance_start);        // first instance
        ++(*draws_count);
    }
    return i;
}

uint32_t _skip_range(Ren::Span<const uint32_t> batch_indices, Ren::Span<const Eng::BasicDrawBatch> batches, uint32_t i,
                     uint64_t mask);
} // namespace ExSharedInternal

void Eng::ExDepthFill::DrawDepth(FgBuilder &builder, FgAllocBuf &vtx_buf1, FgAllocBuf &vtx_buf2, FgAllocBuf &ndx_buf) {
    using namespace ExSharedInternal;

    FgAllocBuf &unif_shared_data_buf = builder.GetReadBuffer(shared_data_buf_);
    FgAllocBuf &instances_buf = builder.GetReadBuffer(instances_buf_);
    FgAllocBuf &instance_indices_buf = builder.GetReadBuffer(instance_indices_buf_);
    FgAllocBuf &materials_buf = builder.GetReadBuffer(materials_buf_);
    FgAllocTex &noise_tex = builder.GetReadTexture(noise_tex_);

    Ren::Context &ctx = builder.ctx();
    Ren::ApiContext *api_ctx = ctx.api_ctx();

    VkCommandBuffer cmd_buf = api_ctx->draw_cmd_buf[api_ctx->backend_frame];
    //
    // Setup viewport
    //
    const VkViewport viewport = {0.0f, 0.0f, float(view_state_->act_res[0]), float(view_state_->act_res[1]),
                                 0.0f, 1.0f};
    api_ctx->vkCmdSetViewport(cmd_buf, 0, 1, &viewport);
    const VkRect2D scissor = {{0, 0}, {uint32_t(view_state_->act_res[0]), uint32_t(view_state_->act_res[1])}};
    api_ctx->vkCmdSetScissor(cmd_buf, 0, 1, &scissor);

    const uint32_t materials_per_descriptor = api_ctx->max_combined_image_samplers / MAX_TEX_PER_MATERIAL;

    BackendInfo _dummy = {};
    uint32_t i = 0;

    using BDB = BasicDrawBatch;

    //
    // Prepare descriptor sets
    //

    VkDescriptorSetLayout simple_descr_set_layout = pi_static_solid_[0].prog()->descr_set_layouts()[0];
    VkDescriptorSet simple_descr_sets[2];
    { // allocate descriptors
        const Ren::Binding bindings[] = {{Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, *unif_shared_data_buf.ref},
                                         {Ren::eBindTarget::UTBuf, BIND_INST_BUF, *instances_buf.tbos[0]},
                                         {Ren::eBindTarget::SBufRO, BIND_INST_NDX_BUF, *instance_indices_buf.ref},
                                         {Ren::eBindTarget::SBufRO, BIND_MATERIALS_BUF, *materials_buf.ref}};
        simple_descr_sets[0] =
            PrepareDescriptorSet(api_ctx, simple_descr_set_layout, bindings, ctx.default_descr_alloc(), ctx.log());
        simple_descr_sets[1] = bindless_tex_->textures_descr_sets[0];
    }

    VkDescriptorSetLayout vege_descr_set_layout = pi_vege_static_solid_vel_[0].prog()->descr_set_layouts()[0];
    VkDescriptorSet vege_descr_sets[2];
    { // allocate descriptors
        const Ren::Binding bindings[] = {{Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, *unif_shared_data_buf.ref},
                                         {Ren::eBindTarget::UTBuf, BIND_INST_BUF, *instances_buf.tbos[0]},
                                         {Ren::eBindTarget::SBufRO, BIND_INST_NDX_BUF, *instance_indices_buf.ref},
                                         {Ren::eBindTarget::SBufRO, BIND_MATERIALS_BUF, *materials_buf.ref},
                                         {Ren::eBindTarget::Tex2DSampled, BIND_NOISE_TEX, *noise_tex.ref}};
        vege_descr_sets[0] =
            PrepareDescriptorSet(api_ctx, vege_descr_set_layout, bindings, ctx.default_descr_alloc(), ctx.log());
        vege_descr_sets[1] = bindless_tex_->textures_descr_sets[0];
    }

    const Ren::Span<const BasicDrawBatch> zfill_batches = {(*p_list_)->basic_batches};
    const Ren::Span<const uint32_t> zfill_batch_indices = {(*p_list_)->basic_batch_indices};

    int draws_count = 0;

    { // solid meshes
        Ren::DebugMarker _m(api_ctx, cmd_buf, "STATIC-SOLID-SIMPLE");
        const int rp_index = (clear_depth_ && !draws_count) ? 0 : 1;

        VkClearValue clear_value = {};
        clear_value.depthStencil.depth = 0.0f;
        clear_value.depthStencil.stencil = 0;

        VkRenderPassBeginInfo rp_begin_info = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        rp_begin_info.renderPass = rp_depth_only_[rp_index].handle();
        rp_begin_info.framebuffer = depth_fill_fb_[ctx.backend_frame()][fb_to_use_].handle();
        rp_begin_info.renderArea = {{0, 0}, {uint32_t(view_state_->act_res[0]), uint32_t(view_state_->act_res[1])}};
        rp_begin_info.pClearValues = &clear_value;
        rp_begin_info.clearValueCount = 1;
        api_ctx->vkCmdBeginRenderPass(cmd_buf, &rp_begin_info, VK_SUBPASS_CONTENTS_INLINE);

        vi_solid_.BindBuffers(api_ctx, cmd_buf, 0, VK_INDEX_TYPE_UINT32);

        { // one-sided
            Ren::DebugMarker _mm(api_ctx, cmd_buf, "ONE-SIDED");
            api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_static_solid_[0].handle());
            api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_static_solid_[0].layout(), 0,
                                             1, simple_descr_sets, 0, nullptr);
            i = _draw_range(api_ctx, cmd_buf, zfill_batch_indices, zfill_batches, i, 0u, &draws_count);
            api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_static_solid_[1].handle());
            i = _draw_range(api_ctx, cmd_buf, zfill_batch_indices, zfill_batches, i, BDB::BitBackSided, &draws_count);
        }

        { // two-sided
            Ren::DebugMarker _mm(api_ctx, cmd_buf, "TWO-SIDED");
            api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_static_solid_[2].handle());
            api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_static_solid_[2].layout(), 0,
                                             1, simple_descr_sets, 0, nullptr);
            i = _draw_range(api_ctx, cmd_buf, zfill_batch_indices, zfill_batches, i, BDB::BitTwoSided, &draws_count);
        }

        api_ctx->vkCmdEndRenderPass(cmd_buf);
    }

    { // moving solid meshes (depth and velocity)
        Ren::DebugMarker _m(api_ctx, cmd_buf, "STATIC-SOLID-MOVING");
        const int rp_index = (clear_depth_ && !draws_count) ? 0 : 1;

        VkClearValue clear_value = {};
        clear_value.depthStencil.depth = 0.0f;
        clear_value.depthStencil.stencil = 0;

        VkRenderPassBeginInfo rp_begin_info = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        rp_begin_info.renderArea = {{0, 0}, {uint32_t(view_state_->act_res[0]), uint32_t(view_state_->act_res[1])}};
        rp_begin_info.pClearValues = &clear_value;
        rp_begin_info.clearValueCount = 1;

        Ren::Pipeline *pipeline_frontsided = nullptr, *pipeline_backsided = nullptr, *pipeline_twosided = nullptr;
        if ((*p_list_)->render_settings.taa_mode != eTAAMode::Off) {
            // Write depth and velocity
            rp_begin_info.renderPass = rp_depth_velocity_[rp_index].handle();
            rp_begin_info.framebuffer = depth_fill_vel_fb_[ctx.backend_frame()][fb_to_use_].handle();
            pipeline_frontsided = &pi_moving_solid_[0];
            pipeline_backsided = &pi_moving_solid_[1];
            pipeline_twosided = &pi_moving_solid_[2];
        } else {
            // Write depth only
            rp_begin_info.renderPass = rp_depth_only_[rp_index].handle();
            rp_begin_info.framebuffer = depth_fill_fb_[ctx.backend_frame()][fb_to_use_].handle();
            pipeline_frontsided = &pi_static_solid_[0];
            pipeline_backsided = &pi_static_solid_[1];
            pipeline_twosided = &pi_static_solid_[2];
        }
        api_ctx->vkCmdBeginRenderPass(cmd_buf, &rp_begin_info, VK_SUBPASS_CONTENTS_INLINE);

        vi_solid_.BindBuffers(api_ctx, cmd_buf, 0, VK_INDEX_TYPE_UINT32);

        { // one-sided
            Ren::DebugMarker _mm(api_ctx, ctx.current_cmd_buf(), "ONE-SIDED");
            api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_frontsided->handle());
            api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_frontsided->layout(), 0,
                                             1, simple_descr_sets, 0, nullptr);
            i = _draw_range(api_ctx, cmd_buf, zfill_batch_indices, zfill_batches, i, BDB::BitMoving, &draws_count);
            api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_backsided->handle());
            api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_backsided->layout(), 0,
                                             1, simple_descr_sets, 0, nullptr);
            i = _draw_range(api_ctx, cmd_buf, zfill_batch_indices, zfill_batches, i, BDB::BitMoving | BDB::BitBackSided,
                            &draws_count);
        }

        { // two-sided
            Ren::DebugMarker _mm(api_ctx, ctx.current_cmd_buf(), "TWO-SIDED");
            api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_twosided->handle());
            api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_twosided->layout(), 0,
                                             1, simple_descr_sets, 0, nullptr);
            i = _draw_range(api_ctx, cmd_buf, zfill_batch_indices, zfill_batches, i, BDB::BitMoving | BDB::BitTwoSided,
                            &draws_count);
        }

        api_ctx->vkCmdEndRenderPass(cmd_buf);
    }

    { // simple alpha-tested meshes (depth only)
        Ren::DebugMarker _m(api_ctx, ctx.current_cmd_buf(), "STATIC-ALPHA-SIMPLE");
        const int rp_index = (clear_depth_ && !draws_count) ? 0 : 1;

        VkClearValue clear_value = {};
        clear_value.depthStencil.depth = 0.0f;
        clear_value.depthStencil.stencil = 0;

        VkRenderPassBeginInfo rp_begin_info = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        rp_begin_info.renderPass = rp_depth_only_[rp_index].handle();
        rp_begin_info.framebuffer = depth_fill_fb_[ctx.backend_frame()][fb_to_use_].handle();
        rp_begin_info.renderArea = {{0, 0}, {uint32_t(view_state_->act_res[0]), uint32_t(view_state_->act_res[1])}};
        rp_begin_info.pClearValues = &clear_value;
        rp_begin_info.clearValueCount = 1;
        api_ctx->vkCmdBeginRenderPass(cmd_buf, &rp_begin_info, VK_SUBPASS_CONTENTS_INLINE);

        vi_transp_.BindBuffers(api_ctx, cmd_buf, 0, VK_INDEX_TYPE_UINT32);

        { // one-sided
            Ren::DebugMarker _mm(api_ctx, ctx.current_cmd_buf(), "ONE-SIDED");
            api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_static_transp_[0].handle());
            api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_static_transp_[0].layout(), 0,
                                             2, simple_descr_sets, 0, nullptr);
            i = _draw_range_ext(api_ctx, cmd_buf, pi_static_transp_[0], zfill_batch_indices, zfill_batches, i,
                                BDB::BitAlphaTest, materials_per_descriptor, bindless_tex_->textures_descr_sets,
                                &draws_count);
            api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_static_transp_[1].handle());
            i = _draw_range_ext(api_ctx, cmd_buf, pi_static_transp_[0], zfill_batch_indices, zfill_batches, i,
                                BDB::BitAlphaTest | BDB::BitBackSided, materials_per_descriptor,
                                bindless_tex_->textures_descr_sets, &draws_count);
        }

        { // two-sided
            Ren::DebugMarker _mm(api_ctx, ctx.current_cmd_buf(), "TWO-SIDED");
            api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_static_transp_[2].handle());
            api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_static_transp_[2].layout(), 0,
                                             2, simple_descr_sets, 0, nullptr);
            i = _draw_range_ext(api_ctx, cmd_buf, pi_static_transp_[2], zfill_batch_indices, zfill_batches, i,
                                BDB::BitAlphaTest | BDB::BitTwoSided, materials_per_descriptor,
                                bindless_tex_->textures_descr_sets, &draws_count);
        }

        api_ctx->vkCmdEndRenderPass(cmd_buf);
    }

    { // moving alpha-tested meshes (depth and velocity)
        Ren::DebugMarker _m(api_ctx, ctx.current_cmd_buf(), "STATIC-ALPHA-MOVING");
        const int rp_index = (clear_depth_ && !draws_count) ? 0 : 1;

        VkClearValue clear_value = {};
        clear_value.depthStencil.depth = 0.0f;
        clear_value.depthStencil.stencil = 0;

        VkRenderPassBeginInfo rp_begin_info = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        rp_begin_info.renderArea = {{0, 0}, {uint32_t(view_state_->act_res[0]), uint32_t(view_state_->act_res[1])}};
        rp_begin_info.pClearValues = &clear_value;
        rp_begin_info.clearValueCount = 1;

        Ren::Pipeline *pipeline_frontsided = nullptr, *pipeline_backsided = nullptr, *pipeline_twosided = nullptr;
        if ((*p_list_)->render_settings.taa_mode != eTAAMode::Off) {
            // Write depth and velocity
            rp_begin_info.renderPass = rp_depth_velocity_[rp_index].handle();
            rp_begin_info.framebuffer = depth_fill_vel_fb_[ctx.backend_frame()][fb_to_use_].handle();
            pipeline_frontsided = &pi_moving_transp_[0];
            pipeline_backsided = &pi_moving_transp_[1];
            pipeline_twosided = &pi_moving_transp_[2];
        } else {
            // Write depth only
            rp_begin_info.renderPass = rp_depth_only_[rp_index].handle();
            rp_begin_info.framebuffer = depth_fill_fb_[ctx.backend_frame()][fb_to_use_].handle();
            pipeline_frontsided = &pi_static_transp_[0];
            pipeline_backsided = &pi_static_transp_[1];
            pipeline_twosided = &pi_static_transp_[2];
        }
        api_ctx->vkCmdBeginRenderPass(cmd_buf, &rp_begin_info, VK_SUBPASS_CONTENTS_INLINE);

        vi_transp_.BindBuffers(api_ctx, cmd_buf, 0, VK_INDEX_TYPE_UINT32);

        { // one-sided
            Ren::DebugMarker _mm(api_ctx, ctx.current_cmd_buf(), "ONE-SIDED");
            api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_frontsided->handle());
            api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_frontsided->layout(), 0,
                                             2, simple_descr_sets, 0, nullptr);
            i = _draw_range_ext(api_ctx, cmd_buf, *pipeline_frontsided, zfill_batch_indices, zfill_batches, i,
                                BDB::BitAlphaTest | BDB::BitMoving, materials_per_descriptor,
                                bindless_tex_->textures_descr_sets, &draws_count);
            api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_backsided->handle());
            api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_backsided->layout(), 0,
                                             2, simple_descr_sets, 0, nullptr);
            i = _draw_range_ext(api_ctx, cmd_buf, *pipeline_backsided, zfill_batch_indices, zfill_batches, i,
                                BDB::BitAlphaTest | BDB::BitMoving | BDB::BitBackSided, materials_per_descriptor,
                                bindless_tex_->textures_descr_sets, &draws_count);
        }

        { // two-sided
            Ren::DebugMarker _mm(api_ctx, ctx.current_cmd_buf(), "TWO-SIDED");
            api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_twosided->handle());
            api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_twosided->layout(), 0,
                                             2, simple_descr_sets, 0, nullptr);
            i = _draw_range_ext(api_ctx, cmd_buf, *pipeline_twosided, zfill_batch_indices, zfill_batches, i,
                                BDB::BitAlphaTest | BDB::BitMoving | BDB::BitTwoSided, materials_per_descriptor,
                                bindless_tex_->textures_descr_sets, &draws_count);
        }

        api_ctx->vkCmdEndRenderPass(cmd_buf);
    }

    { // static solid vegetation
        Ren::DebugMarker _m(api_ctx, cmd_buf, "VEGE-SOLID-SIMPLE");
        const int rp_index = (clear_depth_ && !draws_count) ? 0 : 1;

        VkClearValue clear_value = {};
        clear_value.depthStencil.depth = 0.0f;
        clear_value.depthStencil.stencil = 0;

        VkRenderPassBeginInfo rp_begin_info = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        rp_begin_info.renderArea = {{0, 0}, {uint32_t(view_state_->act_res[0]), uint32_t(view_state_->act_res[1])}};
        rp_begin_info.pClearValues = &clear_value;
        rp_begin_info.clearValueCount = 1;

        Ren::Pipeline *pipeline_onesided = nullptr, *pipeline_twosided = nullptr;
        if ((*p_list_)->render_settings.taa_mode != eTAAMode::Off) {
            // Write depth and velocity
            rp_begin_info.renderPass = rp_depth_velocity_[rp_index].handle();
            rp_begin_info.framebuffer = depth_fill_vel_fb_[ctx.backend_frame()][fb_to_use_].handle();
            pipeline_onesided = &pi_vege_static_solid_vel_[0];
            pipeline_twosided = &pi_vege_static_solid_vel_[1];
        } else {
            // Write depth only
            rp_begin_info.renderPass = rp_depth_only_[rp_index].handle();
            rp_begin_info.framebuffer = depth_fill_fb_[ctx.backend_frame()][fb_to_use_].handle();
            pipeline_onesided = &pi_vege_static_solid_[0];
            pipeline_twosided = &pi_vege_static_solid_[1];
        }
        api_ctx->vkCmdBeginRenderPass(cmd_buf, &rp_begin_info, VK_SUBPASS_CONTENTS_INLINE);

        vi_vege_solid_.BindBuffers(api_ctx, cmd_buf, 0, VK_INDEX_TYPE_UINT32);

        { // one-sided
            Ren::DebugMarker _mm(api_ctx, cmd_buf, "ONE-SIDED");
            api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_onesided->handle());
            api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_onesided->layout(), 0,
                                             2, vege_descr_sets, 0, nullptr);
            i = _draw_range(api_ctx, cmd_buf, zfill_batch_indices, zfill_batches, i, BDB::BitsVege, &draws_count);
        }

        { // two-sided
            Ren::DebugMarker _mm(api_ctx, cmd_buf, "TWO-SIDED");
            api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_twosided->handle());
            api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_twosided->layout(), 0,
                                             2, vege_descr_sets, 0, nullptr);
            i = _draw_range(api_ctx, cmd_buf, zfill_batch_indices, zfill_batches, i, BDB::BitsVege | BDB::BitTwoSided,
                            &draws_count);
        }

        api_ctx->vkCmdEndRenderPass(cmd_buf);
    }

    { // moving solid vegetation (depth and velocity)
        Ren::DebugMarker _m(api_ctx, ctx.current_cmd_buf(), "VEGE-SOLID-MOVING");
        const int rp_index = (clear_depth_ && !draws_count) ? 0 : 1;

        VkClearValue clear_value = {};
        clear_value.depthStencil.depth = 0.0f;
        clear_value.depthStencil.stencil = 0;

        VkRenderPassBeginInfo rp_begin_info = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        rp_begin_info.renderArea = {{0, 0}, {uint32_t(view_state_->act_res[0]), uint32_t(view_state_->act_res[1])}};
        rp_begin_info.pClearValues = &clear_value;
        rp_begin_info.clearValueCount = 1;

        Ren::Pipeline *pipeline_onesided = nullptr, *pipeline_twosided = nullptr;
        if ((*p_list_)->render_settings.taa_mode != eTAAMode::Off) {
            // Write depth and velocity
            rp_begin_info.renderPass = rp_depth_velocity_[rp_index].handle();
            rp_begin_info.framebuffer = depth_fill_vel_fb_[ctx.backend_frame()][fb_to_use_].handle();
            pipeline_onesided = &pi_vege_moving_solid_vel_[0];
            pipeline_twosided = &pi_vege_moving_solid_vel_[1];
        } else {
            // Write depth only
            rp_begin_info.renderPass = rp_depth_only_[rp_index].handle();
            rp_begin_info.framebuffer = depth_fill_fb_[ctx.backend_frame()][fb_to_use_].handle();
            pipeline_onesided = &pi_vege_static_solid_[0];
            pipeline_twosided = &pi_vege_static_solid_[1];
        }
        api_ctx->vkCmdBeginRenderPass(cmd_buf, &rp_begin_info, VK_SUBPASS_CONTENTS_INLINE);

        vi_vege_solid_.BindBuffers(api_ctx, cmd_buf, 0, VK_INDEX_TYPE_UINT32);

        { // one-sided
            Ren::DebugMarker _mm(api_ctx, ctx.current_cmd_buf(), "ONE-SIDED");
            api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_onesided->handle());
            api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_onesided->layout(), 0,
                                             1, vege_descr_sets, 0, nullptr);
            i = _draw_range(api_ctx, cmd_buf, zfill_batch_indices, zfill_batches, i, BDB::BitsVege | BDB::BitMoving,
                            &draws_count);
        }

        { // two-sided
            Ren::DebugMarker _mm(api_ctx, ctx.current_cmd_buf(), "TWO-SIDED");
            api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_twosided->handle());
            api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_twosided->layout(), 0,
                                             1, vege_descr_sets, 0, nullptr);
            i = _draw_range(api_ctx, cmd_buf, zfill_batch_indices, zfill_batches, i,
                            BDB::BitsVege | BDB::BitMoving | BDB::BitTwoSided, &draws_count);
        }

        api_ctx->vkCmdEndRenderPass(cmd_buf);
    }

    { // static alpha-tested vegetation (depth and velocity)
        Ren::DebugMarker _m(api_ctx, ctx.current_cmd_buf(), "VEGE-ALPHA-SIMPLE");
        const int rp_index = (clear_depth_ && !draws_count) ? 0 : 1;

        VkClearValue clear_value = {};
        clear_value.depthStencil.depth = 0.0f;
        clear_value.depthStencil.stencil = 0;

        VkRenderPassBeginInfo rp_begin_info = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        rp_begin_info.renderArea = {{0, 0}, {uint32_t(view_state_->act_res[0]), uint32_t(view_state_->act_res[1])}};
        rp_begin_info.pClearValues = &clear_value;
        rp_begin_info.clearValueCount = 1;

        Ren::Pipeline *pipeline_onesided = nullptr, *pipeline_twosided = nullptr;
        if ((*p_list_)->render_settings.taa_mode != eTAAMode::Off) {
            // Write depth and velocity
            rp_begin_info.renderPass = rp_depth_velocity_[rp_index].handle();
            rp_begin_info.framebuffer = depth_fill_vel_fb_[ctx.backend_frame()][fb_to_use_].handle();
            pipeline_onesided = &pi_vege_static_transp_vel_[0];
            pipeline_twosided = &pi_vege_static_transp_vel_[1];
        } else {
            // Write depth only
            rp_begin_info.renderPass = rp_depth_only_[rp_index].handle();
            rp_begin_info.framebuffer = depth_fill_fb_[ctx.backend_frame()][fb_to_use_].handle();
            pipeline_onesided = &pi_vege_static_transp_[0];
            pipeline_twosided = &pi_vege_static_transp_[1];
        }
        api_ctx->vkCmdBeginRenderPass(cmd_buf, &rp_begin_info, VK_SUBPASS_CONTENTS_INLINE);

        vi_vege_transp_.BindBuffers(api_ctx, cmd_buf, 0, VK_INDEX_TYPE_UINT32);

        { // one-sided
            Ren::DebugMarker _mm(api_ctx, ctx.current_cmd_buf(), "ONE-SIDED");
            api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_onesided->handle());
            api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_onesided->layout(), 0,
                                             2, vege_descr_sets, 0, nullptr);
            i = _draw_range_ext(api_ctx, cmd_buf, *pipeline_onesided, zfill_batch_indices, zfill_batches, i,
                                BDB::BitsVege | BDB::BitAlphaTest, materials_per_descriptor,
                                bindless_tex_->textures_descr_sets, &draws_count);
        }

        { // two-sided
            Ren::DebugMarker _mm(api_ctx, ctx.current_cmd_buf(), "TWO-SIDED");
            api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_twosided->handle());
            api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_twosided->layout(), 0,
                                             2, vege_descr_sets, 0, nullptr);
            i = _draw_range_ext(api_ctx, cmd_buf, *pipeline_twosided, zfill_batch_indices, zfill_batches, i,
                                BDB::BitsVege | BDB::BitAlphaTest | BDB::BitTwoSided, materials_per_descriptor,
                                bindless_tex_->textures_descr_sets, &draws_count);
        }

        api_ctx->vkCmdEndRenderPass(cmd_buf);
    }

    { // moving alpha-tested vegetation (depth and velocity)
        Ren::DebugMarker _m(api_ctx, ctx.current_cmd_buf(), "VEGE-ALPHA-MOVING");
        const int rp_index = (clear_depth_ && !draws_count) ? 0 : 1;

        VkClearValue clear_value = {};
        clear_value.depthStencil.depth = 0.0f;
        clear_value.depthStencil.stencil = 0;

        VkRenderPassBeginInfo rp_begin_info = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        rp_begin_info.renderArea = {{0, 0}, {uint32_t(view_state_->act_res[0]), uint32_t(view_state_->act_res[1])}};
        rp_begin_info.pClearValues = &clear_value;
        rp_begin_info.clearValueCount = 1;

        Ren::Pipeline *pipeline_onesided = nullptr, *pipeline_twosided = nullptr;
        if ((*p_list_)->render_settings.taa_mode != eTAAMode::Off) {
            // Write depth and velocity
            rp_begin_info.renderPass = rp_depth_velocity_[rp_index].handle();
            rp_begin_info.framebuffer = depth_fill_vel_fb_[ctx.backend_frame()][fb_to_use_].handle();
            pipeline_onesided = &pi_vege_moving_transp_vel_[0];
            pipeline_twosided = &pi_vege_moving_transp_vel_[1];
        } else {
            // Write depth only
            rp_begin_info.renderPass = rp_depth_only_[rp_index].handle();
            rp_begin_info.framebuffer = depth_fill_fb_[ctx.backend_frame()][fb_to_use_].handle();
            pipeline_onesided = &pi_vege_static_transp_[0];
            pipeline_twosided = &pi_vege_static_transp_[1];
        }
        api_ctx->vkCmdBeginRenderPass(cmd_buf, &rp_begin_info, VK_SUBPASS_CONTENTS_INLINE);

        vi_vege_transp_.BindBuffers(api_ctx, cmd_buf, 0, VK_INDEX_TYPE_UINT32);

        { // one-sided
            Ren::DebugMarker _mm(api_ctx, ctx.current_cmd_buf(), "ONE-SIDED");
            api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_onesided->handle());
            api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_onesided->layout(), 0,
                                             2, vege_descr_sets, 0, nullptr);
            i = _draw_range_ext(api_ctx, cmd_buf, *pipeline_onesided, zfill_batch_indices, zfill_batches, i,
                                BDB::BitsVege | BDB::BitAlphaTest | BDB::BitMoving, materials_per_descriptor,
                                bindless_tex_->textures_descr_sets, &draws_count);
        }

        { // two-sided
            Ren::DebugMarker _mm(api_ctx, ctx.current_cmd_buf(), "TWO-SIDED");
            api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_twosided->handle());
            api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_twosided->layout(), 0,
                                             2, vege_descr_sets, 0, nullptr);
            i = _draw_range_ext(api_ctx, cmd_buf, *pipeline_twosided, zfill_batch_indices, zfill_batches, i,
                                BDB::BitsVege | BDB::BitAlphaTest | BDB::BitMoving | BDB::BitTwoSided,
                                materials_per_descriptor, bindless_tex_->textures_descr_sets, &draws_count);
        }

        api_ctx->vkCmdEndRenderPass(cmd_buf);
    }

    { // solid skinned meshes (depth and velocity)
        Ren::DebugMarker _m(api_ctx, ctx.current_cmd_buf(), "SKIN-SOLID-SIMPLE");
        const int rp_index = (clear_depth_ && !draws_count) ? 0 : 1;

        VkClearValue clear_value = {};
        clear_value.depthStencil.depth = 0.0f;
        clear_value.depthStencil.stencil = 0;

        VkRenderPassBeginInfo rp_begin_info = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        rp_begin_info.renderArea = {{0, 0}, {uint32_t(view_state_->act_res[0]), uint32_t(view_state_->act_res[1])}};
        rp_begin_info.pClearValues = &clear_value;
        rp_begin_info.clearValueCount = 1;

        Ren::Pipeline *pipeline_onesided = nullptr, *pipeline_twosided = nullptr;
        if ((*p_list_)->render_settings.taa_mode != eTAAMode::Off) {
            // Write depth and velocity
            rp_begin_info.renderPass = rp_depth_velocity_[rp_index].handle();
            rp_begin_info.framebuffer = depth_fill_vel_fb_[ctx.backend_frame()][fb_to_use_].handle();
            pipeline_onesided = &pi_skin_static_solid_vel_[0];
            pipeline_twosided = &pi_skin_static_solid_vel_[1];

            vi_skin_solid_.BindBuffers(api_ctx, cmd_buf, 0, VK_INDEX_TYPE_UINT32);
        } else {
            // Write depth only
            rp_begin_info.renderPass = rp_depth_only_[rp_index].handle();
            rp_begin_info.framebuffer = depth_fill_fb_[ctx.backend_frame()][fb_to_use_].handle();
            pipeline_onesided = &pi_skin_static_solid_[0];
            pipeline_twosided = &pi_skin_static_solid_[1];

            vi_solid_.BindBuffers(api_ctx, cmd_buf, 0, VK_INDEX_TYPE_UINT32);
        }
        api_ctx->vkCmdBeginRenderPass(cmd_buf, &rp_begin_info, VK_SUBPASS_CONTENTS_INLINE);

        { // one-sided
            Ren::DebugMarker _mm(api_ctx, ctx.current_cmd_buf(), "ONE-SIDED");
            api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_onesided->handle());
            api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_onesided->layout(), 0,
                                             1, simple_descr_sets, 0, nullptr);
            i = _draw_range(api_ctx, cmd_buf, zfill_batch_indices, zfill_batches, i, BDB::BitsSkinned, &draws_count);
        }

        { // two-sided
            Ren::DebugMarker _mm(api_ctx, ctx.current_cmd_buf(), "TWO-SIDED");
            api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_twosided->handle());
            api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_twosided->layout(), 0,
                                             1, simple_descr_sets, 0, nullptr);
            i = _draw_range(api_ctx, cmd_buf, zfill_batch_indices, zfill_batches, i,
                            BDB::BitsSkinned | BDB::BitTwoSided, &draws_count);
        }

        api_ctx->vkCmdEndRenderPass(cmd_buf);
    }

    { // moving solid skinned (depth and velocity)
        Ren::DebugMarker _m(api_ctx, ctx.current_cmd_buf(), "SKIN-SOLID-MOVING");
        const int rp_index = (clear_depth_ && !draws_count) ? 0 : 1;

        VkClearValue clear_value = {};
        clear_value.depthStencil.depth = 0.0f;
        clear_value.depthStencil.stencil = 0;

        VkRenderPassBeginInfo rp_begin_info = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        rp_begin_info.renderArea = {{0, 0}, {uint32_t(view_state_->act_res[0]), uint32_t(view_state_->act_res[1])}};
        rp_begin_info.pClearValues = &clear_value;
        rp_begin_info.clearValueCount = 1;

        Ren::Pipeline *pipeline_onesided = nullptr, *pipeline_twosided = nullptr;
        if ((*p_list_)->render_settings.taa_mode != eTAAMode::Off) {
            // Write depth and velocity
            rp_begin_info.renderPass = rp_depth_velocity_[rp_index].handle();
            rp_begin_info.framebuffer = depth_fill_vel_fb_[ctx.backend_frame()][fb_to_use_].handle();
            pipeline_onesided = &pi_skin_moving_solid_vel_[0];
            pipeline_twosided = &pi_skin_moving_solid_vel_[1];

            vi_skin_solid_.BindBuffers(api_ctx, cmd_buf, 0, VK_INDEX_TYPE_UINT32);
        } else {
            // Write depth only
            rp_begin_info.renderPass = rp_depth_only_[rp_index].handle();
            rp_begin_info.framebuffer = depth_fill_fb_[ctx.backend_frame()][fb_to_use_].handle();
            pipeline_onesided = &pi_skin_static_solid_[0];
            pipeline_twosided = &pi_skin_static_solid_[1];

            vi_solid_.BindBuffers(api_ctx, cmd_buf, 0, VK_INDEX_TYPE_UINT32);
        }
        api_ctx->vkCmdBeginRenderPass(cmd_buf, &rp_begin_info, VK_SUBPASS_CONTENTS_INLINE);

        { // one-sided
            Ren::DebugMarker _mm(api_ctx, ctx.current_cmd_buf(), "ONE-SIDED");
            api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_onesided->handle());
            api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_onesided->layout(), 0,
                                             1, simple_descr_sets, 0, nullptr);
            i = _draw_range(api_ctx, cmd_buf, zfill_batch_indices, zfill_batches, i, BDB::BitsSkinned | BDB::BitMoving,
                            &draws_count);
        }

        { // two-sided
            Ren::DebugMarker _mm(api_ctx, ctx.current_cmd_buf(), "TWO-SIDED");
            api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_twosided->handle());
            api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_twosided->layout(), 0,
                                             1, simple_descr_sets, 0, nullptr);
            i = _draw_range(api_ctx, cmd_buf, zfill_batch_indices, zfill_batches, i,
                            BDB::BitsSkinned | BDB::BitMoving | BDB::BitTwoSided, &draws_count);
        }

        api_ctx->vkCmdEndRenderPass(cmd_buf);
    }

    { // static alpha-tested skinned (depth and velocity)
        Ren::DebugMarker _m(api_ctx, ctx.current_cmd_buf(), "SKIN-ALPHA-SIMPLE");
        const int rp_index = (clear_depth_ && !draws_count) ? 0 : 1;

        VkClearValue clear_value = {};
        clear_value.depthStencil.depth = 0.0f;
        clear_value.depthStencil.stencil = 0;

        VkRenderPassBeginInfo rp_begin_info = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        rp_begin_info.renderArea = {{0, 0}, {uint32_t(view_state_->act_res[0]), uint32_t(view_state_->act_res[1])}};
        rp_begin_info.pClearValues = &clear_value;
        rp_begin_info.clearValueCount = 1;

        Ren::Pipeline *pipeline_onesided = nullptr, *pipeline_twosided = nullptr;
        if ((*p_list_)->render_settings.taa_mode != eTAAMode::Off) {
            // Write depth and velocity
            rp_begin_info.renderPass = rp_depth_velocity_[rp_index].handle();
            rp_begin_info.framebuffer = depth_fill_vel_fb_[ctx.backend_frame()][fb_to_use_].handle();
            pipeline_onesided = &pi_skin_static_transp_vel_[0];
            pipeline_twosided = &pi_skin_static_transp_vel_[1];

            vi_skin_solid_.BindBuffers(api_ctx, cmd_buf, 0, VK_INDEX_TYPE_UINT32);
        } else {
            // Write depth only
            rp_begin_info.renderPass = rp_depth_only_[rp_index].handle();
            rp_begin_info.framebuffer = depth_fill_fb_[ctx.backend_frame()][fb_to_use_].handle();
            pipeline_onesided = &pi_skin_static_transp_[0];
            pipeline_twosided = &pi_skin_static_transp_[1];

            vi_solid_.BindBuffers(api_ctx, cmd_buf, 0, VK_INDEX_TYPE_UINT32);
        }
        api_ctx->vkCmdBeginRenderPass(cmd_buf, &rp_begin_info, VK_SUBPASS_CONTENTS_INLINE);

        { // one-sided
            Ren::DebugMarker _mm(api_ctx, ctx.current_cmd_buf(), "ONE-SIDED");
            api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_onesided->handle());
            api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_onesided->layout(), 0,
                                             2, simple_descr_sets, 0, nullptr);
            i = _draw_range_ext(api_ctx, cmd_buf, *pipeline_onesided, zfill_batch_indices, zfill_batches, i,
                                BDB::BitsSkinned | BDB::BitAlphaTest, materials_per_descriptor,
                                bindless_tex_->textures_descr_sets, &draws_count);
        }

        { // two-sided
            Ren::DebugMarker _mm(api_ctx, ctx.current_cmd_buf(), "TWO-SIDED");
            api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_twosided->handle());
            api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_twosided->layout(), 0,
                                             2, simple_descr_sets, 0, nullptr);
            i = _draw_range_ext(api_ctx, cmd_buf, *pipeline_twosided, zfill_batch_indices, zfill_batches, i,
                                BDB::BitsSkinned | BDB::BitAlphaTest | BDB::BitTwoSided, materials_per_descriptor,
                                bindless_tex_->textures_descr_sets, &draws_count);
        }

        api_ctx->vkCmdEndRenderPass(cmd_buf);
    }

    { // moving alpha-tested skinned (depth and velocity)
        Ren::DebugMarker _m(api_ctx, ctx.current_cmd_buf(), "SKIN-ALPHA-MOVING");
        const int rp_index = (clear_depth_ && !draws_count) ? 0 : 1;

        VkClearValue clear_value = {};
        clear_value.depthStencil.depth = 0.0f;
        clear_value.depthStencil.stencil = 0;

        VkRenderPassBeginInfo rp_begin_info = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        rp_begin_info.renderArea = {{0, 0}, {uint32_t(view_state_->act_res[0]), uint32_t(view_state_->act_res[1])}};
        rp_begin_info.pClearValues = &clear_value;
        rp_begin_info.clearValueCount = 1;

        Ren::Pipeline *pipeline_onesided = nullptr, *pipeline_twosided = nullptr;
        if ((*p_list_)->render_settings.taa_mode != eTAAMode::Off) {
            // Write depth and velocity
            rp_begin_info.renderPass = rp_depth_velocity_[rp_index].handle();
            rp_begin_info.framebuffer = depth_fill_vel_fb_[ctx.backend_frame()][fb_to_use_].handle();
            pipeline_onesided = &pi_skin_moving_transp_vel_[0];
            pipeline_twosided = &pi_skin_moving_transp_vel_[1];

            vi_skin_solid_.BindBuffers(api_ctx, cmd_buf, 0, VK_INDEX_TYPE_UINT32);
        } else {
            // Write depth only
            rp_begin_info.renderPass = rp_depth_only_[rp_index].handle();
            rp_begin_info.framebuffer = depth_fill_fb_[ctx.backend_frame()][fb_to_use_].handle();
            pipeline_onesided = &pi_skin_static_transp_[0];
            pipeline_twosided = &pi_skin_static_transp_[1];

            vi_solid_.BindBuffers(api_ctx, cmd_buf, 0, VK_INDEX_TYPE_UINT32);
        }
        api_ctx->vkCmdBeginRenderPass(cmd_buf, &rp_begin_info, VK_SUBPASS_CONTENTS_INLINE);

        { // one-sided
            Ren::DebugMarker _mm(api_ctx, ctx.current_cmd_buf(), "ONE-SIDED");
            api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_onesided->handle());
            api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_onesided->layout(), 0,
                                             2, simple_descr_sets, 0, nullptr);
            i = _draw_range_ext(api_ctx, cmd_buf, *pipeline_onesided, zfill_batch_indices, zfill_batches, i,
                                BDB::BitsSkinned | BDB::BitAlphaTest | BDB::BitMoving, materials_per_descriptor,
                                bindless_tex_->textures_descr_sets, &draws_count);
        }

        { // two-sided
            Ren::DebugMarker _mm(api_ctx, ctx.current_cmd_buf(), "TWO-SIDED");
            api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_twosided->handle());
            api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_twosided->layout(), 0,
                                             2, simple_descr_sets, 0, nullptr);
            i = _draw_range_ext(api_ctx, cmd_buf, *pipeline_twosided, zfill_batch_indices, zfill_batches, i,
                                BDB::BitsSkinned | BDB::BitAlphaTest | BDB::BitMoving | BDB::BitTwoSided,
                                materials_per_descriptor, bindless_tex_->textures_descr_sets, &draws_count);
        }

        api_ctx->vkCmdEndRenderPass(cmd_buf);
    }
}
