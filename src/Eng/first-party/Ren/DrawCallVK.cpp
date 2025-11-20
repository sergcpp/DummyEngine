#include "DrawCall.h"

#include "AccStructure.h"
#include "Bindless.h"
#include "DescriptorPool.h"
#include "Log.h"
#include "Pipeline.h"
#include "ProbeStorage.h"
#include "Sampler.h"
#include "VKCtx.h"

VkDescriptorSet Ren::PrepareDescriptorSet(ApiContext *api_ctx, VkDescriptorSetLayout layout,
                                          Span<const Binding> bindings, DescrMultiPoolAlloc &descr_alloc, ILog *log) {
    VkDescriptorImageInfo sampler_infos[16] = {};
    VkDescriptorImageInfo img_sampler_infos[24];
    VkDescriptorImageInfo img_storage_infos[24];
    VkDescriptorBufferInfo ubuf_infos[24];
    VkDescriptorBufferInfo sbuf_infos[16];
    VkWriteDescriptorSetAccelerationStructureKHR desc_tlas_infos[16];
    DescrSizes descr_sizes;

    SmallVector<VkWriteDescriptorSet, 48> descr_writes;
    uint64_t used_bindings = 0;

    for (const auto &b : bindings) {
        if (b.trg == eBindTarget::Tex || b.trg == eBindTarget::TexSampled) {
            auto &info = img_sampler_infos[descr_sizes.img_sampler_count++];
            if (b.trg == eBindTarget::TexSampled) {
                if (b.handle.sampler) {
                    info.sampler = b.handle.sampler->vk_handle();
                } else {
                    info.sampler = b.handle.img->handle().sampler;
                }
            }
            info.imageView = b.handle.img->handle().views[b.handle.view_index];
            info.imageLayout = VkImageLayout(VKImageLayoutForState(b.handle.img->resource_state));

            auto &new_write = descr_writes.emplace_back();
            new_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            new_write.dstBinding = b.loc;
            new_write.dstArrayElement = b.offset;
            new_write.descriptorType = (b.trg == eBindTarget::TexSampled) ? VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
                                                                          : VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            new_write.descriptorCount = 1;
            new_write.pImageInfo = &info;

            assert((used_bindings & (1ull << (b.loc + b.offset))) == 0 && "Bindings overlap detected!");
            used_bindings |= (1ull << (b.loc + b.offset));
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

            assert((used_bindings & (1ull << b.loc)) == 0 && "Bindings overlap detected!");
            used_bindings |= (1ull << b.loc);
        } else if (b.trg == eBindTarget::UTBuf) {
            ++descr_sizes.utbuf_count;

            auto &new_write = descr_writes.emplace_back();
            new_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            new_write.dstBinding = b.loc;
            new_write.dstArrayElement = 0;
            new_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
            new_write.descriptorCount = 1;
            new_write.pTexelBufferView = &b.handle.buf->view(b.handle.view_index).second;

            assert((used_bindings & (1ull << b.loc)) == 0 && "Bindings overlap detected!");
            used_bindings |= (1ull << b.loc);
        } else if (b.trg == eBindTarget::SBufRO || b.trg == eBindTarget::SBufRW) {
            auto &sbuf = sbuf_infos[descr_sizes.sbuf_count++];
            sbuf.buffer = b.handle.buf->vk_handle();
            sbuf.offset = b.offset;
            sbuf.range = (b.offset || b.size) ? (b.size ? b.size : (b.handle.buf->size() - b.offset)) : VK_WHOLE_SIZE;

            auto &new_write = descr_writes.emplace_back();
            new_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            new_write.dstBinding = b.loc;
            new_write.dstArrayElement = 0;
            new_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            new_write.descriptorCount = 1;
            new_write.pBufferInfo = &sbuf;

            assert((used_bindings & (1ull << b.loc)) == 0 && "Bindings overlap detected!");
            used_bindings |= (1ull << b.loc);
        } else if (b.trg == eBindTarget::STBufRO || b.trg == eBindTarget::STBufRW) {
            ++descr_sizes.stbuf_count;

            auto &new_write = descr_writes.emplace_back();
            new_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            new_write.dstBinding = b.loc;
            new_write.dstArrayElement = 0;
            new_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
            new_write.descriptorCount = 1;
            new_write.pTexelBufferView = &b.handle.buf->view(b.handle.view_index).second;

            assert((used_bindings & (1ull << b.loc)) == 0 && "Bindings overlap detected!");
            used_bindings |= (1ull << b.loc);
        } else if (b.trg == eBindTarget::ImageRO || b.trg == eBindTarget::ImageRW) {
            auto &info = img_storage_infos[descr_sizes.store_img_count++];
            info.sampler = b.handle.img->handle().sampler;
            info.imageView = b.handle.img->handle().views[b.handle.view_index];
            info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

            auto &new_write = descr_writes.emplace_back();
            new_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            new_write.dstBinding = b.loc;
            new_write.dstArrayElement = b.offset;
            new_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            new_write.descriptorCount = b.size ? b.size : 1;
            new_write.pImageInfo = &info;

            assert((used_bindings & (1ull << (b.loc + b.offset))) == 0 && "Bindings overlap detected!");
            used_bindings |= (1ull << (b.loc + b.offset));
        } else if (b.trg == eBindTarget::Sampler) {
            auto &info = sampler_infos[descr_sizes.sampler_count++];
            info.sampler = b.handle.sampler->vk_handle();

            auto &new_write = descr_writes.emplace_back();
            new_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            new_write.dstBinding = b.loc;
            new_write.dstArrayElement = b.offset;
            new_write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
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

            assert((used_bindings & (1ull << b.loc)) == 0 && "Bindings overlap detected!");
            used_bindings |= (1ull << b.loc);
        }
    }

    VkDescriptorSet descr_set = descr_alloc.Alloc(descr_sizes, layout);
    if (!descr_set) {
        log->Error("Failed to allocate descriptor set!");
        return VK_NULL_HANDLE;
    }

    for (auto &d : descr_writes) {
        d.dstSet = descr_set;
    }

    api_ctx->vkUpdateDescriptorSets(api_ctx->device, descr_writes.size(), descr_writes.data(), 0, nullptr);

    return descr_set;
}

