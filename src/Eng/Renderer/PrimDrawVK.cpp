#include "PrimDraw.h"

#include <Ren/Context.h>
#include <Ren/Framebuffer.h>
#include <Ren/ProbeStorage.h>
#include <Ren/VKCtx.h>

#include "Renderer_GL_Defines.inl"

PrimDraw::~PrimDraw() {}

void PrimDraw::Reset() {}

void PrimDraw::DrawPrim(const ePrim prim, const Ren::ProgramRef &p, const Ren::Framebuffer &fb,
                        const Ren::RenderPass &rp, const Ren::RastState &new_rast_state,
                        Ren::RastState &applied_rast_state, const Ren::Binding bindings[], const int bindings_count,
                        const void *uniform_data, const int uniform_data_len, const int uniform_data_offset) {
    Ren::ApiContext *api_ctx = ctx_->api_ctx();

    const Ren::Pipeline *pipeline = FindOrCreatePipeline(p, &rp, &new_rast_state, bindings, bindings_count);
    assert(pipeline);

    VkDescriptorSetLayout descr_set_layout = p->descr_set_layouts()[0];
    VkDescriptorSet descr_set = VK_NULL_HANDLE;

    { // allocate and update descriptor set
        VkDescriptorImageInfo img_infos[16];
        VkDescriptorBufferInfo ubuf_infos[16];
        Ren::DescrSizes descr_sizes;

        Ren::SmallVector<VkWriteDescriptorSet, 48> descr_writes;

        for (int i = 0; i < bindings_count; ++i) {
            const auto &b = bindings[i];

            if (b.trg == Ren::eBindTarget::Tex2D) {
                auto &info = img_infos[descr_sizes.img_sampler_count++];
                info.sampler = b.handle.tex->handle().sampler;
                if (Ren::IsDepthStencilFormat(b.handle.tex->params.format)) {
                    // use depth-only image view
                    info.imageView = b.handle.tex->handle().views[1];
                } else {
                    info.imageView = b.handle.tex->handle().views[0];
                }
                info.imageLayout = Ren::VKImageLayoutForState(b.handle.tex->resource_state);

                auto &new_write = descr_writes.emplace_back();
                new_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
                new_write.dstSet = {};
                new_write.dstBinding = b.loc;
                new_write.dstArrayElement = 0;
                new_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                new_write.descriptorCount = 1;
                new_write.pImageInfo = &info;
            } else if (b.trg == Ren::eBindTarget::UBuf) {
                auto &ubuf = ubuf_infos[descr_sizes.ubuf_count++];
                ubuf.buffer = b.handle.buf->vk_handle();
                ubuf.offset = b.offset;
                ubuf.range = b.offset ? b.size : VK_WHOLE_SIZE;

                auto &new_write = descr_writes.emplace_back();
                new_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
                new_write.dstSet = {};
                new_write.dstBinding = b.loc;
                new_write.dstArrayElement = 0;
                new_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                new_write.descriptorCount = 1;
                new_write.pBufferInfo = &ubuf;
            } else if (b.trg == Ren::eBindTarget::TBuf) {
                ++descr_sizes.tbuf_count;

                auto &new_write = descr_writes.emplace_back();
                new_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
                new_write.dstSet = {};
                new_write.dstBinding = b.loc;
                new_write.dstArrayElement = 0;
                new_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
                new_write.descriptorCount = 1;
                new_write.pTexelBufferView = &b.handle.tex_buf->view();
            } else if (b.trg == Ren::eBindTarget::TexCubeArray) {
                auto &info = img_infos[descr_sizes.img_sampler_count++];
                info.sampler = b.handle.cube_arr->handle().sampler;
                info.imageView = b.handle.cube_arr->handle().views[0];
                info.imageLayout = Ren::VKImageLayoutForState(b.handle.cube_arr->resource_state);

                auto &new_write = descr_writes.emplace_back();
                new_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
                new_write.dstSet = {};
                new_write.dstBinding = b.loc;
                new_write.dstArrayElement = 0;
                new_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                new_write.descriptorCount = 1;
                new_write.pImageInfo = &info;
            }
        }

        descr_set = ctx_->default_descr_alloc()->Alloc(descr_sizes, descr_set_layout);
        if (!descr_set) {
            ctx_->log()->Error("Failed to allocate descriptor set!");
            return;
        }

        for (auto &d : descr_writes) {
            d.dstSet = descr_set;
        }

        vkUpdateDescriptorSets(api_ctx->device, uint32_t(descr_writes.size()), descr_writes.data(), 0, nullptr);
    }

    VkCommandBuffer cmd_buf = api_ctx->draw_cmd_buf[api_ctx->backend_frame];

    {
        VkPipelineStageFlags src_stages = 0, dst_stages = 0;

        Ren::SmallVector<VkImageMemoryBarrier, 16> img_barriers;

        for (int i = 0; i < bindings_count; ++i) {
            const auto &b = bindings[i];

            if (b.trg == Ren::eBindTarget::Tex2D) {
                /*if (b.handle.tex->resource_state != Ren::eResState::ShaderResource) {
                    auto &new_barrier = img_barriers.emplace_back();
                    new_barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
                    new_barrier.srcAccessMask = Ren::VKAccessFlagsForState(b.handle.tex->resource_state);
                    new_barrier.dstAccessMask = Ren::VKAccessFlagsForState(Ren::eResState::ShaderResource);
                    new_barrier.oldLayout = Ren::VKImageLayoutForState(b.handle.tex->resource_state);
                    new_barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                    new_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    new_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    new_barrier.image = b.handle.tex->handle().img;
                    new_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                    new_barrier.subresourceRange.baseMipLevel = 0;
                    new_barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
                    new_barrier.subresourceRange.baseArrayLayer = 0;
                    new_barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

                    src_stages |= Ren::VKPipelineStagesForState(b.handle.tex->resource_state);
                    dst_stages |= Ren::VKPipelineStagesForState(Ren::eResState::ShaderResource);

                    b.handle.tex->resource_state = Ren::eResState::ShaderResource;
                }*/
                assert(b.handle.tex->resource_state == Ren::eResState::ShaderResource ||
                       b.handle.tex->resource_state == Ren::eResState::DepthRead ||
                       b.handle.tex->resource_state == Ren::eResState::StencilTestDepthFetch);
            }
        }

        for (const auto &att : fb.color_attachments) {
            if (att.ref->resource_state != Ren::eResState::RenderTarget) {
                auto &new_barrier = img_barriers.emplace_back();
                new_barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
                new_barrier.srcAccessMask = Ren::VKAccessFlagsForState(att.ref->resource_state);
                new_barrier.dstAccessMask = Ren::VKAccessFlagsForState(Ren::eResState::RenderTarget);
                new_barrier.oldLayout = Ren::VKImageLayoutForState(att.ref->resource_state);
                new_barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                new_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                new_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                new_barrier.image = att.ref->handle().img;
                new_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                new_barrier.subresourceRange.baseMipLevel = 0;
                new_barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
                new_barrier.subresourceRange.baseArrayLayer = 0;
                new_barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

                src_stages |= Ren::VKPipelineStagesForState(att.ref->resource_state);
                dst_stages |= Ren::VKPipelineStagesForState(Ren::eResState::RenderTarget);

                att.ref->resource_state = Ren::eResState::RenderTarget;
            }
        }

        if (fb.depth_attachment.ref) {
            if (fb.depth_attachment.ref->resource_state != Ren::eResState::DepthWrite &&
                fb.depth_attachment.ref->resource_state != Ren::eResState::DepthRead &&
                fb.depth_attachment.ref->resource_state != Ren::eResState::StencilTestDepthFetch) {
                auto &new_barrier = img_barriers.emplace_back();
                new_barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
                new_barrier.srcAccessMask = Ren::VKAccessFlagsForState(fb.depth_attachment.ref->resource_state);
                new_barrier.dstAccessMask = Ren::VKAccessFlagsForState(Ren::eResState::DepthWrite);
                new_barrier.oldLayout = Ren::VKImageLayoutForState(fb.depth_attachment.ref->resource_state);
                new_barrier.newLayout = Ren::VKImageLayoutForState(Ren::eResState::DepthWrite);
                new_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                new_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                new_barrier.image = fb.depth_attachment.ref->handle().img;
                new_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
                new_barrier.subresourceRange.baseMipLevel = 0;
                new_barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
                new_barrier.subresourceRange.baseArrayLayer = 0;
                new_barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

                src_stages |= Ren::VKPipelineStagesForState(fb.depth_attachment.ref->resource_state);
                dst_stages |= Ren::VKPipelineStagesForState(Ren::eResState::DepthWrite);

                fb.depth_attachment.ref->resource_state = Ren::eResState::DepthWrite;
            }
        }

        if (!img_barriers.empty()) {
            vkCmdPipelineBarrier(cmd_buf, src_stages ? src_stages : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, dst_stages, 0, 0,
                                 nullptr, 0, nullptr, uint32_t(img_barriers.size()), img_barriers.data());
        }
    }

    VkRenderPassBeginInfo render_pass_begin_info = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    render_pass_begin_info.renderPass = rp.handle();
    render_pass_begin_info.framebuffer = fb.handle();
    render_pass_begin_info.renderArea = {0, 0, uint32_t(fb.w), uint32_t(fb.h)};

    vkCmdBeginRenderPass(cmd_buf, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->handle());

    const VkViewport viewport = {0.0f, 0.0f, float(fb.w), float(fb.h), 0.0f, 1.0f};
    vkCmdSetViewport(cmd_buf, 0, 1, &viewport);

    const VkRect2D scissor = {0, 0, uint32_t(fb.w), uint32_t(fb.h)};
    vkCmdSetScissor(cmd_buf, 0, 1, &scissor);

    vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->layout(), 0, 1, &descr_set, 0, nullptr);

    vkCmdPushConstants(cmd_buf, pipeline->layout(), pipeline->prog()->pc_ranges()->stageFlags, uniform_data_offset,
                       uniform_data_len, uniform_data);

    if (prim == ePrim::Quad) {
        // const VkDeviceSize offset = {quad_vtx1_offset_};
        // vkCmdBindVertexBuffers(cmd_buf, 0, 1, &vtx_buf, &offset);
        // vkCmdBindIndexBuffer(cmd_buf, ndx_buf, VkDeviceSize(quad_ndx_offset_), VK_INDEX_TYPE_UINT16);

        pipeline->vtx_input()->BindBuffers(cmd_buf, quad_ndx_offset_, VK_INDEX_TYPE_UINT16);

        vkCmdDrawIndexed(cmd_buf, uint32_t(6), // index count
                         1,                    // instance count
                         0,                    // first index
                         0,                    // vertex offset
                         0);                   // first instance
    }

    vkCmdEndRenderPass(cmd_buf);
}

const Ren::Pipeline *PrimDraw::FindOrCreatePipeline(const Ren::ProgramRef p, const Ren::RenderPass *rp,
                                                    const Ren::RastState *rs, const Ren::Binding bindings[],
                                                    const int bindings_count) {
    for (size_t i = 0; i < pipelines_.size(); ++i) {
        if (pipelines_[i].prog() == p && pipelines_[i].render_pass() == rp && pipelines_[i].rast_state() == *rs) {
            return &pipelines_[i];
        }
    }

    Ren::ApiContext *api_ctx = ctx_->api_ctx();
    Ren::Pipeline &new_pipeline = pipelines_.emplace_back();

    if (!new_pipeline.Init(api_ctx, *rs, std::move(p), &fs_quad_vtx_input_, rp, ctx_->log())) {
        log_->Error("Failed to initialize pipeline!");
        pipelines_.pop_back();
        return nullptr;
    }

    return &new_pipeline;
}
