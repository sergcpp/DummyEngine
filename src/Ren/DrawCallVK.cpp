#include "DrawCall.h"

#include "ProbeStorage.h"

VkDescriptorSet Ren::PrepareDescriptorSet(ApiContext *api_ctx, VkDescriptorSetLayout layout,
                                          Span<const Binding> bindings, DescrMultiPoolAlloc *descr_alloc, ILog *log) {
    VkDescriptorImageInfo img_sampler_infos[16];
    VkDescriptorImageInfo img_storage_infos[16];
    VkDescriptorBufferInfo ubuf_infos[16];
    VkDescriptorBufferInfo sbuf_infos[16];
    VkWriteDescriptorSetAccelerationStructureKHR desc_tlas_infos[16];
    DescrSizes descr_sizes;

    SmallVector<VkWriteDescriptorSet, 48> descr_writes;

    for (const auto &b : bindings) {
        if (b.trg == eBindTarget::Tex2D) {
            auto &info = img_sampler_infos[descr_sizes.img_sampler_count++];
            info.sampler = b.handle.tex->handle().sampler;
            if (IsDepthStencilFormat(b.handle.tex->params.format)) {
                // use depth-only image view
                info.imageView = b.handle.tex->handle().views[1];
            } else {
                info.imageView = b.handle.tex->handle().views[0];
            }
            info.imageLayout = VKImageLayoutForState(b.handle.tex->resource_state);

            auto &new_write = descr_writes.emplace_back();
            new_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            new_write.dstBinding = b.loc;
            new_write.dstArrayElement = b.offset;
            new_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            new_write.descriptorCount = 1;
            new_write.pImageInfo = &info;
        } else if (b.trg == eBindTarget::UBuf) {
            auto &ubuf = ubuf_infos[descr_sizes.ubuf_count++];
            ubuf.buffer = b.handle.buf->vk_handle();
            ubuf.offset = b.offset;
            ubuf.range = b.offset ? b.size : VK_WHOLE_SIZE;

            auto &new_write = descr_writes.emplace_back();
            new_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            new_write.dstBinding = b.loc;
            new_write.dstArrayElement = 0;
            new_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            new_write.descriptorCount = 1;
            new_write.pBufferInfo = &ubuf;
        } else if (b.trg == eBindTarget::TBuf) {
            ++descr_sizes.tbuf_count;

            auto &new_write = descr_writes.emplace_back();
            new_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            new_write.dstBinding = b.loc;
            new_write.dstArrayElement = 0;
            new_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
            new_write.descriptorCount = 1;
            new_write.pTexelBufferView = &b.handle.tex_buf->view();
        } else if (b.trg == eBindTarget::SBuf) {
            auto &sbuf = sbuf_infos[descr_sizes.sbuf_count++];
            sbuf.buffer = b.handle.buf->vk_handle();
            sbuf.offset = b.offset;
            sbuf.range = b.offset ? b.size : VK_WHOLE_SIZE;

            auto &new_write = descr_writes.emplace_back();
            new_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            new_write.dstBinding = b.loc;
            new_write.dstArrayElement = 0;
            new_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            new_write.descriptorCount = 1;
            new_write.pBufferInfo = &sbuf;
        } else if (b.trg == eBindTarget::TexCubeArray) {
            auto &info = img_sampler_infos[descr_sizes.img_sampler_count++];
            info.sampler = b.handle.cube_arr->handle().sampler;
            info.imageView = b.handle.cube_arr->handle().views[0];
            info.imageLayout = VKImageLayoutForState(b.handle.cube_arr->resource_state);

            auto &new_write = descr_writes.emplace_back();
            new_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            new_write.dstBinding = b.loc;
            new_write.dstArrayElement = 0;
            new_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            new_write.descriptorCount = 1;
            new_write.pImageInfo = &info;
        } else if (b.trg == eBindTarget::Image) {
            auto &info = img_storage_infos[descr_sizes.store_img_count++];
            info.sampler = b.handle.tex->handle().sampler;
            if (IsDepthStencilFormat(b.handle.tex->params.format)) {
                // use depth-only image view
                info.imageView = b.handle.tex->handle().views[1];
            } else {
                info.imageView = b.handle.tex->handle().views[0];
            }
            info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

            auto &new_write = descr_writes.emplace_back();
            new_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            new_write.dstBinding = b.loc;
            new_write.dstArrayElement = 0;
            new_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            new_write.descriptorCount = 1;
            new_write.pImageInfo = &info;
        } else if (b.trg == eBindTarget::AccStruct) {
            auto &info = desc_tlas_infos[descr_sizes.acc_count++];
            info = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR};
            info.pAccelerationStructures = &b.handle.acc_struct->vk_handle();
            info.accelerationStructureCount = 1;

            auto &new_write = descr_writes.emplace_back();
            new_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            new_write.dstBinding = b.loc;
            new_write.dstArrayElement = 0;
            new_write.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
            new_write.descriptorCount = 1;
            new_write.pNext = &info;
        }
    }

    VkDescriptorSet descr_set = descr_alloc->Alloc(descr_sizes, layout);
    if (!descr_set) {
        log->Error("Failed to allocate descriptor set!");
        return VK_NULL_HANDLE;
    }

    for (auto &d : descr_writes) {
        d.dstSet = descr_set;
    }

    vkUpdateDescriptorSets(api_ctx->device, uint32_t(descr_writes.size()), descr_writes.data(), 0, nullptr);

    return descr_set;
}

