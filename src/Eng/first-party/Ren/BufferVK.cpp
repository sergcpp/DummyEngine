#include "Buffer.h"

#include <algorithm>
#include <cassert>

#include "Log.h"
#include "VKCtx.h"

namespace Ren {
VkBufferUsageFlags GetVkBufferUsageFlags(const ApiContext *api_ctx, const eBufType type) {
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
    } else if (type == eBufType::Readback) {
    } else if (type == eBufType::AccStructure) {
        flags |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR;
    } else if (type == eBufType::ShaderBinding) {
        flags |= VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    } else if (type == eBufType::Indirect) {
        flags |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    }

    if ((type == eBufType::VertexAttribs || type == eBufType::VertexIndices || type == eBufType::Storage ||
         type == eBufType::Indirect)) {
        flags |= VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;
        if (api_ctx->raytracing_supported) {
            flags |= (VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                      VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR);
        }
    }

    return flags;
}

VkMemoryPropertyFlags GetVkMemoryPropertyFlags(const eBufType type) {
    if (type == eBufType::Upload || type == eBufType::Readback) {
        return (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT |
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    }
    return VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
}

uint32_t FindMemoryType(uint32_t search_from, const VkPhysicalDeviceMemoryProperties *mem_properties,
                        uint32_t mem_type_bits, VkMemoryPropertyFlags desired_mem_flags, VkDeviceSize desired_size);
} // namespace Ren

int Ren::Buffer::g_GenCounter = 0;

Ren::Buffer::Buffer(std::string_view name, ApiContext *api_ctx, const eBufType type, const uint32_t initial_size,
                    const uint32_t size_alignment, MemoryAllocators *mem_allocs)
    : name_(name), api_ctx_(api_ctx), mem_allocs_(mem_allocs), type_(type), size_(0), size_alignment_(size_alignment) {
    Resize(initial_size);
}

Ren::Buffer::~Buffer() { Free(); }

Ren::Buffer &Ren::Buffer::operator=(Buffer &&rhs) noexcept {
    RefCounter::operator=(static_cast<RefCounter &&>(rhs));

    Free();

    assert(!mapped_ptr_);
    assert(mapped_offset_ == 0xffffffff);

    api_ctx_ = std::exchange(rhs.api_ctx_, nullptr);
    handle_ = std::exchange(rhs.handle_, {});
    name_ = std::move(rhs.name_);
    mem_allocs_ = std::exchange(rhs.mem_allocs_, nullptr);
    sub_alloc_ = std::move(rhs.sub_alloc_);
    alloc_ = std::move(rhs.alloc_);
    dedicated_mem_ = std::exchange(rhs.dedicated_mem_, {});

    type_ = std::exchange(rhs.type_, eBufType::Undefined);

    size_ = std::exchange(rhs.size_, 0);
    size_alignment_ = std::exchange(rhs.size_alignment_, 0);
    mapped_ptr_ = std::exchange(rhs.mapped_ptr_, nullptr);
    mapped_offset_ = std::exchange(rhs.mapped_offset_, 0xffffffff);

    resource_state = std::exchange(rhs.resource_state, eResState::Undefined);

    return (*this);
}

