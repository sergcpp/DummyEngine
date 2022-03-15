#include "RpDepthFill.h"

#include "../Renderer_Structs.h"

#include <Ren/Context.h>
#include <Ren/DebugMarker.h>
#include <Ren/DescriptorPool.h>
#include <Ren/RastState.h>
#include <Ren/VKCtx.h>

namespace RpSharedInternal {
uint32_t _depth_draw_range(VkCommandBuffer cmd_buf, const Ren::Pipeline &pipeline,
                           const DynArrayConstRef<uint32_t> &zfill_batch_indices,
                           const DynArrayConstRef<DepthDrawBatch> &zfill_batches, uint32_t i, uint32_t mask,
                           BackendInfo &backend_info) {
    for (; i < zfill_batch_indices.count; i++) {
        const DepthDrawBatch &batch = zfill_batches.data[zfill_batch_indices.data[i]];
        if ((batch.sort_key & DepthDrawBatch::FlagBits) != mask) {
            break;
        }

        if (!batch.instance_count) {
            continue;
        }

        vkCmdDrawIndexed(cmd_buf, batch.indices_count, // index count
                         batch.instance_count,         // instance count
                         batch.indices_offset,         // first index
                         batch.base_vertex,            // vertex offset
                         batch.instance_start);        // first instance

        backend_info.depth_fill_draw_calls_count++;
    }
    return i;
}

uint32_t _depth_draw_range_ext(VkCommandBuffer cmd_buf, const Ren::Pipeline &pipeline,
                               const DynArrayConstRef<uint32_t> &zfill_batch_indices,
                               const DynArrayConstRef<DepthDrawBatch> &zfill_batches, uint32_t i, uint32_t mask,
                               const uint32_t materials_per_descriptor,
                               const Ren::SmallVectorImpl<VkDescriptorSet> &descr_sets, BackendInfo &backend_info) {
    uint32_t bound_descr_id = 0;
    for (; i < zfill_batch_indices.count; i++) {
        const DepthDrawBatch &batch = zfill_batches.data[zfill_batch_indices.data[i]];
        if ((batch.sort_key & DepthDrawBatch::FlagBits) != mask) {
            break;
        }

        if (!batch.instance_count) {
            continue;
        }

        const uint32_t descr_id = batch.material_index / materials_per_descriptor;
        if (descr_id != bound_descr_id) {
            vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.layout(), 1, 1,
                                    &descr_sets[descr_id], 0, nullptr);
            bound_descr_id = descr_id;
        }

        vkCmdDrawIndexed(cmd_buf, batch.indices_count, // index count
                         batch.instance_count,         // instance count
                         batch.indices_offset,         // first index
                         batch.base_vertex,            // vertex offset
                         batch.instance_start);        // first instance

        backend_info.depth_fill_draw_calls_count++;
    }
    return i;
}
} // namespace RpSharedInternal