void Ren::DispatchCompute(const Pipeline &comp_pipeline, Vec3u grp_count, Span<const Binding> bindings,
                          const void *uniform_data, int uniform_data_len, DescrMultiPoolAlloc *descr_alloc, ILog *log) {
    ApiContext *api_ctx = descr_alloc->api_ctx();

    VkDescriptorSet descr_set =
        PrepareDescriptorSet(api_ctx, comp_pipeline.prog()->descr_set_layouts()[0], bindings, descr_alloc, log);
    if (!descr_set) {
        log->Error("Failed to allocate descriptor set, skipping draw call!");
        return;
    }

    VkCommandBuffer cmd_buf = api_ctx->draw_cmd_buf[api_ctx->backend_frame];

    vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, comp_pipeline.handle());
    vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, comp_pipeline.layout(), 0, 1, &descr_set, 0,
                            nullptr);

    if (uniform_data) {
        vkCmdPushConstants(cmd_buf, comp_pipeline.layout(), VK_SHADER_STAGE_COMPUTE_BIT, 0, uniform_data_len,
                           uniform_data);
    }

    vkCmdDispatch(cmd_buf, grp_count[0], grp_count[1], grp_count[2]);
}

void Ren::DispatchComputeIndirect(const Pipeline &comp_pipeline, const Buffer &indir_buf,
                                  const uint32_t indir_buf_offset, Span<const Binding> bindings,
                                  const void *uniform_data, int uniform_data_len, DescrMultiPoolAlloc *descr_alloc,
                                  ILog *log) {
    ApiContext *api_ctx = descr_alloc->api_ctx();

    VkDescriptorSet descr_set =
        PrepareDescriptorSet(api_ctx, comp_pipeline.prog()->descr_set_layouts()[0], bindings, descr_alloc, log);
    if (!descr_set) {
        log->Error("Failed to allocate descriptor set, skipping draw call!");
        return;
    }

    VkCommandBuffer cmd_buf = api_ctx->draw_cmd_buf[api_ctx->backend_frame];

    vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, comp_pipeline.handle());
    vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, comp_pipeline.layout(), 0, 1, &descr_set, 0,
                            nullptr);

    if (uniform_data) {
        vkCmdPushConstants(cmd_buf, comp_pipeline.layout(), VK_SHADER_STAGE_COMPUTE_BIT, 0, uniform_data_len,
                           uniform_data);
    }

    vkCmdDispatchIndirect(cmd_buf, indir_buf.vk_handle(), VkDeviceSize(indir_buf_offset));
}
