#include "FgBuilder.h"

#include <Ren/Context.h>
#include <Ren/DebugMarker.h>
#include <Ren/DrawCall.h>
#include <Ren/VKCtx.h>
#include <Sys/ScopeExit.h>

#include "FgNode.h"

namespace Ren {
VkBufferUsageFlags GetVkBufferUsageFlags(const ApiContext *api_ctx, eBufType type);
VkMemoryPropertyFlags GetVkMemoryPropertyFlags(eBufType type);
uint32_t FindMemoryType(uint32_t search_from, const VkPhysicalDeviceMemoryProperties *mem_properties,
                        uint32_t mem_type_bits, VkMemoryPropertyFlags desired_mem_flags, VkDeviceSize desired_size);
VkImageUsageFlags to_vk_image_usage(Bitmask<eTexUsage> usage, eTexFormat format);
} // namespace Ren

namespace FgBuilderInternal {
extern const bool EnableResourceAliasing;

void insert_sorted(Ren::SmallVectorImpl<Eng::FgResRef> &vec, const Eng::FgResRef val) {
    const auto it = std::lower_bound(std::begin(vec), std::end(vec), val);
    if (it == std::end(vec) || val < (*it)) {
        vec.insert(it, val);
    }
}
} // namespace FgBuilderInternal