VkDeviceAddress Ren::Buffer::vk_device_address() const {
    VkBufferDeviceAddressInfo addr_info = {VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
    addr_info.buffer = handle_.buf;
    return api_ctx_->vkGetBufferDeviceAddressKHR(api_ctx_->device, &addr_info);
}

Ren::SubAllocation Ren::Buffer::AllocSubRegion(const uint32_t req_size, const uint32_t req_alignment,
                                               std::string_view tag, const Buffer *init_buf, CommandBuffer cmd_buf,
                                               const uint32_t init_off) {
    if (!sub_alloc_) {
        sub_alloc_ = std::make_unique<FreelistAlloc>(size_);
    }

    FreelistAlloc::Allocation alloc = sub_alloc_->Alloc(req_alignment, req_size);
    while (alloc.pool == 0xffff) {
        const auto new_size = req_alignment * ((uint32_t(size_ * 1.25f) + req_alignment - 1) / req_alignment);
        Resize(new_size);
        alloc = sub_alloc_->Alloc(req_alignment, req_size);
    }
    assert(alloc.pool == 0);
    assert(sub_alloc_->IntegrityCheck());
    const SubAllocation ret = {alloc.offset, alloc.block};
    if (ret.offset != 0xffffffff) {
        if (init_buf) {
            assert(init_buf->type_ == eBufType::Upload);

            VkPipelineStageFlags src_stages = 0, dst_stages = 0;
            SmallVector<VkBufferMemoryBarrier, 2> barriers;

            if (init_buf->resource_state != eResState::Undefined && init_buf->resource_state != eResState::CopySrc) {
                auto &new_barrier = barriers.emplace_back();
                new_barrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
                new_barrier.srcAccessMask = VKAccessFlagsForState(init_buf->resource_state);
                new_barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                new_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                new_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                new_barrier.buffer = init_buf->vk_handle();
                new_barrier.offset = VkDeviceSize{init_off};
                new_barrier.size = VkDeviceSize{req_size};

                src_stages |= VKPipelineStagesForState(init_buf->resource_state);
                dst_stages |= VKPipelineStagesForState(eResState::CopySrc);
            }

            if (this->resource_state != eResState::Undefined && this->resource_state != eResState::CopyDst) {
                auto &new_barrier = barriers.emplace_back();
                new_barrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
                new_barrier.srcAccessMask = VKAccessFlagsForState(this->resource_state);
                new_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                new_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                new_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                new_barrier.buffer = handle_.buf;
                new_barrier.offset = VkDeviceSize{ret.offset};
                new_barrier.size = VkDeviceSize{req_size};

                src_stages |= VKPipelineStagesForState(this->resource_state);
                dst_stages |= VKPipelineStagesForState(eResState::CopyDst);
            }

            src_stages &= api_ctx_->supported_stages_mask;
            dst_stages &= api_ctx_->supported_stages_mask;

            if (!barriers.empty()) {
                api_ctx_->vkCmdPipelineBarrier(cmd_buf, src_stages, dst_stages, 0, 0, nullptr,
                                               uint32_t(barriers.size()), barriers.cdata(), 0, nullptr);
            }

            VkBufferCopy region_to_copy = {};
            region_to_copy.srcOffset = VkDeviceSize{init_off};
            region_to_copy.dstOffset = VkDeviceSize{ret.offset};
            region_to_copy.size = VkDeviceSize{req_size};

            api_ctx_->vkCmdCopyBuffer(cmd_buf, init_buf->handle_.buf, handle_.buf, 1, &region_to_copy);

            init_buf->resource_state = eResState::CopySrc;
            this->resource_state = eResState::CopyDst;
        }
    }
    return ret;
}

void Ren::Buffer::UpdateSubRegion(const uint32_t offset, const uint32_t size, const Buffer &init_buf,
                                  const uint32_t init_off, CommandBuffer cmd_buf) {
    assert(init_buf.type_ == eBufType::Upload);

    VkPipelineStageFlags src_stages = 0, dst_stages = 0;
    SmallVector<VkBufferMemoryBarrier, 2> barriers;

    if (init_buf.resource_state != eResState::Undefined && init_buf.resource_state != eResState::CopySrc) {
        auto &new_barrier = barriers.emplace_back();
        new_barrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
        new_barrier.srcAccessMask = VKAccessFlagsForState(init_buf.resource_state);
        new_barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        new_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        new_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        new_barrier.buffer = init_buf.vk_handle();
        new_barrier.offset = VkDeviceSize{init_off};
        new_barrier.size = VkDeviceSize{size};

        src_stages |= VKPipelineStagesForState(init_buf.resource_state);
        dst_stages |= VKPipelineStagesForState(eResState::CopySrc);
    }

    if (this->resource_state != eResState::Undefined && this->resource_state != eResState::CopyDst) {
        auto &new_barrier = barriers.emplace_back();
        new_barrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
        new_barrier.srcAccessMask = VKAccessFlagsForState(this->resource_state);
        new_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        new_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        new_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        new_barrier.buffer = handle_.buf;
        new_barrier.offset = VkDeviceSize{offset};
        new_barrier.size = VkDeviceSize{size};

        src_stages |= VKPipelineStagesForState(this->resource_state);
        dst_stages |= VKPipelineStagesForState(eResState::CopyDst);
    }

    src_stages &= api_ctx_->supported_stages_mask;
    dst_stages &= api_ctx_->supported_stages_mask;

    if (!barriers.empty()) {
        api_ctx_->vkCmdPipelineBarrier(cmd_buf, src_stages, dst_stages, 0, 0, nullptr, uint32_t(barriers.size()),
                                       barriers.cdata(), 0, nullptr);
    }

    const VkBufferCopy region_to_copy = {
        VkDeviceSize{init_off}, // srcOffset
        VkDeviceSize{offset},   // dstOffset
        VkDeviceSize{size}      // size
    };

    api_ctx_->vkCmdCopyBuffer(cmd_buf, init_buf.handle_.buf, handle_.buf, 1, &region_to_copy);

    init_buf.resource_state = eResState::CopySrc;
    this->resource_state = eResState::CopyDst;
}

bool Ren::Buffer::FreeSubRegion(const SubAllocation alloc) {
    sub_alloc_->Free(alloc.block);
    assert(sub_alloc_->IntegrityCheck());
    return true;
}

void Ren::Buffer::Resize(uint32_t new_size, const bool keep_content) {
    new_size = size_alignment_ * ((new_size + size_alignment_ - 1) / size_alignment_);
    if (size_ >= new_size) {
        return;
    }

    const uint32_t old_size = size_;

    size_ = new_size;
    assert(size_ > 0);

    if (sub_alloc_) {
        sub_alloc_->ResizePool(0, size_);
        assert(sub_alloc_->IntegrityCheck());
    }

    VkBufferCreateInfo buf_create_info = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    buf_create_info.size = VkDeviceSize(new_size);
    buf_create_info.usage = GetVkBufferUsageFlags(api_ctx_, type_);
    buf_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer new_buf = {};
    VkResult res = api_ctx_->vkCreateBuffer(api_ctx_->device, &buf_create_info, nullptr, &new_buf);
    assert(res == VK_SUCCESS && "Failed to create buffer!");

#ifdef ENABLE_OBJ_LABELS
    VkDebugUtilsObjectNameInfoEXT name_info = {VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
    name_info.objectType = VK_OBJECT_TYPE_BUFFER;
    name_info.objectHandle = uint64_t(new_buf);
    name_info.pObjectName = name_.c_str();
    api_ctx_->vkSetDebugUtilsObjectNameEXT(api_ctx_->device, &name_info);
#endif

    VkMemoryRequirements memory_requirements = {};
    api_ctx_->vkGetBufferMemoryRequirements(api_ctx_->device, new_buf, &memory_requirements);

    VkMemoryPropertyFlags memory_props = GetVkMemoryPropertyFlags(type_);

    VkDeviceMemory new_dedicated_mem = {};
    MemAllocation new_allocation = {};
    if (mem_allocs_ && type_ != eBufType::Upload && type_ != eBufType::Readback) {
        new_allocation = mem_allocs_->Allocate(memory_requirements, memory_props);
        if (!new_allocation) {
            // log->Warning("Not enough device memory, falling back to CPU RAM!");
            memory_props &= ~VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
            new_allocation = mem_allocs_->Allocate(memory_requirements, memory_props);
        }

        res = api_ctx_->vkBindBufferMemory(api_ctx_->device, new_buf, new_allocation.owner->mem(new_allocation.pool),
                                           new_allocation.offset);
        assert(res == VK_SUCCESS && "Failed to bind memory!");
    } else {
        // Do a dedicated allocation
        VkMemoryAllocateInfo mem_alloc_info = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        mem_alloc_info.allocationSize = memory_requirements.size;
        mem_alloc_info.memoryTypeIndex =
            FindMemoryType(0, &api_ctx_->mem_properties, memory_requirements.memoryTypeBits, memory_props,
                           mem_alloc_info.allocationSize);

        VkMemoryAllocateFlagsInfoKHR additional_flags = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO_KHR};

        if ((buf_create_info.usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) != 0) {
            additional_flags.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;
            mem_alloc_info.pNext = &additional_flags;
        }

        res = VK_ERROR_OUT_OF_DEVICE_MEMORY;
        while (mem_alloc_info.memoryTypeIndex != 0xffffffff) {
            res = api_ctx_->vkAllocateMemory(api_ctx_->device, &mem_alloc_info, nullptr, &new_dedicated_mem);
            if (res == VK_SUCCESS) {
                break;
            }
            mem_alloc_info.memoryTypeIndex =
                FindMemoryType(mem_alloc_info.memoryTypeIndex + 1, &api_ctx_->mem_properties,
                               memory_requirements.memoryTypeBits, memory_props, mem_alloc_info.allocationSize);
        }
        if (res == VK_ERROR_OUT_OF_DEVICE_MEMORY) {
            // api_ctx_->log()->Warning("Not enough device memory, falling back to CPU RAM!");
            memory_props &= ~VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

            mem_alloc_info.memoryTypeIndex =
                FindMemoryType(0, &api_ctx_->mem_properties, memory_requirements.memoryTypeBits, memory_props,
                               mem_alloc_info.allocationSize);
            while (mem_alloc_info.memoryTypeIndex != 0xffffffff) {
                res = api_ctx_->vkAllocateMemory(api_ctx_->device, &mem_alloc_info, nullptr, &new_dedicated_mem);
                if (res == VK_SUCCESS) {
                    break;
                }
                mem_alloc_info.memoryTypeIndex =
                    FindMemoryType(mem_alloc_info.memoryTypeIndex + 1, &api_ctx_->mem_properties,
                                   memory_requirements.memoryTypeBits, memory_props, mem_alloc_info.allocationSize);
            }
        }
        assert(res == VK_SUCCESS && "Failed to allocate memory!");

        res = api_ctx_->vkBindBufferMemory(api_ctx_->device, new_buf, new_dedicated_mem, 0 /* offset */);
        assert(res == VK_SUCCESS && "Failed to bind memory!");
    }

    if (handle_.buf != VK_NULL_HANDLE) {
        if (keep_content) {
            VkCommandBuffer cmd_buf = api_ctx_->BegSingleTimeCommands();

            VkBufferCopy region_to_copy = {};
            region_to_copy.size = VkDeviceSize{old_size};

            api_ctx_->vkCmdCopyBuffer(cmd_buf, handle_.buf, new_buf, 1, &region_to_copy);

            api_ctx_->EndSingleTimeCommands(cmd_buf);

            // destroy previous buffer
            api_ctx_->vkDestroyBuffer(api_ctx_->device, handle_.buf, nullptr);
            alloc_ = {};
            if (dedicated_mem_) {
                api_ctx_->vkFreeMemory(api_ctx_->device, dedicated_mem_, nullptr);
            }
        } else {
            // destroy previous buffer
            api_ctx_->bufs_to_destroy[api_ctx_->backend_frame].push_back(handle_.buf);
            if (alloc_) {
                api_ctx_->allocs_to_free[api_ctx_->backend_frame].emplace_back(std::move(alloc_));
            }
            if (dedicated_mem_) {
                api_ctx_->mem_to_free[api_ctx_->backend_frame].push_back(dedicated_mem_);
            }
        }
    }

    handle_.buf = new_buf;
    handle_.generation = g_GenCounter++;
    alloc_ = std::move(new_allocation);
    dedicated_mem_ = new_dedicated_mem;
}

void Ren::Buffer::Free() {
    assert(mapped_offset_ == 0xffffffff && !mapped_ptr_);
    if (handle_.buf != VK_NULL_HANDLE) {
        api_ctx_->bufs_to_destroy[api_ctx_->backend_frame].push_back(handle_.buf);
        if (alloc_) {
            api_ctx_->allocs_to_free[api_ctx_->backend_frame].emplace_back(std::move(alloc_));
        }
        if (dedicated_mem_) {
            api_ctx_->mem_to_free[api_ctx_->backend_frame].push_back(dedicated_mem_);
        }

        handle_ = {};
        mem_allocs_ = nullptr;
        dedicated_mem_ = nullptr;
        size_ = 0;
    }
}

void Ren::Buffer::FreeImmediate() {
    assert(mapped_offset_ == 0xffffffff && !mapped_ptr_);
    if (handle_.buf != VK_NULL_HANDLE) {
        api_ctx_->vkDestroyBuffer(api_ctx_->device, handle_.buf, nullptr);
        alloc_ = {};
        if (dedicated_mem_) {
            api_ctx_->vkFreeMemory(api_ctx_->device, dedicated_mem_, nullptr);
        }

        handle_ = {};
        size_ = 0;
    }
}

uint32_t Ren::Buffer::AlignMapOffset(const uint32_t offset) {
    const uint32_t align_to = uint32_t(api_ctx_->device_properties.limits.nonCoherentAtomSize);
    return offset - (offset % align_to);
}

uint8_t *Ren::Buffer::MapRange(const uint32_t offset, const uint32_t size, const bool persistent) {
    assert(dedicated_mem_);
    assert(mapped_offset_ == 0xffffffff && !mapped_ptr_);
    assert(offset + size <= size_);
    assert(type_ == eBufType::Upload || type_ == eBufType::Readback);
    assert(offset == AlignMapOffset(offset));
    assert((offset + size) == size_ || (offset + size) == AlignMapOffset(offset + size));

    void *mapped = nullptr;
    const VkResult res =
        api_ctx_->vkMapMemory(api_ctx_->device, dedicated_mem_, VkDeviceSize(offset), VkDeviceSize(size), 0, &mapped);
    assert(res == VK_SUCCESS && "Failed to map memory!");

    mapped_ptr_ = reinterpret_cast<uint8_t *>(mapped);
    mapped_offset_ = offset;
    return reinterpret_cast<uint8_t *>(mapped);
}

void Ren::Buffer::Unmap() {
    assert(dedicated_mem_);
    assert(mapped_offset_ != 0xffffffff && mapped_ptr_);
    api_ctx_->vkUnmapMemory(api_ctx_->device, dedicated_mem_);
    mapped_ptr_ = nullptr;
    mapped_offset_ = 0xffffffff;
}

void Ren::Buffer::Fill(const uint32_t dst_offset, const uint32_t size, const uint32_t data, CommandBuffer cmd_buf) {
    VkPipelineStageFlags src_stages = 0, dst_stages = 0;
    SmallVector<VkBufferMemoryBarrier, 1> barriers;

    if (resource_state != eResState::CopyDst) {
        auto &new_barrier = barriers.emplace_back();
        new_barrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
        new_barrier.srcAccessMask = VKAccessFlagsForState(resource_state);
        new_barrier.dstAccessMask = VKAccessFlagsForState(eResState::CopyDst);
        new_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        new_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        new_barrier.buffer = handle_.buf;
        new_barrier.offset = VkDeviceSize{dst_offset};
        new_barrier.size = VkDeviceSize{size};

        src_stages |= VKPipelineStagesForState(resource_state);
        dst_stages |= VKPipelineStagesForState(eResState::CopySrc);
    }

    src_stages &= api_ctx_->supported_stages_mask;
    dst_stages &= api_ctx_->supported_stages_mask;

    if (!barriers.empty()) {
        api_ctx_->vkCmdPipelineBarrier(cmd_buf, src_stages, dst_stages, 0, 0, nullptr, uint32_t(barriers.size()),
                                       barriers.cdata(), 0, nullptr);
    }

    api_ctx_->vkCmdFillBuffer(cmd_buf, handle_.buf, VkDeviceSize{dst_offset}, VkDeviceSize{size}, data);

    resource_state = eResState::CopyDst;
}

void Ren::Buffer::UpdateImmediate(uint32_t dst_offset, uint32_t size, const void *data, CommandBuffer cmd_buf) {
    VkPipelineStageFlags src_stages = 0, dst_stages = 0;
    SmallVector<VkBufferMemoryBarrier, 1> barriers;

    if (resource_state != eResState::CopyDst) {
        auto &new_barrier = barriers.emplace_back();
        new_barrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
        new_barrier.srcAccessMask = VKAccessFlagsForState(resource_state);
        new_barrier.dstAccessMask = VKAccessFlagsForState(eResState::CopyDst);
        new_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        new_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        new_barrier.buffer = handle_.buf;
        new_barrier.offset = VkDeviceSize{dst_offset};
        new_barrier.size = VkDeviceSize{size};

        src_stages |= VKPipelineStagesForState(resource_state);
        dst_stages |= VKPipelineStagesForState(eResState::CopySrc);
    }

    src_stages &= api_ctx_->supported_stages_mask;
    dst_stages &= api_ctx_->supported_stages_mask;

    if (!barriers.empty()) {
        api_ctx_->vkCmdPipelineBarrier(cmd_buf, src_stages, dst_stages, 0, 0, nullptr, uint32_t(barriers.size()),
                                       barriers.cdata(), 0, nullptr);
    }

    api_ctx_->vkCmdUpdateBuffer(cmd_buf, handle_.buf, VkDeviceSize{dst_offset}, VkDeviceSize{size}, data);

    resource_state = eResState::CopyDst;
}

void Ren::Buffer::Print(ILog *log) {
#if 0
    log->Info("=================================================================");
    log->Info("Buffer %s, %f MB, %i nodes", name_.c_str(), float(size_) / (1024.0f * 1024.0f), int(nodes_.size()));
    PrintNode(0, "", true, log);
    log->Info("=================================================================");
#endif
}

void Ren::CopyBufferToBuffer(Buffer &src, const uint32_t src_offset, Buffer &dst, const uint32_t dst_offset,
                             const uint32_t size, CommandBuffer cmd_buf) {
    VkPipelineStageFlags src_stages = 0, dst_stages = 0;
    SmallVector<VkBufferMemoryBarrier, 2> barriers;

    if (src.resource_state != eResState::Undefined && src.resource_state != eResState::CopySrc) {
        auto &new_barrier = barriers.emplace_back();
        new_barrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
        new_barrier.srcAccessMask = VKAccessFlagsForState(src.resource_state);
        new_barrier.dstAccessMask = VKAccessFlagsForState(eResState::CopySrc);
        new_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        new_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        new_barrier.buffer = src.vk_handle();
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
        new_barrier.buffer = dst.vk_handle();
        new_barrier.offset = VkDeviceSize{dst_offset};
        new_barrier.size = VkDeviceSize{size};

        src_stages |= VKPipelineStagesForState(dst.resource_state);
        dst_stages |= VKPipelineStagesForState(eResState::CopyDst);
    }

    src_stages &= src.api_ctx()->supported_stages_mask;
    dst_stages &= src.api_ctx()->supported_stages_mask;

    if (!barriers.empty()) {
        src.api_ctx()->vkCmdPipelineBarrier(cmd_buf, src_stages, dst_stages, 0, 0, nullptr, uint32_t(barriers.size()),
                                            barriers.cdata(), 0, nullptr);
    }

    VkBufferCopy region_to_copy = {};
    region_to_copy.srcOffset = VkDeviceSize{src_offset};
    region_to_copy.dstOffset = VkDeviceSize{dst_offset};
    region_to_copy.size = VkDeviceSize{size};

    src.api_ctx()->vkCmdCopyBuffer(cmd_buf, src.vk_handle(), dst.vk_handle(), 1, &region_to_copy);

    src.resource_state = eResState::CopySrc;
    dst.resource_state = eResState::CopyDst;
}