void RpDepthFill::DrawDepth(RpBuilder &builder, RpAllocBuf &vtx_buf1, RpAllocBuf &vtx_buf2, RpAllocBuf &ndx_buf) {
    using namespace RpSharedInternal;

    RpAllocBuf &unif_shared_data_buf = builder.GetReadBuffer(shared_data_buf_);
    RpAllocBuf &instances_buf = builder.GetReadBuffer(instances_buf_);
    RpAllocBuf &instance_indices_buf = builder.GetReadBuffer(instance_indices_buf_);
    RpAllocBuf &materials_buf = builder.GetReadBuffer(materials_buf_);
    RpAllocTex &noise_tex = builder.GetReadTexture(noise_tex_);

    Ren::Context &ctx = builder.ctx();
    Ren::ApiContext *api_ctx = ctx.api_ctx();

    VkCommandBuffer cmd_buf = api_ctx->draw_cmd_buf[api_ctx->backend_frame];
    //
    // Setup viewport
    //
    const VkViewport viewport = {0.0f, 0.0f, float(view_state_->act_res[0]), float(view_state_->act_res[1]),
                                 0.0f, 1.0f};
    vkCmdSetViewport(cmd_buf, 0, 1, &viewport);
    const VkRect2D scissor = {0, 0, uint32_t(view_state_->act_res[0]), uint32_t(view_state_->act_res[1])};
    vkCmdSetScissor(cmd_buf, 0, 1, &scissor);

    const uint32_t materials_per_descriptor = api_ctx->max_combined_image_samplers / REN_MAX_TEX_PER_MATERIAL;

    BackendInfo _dummy = {};
    uint32_t i = 0;

    using DDB = DepthDrawBatch;

    //
    // Prepare descriptor sets
    //

    VkDescriptorSetLayout simple_descr_set_layout = pi_static_solid_[0].prog()->descr_set_layouts()[0];
    VkDescriptorSet simple_descr_sets[2];
    { // allocate descriptors
        Ren::DescrSizes descr_sizes;
        descr_sizes.ubuf_count = 1;
        descr_sizes.sbuf_count = 1;
        descr_sizes.tbuf_count = 1;

        simple_descr_sets[0] = ctx.default_descr_alloc()->Alloc(descr_sizes, simple_descr_set_layout);
        simple_descr_sets[1] = (*bindless_tex_->textures_descr_sets)[0];
    }

    { // update descriptor sets
        const VkDescriptorBufferInfo ubuf_info = {unif_shared_data_buf.ref->vk_handle(), 0, VK_WHOLE_SIZE};
        const VkBufferView instances_buf_view = instances_buf.tbos[0]->view();
        const VkDescriptorBufferInfo instance_indices_buf_info = {instance_indices_buf.ref->vk_handle(), 0,
                                                                  VK_WHOLE_SIZE};
        const VkDescriptorBufferInfo mat_buf_info = {materials_buf.ref->vk_handle(), 0, VK_WHOLE_SIZE};

        VkWriteDescriptorSet descr_writes[4];
        descr_writes[0] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        descr_writes[0].dstSet = simple_descr_sets[0];
        descr_writes[0].dstBinding = REN_UB_SHARED_DATA_LOC;
        descr_writes[0].dstArrayElement = 0;
        descr_writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descr_writes[0].descriptorCount = 1;
        descr_writes[0].pBufferInfo = &ubuf_info;

        descr_writes[1] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        descr_writes[1].dstSet = simple_descr_sets[0];
        descr_writes[1].dstBinding = REN_INST_BUF_SLOT;
        descr_writes[1].dstArrayElement = 0;
        descr_writes[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
        descr_writes[1].descriptorCount = 1;
        descr_writes[1].pTexelBufferView = &instances_buf_view;

        descr_writes[2] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        descr_writes[2].dstSet = simple_descr_sets[0];
        descr_writes[2].dstBinding = REN_INST_INDICES_BUF_SLOT;
        descr_writes[2].dstArrayElement = 0;
        descr_writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descr_writes[2].descriptorCount = 1;
        descr_writes[2].pBufferInfo = &instance_indices_buf_info;

        descr_writes[3] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        descr_writes[3].dstSet = simple_descr_sets[0];
        descr_writes[3].dstBinding = REN_MATERIALS_SLOT;
        descr_writes[3].dstArrayElement = 0;
        descr_writes[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descr_writes[3].descriptorCount = 1;
        descr_writes[3].pBufferInfo = &mat_buf_info;

        vkUpdateDescriptorSets(api_ctx->device, COUNT_OF(descr_writes), descr_writes, 0, nullptr);
    }

    VkDescriptorSetLayout vege_descr_set_layout = pi_vege_static_solid_vel_[0].prog()->descr_set_layouts()[0];
    VkDescriptorSet vege_descr_sets[2];
    { // allocate descriptors
        Ren::DescrSizes descr_sizes;
        descr_sizes.img_sampler_count = 1;
        descr_sizes.ubuf_count = 1;
        descr_sizes.tbuf_count = 1;

        vege_descr_sets[0] = ctx.default_descr_alloc()->Alloc(descr_sizes, vege_descr_set_layout);
        vege_descr_sets[1] = (*bindless_tex_->textures_descr_sets)[0];
    }

    { // update descriptor set
        const VkDescriptorBufferInfo ubuf_info = {unif_shared_data_buf.ref->vk_handle(), 0, VK_WHOLE_SIZE};
        const VkBufferView instances_buf_view = instances_buf.tbos[0]->view();
        const VkDescriptorBufferInfo instance_indices_buf_info = {instance_indices_buf.ref->vk_handle(), 0, VK_WHOLE_SIZE};
        const VkDescriptorImageInfo img_info = noise_tex.ref->vk_desc_image_info();
        const VkDescriptorBufferInfo mat_buf_info = {materials_buf.ref->vk_handle(), 0, VK_WHOLE_SIZE};

        VkWriteDescriptorSet descr_writes[5];
        descr_writes[0] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        descr_writes[0].dstSet = vege_descr_sets[0];
        descr_writes[0].dstBinding = REN_UB_SHARED_DATA_LOC;
        descr_writes[0].dstArrayElement = 0;
        descr_writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descr_writes[0].descriptorCount = 1;
        descr_writes[0].pBufferInfo = &ubuf_info;

        descr_writes[1] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        descr_writes[1].dstSet = vege_descr_sets[0];
        descr_writes[1].dstBinding = REN_INST_BUF_SLOT;
        descr_writes[1].dstArrayElement = 0;
        descr_writes[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
        descr_writes[1].descriptorCount = 1;
        descr_writes[1].pTexelBufferView = &instances_buf_view;

        descr_writes[2] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        descr_writes[2].dstSet = vege_descr_sets[0];
        descr_writes[2].dstBinding = REN_INST_INDICES_BUF_SLOT;
        descr_writes[2].dstArrayElement = 0;
        descr_writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descr_writes[2].descriptorCount = 1;
        descr_writes[2].pBufferInfo = &instance_indices_buf_info;

        descr_writes[3] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        descr_writes[3].dstSet = vege_descr_sets[0];
        descr_writes[3].dstBinding = REN_NOISE_TEX_SLOT;
        descr_writes[3].dstArrayElement = 0;
        descr_writes[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descr_writes[3].descriptorCount = 1;
        descr_writes[3].pImageInfo = &img_info;

        descr_writes[4] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        descr_writes[4].dstSet = vege_descr_sets[0];
        descr_writes[4].dstBinding = REN_MATERIALS_SLOT;
        descr_writes[4].dstArrayElement = 0;
        descr_writes[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descr_writes[4].descriptorCount = 1;
        descr_writes[4].pBufferInfo = &mat_buf_info;

        vkUpdateDescriptorSets(api_ctx->device, COUNT_OF(descr_writes), descr_writes, 0, nullptr);
    }

    { // solid meshes
        Ren::DebugMarker _m(cmd_buf, "STATIC-SOLID-SIMPLE");

        VkRenderPassBeginInfo rp_begin_info = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        rp_begin_info.renderPass = rp_depth_only_.handle();
        rp_begin_info.framebuffer = depth_fill_fb_[ctx.backend_frame()].handle();
        rp_begin_info.renderArea = {0, 0, uint32_t(view_state_->act_res[0]), uint32_t(view_state_->act_res[1])};
        vkCmdBeginRenderPass(cmd_buf, &rp_begin_info, VK_SUBPASS_CONTENTS_INLINE);

        vi_depth_pass_solid_.BindBuffers(cmd_buf, 0, VK_INDEX_TYPE_UINT32);

        { // one-sided
            Ren::DebugMarker _mm(cmd_buf, "ONE-SIDED");
            vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_static_solid_[0].handle());
            vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_static_solid_[0].layout(), 0, 1,
                                    simple_descr_sets, 0, nullptr);
            i = _depth_draw_range(cmd_buf, pi_static_solid_[0], zfill_batch_indices, zfill_batches, i, 0u, _dummy);
        }

        { // two-sided
            Ren::DebugMarker _mm(cmd_buf, "TWO-SIDED");
            vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_static_solid_[1].handle());
            vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_static_solid_[1].layout(), 0, 1,
                                    simple_descr_sets, 0, nullptr);
            i = _depth_draw_range(cmd_buf, pi_static_solid_[1], zfill_batch_indices, zfill_batches, i, DDB::BitTwoSided,
                                  _dummy);
        }

        vkCmdEndRenderPass(cmd_buf);
    }

    { // moving solid meshes (depth and velocity)
        Ren::DebugMarker _m(cmd_buf, "STATIC-SOLID-MOVING");

        VkRenderPassBeginInfo rp_begin_info = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        rp_begin_info.renderArea = {0, 0, uint32_t(view_state_->act_res[0]), uint32_t(view_state_->act_res[1])};

        Ren::Pipeline *pipeline_onesided = nullptr, *pipeline_twosided = nullptr;
        if ((render_flags_ & EnableTaa) != 0) {
            // Write depth and velocity
            rp_begin_info.renderPass = rp_depth_velocity_.handle();
            rp_begin_info.framebuffer = depth_fill_vel_fb_[ctx.backend_frame()].handle();
            pipeline_onesided = &pi_moving_solid_[0];
            pipeline_twosided = &pi_moving_solid_[1];
        } else {
            // Write depth only
            rp_begin_info.renderPass = rp_depth_only_.handle();
            rp_begin_info.framebuffer = depth_fill_fb_[ctx.backend_frame()].handle();
            pipeline_onesided = &pi_static_solid_[0];
            pipeline_twosided = &pi_static_solid_[1];
        }
        VkClearValue clear_val = {};
        rp_begin_info.pClearValues = &clear_val;
        rp_begin_info.clearValueCount = 1;
        vkCmdBeginRenderPass(cmd_buf, &rp_begin_info, VK_SUBPASS_CONTENTS_INLINE);

        vi_depth_pass_solid_.BindBuffers(cmd_buf, 0, VK_INDEX_TYPE_UINT32);

        { // one-sided
            Ren::DebugMarker _mm(ctx.current_cmd_buf(), "ONE-SIDED");
            vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_onesided->handle());
            vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_onesided->layout(), 0, 1,
                                    simple_descr_sets, 0, nullptr);
            i = _depth_draw_range(cmd_buf, *pipeline_onesided, zfill_batch_indices, zfill_batches, i, DDB::BitMoving,
                                  _dummy);
        }

        { // two-sided
            Ren::DebugMarker _mm(ctx.current_cmd_buf(), "TWO-SIDED");
            vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_twosided->handle());
            vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_twosided->layout(), 0, 1,
                                    simple_descr_sets, 0, nullptr);
            i = _depth_draw_range(cmd_buf, *pipeline_twosided, zfill_batch_indices, zfill_batches, i,
                                  DDB::BitMoving | DDB::BitTwoSided, _dummy);
        }

        vkCmdEndRenderPass(cmd_buf);
    }

    { // simple alpha-tested meshes (depth only)
        Ren::DebugMarker _m(ctx.current_cmd_buf(), "STATIC-ALPHA-SIMPLE");

        VkRenderPassBeginInfo rp_begin_info = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        rp_begin_info.renderPass = rp_depth_only_.handle();
        rp_begin_info.framebuffer = depth_fill_fb_[ctx.backend_frame()].handle();
        rp_begin_info.renderArea = {0, 0, uint32_t(view_state_->act_res[0]), uint32_t(view_state_->act_res[1])};
        vkCmdBeginRenderPass(cmd_buf, &rp_begin_info, VK_SUBPASS_CONTENTS_INLINE);

        vi_depth_pass_transp_.BindBuffers(cmd_buf, 0, VK_INDEX_TYPE_UINT32);

        { // one-sided
            Ren::DebugMarker _mm(ctx.current_cmd_buf(), "ONE-SIDED");
            vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_static_transp_[0].handle());
            vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_static_transp_[0].layout(), 0, 2,
                                    simple_descr_sets, 0, nullptr);
            i = _depth_draw_range_ext(cmd_buf, pi_static_transp_[0], zfill_batch_indices, zfill_batches, i,
                                      DDB::BitAlphaTest, materials_per_descriptor, *bindless_tex_->textures_descr_sets,
                                      _dummy);
        }

        { // two-sided
            Ren::DebugMarker _mm(ctx.current_cmd_buf(), "TWO-SIDED");
            vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_static_transp_[1].handle());
            vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pi_static_transp_[1].layout(), 0, 2,
                                    simple_descr_sets, 0, nullptr);
            i = _depth_draw_range_ext(cmd_buf, pi_static_transp_[1], zfill_batch_indices, zfill_batches, i,
                                      DDB::BitAlphaTest | DDB::BitTwoSided, materials_per_descriptor,
                                      *bindless_tex_->textures_descr_sets, _dummy);
        }

        vkCmdEndRenderPass(cmd_buf);
    }

    { // moving alpha-tested meshes (depth and velocity)
        Ren::DebugMarker _m(ctx.current_cmd_buf(), "STATIC-ALPHA-MOVING");

        VkRenderPassBeginInfo rp_begin_info = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        rp_begin_info.renderArea = {0, 0, uint32_t(view_state_->act_res[0]), uint32_t(view_state_->act_res[1])};

        Ren::Pipeline *pipeline_onesided = nullptr, *pipeline_twosided = nullptr;
        if ((render_flags_ & EnableTaa) != 0) {
            // Write depth and velocity
            rp_begin_info.renderPass = rp_depth_velocity_.handle();
            rp_begin_info.framebuffer = depth_fill_vel_fb_[ctx.backend_frame()].handle();
            pipeline_onesided = &pi_moving_transp_[0];
            pipeline_twosided = &pi_moving_transp_[1];
        } else {
            // Write depth only
            rp_begin_info.renderPass = rp_depth_only_.handle();
            rp_begin_info.framebuffer = depth_fill_fb_[ctx.backend_frame()].handle();
            pipeline_onesided = &pi_static_transp_[0];
            pipeline_twosided = &pi_static_transp_[1];
        }
        VkClearValue clear_val = {};
        rp_begin_info.pClearValues = &clear_val;
        rp_begin_info.clearValueCount = 1;
        vkCmdBeginRenderPass(cmd_buf, &rp_begin_info, VK_SUBPASS_CONTENTS_INLINE);

        vi_depth_pass_transp_.BindBuffers(cmd_buf, 0, VK_INDEX_TYPE_UINT32);

        { // one-sided
            Ren::DebugMarker _mm(ctx.current_cmd_buf(), "ONE-SIDED");
            vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_onesided->handle());
            vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_onesided->layout(), 0, 2,
                                    simple_descr_sets, 0, nullptr);
            i = _depth_draw_range_ext(cmd_buf, *pipeline_onesided, zfill_batch_indices, zfill_batches, i,
                                      DDB::BitAlphaTest | DDB::BitMoving, materials_per_descriptor,
                                      *bindless_tex_->textures_descr_sets, _dummy);
        }

        { // two-sided
            Ren::DebugMarker _mm(ctx.current_cmd_buf(), "TWO-SIDED");
            vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_twosided->handle());
            vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_twosided->layout(), 0, 2,
                                    simple_descr_sets, 0, nullptr);
            i = _depth_draw_range_ext(cmd_buf, *pipeline_twosided, zfill_batch_indices, zfill_batches, i,
                                      DDB::BitAlphaTest | DDB::BitMoving | DDB::BitTwoSided, materials_per_descriptor,
                                      *bindless_tex_->textures_descr_sets, _dummy);
        }

        vkCmdEndRenderPass(cmd_buf);
    }

    { // static solid vegetation
        Ren::DebugMarker _m(cmd_buf, "VEGE-SOLID-SIMPLE");

        VkRenderPassBeginInfo rp_begin_info = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        rp_begin_info.renderArea = {0, 0, uint32_t(view_state_->act_res[0]), uint32_t(view_state_->act_res[1])};

        Ren::Pipeline *pipeline_onesided = nullptr, *pipeline_twosided = nullptr;
        if ((render_flags_ & EnableTaa) != 0) {
            // Write depth and velocity
            rp_begin_info.renderPass = rp_depth_velocity_.handle();
            rp_begin_info.framebuffer = depth_fill_vel_fb_[ctx.backend_frame()].handle();
            pipeline_onesided = &pi_vege_static_solid_vel_[0];
            pipeline_twosided = &pi_vege_static_solid_vel_[1];
        } else {
            // Write depth only
            rp_begin_info.renderPass = rp_depth_only_.handle();
            rp_begin_info.framebuffer = depth_fill_fb_[ctx.backend_frame()].handle();
            pipeline_onesided = &pi_vege_static_solid_[0];
            pipeline_twosided = &pi_vege_static_solid_[1];
        }
        vkCmdBeginRenderPass(cmd_buf, &rp_begin_info, VK_SUBPASS_CONTENTS_INLINE);

        vi_depth_pass_vege_solid_.BindBuffers(cmd_buf, 0, VK_INDEX_TYPE_UINT32);

        { // one-sided
            Ren::DebugMarker _mm(cmd_buf, "ONE-SIDED");
            vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_onesided->handle());
            vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_onesided->layout(), 0, 1,
                                    vege_descr_sets, 0, nullptr);
            i = _depth_draw_range(cmd_buf, *pipeline_onesided, zfill_batch_indices, zfill_batches, i, DDB::BitsVege,
                                  _dummy);
        }

        { // two-sided
            Ren::DebugMarker _mm(cmd_buf, "TWO-SIDED");
            vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_twosided->handle());
            vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_twosided->layout(), 0, 1,
                                    vege_descr_sets, 0, nullptr);
            i = _depth_draw_range(cmd_buf, *pipeline_twosided, zfill_batch_indices, zfill_batches, i,
                                  DDB::BitsVege | DDB::BitTwoSided, _dummy);
        }

        vkCmdEndRenderPass(cmd_buf);
    }

    { // moving solid vegetation (depth and velocity)
        Ren::DebugMarker _m(ctx.current_cmd_buf(), "VEGE-SOLID-MOVING");

        VkRenderPassBeginInfo rp_begin_info = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        rp_begin_info.renderArea = {0, 0, uint32_t(view_state_->act_res[0]), uint32_t(view_state_->act_res[1])};

        Ren::Pipeline *pipeline_onesided = nullptr, *pipeline_twosided = nullptr;
        if ((render_flags_ & EnableTaa) != 0) {
            // Write depth and velocity
            rp_begin_info.renderPass = rp_depth_velocity_.handle();
            rp_begin_info.framebuffer = depth_fill_vel_fb_[ctx.backend_frame()].handle();
            pipeline_onesided = &pi_vege_moving_solid_vel_[0];
            pipeline_twosided = &pi_vege_moving_solid_vel_[1];
        } else {
            // Write depth only
            rp_begin_info.renderPass = rp_depth_only_.handle();
            rp_begin_info.framebuffer = depth_fill_fb_[ctx.backend_frame()].handle();
            pipeline_onesided = &pi_vege_static_solid_[0];
            pipeline_twosided = &pi_vege_static_solid_[1];
        }
        vkCmdBeginRenderPass(cmd_buf, &rp_begin_info, VK_SUBPASS_CONTENTS_INLINE);

        vi_depth_pass_vege_solid_.BindBuffers(cmd_buf, 0, VK_INDEX_TYPE_UINT32);

        { // one-sided
            Ren::DebugMarker _mm(ctx.current_cmd_buf(), "ONE-SIDED");
            vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_onesided->handle());
            vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_onesided->layout(), 0, 1,
                                    vege_descr_sets, 0, nullptr);
            i = _depth_draw_range(cmd_buf, *pipeline_onesided, zfill_batch_indices, zfill_batches, i,
                                  DDB::BitsVege | DDB::BitMoving, _dummy);
        }

        { // two-sided
            Ren::DebugMarker _mm(ctx.current_cmd_buf(), "TWO-SIDED");
            vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_twosided->handle());
            vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_twosided->layout(), 0, 1,
                                    vege_descr_sets, 0, nullptr);
            i = _depth_draw_range(cmd_buf, *pipeline_twosided, zfill_batch_indices, zfill_batches, i,
                                  DDB::BitsVege | DDB::BitMoving | DDB::BitTwoSided, _dummy);
        }

        vkCmdEndRenderPass(cmd_buf);
    }

    { // static alpha-tested vegetation (depth and velocity)
        Ren::DebugMarker _m(ctx.current_cmd_buf(), "VEGE-ALPHA-SIMPLE");

        VkRenderPassBeginInfo rp_begin_info = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        rp_begin_info.renderArea = {0, 0, uint32_t(view_state_->act_res[0]), uint32_t(view_state_->act_res[1])};

        Ren::Pipeline *pipeline_onesided = nullptr, *pipeline_twosided = nullptr;
        if ((render_flags_ & EnableTaa) != 0) {
            // Write depth and velocity
            rp_begin_info.renderPass = rp_depth_velocity_.handle();
            rp_begin_info.framebuffer = depth_fill_vel_fb_[ctx.backend_frame()].handle();
            pipeline_onesided = &pi_vege_static_transp_vel_[0];
            pipeline_twosided = &pi_vege_static_transp_vel_[1];
        } else {
            // Write depth only
            rp_begin_info.renderPass = rp_depth_only_.handle();
            rp_begin_info.framebuffer = depth_fill_fb_[ctx.backend_frame()].handle();
            pipeline_onesided = &pi_vege_static_transp_[0];
            pipeline_twosided = &pi_vege_static_transp_[1];
        }
        vkCmdBeginRenderPass(cmd_buf, &rp_begin_info, VK_SUBPASS_CONTENTS_INLINE);

        vi_depth_pass_vege_transp_.BindBuffers(cmd_buf, 0, VK_INDEX_TYPE_UINT32);

        { // one-sided
            Ren::DebugMarker _mm(ctx.current_cmd_buf(), "ONE-SIDED");
            vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_onesided->handle());
            vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_onesided->layout(), 0, 2,
                                    vege_descr_sets, 0, nullptr);
            i = _depth_draw_range_ext(cmd_buf, *pipeline_onesided, zfill_batch_indices, zfill_batches, i,
                                      DDB::BitsVege | DDB::BitAlphaTest, materials_per_descriptor,
                                      *bindless_tex_->textures_descr_sets, _dummy);
        }

        { // two-sided
            Ren::DebugMarker _mm(ctx.current_cmd_buf(), "TWO-SIDED");
            vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_twosided->handle());
            vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_twosided->layout(), 0, 2,
                                    vege_descr_sets, 0, nullptr);
            i = _depth_draw_range_ext(cmd_buf, *pipeline_twosided, zfill_batch_indices, zfill_batches, i,
                                      DDB::BitsVege | DDB::BitAlphaTest | DDB::BitTwoSided, materials_per_descriptor,
                                      *bindless_tex_->textures_descr_sets, _dummy);
        }

        vkCmdEndRenderPass(cmd_buf);
    }

    { // moving alpha-tested vegetation (depth and velocity)
        Ren::DebugMarker _m(ctx.current_cmd_buf(), "VEGE-ALPHA-MOVING");

        VkRenderPassBeginInfo rp_begin_info = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        rp_begin_info.renderArea = {0, 0, uint32_t(view_state_->act_res[0]), uint32_t(view_state_->act_res[1])};

        Ren::Pipeline *pipeline_onesided = nullptr, *pipeline_twosided = nullptr;
        if ((render_flags_ & EnableTaa) != 0) {
            // Write depth and velocity
            rp_begin_info.renderPass = rp_depth_velocity_.handle();
            rp_begin_info.framebuffer = depth_fill_vel_fb_[ctx.backend_frame()].handle();
            pipeline_onesided = &pi_vege_moving_transp_vel_[0];
            pipeline_twosided = &pi_vege_moving_transp_vel_[1];
        } else {
            // Write depth only
            rp_begin_info.renderPass = rp_depth_only_.handle();
            rp_begin_info.framebuffer = depth_fill_fb_[ctx.backend_frame()].handle();
            pipeline_onesided = &pi_vege_static_transp_[0];
            pipeline_twosided = &pi_vege_static_transp_[1];
        }
        vkCmdBeginRenderPass(cmd_buf, &rp_begin_info, VK_SUBPASS_CONTENTS_INLINE);

        vi_depth_pass_vege_transp_.BindBuffers(cmd_buf, 0, VK_INDEX_TYPE_UINT32);

        { // one-sided
            Ren::DebugMarker _mm(ctx.current_cmd_buf(), "ONE-SIDED");
            vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_onesided->handle());
            vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_onesided->layout(), 0, 2,
                                    vege_descr_sets, 0, nullptr);
            i = _depth_draw_range_ext(cmd_buf, *pipeline_onesided, zfill_batch_indices, zfill_batches, i,
                                      DDB::BitsVege | DDB::BitAlphaTest | DDB::BitMoving, materials_per_descriptor,
                                      *bindless_tex_->textures_descr_sets, _dummy);
        }

        { // two-sided
            Ren::DebugMarker _mm(ctx.current_cmd_buf(), "TWO-SIDED");
            vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_twosided->handle());
            vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_twosided->layout(), 0, 2,
                                    vege_descr_sets, 0, nullptr);
            i = _depth_draw_range_ext(cmd_buf, *pipeline_twosided, zfill_batch_indices, zfill_batches, i,
                                      DDB::BitsVege | DDB::BitAlphaTest | DDB::BitMoving | DDB::BitTwoSided,
                                      materials_per_descriptor, *bindless_tex_->textures_descr_sets, _dummy);
        }

        vkCmdEndRenderPass(cmd_buf);
    }

    { // solid skinned meshes (depth and velocity)
        Ren::DebugMarker _m(ctx.current_cmd_buf(), "SKIN-SOLID-SIMPLE");

        VkRenderPassBeginInfo rp_begin_info = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        rp_begin_info.renderArea = {0, 0, uint32_t(view_state_->act_res[0]), uint32_t(view_state_->act_res[1])};

        Ren::Pipeline *pipeline_onesided = nullptr, *pipeline_twosided = nullptr;
        if ((render_flags_ & EnableTaa) != 0) {
            // Write depth and velocity
            rp_begin_info.renderPass = rp_depth_velocity_.handle();
            rp_begin_info.framebuffer = depth_fill_vel_fb_[ctx.backend_frame()].handle();
            pipeline_onesided = &pi_skin_static_solid_vel_[0];
            pipeline_twosided = &pi_skin_static_solid_vel_[1];

            vi_depth_pass_skin_solid_.BindBuffers(cmd_buf, 0, VK_INDEX_TYPE_UINT32);
        } else {
            // Write depth only
            rp_begin_info.renderPass = rp_depth_only_.handle();
            rp_begin_info.framebuffer = depth_fill_fb_[ctx.backend_frame()].handle();
            pipeline_onesided = &pi_skin_static_solid_[0];
            pipeline_twosided = &pi_skin_static_solid_[1];

            vi_depth_pass_solid_.BindBuffers(cmd_buf, 0, VK_INDEX_TYPE_UINT32);
        }
        vkCmdBeginRenderPass(cmd_buf, &rp_begin_info, VK_SUBPASS_CONTENTS_INLINE);

        { // one-sided
            Ren::DebugMarker _mm(ctx.current_cmd_buf(), "ONE-SIDED");
            vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_onesided->handle());
            vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_onesided->layout(), 0, 1,
                                    simple_descr_sets, 0, nullptr);
            i = _depth_draw_range(cmd_buf, *pipeline_onesided, zfill_batch_indices, zfill_batches, i, DDB::BitsSkinned,
                                  _dummy);
        }

        { // two-sided
            Ren::DebugMarker _mm(ctx.current_cmd_buf(), "TWO-SIDED");
            vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_twosided->handle());
            vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_twosided->layout(), 0, 1,
                                    simple_descr_sets, 0, nullptr);
            i = _depth_draw_range(cmd_buf, *pipeline_twosided, zfill_batch_indices, zfill_batches, i,
                                  DDB::BitsSkinned | DDB::BitTwoSided, _dummy);
        }

        vkCmdEndRenderPass(cmd_buf);
    }

    { // moving solid skinned (depth and velocity)
        Ren::DebugMarker _m(ctx.current_cmd_buf(), "SKIN-SOLID-MOVING");

        VkRenderPassBeginInfo rp_begin_info = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        rp_begin_info.renderArea = {0, 0, uint32_t(view_state_->act_res[0]), uint32_t(view_state_->act_res[1])};

        Ren::Pipeline *pipeline_onesided = nullptr, *pipeline_twosided = nullptr;
        if ((render_flags_ & EnableTaa) != 0) {
            // Write depth and velocity
            rp_begin_info.renderPass = rp_depth_velocity_.handle();
            rp_begin_info.framebuffer = depth_fill_vel_fb_[ctx.backend_frame()].handle();
            pipeline_onesided = &pi_skin_moving_solid_vel_[0];
            pipeline_twosided = &pi_skin_moving_solid_vel_[1];

            vi_depth_pass_skin_solid_.BindBuffers(cmd_buf, 0, VK_INDEX_TYPE_UINT32);
        } else {
            // Write depth only
            rp_begin_info.renderPass = rp_depth_only_.handle();
            rp_begin_info.framebuffer = depth_fill_fb_[ctx.backend_frame()].handle();
            pipeline_onesided = &pi_skin_static_solid_[0];
            pipeline_twosided = &pi_skin_static_solid_[1];

            vi_depth_pass_solid_.BindBuffers(cmd_buf, 0, VK_INDEX_TYPE_UINT32);
        }
        vkCmdBeginRenderPass(cmd_buf, &rp_begin_info, VK_SUBPASS_CONTENTS_INLINE);

        { // one-sided
            Ren::DebugMarker _mm(ctx.current_cmd_buf(), "ONE-SIDED");
            vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_onesided->handle());
            vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_onesided->layout(), 0, 1,
                                    simple_descr_sets, 0, nullptr);
            i = _depth_draw_range(cmd_buf, *pipeline_onesided, zfill_batch_indices, zfill_batches, i,
                                  DDB::BitsSkinned | DDB::BitMoving, _dummy);
        }

        { // two-sided
            Ren::DebugMarker _mm(ctx.current_cmd_buf(), "TWO-SIDED");
            vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_twosided->handle());
            vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_twosided->layout(), 0, 1,
                                    simple_descr_sets, 0, nullptr);
            i = _depth_draw_range(cmd_buf, *pipeline_twosided, zfill_batch_indices, zfill_batches, i,
                                  DDB::BitsSkinned | DDB::BitMoving | DDB::BitTwoSided, _dummy);
        }

        vkCmdEndRenderPass(cmd_buf);
    }

    { // static alpha-tested skinned (depth and velocity)
        Ren::DebugMarker _m(ctx.current_cmd_buf(), "SKIN-ALPHA-SIMPLE");

        VkRenderPassBeginInfo rp_begin_info = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        rp_begin_info.renderArea = {0, 0, uint32_t(view_state_->act_res[0]), uint32_t(view_state_->act_res[1])};

        Ren::Pipeline *pipeline_onesided = nullptr, *pipeline_twosided = nullptr;
        if ((render_flags_ & EnableTaa) != 0) {
            // Write depth and velocity
            rp_begin_info.renderPass = rp_depth_velocity_.handle();
            rp_begin_info.framebuffer = depth_fill_vel_fb_[ctx.backend_frame()].handle();
            pipeline_onesided = &pi_skin_static_transp_vel_[0];
            pipeline_twosided = &pi_skin_static_transp_vel_[1];

            vi_depth_pass_skin_solid_.BindBuffers(cmd_buf, 0, VK_INDEX_TYPE_UINT32);
        } else {
            // Write depth only
            rp_begin_info.renderPass = rp_depth_only_.handle();
            rp_begin_info.framebuffer = depth_fill_fb_[ctx.backend_frame()].handle();
            pipeline_onesided = &pi_skin_static_transp_[0];
            pipeline_twosided = &pi_skin_static_transp_[1];

            vi_depth_pass_solid_.BindBuffers(cmd_buf, 0, VK_INDEX_TYPE_UINT32);
        }
        vkCmdBeginRenderPass(cmd_buf, &rp_begin_info, VK_SUBPASS_CONTENTS_INLINE);

        { // one-sided
            Ren::DebugMarker _mm(ctx.current_cmd_buf(), "ONE-SIDED");
            vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_onesided->handle());
            vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_onesided->layout(), 0, 2,
                                    simple_descr_sets, 0, nullptr);
            i = _depth_draw_range_ext(cmd_buf, *pipeline_onesided, zfill_batch_indices, zfill_batches, i,
                                      DDB::BitsSkinned | DDB::BitAlphaTest, materials_per_descriptor,
                                      *bindless_tex_->textures_descr_sets, _dummy);
        }

        { // two-sided
            Ren::DebugMarker _mm(ctx.current_cmd_buf(), "TWO-SIDED");
            vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_twosided->handle());
            vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_twosided->layout(), 0, 2,
                                    simple_descr_sets, 0, nullptr);
            i = _depth_draw_range_ext(cmd_buf, *pipeline_twosided, zfill_batch_indices, zfill_batches, i,
                                      DDB::BitsSkinned | DDB::BitAlphaTest | DDB::BitTwoSided, materials_per_descriptor,
                                      *bindless_tex_->textures_descr_sets, _dummy);
        }

        vkCmdEndRenderPass(cmd_buf);
    }

    { // moving alpha-tested skinned (depth and velocity)
        Ren::DebugMarker _m(ctx.current_cmd_buf(), "SKIN-ALPHA-MOVING");

        VkRenderPassBeginInfo rp_begin_info = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        rp_begin_info.renderArea = {0, 0, uint32_t(view_state_->act_res[0]), uint32_t(view_state_->act_res[1])};

        Ren::Pipeline *pipeline_onesided = nullptr, *pipeline_twosided = nullptr;
        if ((render_flags_ & EnableTaa) != 0) {
            // Write depth and velocity
            rp_begin_info.renderPass = rp_depth_velocity_.handle();
            rp_begin_info.framebuffer = depth_fill_vel_fb_[ctx.backend_frame()].handle();
            pipeline_onesided = &pi_skin_moving_transp_vel_[0];
            pipeline_twosided = &pi_skin_moving_transp_vel_[1];

            vi_depth_pass_skin_solid_.BindBuffers(cmd_buf, 0, VK_INDEX_TYPE_UINT32);
        } else {
            // Write depth only
            rp_begin_info.renderPass = rp_depth_only_.handle();
            rp_begin_info.framebuffer = depth_fill_fb_[ctx.backend_frame()].handle();
            pipeline_onesided = &pi_skin_static_transp_[0];
            pipeline_twosided = &pi_skin_static_transp_[1];

            vi_depth_pass_solid_.BindBuffers(cmd_buf, 0, VK_INDEX_TYPE_UINT32);
        }
        vkCmdBeginRenderPass(cmd_buf, &rp_begin_info, VK_SUBPASS_CONTENTS_INLINE);

        { // one-sided
            Ren::DebugMarker _mm(ctx.current_cmd_buf(), "ONE-SIDED");
            vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_onesided->handle());
            vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_onesided->layout(), 0, 2,
                                    simple_descr_sets, 0, nullptr);
            i = _depth_draw_range_ext(cmd_buf, *pipeline_onesided, zfill_batch_indices, zfill_batches, i,
                                      DDB::BitsSkinned | DDB::BitAlphaTest | DDB::BitMoving, materials_per_descriptor,
                                      *bindless_tex_->textures_descr_sets, _dummy);
        }

        { // two-sided
            Ren::DebugMarker _mm(ctx.current_cmd_buf(), "TWO-SIDED");
            vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_twosided->handle());
            vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_twosided->layout(), 0, 2,
                                    simple_descr_sets, 0, nullptr);
            i = _depth_draw_range_ext(cmd_buf, *pipeline_twosided, zfill_batch_indices, zfill_batches, i,
                                      DDB::BitsSkinned | DDB::BitAlphaTest | DDB::BitMoving | DDB::BitTwoSided,
                                      materials_per_descriptor, *bindless_tex_->textures_descr_sets, _dummy);
        }

        vkCmdEndRenderPass(cmd_buf);
    }
}