bool Eng::FgBuilder::AllocateNeededResources_MemHeaps() {
    using namespace FgBuilderInternal;

    struct resource_t {
        eFgResType type;
        uint8_t mem_heap = 0xff;
        uint16_t index;
        uint16_t lifetime[2][2];
        uint32_t mem_offset = 0xffffffff;
        uint32_t mem_size = 0;
        uint32_t mem_alignment = 0;

        std::variant<std::monostate, Ren::BufHandle, Ren::TexHandle> handle;
    };

    Ren::ApiContext *api_ctx = ctx_.api_ctx();

    std::vector<resource_t> all_resources;
    std::vector<int> resources_by_memory_type[32];
    std::vector<int> buffer_to_resource(buffers_.capacity(), -1);
    std::vector<int> texture_to_resource(textures_.capacity(), -1);

    SCOPE_EXIT({
        for (resource_t &res : all_resources) {
            if (std::holds_alternative<std::monostate>(res.handle)) {
                continue;
            }
            if (res.type == eFgResType::Buffer) {
                api_ctx->vkDestroyBuffer(api_ctx->device, std::get<Ren::BufHandle>(res.handle).buf, nullptr);
            } else if (res.type == eFgResType::Texture) {
                api_ctx->vkDestroyImage(api_ctx->device, std::get<Ren::TexHandle>(res.handle).img, nullptr);
            }
        }
    });

    //
    // Gather resources info
    //
    for (auto it = std::begin(buffers_); it != std::end(buffers_); ++it) {
        const FgAllocBuf &b = *it;
        if (b.external || !b.lifetime.is_used()) {
            continue;
        }
        if (b.desc.type == Ren::eBufType::Upload || b.desc.type == Ren::eBufType::Readback) {
            // Upload/Readback buffers will use dedicated allocation
            continue;
        }

        resource_t &new_res = all_resources.emplace_back();
        new_res.type = eFgResType::Buffer;
        new_res.index = uint16_t(it.index());
        if (EnableResourceAliasing) {
            new_res.lifetime[0][0] = new_res.lifetime[1][0] = b.lifetime.first_used_node();
            new_res.lifetime[0][1] = new_res.lifetime[1][1] = b.lifetime.last_used_node() + 1;
        } else {
            new_res.lifetime[0][0] = new_res.lifetime[1][0] = 0;
            new_res.lifetime[0][1] = new_res.lifetime[1][1] = uint16_t(reordered_nodes_.size());
        }

        VkBufferCreateInfo buf_create_info = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        buf_create_info.size = VkDeviceSize(b.desc.size);
        buf_create_info.usage = GetVkBufferUsageFlags(api_ctx, b.desc.type);
        buf_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        Ren::BufHandle new_buf = {};
        VkResult res = api_ctx->vkCreateBuffer(api_ctx->device, &buf_create_info, nullptr, &new_buf.buf);
        if (res != VK_SUCCESS) {
            ctx_.log()->Error("Failed to create buffer %s!", b.name.c_str());
            return false;
        }

#ifdef ENABLE_GPU_DEBUG
        VkDebugUtilsObjectNameInfoEXT name_info = {VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
        name_info.objectType = VK_OBJECT_TYPE_BUFFER;
        name_info.objectHandle = uint64_t(new_buf.buf);
        name_info.pObjectName = b.name.c_str();
        api_ctx->vkSetDebugUtilsObjectNameEXT(api_ctx->device, &name_info);
#endif

        VkMemoryRequirements memory_requirements = {};
        api_ctx->vkGetBufferMemoryRequirements(api_ctx->device, new_buf.buf, &memory_requirements);

        const VkMemoryPropertyFlags memory_props = Ren::GetVkMemoryPropertyFlags(b.desc.type);

        const uint32_t memory_type = Ren::FindMemoryType(
            0, &api_ctx->mem_properties, memory_requirements.memoryTypeBits, memory_props, memory_requirements.size);
        assert(memory_type < 32);

        new_res.mem_size = uint32_t(memory_requirements.size);
        new_res.mem_alignment = uint32_t(memory_requirements.alignment);
        if (b.desc.type == Ren::eBufType::Storage && api_ctx->raytracing_supported) {
            // Account for the usage as scratch buffer for acceleration structures
            new_res.mem_alignment =
                std::max(new_res.mem_alignment, api_ctx->acc_props.minAccelerationStructureScratchOffsetAlignment);
        }
        new_res.handle = new_buf;
        resources_by_memory_type[memory_type].push_back(int(all_resources.size()) - 1);
        buffer_to_resource[it.index()] = int(all_resources.size()) - 1;
    }

    for (auto it = std::begin(textures_); it != std::end(textures_); ++it) {
        FgAllocTex &t = *it;
        if (t.external || !t.lifetime.is_used()) {
            continue;
        }

        resource_t &new_res = all_resources.emplace_back();
        new_res.type = eFgResType::Texture;
        new_res.index = uint16_t(it.index());
        if (EnableResourceAliasing) {
            GetResourceFrameLifetime(t, new_res.lifetime);
        } else {
            // In case of no aliasing lifetime spans for the whole frame
            new_res.lifetime[0][0] = new_res.lifetime[1][0] = 0;
            new_res.lifetime[0][1] = new_res.lifetime[1][1] = uint16_t(reordered_nodes_.size());
        }

        Ren::TexParams &p = t.desc;
        if (t.history_index != -1) {
            // combine usage flags
            FgAllocTex &hist_tex = textures_[t.history_index];
            p.usage |= hist_tex.desc.usage;
            hist_tex.desc.usage = p.usage;
        }
        if (t.history_of != -1) {
            // combine usage flags
            FgAllocTex &hist_tex = textures_[t.history_of];
            p.usage |= hist_tex.desc.usage;
            hist_tex.desc.usage = p.usage;
        }

        Ren::TexHandle new_tex = {};
        { // create new image
            int mip_count = p.mip_count;
            if (!mip_count) {
                mip_count = Ren::CalcMipCount(p.w, p.h, 1);
            }

            VkImageCreateInfo img_info = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
            img_info.imageType = VK_IMAGE_TYPE_2D;
            img_info.extent.width = uint32_t(p.w);
            img_info.extent.height = uint32_t(p.h);
            img_info.extent.depth = 1;
            img_info.mipLevels = mip_count;
            img_info.arrayLayers = 1;
            img_info.format = Ren::VKFormatFromTexFormat(p.format);
            img_info.tiling = VK_IMAGE_TILING_OPTIMAL;
            img_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            assert(uint8_t(p.usage) != 0);
            img_info.usage = Ren::to_vk_image_usage(p.usage, p.format);

            img_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            img_info.samples = VkSampleCountFlagBits(p.samples);
            img_info.flags = 0;

            const VkResult res = api_ctx->vkCreateImage(api_ctx->device, &img_info, nullptr, &new_tex.img);
            if (res != VK_SUCCESS) {
                ctx_.log()->Error("Failed to create image %s!", t.name.c_str());
                return false;
            }

#ifdef ENABLE_GPU_DEBUG
            VkDebugUtilsObjectNameInfoEXT name_info = {VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
            name_info.objectType = VK_OBJECT_TYPE_IMAGE;
            name_info.objectHandle = uint64_t(new_tex.img);
            name_info.pObjectName = t.name.c_str();
            api_ctx->vkSetDebugUtilsObjectNameEXT(api_ctx->device, &name_info);
#endif
        }

        VkMemoryRequirements memory_requirements;
        api_ctx->vkGetImageMemoryRequirements(api_ctx->device, new_tex.img, &memory_requirements);

        VkMemoryPropertyFlags memory_props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        const uint32_t memory_type = Ren::FindMemoryType(
            0, &api_ctx->mem_properties, memory_requirements.memoryTypeBits, memory_props, memory_requirements.size);
        assert(memory_type < 32);

        new_res.mem_size = uint32_t(memory_requirements.size);
        new_res.mem_alignment = uint32_t(memory_requirements.alignment);
        new_res.handle = new_tex;
        resources_by_memory_type[memory_type].push_back(int(all_resources.size()) - 1);
        texture_to_resource[it.index()] = int(all_resources.size()) - 1;
    }

    //
    // Allocate memory heaps, bind resources
    //
    for (int i = 0; i < 32; ++i) {
        if (resources_by_memory_type[i].empty()) {
            continue;
        }

        // sort by lifetime length
        std::sort(begin(resources_by_memory_type[i]), end(resources_by_memory_type[i]),
                  [&](const int lhs, const int rhs) {
                      return (all_resources[lhs].lifetime[0][1] - all_resources[lhs].lifetime[0][0]) +
                                 (all_resources[lhs].lifetime[1][1] - all_resources[lhs].lifetime[1][0]) >
                             (all_resources[rhs].lifetime[0][1] - all_resources[rhs].lifetime[0][0]) +
                                 (all_resources[rhs].lifetime[1][1] - all_resources[rhs].lifetime[1][0]);
                  });

        const int NodesCount = int(reordered_nodes_.size());
        std::vector<uint32_t> heap_tops(2 * NodesCount, 0);
        for (const int res_index : resources_by_memory_type[i]) {
            resource_t &res = all_resources[res_index];

            uint32_t heap_top = 0;
            for (int j = res.lifetime[0][0]; j < res.lifetime[0][1]; ++j) {
                heap_top = std::max(heap_top, heap_tops[j]);
            }
            for (int j = NodesCount + res.lifetime[1][0]; j < NodesCount + res.lifetime[1][1]; ++j) {
                heap_top = std::max(heap_top, heap_tops[j]);
            }

            // round current top up to required alignment
            heap_top = res.mem_alignment * ((heap_top + res.mem_alignment - 1) / res.mem_alignment);
            res.mem_offset = heap_top;
            heap_top += res.mem_size;

            for (int j = res.lifetime[0][0]; j < res.lifetime[0][1]; ++j) {
                heap_tops[j] = heap_top;
            }
            for (int j = NodesCount + res.lifetime[1][0]; j < NodesCount + res.lifetime[1][1]; ++j) {
                heap_tops[j] = heap_top;
            }
        }

        uint32_t total_heap_size = 0;
        for (uint32_t ht : heap_tops) {
            total_heap_size = std::max(total_heap_size, ht);
        }

        VkMemoryAllocateInfo mem_alloc_info = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        mem_alloc_info.allocationSize = total_heap_size;
        mem_alloc_info.memoryTypeIndex = i;

        VkMemoryAllocateFlagsInfoKHR additional_flags = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO_KHR};
        if (api_ctx->raytracing_supported) {
            additional_flags.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;
            mem_alloc_info.pNext = &additional_flags;
        }

        auto &heap = memory_heaps_.emplace_back();
        heap.size = total_heap_size;

        // TODO: Handle failure properly
        VkResult result = api_ctx->vkAllocateMemory(api_ctx->device, &mem_alloc_info, nullptr, &heap.mem);
        if (result != VK_SUCCESS) {
            ctx_.log()->Error("Failed to allocate memory!");
            return false;
        }

        for (const int res_index : resources_by_memory_type[i]) {
            resource_t &res = all_resources[res_index];
            res.mem_heap = int8_t(memory_heaps_.size()) - 1;
            if (res.type == eFgResType::Buffer) {
                result = api_ctx->vkBindBufferMemory(api_ctx->device, std::get<Ren::BufHandle>(res.handle).buf,
                                                     heap.mem, res.mem_offset);
            } else if (res.type == eFgResType::Texture) {
                result = api_ctx->vkBindImageMemory(api_ctx->device, std::get<Ren::TexHandle>(res.handle).img, heap.mem,
                                                    res.mem_offset);
            }
            if (result != VK_SUCCESS) {
                ctx_.log()->Error("Failed to bind memory!");
                return false;
            }
        }
    }

    for (auto it = std::begin(buffers_); it != std::end(buffers_); ++it) {
        FgAllocBuf &buf = *it;
        if (buf.external || !buf.lifetime.is_used()) {
            continue;
        }
        buf.alias_of = -1;
        assert(!buf.ref);
        if (buffer_to_resource[it.index()] != -1) {
            const resource_t &resource = all_resources[buffer_to_resource[it.index()]];
            Ren::MemAllocation alloc = {resource.mem_offset, resource.mem_size, resource.mem_heap};
            buf.strong_ref = ctx_.LoadBuffer(buf.name, buf.desc.type, std::get<Ren::BufHandle>(resource.handle),
                                             std::move(alloc), buf.desc.size, 16);
        } else {
            buf.strong_ref = ctx_.LoadBuffer(buf.name, buf.desc.type, buf.desc.size, 16);
        }
        buf.ref = buf.strong_ref;
        for (int i = 0; i < int(buf.desc.views.size()); ++i) {
            const int view_index = buf.ref->AddBufferView(buf.desc.views[i]);
            assert(view_index == i);
        }
    }
    for (auto it = std::begin(textures_); it != std::end(textures_); ++it) {
        FgAllocTex &tex = *it;
        if (tex.external || !tex.lifetime.is_used()) {
            continue;
        }
        tex.alias_of = -1;
        assert(!tex.ref);
        assert(texture_to_resource[it.index()] != -1);

        const resource_t &resource = all_resources[texture_to_resource[it.index()]];
        Ren::MemAllocation alloc = {resource.mem_offset, resource.mem_size, resource.mem_heap};

        Ren::eTexLoadStatus status;
        tex.strong_ref =
            ctx_.LoadTexture(tex.name, std::get<Ren::TexHandle>(resource.handle), tex.desc, std::move(alloc), &status);
        assert(status == Ren::eTexLoadStatus::CreatedDefault);
        tex.ref = tex.strong_ref;
    }

    //
    // Memory aliasing barriers for overlapped regions
    //
    struct region_t {
        uint32_t offset, size;
        FgResRef res;
        bool barrier_placed = false;
    };
    std::vector<region_t> deactivated_regions;
    for (int j = 0; j < 2; ++j) {
        for (int i = 0; i < int(reordered_nodes_.size()); ++i) {
            FgNode *node = reordered_nodes_[i];
            for (const FgResource &res : node->input_) {
                if (res.type == eFgResType::Buffer) {
                    const FgAllocBuf &buf = buffers_.at(res.index);
                    if (buf.external) {
                        continue;
                    }
                    const Ren::MemAllocation &alloc = buf.ref->mem_alloc();
                    if (alloc.pool == 0xffff) {
                        // this is dedicated allocation
                        continue;
                    }
                    assert(buffer_to_resource[res.index] != -1);
                    const resource_t &r = all_resources[buffer_to_resource[res.index]];
                    if (r.lifetime[j][1] == i + 1 &&
                        (i != int(reordered_nodes_.size()) - 1 || r.lifetime[!j][0] != 0)) {
                        deactivated_regions.push_back(region_t{alloc.offset, alloc.block, res});
                    }
                } else if (res.type == eFgResType::Texture) {
                    const FgAllocTex &tex = textures_.at(res.index);
                    if (tex.external) {
                        continue;
                    }
                    const Ren::MemAllocation &alloc = tex.ref->mem_alloc();
                    if (alloc.pool == 0xffff) {
                        // this is dedicated allocation
                        continue;
                    }
                    assert(texture_to_resource[res.index] != -1);
                    const resource_t &r = all_resources[texture_to_resource[res.index]];
                    if (r.lifetime[j][1] == i + 1 &&
                        (i != int(reordered_nodes_.size()) - 1 || r.lifetime[!j][0] != 0)) {
                        deactivated_regions.push_back(region_t{alloc.offset, alloc.block, res});
                    }
                }
            }
            for (const FgResource &res : node->output_) {
                FgAllocRes *this_res = nullptr;
                const Ren::MemAllocation *this_alloc = nullptr;
                bool deactivate = false;
                if (res.type == eFgResType::Buffer) {
                    this_res = &buffers_.at(res.index);
                    if (buffers_.at(res.index).ref) {
                        this_alloc = &buffers_.at(res.index).ref->mem_alloc();
                        if (buffer_to_resource[res.index] != -1) {
                            const resource_t &r = all_resources[buffer_to_resource[res.index]];
                            deactivate = (r.lifetime[j][1] == i + 1 &&
                                          (i != int(reordered_nodes_.size()) - 1 || r.lifetime[!j][0] != 0));
                        }
                    }
                } else /*if (res.type == eFgResType::Texture)*/ {
                    assert(res.type == eFgResType::Texture);
                    this_res = &textures_.at(res.index);
                    if (textures_.at(res.index).ref) {
                        this_alloc = &textures_.at(res.index).ref->mem_alloc();
                        if (texture_to_resource[res.index] != -1) {
                            const resource_t &r = all_resources[texture_to_resource[res.index]];
                            deactivate = (r.lifetime[j][1] == i + 1 &&
                                          (i != int(reordered_nodes_.size()) - 1 || r.lifetime[!j][0] != 0));
                        }
                    }
                }

                if (this_res->external) {
                    continue;
                }
                assert(this_alloc);
                if (!this_alloc || this_alloc->pool == 0xffff) {
                    // this is dedicated allocation
                    continue;
                }

                if (this_res->lifetime.first_used_node() == i) { // Resource activation
                    // Remove from deactivated regions
                    for (auto it = begin(deactivated_regions); it != end(deactivated_regions);) {
                        if (it->res.type == res.type && it->res.index == res.index) {
                            it = deactivated_regions.erase(it);
                        } else {
                            ++it;
                        }
                    }
                    // Find overlapping regions
                    for (auto it = begin(deactivated_regions); it != end(deactivated_regions);) {
                        assert(it->res.type != res.type || it->res.index != res.index);
                        FgAllocRes *other_res = nullptr;
                        if (it->res.type == eFgResType::Buffer) {
                            other_res = &buffers_.at(it->res.index);
                            const Ren::MemAllocation &other_alloc = buffers_.at(it->res.index).ref->mem_alloc();
                            if (other_alloc.pool != this_alloc->pool) {
                                ++it;
                                continue;
                            }
                        } else if (it->res.type == eFgResType::Texture) {
                            other_res = &textures_.at(it->res.index);
                            const Ren::MemAllocation &other_alloc = textures_.at(it->res.index).ref->mem_alloc();
                            if (other_alloc.pool != this_alloc->pool) {
                                ++it;
                                continue;
                            }
                        }
                        if (it->offset < this_alloc->offset + this_alloc->block &&
                            it->offset + it->size > this_alloc->offset) {
                            if (!it->barrier_placed) {
                                insert_sorted(this_res->overlaps_with, it->res);
                                it->barrier_placed = true;
                            }
                            other_res->aliased_in_stages |= StageBitsForState(res.desired_state);

                            if (this_alloc->offset > it->offset &&
                                this_alloc->offset + this_alloc->block < it->offset + it->size) {
                                // current region splits overlapped in two
                                region_t new_region;
                                new_region.offset = this_alloc->offset + this_alloc->block;
                                new_region.size = it->offset + it->size - new_region.offset;
                                new_region.res = it->res;
                                new_region.barrier_placed = it->barrier_placed;

                                it->size = this_alloc->offset - it->offset;
                                const ptrdiff_t dist = std::distance(begin(deactivated_regions), it);
                                deactivated_regions.push_back(new_region);
                                it = begin(deactivated_regions) + dist + 1;
                            } else if (this_alloc->offset <= it->offset) {
                                // simple overlap from left side
                                const uint32_t end = it->offset + it->size;
                                it->offset = this_alloc->offset + this_alloc->block;
                                if (end > it->offset) {
                                    it->size = end - it->offset;
                                    ++it;
                                } else {
                                    it = deactivated_regions.erase(it);
                                }
                            } else {
                                // simple overlap from right side
                                assert(this_alloc->offset > it->offset);
                                it->size = this_alloc->offset - it->offset;
                                ++it;
                            }
                        } else {
                            ++it;
                        }
                    }
                }
                if (deactivate) {
                    deactivated_regions.push_back(region_t{this_alloc->offset, this_alloc->block, res});
                }
            }
        }
    }

    all_resources.clear();

    return true;
}

void Eng::FgBuilder::ClearResources_MemHeaps() {
    Ren::CommandBuffer cmd_buf = ctx_.BegTempSingleTimeCommands();
    //
    // Simulate execution over 2 frames, but perform clear instead of actual work
    //
    for (int j = 0; j < 2; ++j) {
        Ren::DebugMarker exec_marker(ctx_.api_ctx(), ctx_.current_cmd_buf(), "Eng::FgBuilder::ClearResources_MemHeaps");

        // Swap history images
        for (FgAllocTex &tex : textures_) {
            if (tex.history_index != -1) {
                auto &hist_tex = textures_.at(tex.history_index);
                if (hist_tex.ref) {
                    assert(hist_tex.lifetime.is_used());
                    std::swap(tex.ref, hist_tex.ref);
                }
            }
        }
        // Reset resources
        for (FgAllocBuf &buf : buffers_) {
            buf._generation = 0;
            buf.used_in_stages = Ren::eStageBits::None;
            if (buf.ref) {
                buf.used_in_stages = StageBitsForState(buf.ref->resource_state);
            }
        }
        for (FgAllocTex &tex : textures_) {
            tex._generation = 0;
            tex.used_in_stages = Ren::eStageBits::None;
            if (tex.ref) {
                tex.used_in_stages = StageBitsForState(tex.ref->resource_state);
            }
        }

        BuildResourceLinkedLists();

#if defined(REN_GL_BACKEND)
        rast_state_.Apply();
#endif

        for (int i = 0; i < int(reordered_nodes_.size()); ++i) {
            const FgNode *node = reordered_nodes_[i];

            Ren::SmallVector<Ren::TransitionInfo, 32> res_transitions;
            Ren::eStageBits src_stages = Ren::eStageBits::None;
            Ren::eStageBits dst_stages = Ren::eStageBits::None;

            std::vector<Ren::BufRef> bufs_to_clear;
            std::vector<Ren::TexRef> texs_to_clear;

            // for (const FgResource &res : node->input_) {
            //     HandleResourceTransition(res, res_transitions, src_stages, dst_stages);
            // }
            for (const FgResource &res : node->output_) {
                if (res.type == eFgResType::Buffer) {
                    FgAllocBuf &buf = buffers_.at(res.index);
                    if (buf.external) {
                        continue;
                    }
                    bufs_to_clear.push_back(buf.ref);
                    HandleResourceTransition(res, res_transitions, src_stages, dst_stages);
                } else if (res.type == eFgResType::Texture) {
                    FgAllocTex &tex = textures_.at(res.index);
                    if (tex.external) {
                        continue;
                    }
                    texs_to_clear.push_back(tex.ref);
                    HandleResourceTransition(res, res_transitions, src_stages, dst_stages);
                }
            }
            TransitionResourceStates(ctx_.api_ctx(), cmd_buf, Ren::AllStages, Ren::AllStages, res_transitions);
            for (Ren::BufRef &b : bufs_to_clear) {
                if (b->resource_state == Ren::eResState::CopyDst) {
                    ClearBuffer_AsTransfer(b, cmd_buf);
                } else if (b->resource_state == Ren::eResState::UnorderedAccess) {
                    ClearBuffer_AsStorage(b, cmd_buf);
                } else if (b->resource_state == Ren::eResState::BuildASWrite) {
                    // NOTE: Skipped
                } else {
                    assert(false);
                }
            }
            for (Ren::TexRef &t : texs_to_clear) {
                if (t->resource_state == Ren::eResState::CopyDst) {
                    ClearImage_AsTransfer(t, cmd_buf);
                } else if (t->resource_state == Ren::eResState::UnorderedAccess) {
                    ClearImage_AsStorage(t, cmd_buf);
                } else if (t->resource_state == Ren::eResState::RenderTarget ||
                           t->resource_state == Ren::eResState::DepthWrite) {
                    ClearImage_AsTarget(t, cmd_buf);
                } else {
                    assert(false);
                }
            }
        }
    }
    ctx_.EndTempSingleTimeCommands(cmd_buf);
}

void Eng::FgBuilder::ReleaseMemHeaps() {
    Ren::ApiContext *api_ctx = ctx_.api_ctx();
    api_ctx->vkDeviceWaitIdle(api_ctx->device);
    for (Ren::MemHeap &heap : memory_heaps_) {
        api_ctx->vkFreeMemory(api_ctx->device, heap.mem, nullptr);
    }
    memory_heaps_.clear();
}
