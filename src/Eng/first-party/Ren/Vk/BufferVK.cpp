#include "../Buffer.h"

#include <algorithm>
#include <cassert>

#include "../Config.h"
#include "../Log.h"
#include "VKCtx.h"

namespace Ren {
extern const VkFormat g_formats_vk[];

VkBufferUsageFlags GetVkBufferUsageFlags(const ApiContext &api, const eBufType type) {
    VkBufferUsageFlags flags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    if (type == eBufType::VertexAttribs) {
        flags |= (VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    } else if (type == eBufType::VertexIndices) {
        flags |= (VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    } else if (type == eBufType::Texture) {
        flags |= (VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT |
                  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    } else if (type == eBufType::Uniform) {
        flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    } else if (type == eBufType::Storage) {
        flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    } else if (type == eBufType::Upload) {
        flags &= ~VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    } else if (type == eBufType::Readback) {
        flags &= ~VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    } else if (type == eBufType::AccStructure) {
        flags |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    } else if (type == eBufType::ShaderBinding) {
        flags |= VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    } else if (type == eBufType::Indirect) {
        flags |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    }

    if ((type == eBufType::VertexAttribs || type == eBufType::VertexIndices || type == eBufType::Storage ||
         type == eBufType::Indirect)) {
        flags |= VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;
        if (api.raytracing_supported) {
            flags |= (VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                      VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR);
        }
    }

    return flags;
}

VkMemoryPropertyFlags GetVkMemoryPropertyFlags(const eBufType type) {
    if (type == eBufType::Upload) {
        return (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    } else if (type == eBufType::Readback) {
        return (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT |
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    }
    return VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
}

uint32_t FindMemoryType(uint32_t search_from, const VkPhysicalDeviceMemoryProperties *mem_properties,
                        uint32_t mem_type_bits, VkMemoryPropertyFlags desired_mem_flags, VkDeviceSize desired_size);
} // namespace Ren

bool Ren::Buffer_Init(const ApiContext &api, BufferMain &buf_main, BufferCold &buf_cold, String name,
                      const eBufType type, const uint32_t initial_size, ILog *log, const uint32_t size_alignment,
                      MemAllocators *mem_allocs) {
    buf_cold.name = std::move(name);
    buf_cold.type = type;
    buf_cold.size = 0;
    buf_cold.size_alignment = size_alignment;
    buf_cold.mem_allocs = mem_allocs;

    return Buffer_Resize(api, buf_main, buf_cold, initial_size, log);
}

bool Ren::Buffer_Init(const ApiContext &api, BufferCold &buf_cold, String name, const eBufType type,
                      MemAllocation &&alloc, const uint32_t initial_size, ILog *log, const uint32_t size_alignment) {
    buf_cold.name = std::move(name);
    buf_cold.type = type;
    buf_cold.alloc = std::move(alloc);
    buf_cold.size = initial_size;
    buf_cold.size_alignment = size_alignment;

    return true;
}

void Ren::Buffer_Destroy(const ApiContext &api, BufferMain &buf_main, BufferCold &buf_cold) {
    assert(buf_cold.mapped_offset == 0xffffffff && !buf_cold.mapped_ptr);
    if (buf_main.buf != VK_NULL_HANDLE) {
        api.bufs_to_destroy[api.backend_frame].push_back(buf_main.buf);
        for (auto view : buf_main.views) {
            api.buf_views_to_destroy[api.backend_frame].push_back(view.second);
        }
        if (buf_cold.alloc) {
            api.allocations_to_free[api.backend_frame].emplace_back(std::move(buf_cold.alloc));
        }
        if (buf_cold.dedicated_mem) {
            api.mem_to_free[api.backend_frame].push_back(buf_cold.dedicated_mem);
        }
    }
    buf_main = {};
    buf_cold = {};
}

void Ren::Buffer_DestroyImmediately(const ApiContext &api, BufferMain &buf_main, BufferCold &buf_cold) {
    assert(buf_cold.mapped_offset == 0xffffffff && !buf_cold.mapped_ptr);
    if (buf_main.buf != VK_NULL_HANDLE) {
        api.vkDestroyBuffer(api.device, buf_main.buf, nullptr);
        for (auto view : buf_main.views) {
            api.vkDestroyBufferView(api.device, view.second, nullptr);
        }
        buf_cold.alloc = {};
        if (buf_cold.dedicated_mem) {
            api.vkFreeMemory(api.device, buf_cold.dedicated_mem, nullptr);
        }
    }
    buf_main = {};
    buf_cold = {};
}

bool Ren::Buffer_Resize(const ApiContext &api, BufferMain &buf_main, BufferCold &buf_cold, uint32_t new_size, ILog *log,
                        const bool keep_content, const bool release_immediately) {
    new_size = RoundUp(new_size, buf_cold.size_alignment);
    if (buf_cold.size == new_size) {
        return true;
    }

    const uint32_t old_size = buf_cold.size;

    buf_cold.size = new_size;
    assert(buf_cold.size > 0);

    if (buf_cold.sub_alloc) {
        buf_cold.sub_alloc->ResizePool(0, buf_cold.size);
        assert(buf_cold.sub_alloc->IntegrityCheck());
    }

    VkBufferCreateInfo buf_create_info = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    buf_create_info.size = VkDeviceSize(new_size);
    buf_create_info.usage = GetVkBufferUsageFlags(api, buf_cold.type);
    buf_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer new_buf = {};
    VkResult res = api.vkCreateBuffer(api.device, &buf_create_info, nullptr, &new_buf);
    if (res != VK_SUCCESS) {
        log->Error("Failed to create Buffer!");
        return false;
    }

#ifdef ENABLE_GPU_DEBUG
    VkDebugUtilsObjectNameInfoEXT name_info = {VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
    name_info.objectType = VK_OBJECT_TYPE_BUFFER;
    name_info.objectHandle = uint64_t(new_buf);
    name_info.pObjectName = buf_cold.name.c_str();
    api.vkSetDebugUtilsObjectNameEXT(api.device, &name_info);
#endif

    VkMemoryRequirements memory_requirements = {};
    api.vkGetBufferMemoryRequirements(api.device, new_buf, &memory_requirements);
    if (api.raytracing_supported && buf_cold.type == eBufType::Storage) {
        // Account for acceleration structure scratch usage
        memory_requirements.alignment = std::max<VkDeviceSize>(
            memory_requirements.alignment, api.acc_props.minAccelerationStructureScratchOffsetAlignment);
    }

    VkMemoryPropertyFlags memory_props = GetVkMemoryPropertyFlags(buf_cold.type);

    VkDeviceMemory new_dedicated_mem = {};
    MemAllocation new_allocation = {};
    if (buf_cold.mem_allocs && buf_cold.type != eBufType::Upload && buf_cold.type != eBufType::Readback) {
        new_allocation = buf_cold.mem_allocs->Allocate(memory_requirements, memory_props);
        if (!new_allocation) {
            log->Warning("Not enough device memory, falling back to CPU RAM!");
            memory_props &= ~VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
            new_allocation = buf_cold.mem_allocs->Allocate(memory_requirements, memory_props);
        }
        if (!new_allocation) {
            log->Error("Failed to allocate memory!");
            return false;
        }

        res = api.vkBindBufferMemory(api.device, new_buf, new_allocation.owner->mem(new_allocation.pool),
                                     new_allocation.offset);
        if (res != VK_SUCCESS) {
            log->Error("Failed to bind memory!");
            return false;
        }
    } else {
        // Do a dedicated allocation
        VkMemoryAllocateInfo mem_alloc_info = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        mem_alloc_info.allocationSize = memory_requirements.size;
        mem_alloc_info.memoryTypeIndex = FindMemoryType(0, &api.mem_properties, memory_requirements.memoryTypeBits,
                                                        memory_props, mem_alloc_info.allocationSize);

        VkMemoryAllocateFlagsInfoKHR additional_flags = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO_KHR};

        if ((buf_create_info.usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) != 0) {
            additional_flags.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
            mem_alloc_info.pNext = &additional_flags;
        }

        res = VK_ERROR_OUT_OF_DEVICE_MEMORY;
        while (mem_alloc_info.memoryTypeIndex != 0xffffffff) {
            res = api.vkAllocateMemory(api.device, &mem_alloc_info, nullptr, &new_dedicated_mem);
            if (res == VK_SUCCESS) {
                break;
            }
            mem_alloc_info.memoryTypeIndex =
                FindMemoryType(mem_alloc_info.memoryTypeIndex + 1, &api.mem_properties,
                               memory_requirements.memoryTypeBits, memory_props, mem_alloc_info.allocationSize);
        }
        if (res == VK_ERROR_OUT_OF_DEVICE_MEMORY) {
            memory_props &= ~VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

            mem_alloc_info.memoryTypeIndex = FindMemoryType(0, &api.mem_properties, memory_requirements.memoryTypeBits,
                                                            memory_props, mem_alloc_info.allocationSize);
            while (mem_alloc_info.memoryTypeIndex != 0xffffffff) {
                res = api.vkAllocateMemory(api.device, &mem_alloc_info, nullptr, &new_dedicated_mem);
                if (res == VK_SUCCESS) {
                    break;
                }
                mem_alloc_info.memoryTypeIndex =
                    FindMemoryType(mem_alloc_info.memoryTypeIndex + 1, &api.mem_properties,
                                   memory_requirements.memoryTypeBits, memory_props, mem_alloc_info.allocationSize);
            }
        }
        if (res != VK_SUCCESS) {
            log->Error("Failed to allocated memory!");
            return false;
        }

        res = api.vkBindBufferMemory(api.device, new_buf, new_dedicated_mem, 0 /* offset */);
        if (res != VK_SUCCESS) {
            log->Error("Failed to bind memory!");
            return false;
        }
    }

    auto views = std::move(buf_main.views);

    if (buf_main.buf != VK_NULL_HANDLE) {
        if (keep_content) {
            VkCommandBuffer cmd_buf = api.BegSingleTimeCommands();

            if (buf_main.resource_state != eResState::Undefined && buf_main.resource_state != eResState::CopySrc) {
                VkBufferMemoryBarrier barrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
                barrier.srcAccessMask = VKAccessFlagsForState(buf_main.resource_state);
                barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.buffer = buf_main.buf;
                barrier.offset = 0;
                barrier.size = VK_WHOLE_SIZE;

                const VkPipelineStageFlags src_stages =
                    VKPipelineStagesForState(buf_main.resource_state) & api.supported_stages_mask;
                const VkPipelineStageFlags dst_stages =
                    VKPipelineStagesForState(eResState::CopySrc) & api.supported_stages_mask;

                api.vkCmdPipelineBarrier(cmd_buf, src_stages ? src_stages : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                         dst_stages, 0, 0, nullptr, 1, &barrier, 0, nullptr);
            }

            VkBufferCopy region_to_copy = {};
            region_to_copy.size = VkDeviceSize{old_size};

            api.vkCmdCopyBuffer(cmd_buf, buf_main.buf, new_buf, 1, &region_to_copy);

            api.EndSingleTimeCommands(cmd_buf);

            // destroy previous buffer
            api.vkDestroyBuffer(api.device, buf_main.buf, nullptr);
            for (auto view : views) {
                api.vkDestroyBufferView(api.device, view.second, nullptr);
            }
            buf_cold.alloc = {};
            if (buf_cold.dedicated_mem) {
                api.vkFreeMemory(api.device, buf_cold.dedicated_mem, nullptr);
            }
        } else {
            // destroy previous buffer
            if (release_immediately) {
                api.vkDestroyBuffer(api.device, buf_main.buf, nullptr);
                for (auto view : views) {
                    api.vkDestroyBufferView(api.device, view.second, nullptr);
                }
                buf_cold.alloc = {};
                if (buf_cold.dedicated_mem) {
                    api.vkFreeMemory(api.device, buf_cold.dedicated_mem, nullptr);
                }
            } else {
                api.bufs_to_destroy[api.backend_frame].push_back(buf_main.buf);
                for (auto view : views) {
                    api.buf_views_to_destroy[api.backend_frame].push_back(view.second);
                }
                if (buf_cold.alloc) {
                    api.allocations_to_free[api.backend_frame].emplace_back(std::move(buf_cold.alloc));
                }
                if (buf_cold.dedicated_mem) {
                    api.mem_to_free[api.backend_frame].push_back(buf_cold.dedicated_mem);
                }
            }
        }
    }

    buf_main.buf = new_buf;
    for (auto view : views) {
        Buffer_AddView(api, buf_main, buf_cold, view.first);
    }
    buf_cold.alloc = std::move(new_allocation);
    buf_cold.dedicated_mem = new_dedicated_mem;

    return true;
}

int Ren::Buffer_AddView(const ApiContext &api, BufferMain &buf_main, BufferCold &buf_cold, const eFormat format) {
    VkBufferViewCreateInfo view_info = {VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO};
    view_info.buffer = buf_main.buf;
    view_info.format = g_formats_vk[size_t(format)];
    view_info.offset = VkDeviceSize(0);
    view_info.range = VK_WHOLE_SIZE;

    VkBufferView buf_view = {};
    const VkResult res = api.vkCreateBufferView(api.device, &view_info, nullptr, &buf_view);
    if (res != VK_SUCCESS) {
        return -1;
    }

    buf_main.views.emplace_back(format, buf_view);
    return int(buf_main.views.size()) - 1;
}

uint8_t *Ren::Buffer_MapRange(const ApiContext &api, BufferMain &buf_main, BufferCold &buf_cold, const uint32_t offset,
                              const uint32_t size, const bool persistent) {
    const uint32_t align_to = uint32_t(api.device_properties.limits.nonCoherentAtomSize);

    assert(buf_cold.dedicated_mem);
    assert(buf_cold.mapped_offset == 0xffffffff && !buf_cold.mapped_ptr);
    assert(offset + size <= buf_cold.size);
    assert(buf_cold.type == eBufType::Upload || buf_cold.type == eBufType::Readback);
    assert(offset == RoundDown(offset, align_to));
    assert((offset + size) == buf_cold.size || offset == RoundUp(offset, align_to));

    void *mapped = nullptr;
    const VkResult res =
        api.vkMapMemory(api.device, buf_cold.dedicated_mem, VkDeviceSize(offset), VkDeviceSize(size), 0, &mapped);
    if (res != VK_SUCCESS) {
        assert(false && "Failed to map memory!");
        return nullptr;
    }

    buf_cold.mapped_ptr = reinterpret_cast<uint8_t *>(mapped);
    buf_cold.mapped_offset = offset;

    return reinterpret_cast<uint8_t *>(mapped);
}

void Ren::Buffer_Unmap(const ApiContext &api, BufferMain &buf_main, BufferCold &buf_cold) {
    assert(buf_cold.dedicated_mem);
    assert(buf_cold.mapped_offset != 0xffffffff && buf_cold.mapped_ptr);
    api.vkUnmapMemory(api.device, buf_cold.dedicated_mem);
    buf_cold.mapped_ptr = nullptr;
    buf_cold.mapped_offset = 0xffffffff;
}

Ren::SubAllocation Ren::Buffer_AllocSubRegion(const ApiContext &api, BufferMain &buf_main, BufferCold &buf_cold,
                                              const uint32_t req_size, const uint32_t req_alignment,
                                              std::string_view tag, ILog *log, const BufferMain *init_buf,
                                              CommandBuffer cmd_buf, const uint32_t init_off) {
    if (!buf_cold.sub_alloc) {
        buf_cold.sub_alloc = std::make_unique<FreelistAlloc>(buf_cold.size);
    }

    FreelistAlloc::Allocation alloc = buf_cold.sub_alloc->Alloc(req_alignment, req_size);
    if (alloc.pool == 0xffff) {
        return {};
    }

    assert(alloc.pool == 0);
    assert(buf_cold.sub_alloc->IntegrityCheck());
    const SubAllocation ret = {alloc.offset, alloc.block};
    if (ret.offset != 0xffffffff) {
        if (init_buf) {
            Buffer_UpdateSubRegion(api, buf_main, buf_cold, ret.offset, req_size, *init_buf, init_off, cmd_buf);
        }
    }
    return ret;
}

void Ren::Buffer_UpdateSubRegion(const ApiContext &api, BufferMain &buf_main, BufferCold &buf_cold,
                                 const uint32_t offset, const uint32_t size, const BufferMain &init_buf,
                                 const uint32_t init_off, CommandBuffer cmd_buf) {
    VkPipelineStageFlags src_stages = 0, dst_stages = 0;
    SmallVector<VkBufferMemoryBarrier, 2> barriers;

    if (init_buf.resource_state != eResState::Undefined && init_buf.resource_state != eResState::CopySrc) {
        auto &new_barrier = barriers.emplace_back();
        new_barrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
        new_barrier.srcAccessMask = VKAccessFlagsForState(init_buf.resource_state);
        new_barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        new_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        new_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        new_barrier.buffer = init_buf.buf;
        new_barrier.offset = VkDeviceSize{init_off};
        new_barrier.size = VkDeviceSize{size};

        src_stages |= VKPipelineStagesForState(init_buf.resource_state);
        dst_stages |= VKPipelineStagesForState(eResState::CopySrc);
    }

    if (buf_main.resource_state != eResState::Undefined && buf_main.resource_state != eResState::CopyDst) {
        auto &new_barrier = barriers.emplace_back();
        new_barrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
        new_barrier.srcAccessMask = VKAccessFlagsForState(buf_main.resource_state);
        new_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        new_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        new_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        new_barrier.buffer = buf_main.buf;
        new_barrier.offset = VkDeviceSize{offset};
        new_barrier.size = VkDeviceSize{size};

        src_stages |= VKPipelineStagesForState(buf_main.resource_state);
        dst_stages |= VKPipelineStagesForState(eResState::CopyDst);
    }

    src_stages &= api.supported_stages_mask;
    dst_stages &= api.supported_stages_mask;

    if (!barriers.empty()) {
        api.vkCmdPipelineBarrier(cmd_buf, src_stages, dst_stages, 0, 0, nullptr, uint32_t(barriers.size()),
                                 barriers.cdata(), 0, nullptr);
    }

    const VkBufferCopy region_to_copy = {
        VkDeviceSize{init_off}, // srcOffset
        VkDeviceSize{offset},   // dstOffset
        VkDeviceSize{size}      // size
    };

    api.vkCmdCopyBuffer(cmd_buf, init_buf.buf, buf_main.buf, 1, &region_to_copy);

    init_buf.resource_state = eResState::CopySrc;
    buf_main.resource_state = eResState::CopyDst;
}

bool Ren::Buffer_FreeSubRegion(BufferCold &buf_cold, const SubAllocation alloc) {
    buf_cold.sub_alloc->Free(alloc.block);
    assert(buf_cold.sub_alloc->IntegrityCheck());
    return true;
}

void Ren::Buffer_Fill(const ApiContext &api, BufferMain &buf_main, const uint32_t dst_offset, const uint32_t size,
                      const uint32_t data, CommandBuffer cmd_buf) {
    VkPipelineStageFlags src_stages = 0, dst_stages = 0;
    SmallVector<VkBufferMemoryBarrier, 1> barriers;

    if (buf_main.resource_state != eResState::CopyDst) {
        auto &new_barrier = barriers.emplace_back();
        new_barrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
        new_barrier.srcAccessMask = VKAccessFlagsForState(buf_main.resource_state);
        new_barrier.dstAccessMask = VKAccessFlagsForState(eResState::CopyDst);
        new_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        new_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        new_barrier.buffer = buf_main.buf;
        new_barrier.offset = VkDeviceSize{dst_offset};
        new_barrier.size = VkDeviceSize{size};

        src_stages |= VKPipelineStagesForState(buf_main.resource_state);
        dst_stages |= VKPipelineStagesForState(eResState::CopyDst);
    }

    src_stages &= api.supported_stages_mask;
    dst_stages &= api.supported_stages_mask;

    if (!barriers.empty()) {
        api.vkCmdPipelineBarrier(cmd_buf, src_stages, dst_stages, 0, 0, nullptr, barriers.size(), barriers.cdata(), 0,
                                 nullptr);
    }

    api.vkCmdFillBuffer(cmd_buf, buf_main.buf, VkDeviceSize{dst_offset}, VkDeviceSize{size}, data);

    buf_main.resource_state = eResState::CopyDst;
}

void Ren::Buffer_UpdateInPlace(const ApiContext &api, BufferMain &buf_main, uint32_t dst_offset, uint32_t size,
                               const void *data, CommandBuffer cmd_buf) {
    VkPipelineStageFlags src_stages = 0, dst_stages = 0;
    SmallVector<VkBufferMemoryBarrier, 1> barriers;

    if (buf_main.resource_state != eResState::CopyDst) {
        auto &new_barrier = barriers.emplace_back();
        new_barrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
        new_barrier.srcAccessMask = VKAccessFlagsForState(buf_main.resource_state);
        new_barrier.dstAccessMask = VKAccessFlagsForState(eResState::CopyDst);
        new_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        new_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        new_barrier.buffer = buf_main.buf;
        new_barrier.offset = VkDeviceSize{dst_offset};
        new_barrier.size = VkDeviceSize{size};

        src_stages |= VKPipelineStagesForState(buf_main.resource_state);
        dst_stages |= VKPipelineStagesForState(eResState::CopyDst);
    }

    src_stages &= api.supported_stages_mask;
    dst_stages &= api.supported_stages_mask;

    if (!barriers.empty()) {
        api.vkCmdPipelineBarrier(cmd_buf, src_stages, dst_stages, 0, 0, nullptr, barriers.size(), barriers.cdata(), 0,
                                 nullptr);
    }

    api.vkCmdUpdateBuffer(cmd_buf, buf_main.buf, VkDeviceSize{dst_offset}, VkDeviceSize{size}, data);

    buf_main.resource_state = eResState::CopyDst;
}

VkDeviceAddress Ren::Buffer_GetDeviceAddress(const ApiContext &api, const BufferMain &buf_main) {
    VkBufferDeviceAddressInfo addr_info = {VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
    addr_info.buffer = buf_main.buf;
    return api.vkGetBufferDeviceAddressKHR(api.device, &addr_info);
}

void Ren::CopyBufferToBuffer(const ApiContext &api, const BufferMain &src, const uint32_t src_offset, BufferMain &dst,
                             const uint32_t dst_offset, const uint32_t size, CommandBuffer cmd_buf) {
    VkPipelineStageFlags src_stages = 0, dst_stages = 0;
    SmallVector<VkBufferMemoryBarrier, 2> barriers;

    if (src.resource_state != eResState::Undefined && src.resource_state != eResState::CopySrc) {
        auto &new_barrier = barriers.emplace_back();
        new_barrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
        new_barrier.srcAccessMask = VKAccessFlagsForState(src.resource_state);
        new_barrier.dstAccessMask = VKAccessFlagsForState(eResState::CopySrc);
        new_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        new_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        new_barrier.buffer = src.buf;
        new_barrier.offset = VkDeviceSize{src_offset};
        new_barrier.size = VkDeviceSize{size};

        src_stages |= VKPipelineStagesForState(src.resource_state);
        dst_stages |= VKPipelineStagesForState(eResState::CopySrc);
    }

    if (dst.resource_state != eResState::Undefined && dst.resource_state != eResState::CopyDst) {
        auto &new_barrier = barriers.emplace_back();
        new_barrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
        new_barrier.srcAccessMask = VKAccessFlagsForState(dst.resource_state);
        new_barrier.dstAccessMask = VKAccessFlagsForState(eResState::CopyDst);
        new_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        new_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        new_barrier.buffer = dst.buf;
        new_barrier.offset = VkDeviceSize{dst_offset};
        new_barrier.size = VkDeviceSize{size};

        src_stages |= VKPipelineStagesForState(dst.resource_state);
        dst_stages |= VKPipelineStagesForState(eResState::CopyDst);
    }

    src_stages &= api.supported_stages_mask;
    dst_stages &= api.supported_stages_mask;

    if (!barriers.empty()) {
        api.vkCmdPipelineBarrier(cmd_buf, src_stages, dst_stages, 0, 0, nullptr, barriers.size(), barriers.cdata(), 0,
                                 nullptr);
    }

    VkBufferCopy region_to_copy = {};
    region_to_copy.srcOffset = VkDeviceSize{src_offset};
    region_to_copy.dstOffset = VkDeviceSize{dst_offset};
    region_to_copy.size = VkDeviceSize{size};

    api.vkCmdCopyBuffer(cmd_buf, src.buf, dst.buf, 1, &region_to_copy);

    src.resource_state = eResState::CopySrc;
    dst.resource_state = eResState::CopyDst;
}

void Ren::CopyBufferToBuffer(const ApiContext &api, const StoragesRef &storages, const BufferROHandle src,
                             const uint32_t src_offset, const BufferHandle dst, const uint32_t dst_offset,
                             const uint32_t size, CommandBuffer cmd_buf) {
    const auto &[src_main, src_cold] = storages.buffers[src];
    const auto &[dst_main, dst_cold] = storages.buffers[dst];

    VkPipelineStageFlags src_stages = 0, dst_stages = 0;
    SmallVector<VkBufferMemoryBarrier, 2> barriers;

    if (src_main.resource_state != eResState::Undefined && src_main.resource_state != eResState::CopySrc) {
        auto &new_barrier = barriers.emplace_back();
        new_barrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
        new_barrier.srcAccessMask = VKAccessFlagsForState(src_main.resource_state);
        new_barrier.dstAccessMask = VKAccessFlagsForState(eResState::CopySrc);
        new_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        new_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        new_barrier.buffer = src_main.buf;
        new_barrier.offset = VkDeviceSize{src_offset};
        new_barrier.size = VkDeviceSize{size};

        src_stages |= VKPipelineStagesForState(src_main.resource_state);
        dst_stages |= VKPipelineStagesForState(eResState::CopySrc);
    }

    if (dst_main.resource_state != eResState::Undefined && dst_main.resource_state != eResState::CopyDst) {
        auto &new_barrier = barriers.emplace_back();
        new_barrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
        new_barrier.srcAccessMask = VKAccessFlagsForState(dst_main.resource_state);
        new_barrier.dstAccessMask = VKAccessFlagsForState(eResState::CopyDst);
        new_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        new_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        new_barrier.buffer = dst_main.buf;
        new_barrier.offset = VkDeviceSize{dst_offset};
        new_barrier.size = VkDeviceSize{size};

        src_stages |= VKPipelineStagesForState(dst_main.resource_state);
        dst_stages |= VKPipelineStagesForState(eResState::CopyDst);
    }

    src_stages &= api.supported_stages_mask;
    dst_stages &= api.supported_stages_mask;

    if (!barriers.empty()) {
        api.vkCmdPipelineBarrier(cmd_buf, src_stages, dst_stages, 0, 0, nullptr, barriers.size(), barriers.cdata(), 0,
                                 nullptr);
    }

    VkBufferCopy region_to_copy = {};
    region_to_copy.srcOffset = VkDeviceSize{src_offset};
    region_to_copy.dstOffset = VkDeviceSize{dst_offset};
    region_to_copy.size = VkDeviceSize{size};

    api.vkCmdCopyBuffer(cmd_buf, src_main.buf, dst_main.buf, 1, &region_to_copy);
}