void Ren::DispatchCompute(CommandBuffer cmd_buf, const Pipeline &comp_pipeline, const Vec3u grp_count,
                          Span<const Binding> bindings, const void *uniform_data, const int uniform_data_len,
                          DescrMultiPoolAlloc &descr_alloc, ILog *log) {
    ApiContext *api_ctx = descr_alloc.api_ctx();

    SmallVector<VkDescriptorSet, 2> descr_sets;
    descr_sets.push_back(
        PrepareDescriptorSet(api_ctx, comp_pipeline.prog()->descr_set_layouts()[0], bindings, descr_alloc, log));
    if (!descr_sets.back()) {
        log->Error("Failed to allocate descriptor set, skipping draw call!");
        return;
    }
    for (const Binding &b : bindings) {
        if (b.trg == eBindTarget::BindlessDescriptors) {
            descr_sets.push_back(b.handle.bindless->descr_set);
        }
    }

    api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, comp_pipeline.handle());
    api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, comp_pipeline.layout(), 0,
                                     descr_sets.size(), descr_sets.data(), 0, nullptr);

    if (uniform_data) {
        api_ctx->vkCmdPushConstants(cmd_buf, comp_pipeline.layout(), VK_SHADER_STAGE_COMPUTE_BIT, 0, uniform_data_len,
                                    uniform_data);
    }

    api_ctx->vkCmdDispatch(cmd_buf, grp_count[0], grp_count[1], grp_count[2]);
}

void Ren::DispatchCompute(const Pipeline &comp_pipeline, const Vec3u grp_count, Span<const Binding> bindings,
                          const void *uniform_data, const int uniform_data_len, DescrMultiPoolAlloc &descr_alloc,
                          ILog *log) {
    ApiContext *api_ctx = descr_alloc.api_ctx();
    VkCommandBuffer cmd_buf = api_ctx->draw_cmd_buf[api_ctx->backend_frame];
    DispatchCompute(cmd_buf, comp_pipeline, grp_count, bindings, uniform_data, uniform_data_len, descr_alloc, log);
}

void Ren::DispatchComputeIndirect(CommandBuffer cmd_buf, const Pipeline &comp_pipeline, const Buffer &indir_buf,
                                  const uint32_t indir_buf_offset, Span<const Binding> bindings,
                                  const void *uniform_data, int uniform_data_len, DescrMultiPoolAlloc &descr_alloc,
                                  ILog *log) {
    ApiContext *api_ctx = descr_alloc.api_ctx();

    SmallVector<VkDescriptorSet, 2> descr_sets;
    descr_sets.push_back(
        PrepareDescriptorSet(api_ctx, comp_pipeline.prog()->descr_set_layouts()[0], bindings, descr_alloc, log));
    if (!descr_sets.back()) {
        log->Error("Failed to allocate descriptor set, skipping draw call!");
        return;
    }
    for (const Binding &b : bindings) {
        if (b.trg == eBindTarget::BindlessDescriptors) {
            descr_sets.push_back(b.handle.bindless->descr_set);
        }
    }

    api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, comp_pipeline.handle());
    api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, comp_pipeline.layout(), 0,
                                     descr_sets.size(), descr_sets.data(), 0, nullptr);

    if (uniform_data) {
        api_ctx->vkCmdPushConstants(cmd_buf, comp_pipeline.layout(), VK_SHADER_STAGE_COMPUTE_BIT, 0, uniform_data_len,
                                    uniform_data);
    }

    api_ctx->vkCmdDispatchIndirect(cmd_buf, indir_buf.vk_handle(), VkDeviceSize(indir_buf_offset));
}

void Ren::DispatchComputeIndirect(const Pipeline &comp_pipeline, const Buffer &indir_buf,
                                  const uint32_t indir_buf_offset, Span<const Binding> bindings,
                                  const void *uniform_data, int uniform_data_len, DescrMultiPoolAlloc &descr_alloc,
                                  ILog *log) {
    ApiContext *api_ctx = descr_alloc.api_ctx();
    VkCommandBuffer cmd_buf = api_ctx->draw_cmd_buf[api_ctx->backend_frame];
    DispatchComputeIndirect(cmd_buf, comp_pipeline, indir_buf, indir_buf_offset, bindings, uniform_data,
                            uniform_data_len, descr_alloc, log);
}
