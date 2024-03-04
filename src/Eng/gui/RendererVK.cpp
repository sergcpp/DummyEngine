#include "Renderer.h"

#include "Utils.h"

#include <cassert>
#include <fstream>

#include <Ren/Context.h>
#include <Ren/DebugMarker.h>
#include <Ren/DescriptorPool.h>
#include <Ren/VKCtx.h>
#include <Sys/Json.h>

namespace UIRendererConstants {
const int MaxVerticesPerRange = 64 * 1024;
const int MaxIndicesPerRange = 128 * 1024;

extern const int TexAtlasSlot;
} // namespace UIRendererConstants

Gui::Renderer::Renderer(Ren::Context &ctx) : ctx_(ctx) {
    instance_index_ = g_instance_count++;

    Ren::ApiContext *api_ctx = ctx_.api_ctx();

#if !defined(NDEBUG) && defined(USE_VK_RENDER)
    for (int i = 0; i < Ren::MaxFramesInFlight; ++i) {
        VkFenceCreateInfo fence_info = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        VkFence new_fence;
        const VkResult res = api_ctx->vkCreateFence(api_ctx->device, &fence_info, nullptr, &new_fence);
        if (res != VK_SUCCESS) {
            ctx_.log()->Error("Failed to create fence!");
        }

        buf_range_fences_[i] = Ren::SyncFence{api_ctx, new_fence};
    }
#endif
}

Gui::Renderer::~Renderer() {
    Ren::ApiContext *api_ctx = ctx_.api_ctx();

    api_ctx->vkDeviceWaitIdle(api_ctx->device);

    vertex_stage_buf_->Unmap();
    index_stage_buf_->Unmap();
}

