#include "FgBuilder.h"

#include <Ren/Config.h>
#include <Ren/Context.h>
#include <Ren/DebugMarker.h>
#include <Ren/DrawCall.h>
#include <Ren/Vk/VKCtx.h>
#include <Sys/ScopeExit.h>

#include "FgNode.h"

namespace Ren {
VkBufferUsageFlags GetVkBufferUsageFlags(const ApiContext &api, eBufType type);
VkMemoryPropertyFlags GetVkMemoryPropertyFlags(eBufType type);
uint32_t FindMemoryType(uint32_t search_from, const VkPhysicalDeviceMemoryProperties *mem_properties,
                        uint32_t mem_type_bits, VkMemoryPropertyFlags desired_mem_flags, VkDeviceSize desired_size);
VkImageUsageFlags to_vk_image_usage(Bitmask<eImgUsage> usage, eFormat format);
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

        std::variant<std::monostate, Ren::BufferMain, Ren::ImageMain> main;
    };

    const Ren::ApiContext &api = ctx_.api();
    const Ren::StoragesRef &storages = ctx_.storages();

    std::vector<resource_t> all_resources;
    std::vector<int> resources_by_memory_type[32];
    std::vector<int> buffer_to_resource(name_to_buffer_.capacity(), -1);
    std::vector<int> image_to_resource(name_to_image_.capacity(), -1);

    SCOPE_EXIT({
        for (resource_t &res : all_resources) {
            if (std::holds_alternative<std::monostate>(res.main)) {
                continue;
            }
            if (res.type == eFgResType::Buffer) {
                api.vkDestroyBuffer(api.device, std::get<Ren::BufferMain>(res.main).buf, nullptr);
            } else if (res.type == eFgResType::Image) {
                api.vkDestroyImage(api.device, std::get<Ren::ImageMain>(res.main).img, nullptr);
            }
        }
    });

    //
    // Gather resources info
    //
    for (auto it = std::begin(name_to_buffer_); it != std::end(name_to_buffer_); ++it) {
        const auto &[fgbuf_main, fgbuf_cold] = buffers_.GetUnsafe(it->val);
        if (fgbuf_cold.external || !fgbuf_cold.lifetime.is_used()) {
            continue;
        }
        if (fgbuf_cold.desc.type == Ren::eBufType::Upload || fgbuf_cold.desc.type == Ren::eBufType::Readback) {
            // Upload/Readback buffers will use dedicated allocation
            continue;
        }

        resource_t &new_res = all_resources.emplace_back();
        new_res.type = eFgResType::Buffer;
        new_res.index = uint16_t(it.index());
        if (EnableResourceAliasing) {
            GetResourceFrameLifetime(fgbuf_cold, new_res.lifetime);
        } else {
            new_res.lifetime[0][0] = new_res.lifetime[1][0] = 0;
            new_res.lifetime[0][1] = new_res.lifetime[1][1] = uint16_t(reordered_nodes_.size());
        }

        VkBufferCreateInfo buf_create_info = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        buf_create_info.size = VkDeviceSize(fgbuf_cold.desc.size);
        buf_create_info.usage = GetVkBufferUsageFlags(api, fgbuf_cold.desc.type);
        buf_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        Ren::BufferMain new_buf = {};
        VkResult res = api.vkCreateBuffer(api.device, &buf_create_info, nullptr, &new_buf.buf);
        if (res != VK_SUCCESS) {
            ctx_.log()->Error("Failed to create buffer %s!", fgbuf_cold.name.c_str());
            return false;
        }

#ifdef ENABLE_GPU_DEBUG
        VkDebugUtilsObjectNameInfoEXT name_info = {VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
        name_info.objectType = VK_OBJECT_TYPE_BUFFER;
        name_info.objectHandle = uint64_t(new_buf.buf);
        name_info.pObjectName = fgbuf_cold.name.c_str();
        api.vkSetDebugUtilsObjectNameEXT(api.device, &name_info);
#endif

        VkMemoryRequirements memory_requirements = {};
        api.vkGetBufferMemoryRequirements(api.device, new_buf.buf, &memory_requirements);

        const VkMemoryPropertyFlags memory_props = Ren::GetVkMemoryPropertyFlags(fgbuf_cold.desc.type);

        const uint32_t memory_type = Ren::FindMemoryType(0, &api.mem_properties, memory_requirements.memoryTypeBits,
                                                         memory_props, memory_requirements.size);
        assert(memory_type < 32);

        new_res.mem_size = uint32_t(memory_requirements.size);
        new_res.mem_alignment = uint32_t(memory_requirements.alignment);
        if (fgbuf_cold.desc.type == Ren::eBufType::Storage && api.raytracing_supported) {
            // Account for the usage as scratch buffer for acceleration structures
            new_res.mem_alignment =
                std::max(new_res.mem_alignment, api.acc_props.minAccelerationStructureScratchOffsetAlignment);
        }
        new_res.main = new_buf;
        resources_by_memory_type[memory_type].push_back(int(all_resources.size()) - 1);
        buffer_to_resource[it->val] = int(all_resources.size()) - 1;
    }

    for (auto it = std::begin(name_to_image_); it != std::end(name_to_image_); ++it) {
        const auto &[fgimg_main, fgimg_cold] = images_.GetUnsafe(it->val);
        if (fgimg_cold.external || !fgimg_cold.lifetime.is_used()) {
            continue;
        }

        resource_t &new_res = all_resources.emplace_back();
        new_res.type = eFgResType::Image;
        new_res.index = uint16_t(it.index());
        if (EnableResourceAliasing) {
            GetResourceFrameLifetime(fgimg_cold, new_res.lifetime);
        } else {
            // In case of no aliasing lifetime spans for the whole frame
            new_res.lifetime[0][0] = new_res.lifetime[1][0] = 0;
            new_res.lifetime[0][1] = new_res.lifetime[1][1] = uint16_t(reordered_nodes_.size());
        }

        Ren::ImgParams &p = fgimg_cold.desc;
        if (fgimg_cold.history_index != 0xffff) {
            // combine usage flags
            FgAllocImgCold &hist_img = images_.GetUnsafe(fgimg_cold.history_index).second;
            p.usage |= hist_img.desc.usage;
            hist_img.desc.usage = p.usage;
        }
        if (fgimg_cold.history_of != 0xffff) {
            // combine usage flags
            FgAllocImgCold &hist_img = images_.GetUnsafe(fgimg_cold.history_of).second;
            p.usage |= hist_img.desc.usage;
            hist_img.desc.usage = p.usage;
        }

        Ren::ImageMain new_img = {};
        { // create new image
            int mip_count = p.mip_count;
            if (!mip_count) {
                mip_count = Ren::CalcMipCount(p.w, p.h, 1);
            }

            VkImageCreateInfo img_info = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
            img_info.imageType =
                (p.flags & Ren::eImgFlags::Array) ? VK_IMAGE_TYPE_2D : (p.d ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D);
            img_info.extent.width = uint32_t(p.w);
            img_info.extent.height = uint32_t(p.h);
            img_info.extent.depth = (p.flags & Ren::eImgFlags::Array) ? 1 : std::max<uint32_t>(p.d, 1);
            img_info.mipLevels = mip_count;
            img_info.arrayLayers = (p.flags & Ren::eImgFlags::Array) ? std::max<uint32_t>(p.d, 1) : 1;
            img_info.format = Ren::VKFormatFromFormat(p.format);
            img_info.tiling = VK_IMAGE_TILING_OPTIMAL;
            img_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            assert(uint8_t(p.usage) != 0);
            img_info.usage = Ren::to_vk_image_usage(p.usage, p.format);

            img_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            img_info.samples = VkSampleCountFlagBits(p.samples);
            img_info.flags = 0;

            const VkResult res = api.vkCreateImage(api.device, &img_info, nullptr, &new_img.img);
            if (res != VK_SUCCESS) {
                ctx_.log()->Error("Failed to create image %s!", fgimg_cold.name.c_str());
                return false;
            }

#ifdef ENABLE_GPU_DEBUG
            VkDebugUtilsObjectNameInfoEXT name_info = {VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
            name_info.objectType = VK_OBJECT_TYPE_IMAGE;
            name_info.objectHandle = uint64_t(new_img.img);
            name_info.pObjectName = fgimg_cold.name.c_str();
            api.vkSetDebugUtilsObjectNameEXT(api.device, &name_info);
#endif
        }

        VkMemoryRequirements memory_requirements;
        api.vkGetImageMemoryRequirements(api.device, new_img.img, &memory_requirements);

        VkMemoryPropertyFlags memory_props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        const uint32_t memory_type = Ren::FindMemoryType(0, &api.mem_properties, memory_requirements.memoryTypeBits,
                                                         memory_props, memory_requirements.size);
        assert(memory_type < 32);

        new_res.mem_size = uint32_t(memory_requirements.size);
        new_res.mem_alignment = uint32_t(memory_requirements.alignment);
        new_res.main = new_img;
        resources_by_memory_type[memory_type].push_back(int(all_resources.size()) - 1);
        image_to_resource[it->val] = int(all_resources.size()) - 1;
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
        for (const uint32_t ht : heap_tops) {
            total_heap_size = std::max(total_heap_size, ht);
        }

        VkMemoryAllocateInfo mem_alloc_info = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        mem_alloc_info.allocationSize = total_heap_size;
        mem_alloc_info.memoryTypeIndex = i;
        const void **pp_next = &mem_alloc_info.pNext;

        VkMemoryPriorityAllocateInfoEXT prio_info = {VK_STRUCTURE_TYPE_MEMORY_PRIORITY_ALLOCATE_INFO_EXT};
        prio_info.priority = 1.0f; // highest priority
        if (api.pageable_memory_supported) {
            (*pp_next) = &prio_info;
            pp_next = &prio_info.pNext;
        }

        VkMemoryAllocateFlagsInfoKHR additional_flags = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO_KHR};
        additional_flags.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;
        if (api.raytracing_supported) {
            (*pp_next) = &additional_flags;
            pp_next = &additional_flags.pNext;
        }

        auto &heap = memory_heaps_.emplace_back();
        heap.size = total_heap_size;

        // TODO: Handle failure properly
        VkResult result = api.vkAllocateMemory(api.device, &mem_alloc_info, nullptr, &heap.mem);
        if (result != VK_SUCCESS) {
            ctx_.log()->Error("Failed to allocate memory!");
            return false;
        }

        for (const int res_index : resources_by_memory_type[i]) {
            resource_t &res = all_resources[res_index];
            res.mem_heap = uint8_t(memory_heaps_.size() - 1);
            if (res.type == eFgResType::Buffer) {
                result = api.vkBindBufferMemory(api.device, std::get<Ren::BufferMain>(res.main).buf, heap.mem,
                                                res.mem_offset);
            } else if (res.type == eFgResType::Image) {
                result =
                    api.vkBindImageMemory(api.device, std::get<Ren::ImageMain>(res.main).img, heap.mem, res.mem_offset);
            }
            if (result != VK_SUCCESS) {
                ctx_.log()->Error("Failed to bind memory!");
                return false;
            }
        }
    }

    for (auto it = std::begin(name_to_buffer_); it != std::end(name_to_buffer_); ++it) {
        const auto &[fgbuf_main, fgbuf_cold] = buffers_.GetUnsafe(it->val);
        if (fgbuf_cold.external || !fgbuf_cold.lifetime.is_used()) {
            continue;
        }
        fgbuf_cold.alias_of = -1;
        assert(!fgbuf_main.handle);
        if (buffer_to_resource[it->val] != -1) {
            const resource_t &resource = all_resources[buffer_to_resource[it->val]];
            Ren::MemAllocation alloc = {resource.mem_offset, resource.mem_size, resource.mem_heap};
            fgbuf_main.handle =
                ctx_.CreateBuffer(fgbuf_cold.name, fgbuf_cold.desc.type, std::get<Ren::BufferMain>(resource.main),
                                  std::move(alloc), fgbuf_cold.desc.size, 16);
        } else {
            fgbuf_main.handle = ctx_.CreateBuffer(fgbuf_cold.name, fgbuf_cold.desc.type, fgbuf_cold.desc.size, 16);
        }

        const auto &[buf_main, buf_cold] = storages.buffers[fgbuf_main.handle];
        for (int i = 0; i < int(fgbuf_cold.desc.views.size()); ++i) {
            const int view_index = Buffer_AddView(api, buf_main, buf_cold, fgbuf_cold.desc.views[i]);
            assert(view_index == i);
        }
    }
    for (auto it = std::begin(name_to_image_); it != std::end(name_to_image_); ++it) {
        const auto &[fgimg_main, fgimg_cold] = images_.GetUnsafe(it->val);
        if (fgimg_cold.external || !fgimg_cold.lifetime.is_used()) {
            continue;
        }
        fgimg_cold.alias_of = -1;
        assert(!fgimg_main.handle_to_own);
        assert(image_to_resource[it->val] != -1);

        const resource_t &resource = all_resources[image_to_resource[it->val]];
        Ren::MemAllocation alloc = {resource.mem_offset, resource.mem_size, resource.mem_heap};

        fgimg_main.handle_to_own = ctx_.CreateImage(fgimg_cold.name, fgimg_cold.desc,
                                                    std::get<Ren::ImageMain>(resource.main), std::move(alloc));
        fgimg_main.handle_to_use = fgimg_main.handle_to_own;

        const auto &[img_main, img_cold] = storages.images[fgimg_main.handle_to_own];
        Image_SetSampling(ctx_.api(), img_main, img_cold, fgimg_cold.desc.sampling, ctx_.log());
        Image_AddDefaultViews(ctx_.api(), img_main, img_cold, ctx_.log());

        for (int i = 0; i < int(fgimg_cold.desc.views.size()); ++i) {
            const auto &v = fgimg_cold.desc.views[i];
            const int view_index = Image_AddView(ctx_.api(), img_main, img_cold, v.format, v.mip_level, v.mip_count,
                                                 v.base_layer, v.layer_count);
            assert(view_index == i + 1);
        }
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
        // Reset resources
        for (auto it = std::begin(name_to_buffer_); it != std::end(name_to_buffer_); ++it) {
            const auto &[fgbuf_main, fgbuf_cold] = buffers_.GetUnsafe(it->val);
            buffers_.SetGeneration(it->val, 0);
            fgbuf_cold.used_in_stages = {};
        }
        for (auto it = std::begin(name_to_image_); it != std::end(name_to_image_); ++it) {
            const auto &[fgimg_main, fgimg_cold] = images_.GetUnsafe(it->val);
            images_.SetGeneration(it->val, 0);
            fgimg_cold.used_in_stages = {};
        }
        for (int i = 0; i < int(reordered_nodes_.size()); ++i) {
            FgNode *node = reordered_nodes_[i];
            for (const FgResource &res : node->input_) {
                if (res.type == eFgResType::Buffer) {
                    const auto &[fgbuf_main, fgbuf_cold] = buffers_[FgBufROHandle{res.opaque_handle}];
                    if (fgbuf_cold.external) {
                        continue;
                    }
                    const auto &[buf_main, buf_cold] = storages.buffers[fgbuf_main.handle];
                    const Ren::MemAllocation &alloc = buf_cold.alloc;
                    if (alloc.pool == 0xffff) {
                        // this is dedicated allocation
                        continue;
                    }
                    assert(buffer_to_resource[res.opaque_handle.index] != -1);
                    const resource_t &r = all_resources[buffer_to_resource[res.opaque_handle.index]];
                    if (r.lifetime[j][1] == i + 1 &&
                        (i != int(reordered_nodes_.size()) - 1 || r.lifetime[!j][0] != 0)) {
                        deactivated_regions.push_back(region_t{alloc.offset, alloc.block, res});
                    }
                } else if (res.type == eFgResType::Image) {
                    const auto &[fgimg_main, fgimg_cold] = images_[FgImgROHandle{res.opaque_handle}];
                    if (fgimg_cold.external) {
                        continue;
                    }
                    const auto &[img_main, img_cold] = storages.images[fgimg_main.handle_to_own];
                    const Ren::MemAllocation &alloc = img_cold.alloc;
                    if (alloc.pool == 0xffff) {
                        // this is dedicated allocation
                        continue;
                    }
                    assert(image_to_resource[res.opaque_handle.index] != -1);
                    const resource_t &r = all_resources[image_to_resource[res.opaque_handle.index]];
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
                    const auto &[fgbuf_main, fgbuf_cold] = buffers_[FgBufRWHandle{res.opaque_handle}];
                    this_res = &fgbuf_cold;

                    buffers_.SetGeneration(res.opaque_handle.index, res.opaque_handle.generation + 1);
                    if (fgbuf_main.handle) {
                        const auto &[buf_main, buf_cold] = storages.buffers[fgbuf_main.handle];
                        this_alloc = &buf_cold.alloc;
                        if (buffer_to_resource[res.opaque_handle.index] != -1) {
                            const resource_t &r = all_resources[buffer_to_resource[res.opaque_handle.index]];
                            deactivate = (r.lifetime[j][1] == i + 1 &&
                                          (i != int(reordered_nodes_.size()) - 1 || r.lifetime[!j][0] != 0));
                        }
                    }
                } else if (res.type == eFgResType::Image) {
                    const auto &[fgimg_main, fgimg_cold] = images_[FgImgRWHandle{res.opaque_handle}];
                    this_res = &fgimg_cold;

                    images_.SetGeneration(res.opaque_handle.index, res.opaque_handle.generation + 1);
                    if (fgimg_main.handle_to_own) {
                        const auto &[img_main, img_cold] = storages.images[fgimg_main.handle_to_own];
                        this_alloc = &img_cold.alloc;
                        if (image_to_resource[res.opaque_handle.index] != -1) {
                            const resource_t &r = all_resources[image_to_resource[res.opaque_handle.index]];
                            deactivate = (r.lifetime[j][1] == i + 1 &&
                                          (i != int(reordered_nodes_.size()) - 1 || r.lifetime[!j][0] != 0));
                        }
                    }
                }

                if (!this_res || this_res->external) {
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
                        if (it->res.type == res.type && it->res.index == res.opaque_handle.index) {
                            it = deactivated_regions.erase(it);
                        } else {
                            ++it;
                        }
                    }
                    // Find overlapping regions
                    for (auto it = begin(deactivated_regions); it != end(deactivated_regions);) {
                        assert(it->res.type != res.type || it->res.index != res.opaque_handle.index);
                        FgAllocRes *other_res = nullptr;
                        if (it->res.type == eFgResType::Buffer) {
                            const auto &[fgbuf_main, fgbuf_cold] = buffers_.GetUnsafe(it->res.index);
                            other_res = &fgbuf_cold;

                            const Ren::BufferCold &buf_cold = storages.buffers[fgbuf_main.handle].second;
                            if (buf_cold.alloc.pool != this_alloc->pool) {
                                ++it;
                                continue;
                            }
                        } else if (it->res.type == eFgResType::Image) {
                            const auto &[fgimg_main, fgimg_cold] = images_.GetUnsafe(it->res.index);
                            other_res = &fgimg_cold;

                            const Ren::ImageCold &img_cold = storages.images[fgimg_main.handle_to_own].second;
                            if (img_cold.alloc.pool != this_alloc->pool) {
                                ++it;
                                continue;
                            }
                        }
                        if (it->offset < this_alloc->offset + this_alloc->block &&
                            it->offset + it->size > this_alloc->offset) {
                            if (!std::exchange(it->barrier_placed, true)) {
                                insert_sorted(this_res->overlaps_with, it->res);
                            }
                            other_res->aliased_in_stages |= StagesForState(res.desired_state);

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
    const Ren::StoragesRef &storages = ctx_.storages();
    Ren::CommandBuffer cmd_buf = ctx_.BegTempSingleTimeCommands();
    //
    // Simulate execution over 2 frames, but perform clear instead of actual work
    //
    for (int j = 0; j < 2; ++j) {
        Ren::DebugMarker exec_marker(ctx_.api(), cmd_buf, "Eng::FgBuilder::ClearResources_MemHeaps");

        // Swap history images
        for (auto it = std::begin(name_to_image_); it != std::end(name_to_image_); ++it) {
            const auto &[fgimg_main, fgimg_cold] = images_.GetUnsafe(it->val);
            if (fgimg_cold.history_index != 0xffff) {
                const auto &[hist_main, hist_cold] = images_.GetUnsafe(fgimg_cold.history_index);
                if (hist_main.handle_to_use) {
                    assert(hist_cold.lifetime.is_used());
                    std::swap(fgimg_main.handle_to_use, hist_main.handle_to_use);
                }
            }
        }
        // Reset resources
        for (auto it = std::begin(name_to_buffer_); it != std::end(name_to_buffer_); ++it) {
            buffers_.SetGeneration(it->val, 0);

            const auto &[fgbuf_main, fgbuf_cold] = buffers_.GetUnsafe(it->val);
            fgbuf_cold.used_in_stages = {};
            if (fgbuf_main.handle) {
                const auto &[buf_main, buf_cold] = storages.buffers[fgbuf_main.handle];
                fgbuf_cold.used_in_stages = StagesForState(buf_main.resource_state);
            }
        }
        for (auto it = std::begin(name_to_image_); it != std::end(name_to_image_); ++it) {
            images_.SetGeneration(it->val, 0);

            const auto &[fgimg_main, fgimg_cold] = images_.GetUnsafe(it->val);
            fgimg_cold.used_in_stages = {};
            if (fgimg_main.handle_to_use) {
                const auto &[img_main, img_cold] = storages.images[fgimg_main.handle_to_use];
                fgimg_cold.used_in_stages = StagesForState(img_main.resource_state);
            }
        }

        BuildResourceLinkedLists();

#if defined(REN_GL_BACKEND)
        rast_state_.Apply();
#endif

        for (int i = 0; i < int(reordered_nodes_.size()); ++i) {
            const FgNode *node = reordered_nodes_[i];

            Ren::DebugMarker _(ctx_.api(), cmd_buf, node->name());

            Ren::SmallVector<Ren::TransitionInfo, 32> res_transitions;
            Ren::Bitmask<Ren::eStage> src_stages, dst_stages;

            std::vector<Ren::BufferHandle> bufs_to_clear;
            std::vector<Ren::ImageHandle> imgs_to_clear;

            for (const FgResource &res : node->output_) {
                if (res.type == eFgResType::Buffer) {
                    const auto &[fgbuf_main, fgbuf_cold] = buffers_[FgBufROHandle{res.opaque_handle}];
                    if (!fgbuf_cold.external) {
                        bufs_to_clear.push_back(fgbuf_main.handle);
                        HandleResourceTransition(res, res_transitions, src_stages, dst_stages);
                    }
                    buffers_.SetGeneration(res.opaque_handle.index, res.opaque_handle.generation + 1);
                } else if (res.type == eFgResType::Image) {
                    const auto &[fgimg_main, fgimg_cold] = images_[FgImgROHandle{res.opaque_handle}];
                    if (!fgimg_cold.external) {
                        imgs_to_clear.push_back(fgimg_main.handle_to_use);
                        HandleResourceTransition(res, res_transitions, src_stages, dst_stages);
                    }
                    images_.SetGeneration(res.opaque_handle.index, res.opaque_handle.generation + 1);
                }
            }
            TransitionResourceStates(ctx_.api(), storages, cmd_buf, Ren::AllStages, Ren::AllStages, res_transitions);
            for (const Ren::BufferHandle buf : bufs_to_clear) {
                const Ren::BufferMain &buf_main = storages.buffers[buf].first;
                if (buf_main.resource_state == Ren::eResState::CopyDst) {
                    ClearBuffer_AsTransfer(buf, cmd_buf);
                } else if (buf_main.resource_state == Ren::eResState::UnorderedAccess) {
                    ClearBuffer_AsStorage(buf, cmd_buf);
                } else if (buf_main.resource_state == Ren::eResState::BuildASWrite) {
                    // NOTE: Skipped
                } else {
                    assert(false);
                }
            }
            for (Ren::ImageHandle img : imgs_to_clear) {
                const Ren::ImageMain &img_main = storages.images[img].first;
                if (img_main.resource_state == Ren::eResState::CopyDst) {
                    ClearImage_AsTransfer(img, cmd_buf);
                } else if (img_main.resource_state == Ren::eResState::UnorderedAccess) {
                    ClearImage_AsStorage(img, cmd_buf);
                } else if (img_main.resource_state == Ren::eResState::RenderTarget ||
                           img_main.resource_state == Ren::eResState::DepthWrite) {
                    ClearImage_AsTarget(img, cmd_buf);
                } else {
                    assert(false);
                }
            }
        }
    }
    ctx_.EndTempSingleTimeCommands(cmd_buf);
}

void Eng::FgBuilder::ReleaseMemHeaps() {
    const Ren::ApiContext &api = ctx_.api();
    api.vkDeviceWaitIdle(api.device);
    for (Ren::MemHeap &heap : memory_heaps_) {
        api.vkFreeMemory(api.device, heap.mem, nullptr);
    }
    memory_heaps_.clear();
}