void Gui::Renderer::Draw(const int w, const int h) {
    using namespace UIRendererConstants;

    Ren::ApiContext *api_ctx = ctx_.api_ctx();
    VkCommandBuffer cmd_buf = api_ctx->draw_cmd_buf[api_ctx->backend_frame];

    if (!ndx_count_[api_ctx->backend_frame]) {
        // nothing to draw
        return;
    }

    Ren::DebugMarker _(api_ctx, cmd_buf, name_);

    //
    // Update buffers
    //
    const uint32_t vtx_data_offset = uint32_t(api_ctx->backend_frame) * MaxVerticesPerRange * sizeof(vertex_t);
    const uint32_t vtx_data_size = uint32_t(vtx_count_[api_ctx->backend_frame]) * sizeof(vertex_t);
    vertex_stage_buf_->FlushMappedRange(vtx_data_offset, vertex_stage_buf_->AlignMapOffset(vtx_data_size));

    const uint32_t ndx_data_offset = uint32_t(api_ctx->backend_frame) * MaxIndicesPerRange * sizeof(uint16_t);
    const uint32_t ndx_data_size = uint32_t(ndx_count_[api_ctx->backend_frame]) * sizeof(uint16_t);
    index_stage_buf_->FlushMappedRange(ndx_data_offset, index_stage_buf_->AlignMapOffset(ndx_data_size));

    { // insert needed barriers before copying
        // NOTE: stage buffer barriers are not needed as we know all operations for used region have finished
        vertex_stage_buf_->resource_state = Ren::eResState::CopySrc;
        index_stage_buf_->resource_state = Ren::eResState::CopySrc;

        VkPipelineStageFlags src_stages = 0, dst_stages = 0;
        Ren::SmallVector<VkBufferMemoryBarrier, 4> buf_barriers;

        if (vertex_buf_->resource_state != Ren::eResState::Undefined &&
            vertex_buf_->resource_state != Ren::eResState::CopyDst) {
            auto &new_barrier = buf_barriers.emplace_back();
            new_barrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
            new_barrier.srcAccessMask = Ren::VKAccessFlagsForState(vertex_buf_->resource_state);
            new_barrier.dstAccessMask = Ren::VKAccessFlagsForState(Ren::eResState::CopyDst);
            new_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            new_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            new_barrier.buffer = vertex_buf_->vk_handle();
            new_barrier.offset = 0;
            new_barrier.size = VK_WHOLE_SIZE;

            src_stages |= Ren::VKPipelineStagesForState(vertex_buf_->resource_state);
            dst_stages |= Ren::VKPipelineStagesForState(Ren::eResState::CopyDst);
        }

        if (index_buf_->resource_state != Ren::eResState::Undefined &&
            index_buf_->resource_state != Ren::eResState::CopyDst) {
            auto &new_barrier = buf_barriers.emplace_back();
            new_barrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
            new_barrier.srcAccessMask = Ren::VKAccessFlagsForState(index_buf_->resource_state);
            new_barrier.dstAccessMask = Ren::VKAccessFlagsForState(Ren::eResState::CopyDst);
            new_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            new_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            new_barrier.buffer = index_buf_->vk_handle();
            new_barrier.offset = 0;
            new_barrier.size = VK_WHOLE_SIZE;

            src_stages |= Ren::VKPipelineStagesForState(index_buf_->resource_state);
            dst_stages |= Ren::VKPipelineStagesForState(Ren::eResState::CopyDst);
        }

        src_stages &= ctx_.api_ctx()->supported_stages_mask;
        dst_stages &= ctx_.api_ctx()->supported_stages_mask;

        if (!buf_barriers.empty()) {
            ctx_.api_ctx()->vkCmdPipelineBarrier(cmd_buf, src_stages, dst_stages, 0, 0, nullptr,
                                                 uint32_t(buf_barriers.size()), buf_barriers.cdata(), 0, nullptr);
        }

        vertex_buf_->resource_state = Ren::eResState::CopyDst;
        index_buf_->resource_state = Ren::eResState::CopyDst;
    }

    { // copy vertex data
        assert(vertex_stage_buf_->resource_state == Ren::eResState::CopySrc);
        assert(vertex_buf_->resource_state == Ren::eResState::CopyDst);

        VkBufferCopy region_to_copy = {};
        region_to_copy.srcOffset = vtx_data_offset;
        region_to_copy.dstOffset = 0;
        region_to_copy.size = vtx_data_size;

        api_ctx->vkCmdCopyBuffer(cmd_buf, vertex_stage_buf_->vk_handle(), vertex_buf_->vk_handle(), 1, &region_to_copy);
    }

    { // copy index data
        assert(index_stage_buf_->resource_state == Ren::eResState::CopySrc);
        assert(index_buf_->resource_state == Ren::eResState::CopyDst);

        VkBufferCopy region_to_copy = {};
        region_to_copy.srcOffset = ndx_data_offset;
        region_to_copy.dstOffset = 0;
        region_to_copy.size = ndx_data_size;

        api_ctx->vkCmdCopyBuffer(cmd_buf, index_stage_buf_->vk_handle(), index_buf_->vk_handle(), 1, &region_to_copy);
    }

    auto &atlas = ctx_.texture_atlas();

    //
    // Insert needed barriers before drawing
    //

    VkPipelineStageFlags src_stages = 0, dst_stages = 0;
    Ren::SmallVector<VkBufferMemoryBarrier, 4> buf_barriers;
    Ren::SmallVector<VkImageMemoryBarrier, 4> img_barriers;

    { // vertex buffer barrier [CopyDst -> VertexBuffer]
        auto &new_barrier = buf_barriers.emplace_back();
        new_barrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
        new_barrier.srcAccessMask = Ren::VKAccessFlagsForState(Ren::eResState::CopyDst);
        new_barrier.dstAccessMask = Ren::VKAccessFlagsForState(Ren::eResState::VertexBuffer);
        new_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        new_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        new_barrier.buffer = vertex_buf_->vk_handle();
        new_barrier.offset = 0;
        new_barrier.size = vtx_data_size;

        src_stages |= Ren::VKPipelineStagesForState(Ren::eResState::CopyDst);
        dst_stages |= Ren::VKPipelineStagesForState(Ren::eResState::VertexBuffer);
    }

    { // index buffer barrier [CopyDst -> IndexBuffer]
        auto &new_barrier = buf_barriers.emplace_back();
        new_barrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
        new_barrier.srcAccessMask = Ren::VKAccessFlagsForState(Ren::eResState::CopyDst);
        new_barrier.dstAccessMask = Ren::VKAccessFlagsForState(Ren::eResState::IndexBuffer);
        new_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        new_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        new_barrier.buffer = index_buf_->vk_handle();
        new_barrier.offset = 0;
        new_barrier.size = ndx_data_size;

        src_stages |= Ren::VKPipelineStagesForState(Ren::eResState::CopyDst);
        dst_stages |= Ren::VKPipelineStagesForState(Ren::eResState::IndexBuffer);
    }

    if (atlas.resource_state != Ren::eResState::ShaderResource) {
        auto &new_barrier = img_barriers.emplace_back();
        new_barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        new_barrier.srcAccessMask = Ren::VKAccessFlagsForState(atlas.resource_state);
        new_barrier.dstAccessMask = Ren::VKAccessFlagsForState(Ren::eResState::ShaderResource);
        new_barrier.oldLayout = VkImageLayout(Ren::VKImageLayoutForState(atlas.resource_state));
        new_barrier.newLayout = VkImageLayout(Ren::VKImageLayoutForState(Ren::eResState::ShaderResource));
        new_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        new_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        new_barrier.image = atlas.img();
        new_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        new_barrier.subresourceRange.baseMipLevel = 0;
        new_barrier.subresourceRange.levelCount = atlas.mip_count();
        new_barrier.subresourceRange.baseArrayLayer = 0;
        new_barrier.subresourceRange.layerCount = atlas.layer_count();

        src_stages |= Ren::VKPipelineStagesForState(atlas.resource_state);
        dst_stages |= Ren::VKPipelineStagesForState(Ren::eResState::ShaderResource);
    }

    src_stages &= ctx_.api_ctx()->supported_stages_mask;
    dst_stages &= ctx_.api_ctx()->supported_stages_mask;

    if (!buf_barriers.empty() || !img_barriers.empty()) {
        ctx_.api_ctx()->vkCmdPipelineBarrier(cmd_buf, src_stages, dst_stages, 0, 0, nullptr,
                                             uint32_t(buf_barriers.size()), buf_barriers.cdata(),
                                             uint32_t(img_barriers.size()), img_barriers.cdata());

        vertex_buf_->resource_state = Ren::eResState::VertexBuffer;
        index_buf_->resource_state = Ren::eResState::IndexBuffer;

        atlas.resource_state = Ren::eResState::ShaderResource;
    }

    //
    // (Re)create framebuffer
    //
    framebuffers_.resize(api_ctx->present_images.size());
    if (!framebuffers_[api_ctx->active_present_image].Setup(api_ctx, render_pass_, w, h, ctx_.backbuffer_ref(), {},
                                                            Ren::WeakTex2DRef{}, false, ctx_.log())) {
        ctx_.log()->Error("Failed to create framebuffer!");
    }

    //
    // Update descriptor set
    //

    VkDescriptorSetLayout descr_set_layout = pipeline_.prog()->descr_set_layouts()[0];
    Ren::DescrSizes descr_sizes;
    descr_sizes.img_sampler_count = 1;
    VkDescriptorSet descr_set = ctx_.default_descr_alloc()->Alloc(descr_sizes, descr_set_layout);

    VkDescriptorImageInfo img_info = {};
    img_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    img_info.imageView = ctx_.texture_atlas().img_view();
    img_info.sampler = ctx_.texture_atlas().sampler().vk_handle();

    VkWriteDescriptorSet descr_write;
    descr_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    descr_write.dstSet = descr_set;
    descr_write.dstBinding = UIRendererConstants::TexAtlasSlot;
    descr_write.dstArrayElement = 0;
    descr_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descr_write.descriptorCount = 1;
    descr_write.pBufferInfo = nullptr;
    descr_write.pImageInfo = &img_info;
    descr_write.pTexelBufferView = nullptr;
    descr_write.pNext = nullptr;

    api_ctx->vkUpdateDescriptorSets(api_ctx->device, 1, &descr_write, 0, nullptr);

    //
    // Submit draw call
    //

    assert(vertex_buf_->resource_state == Ren::eResState::VertexBuffer);
    assert(index_buf_->resource_state == Ren::eResState::IndexBuffer);

    VkRenderPassBeginInfo render_pass_begin_info = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    render_pass_begin_info.renderPass = render_pass_.handle();
    render_pass_begin_info.framebuffer = framebuffers_[api_ctx->active_present_image].handle();
    render_pass_begin_info.renderArea = {0, 0, uint32_t(w), uint32_t(h)};

    api_ctx->vkCmdBeginRenderPass(cmd_buf, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

    api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_.handle());

    const VkViewport viewport = {0.0f, 0.0f, float(w), float(h), 0.0f, 1.0f};
    api_ctx->vkCmdSetViewport(cmd_buf, 0, 1, &viewport);

    const VkRect2D scissor = {0, 0, uint32_t(w), uint32_t(h)};
    api_ctx->vkCmdSetScissor(cmd_buf, 0, 1, &scissor);

    api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_.layout(), 0, 1, &descr_set, 0,
                                     nullptr);

    VkBuffer vtx_buf = vertex_buf_->vk_handle();

    VkDeviceSize offset = {};
    api_ctx->vkCmdBindVertexBuffers(cmd_buf, 0, 1, &vtx_buf, &offset);
    api_ctx->vkCmdBindIndexBuffer(cmd_buf, index_buf_->vk_handle(), 0, VK_INDEX_TYPE_UINT16);

    api_ctx->vkCmdDrawIndexed(cmd_buf,
                              ndx_count_[api_ctx->backend_frame], // index count
                              1,                                  // instance count
                              0,                                  // first index
                              0,                                  // vertex offset
                              0);                                 // first instance

    api_ctx->vkCmdEndRenderPass(cmd_buf);

    vtx_count_[api_ctx->backend_frame] = 0;
    ndx_count_[api_ctx->backend_frame] = 0;
}
