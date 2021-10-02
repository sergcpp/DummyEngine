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
        flags |= (VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    } else if (type == eBufType::Uniform) {
        flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    } else if (type == eBufType::Storage) {
        flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    } else if (type == eBufType::Stage) {
    } else if (type == eBufType::AccStructure) {
        flags |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR;
    } else if (type == eBufType::ShaderBinding) {
        flags |= VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    }

    if ((type == eBufType::VertexAttribs || type == eBufType::VertexIndices || type == eBufType::Storage) &&
        api_ctx->raytracing_supported) {
        flags |= (VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                  VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR);
    }

    return flags;
}

VkMemoryPropertyFlags GetVkMemoryPropertyFlags(const eBufType type) {
    return (type == eBufType::Stage) ? VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
}

uint32_t FindMemoryType(const VkPhysicalDeviceMemoryProperties *mem_properties, uint32_t mem_type_bits,
                        VkMemoryPropertyFlags desired_mem_flags);
} // namespace Ren

int Ren::Buffer::g_GenCounter = 0;

Ren::Buffer::Buffer(const char *name, ApiContext *api_ctx, const eBufType type, const uint32_t initial_size,
                    const uint32_t suballoc_align)
    : LinearAlloc(suballoc_align, initial_size), name_(name), api_ctx_(api_ctx), type_(type), size_(0) {
    Resize(size());
}

Ren::Buffer::~Buffer() { Free(); }

Ren::Buffer &Ren::Buffer::operator=(Buffer &&rhs) noexcept {
    RefCounter::operator=(static_cast<RefCounter &&>(rhs));
    LinearAlloc::operator=(static_cast<LinearAlloc &&>(rhs));

    Free();

    assert(!mapped_ptr_);
    assert(mapped_offset_ == 0xffffffff);

    api_ctx_ = exchange(rhs.api_ctx_, nullptr);
    handle_ = exchange(rhs.handle_, {});
    name_ = std::move(rhs.name_);
    mem_ = exchange(rhs.mem_, {});

    type_ = exchange(rhs.type_, eBufType::Undefined);

    size_ = exchange(rhs.size_, 0);
    mapped_ptr_ = exchange(rhs.mapped_ptr_, nullptr);
    mapped_offset_ = exchange(rhs.mapped_offset_, 0xffffffff);

#ifndef NDEBUG
    flushed_ranges_ = std::move(rhs.flushed_ranges_);
#endif

    resource_state = exchange(rhs.resource_state, eResState::Undefined);

    return (*this);
}

VkDeviceAddress Ren::Buffer::vk_device_address() const {
    VkBufferDeviceAddressInfo addr_info = {VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
    addr_info.buffer = handle_.buf;
    return vkGetBufferDeviceAddressKHR(api_ctx_->device, &addr_info);
}

uint32_t Ren::Buffer::AllocSubRegion(const uint32_t req_size, const char *tag, const Buffer *init_buf, void *_cmd_buf,
                                     const uint32_t init_off) {
    const uint32_t alloc_off = Alloc(req_size, tag);
    if (alloc_off != 0xffffffff) {
        if (init_buf) {
            assert(init_buf->type_ == eBufType::Stage);
            VkCommandBuffer cmd_buf = reinterpret_cast<VkCommandBuffer>(_cmd_buf);

            VkPipelineStageFlags src_stages = 0, dst_stages = 0;
            SmallVector<VkBufferMemoryBarrier, 2> barriers;

            if (init_buf->resource_state != eResState::Undefined && init_buf->resource_state != eResState::CopySrc) {
                auto &new_barrier = barriers.emplace_back();
                new_barrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
                new_barrier.srcAccessMask = VKAccessFlagsForState(init_buf->resource_state);
                new_barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                new_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                new_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                new_barrier.buffer = init_buf->handle().buf;
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
                new_barrier.offset = VkDeviceSize{alloc_off};
                new_barrier.size = VkDeviceSize{req_size};

                src_stages |= VKPipelineStagesForState(this->resource_state);
                dst_stages |= VKPipelineStagesForState(eResState::CopyDst);
            }

            if (!barriers.empty()) {
                vkCmdPipelineBarrier(cmd_buf, src_stages, dst_stages, 0, 0, nullptr, uint32_t(barriers.size()),
                                     barriers.cdata(), 0, nullptr);
            }

            VkBufferCopy region_to_copy = {};
            region_to_copy.srcOffset = VkDeviceSize{init_off};
            region_to_copy.dstOffset = VkDeviceSize{alloc_off};
            region_to_copy.size = VkDeviceSize{req_size};

            vkCmdCopyBuffer(cmd_buf, init_buf->handle_.buf, handle_.buf, 1, &region_to_copy);

            init_buf->resource_state = eResState::CopySrc;
            this->resource_state = eResState::CopyDst;
        }

        return alloc_off;
    }

    return 0xffffffff;
}

void Ren::Buffer::UpdateSubRegion(const uint32_t offset, const uint32_t size, const Buffer &init_buf,
                                  const uint32_t init_off, void *_cmd_buf) {
    assert(init_buf.type_ == eBufType::Stage);
    VkCommandBuffer cmd_buf = reinterpret_cast<VkCommandBuffer>(_cmd_buf);

    VkPipelineStageFlags src_stages = 0, dst_stages = 0;
    SmallVector<VkBufferMemoryBarrier, 2> barriers;

    if (init_buf.resource_state != eResState::Undefined && init_buf.resource_state != eResState::CopySrc) {
        auto &new_barrier = barriers.emplace_back();
        new_barrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
        new_barrier.srcAccessMask = VKAccessFlagsForState(init_buf.resource_state);
        new_barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        new_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        new_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        new_barrier.buffer = init_buf.handle().buf;
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

    if (!barriers.empty()) {
        vkCmdPipelineBarrier(cmd_buf, src_stages, dst_stages, 0, 0, nullptr, uint32_t(barriers.size()),
                             barriers.cdata(), 0, nullptr);
    }

    const VkBufferCopy region_to_copy = {
        VkDeviceSize{init_off}, // srcOffset
        VkDeviceSize{offset},   // dstOffset
        VkDeviceSize{size}      // size
    };

    vkCmdCopyBuffer(cmd_buf, init_buf.handle_.buf, handle_.buf, 1, &region_to_copy);

    init_buf.resource_state = eResState::CopySrc;
    this->resource_state = eResState::CopyDst;
}

bool Ren::Buffer::FreeSubRegion(const uint32_t offset, const uint32_t size) {
    LinearAlloc::Free(offset, size);
    return true;
}

void Ren::Buffer::Resize(const uint32_t new_size) {
    if (size_ >= new_size) {
        return;
    }

    const uint32_t old_size = size_;

    if (!size_) {
        size_ = new_size;
        assert(size_ > 0);
    }

    while (size_ < new_size) {
        size_ *= 2;
    }

    VkBufferCreateInfo buf_create_info = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    buf_create_info.size = VkDeviceSize(new_size);
    buf_create_info.usage = GetVkBufferUsageFlags(api_ctx_, type_);
    buf_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer new_buf = {};
    VkResult res = vkCreateBuffer(api_ctx_->device, &buf_create_info, nullptr, &new_buf);
    assert(res == VK_SUCCESS && "Failed to create vertex buffer!");
    
#ifdef ENABLE_OBJ_LABELS
    VkDebugUtilsObjectNameInfoEXT name_info = {VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
    name_info.objectType = VK_OBJECT_TYPE_BUFFER;
    name_info.objectHandle = uint64_t(new_buf);
    name_info.pObjectName = name_.c_str();
    vkSetDebugUtilsObjectNameEXT(api_ctx_->device, &name_info);
#endif

    VkMemoryRequirements memory_requirements = {};
    vkGetBufferMemoryRequirements(api_ctx_->device, new_buf, &memory_requirements);

    VkMemoryAllocateInfo buf_alloc_info = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    buf_alloc_info.allocationSize = memory_requirements.size;
    buf_alloc_info.memoryTypeIndex =
        FindMemoryType(&api_ctx_->mem_properties, memory_requirements.memoryTypeBits, GetVkMemoryPropertyFlags(type_));

    VkMemoryAllocateFlagsInfoKHR additional_flags = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO_KHR};
    additional_flags.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;

    if (type_ != eBufType::Stage && api_ctx_->raytracing_supported) {
        buf_alloc_info.pNext = &additional_flags;
    }

    VkDeviceMemory buffer_mem = {};
    res = vkAllocateMemory(api_ctx_->device, &buf_alloc_info, nullptr, &buffer_mem);
    assert(res == VK_SUCCESS && "Failed to allocate memory!");
    
    res = vkBindBufferMemory(api_ctx_->device, new_buf, buffer_mem, 0 /* offset */);
    assert(res == VK_SUCCESS && "Failed to bind memory!");

    if (handle_.buf != VK_NULL_HANDLE) {
        { // copy previous buffer contents
            VkCommandBuffer cmd_buf = BegSingleTimeCommands(api_ctx_->device, api_ctx_->temp_command_pool);

            VkBufferCopy region_to_copy = {};
            region_to_copy.size = VkDeviceSize{old_size};

            vkCmdCopyBuffer(cmd_buf, handle_.buf, new_buf, 1, &region_to_copy);

            EndSingleTimeCommands(api_ctx_->device, api_ctx_->graphics_queue, cmd_buf, api_ctx_->temp_command_pool);
        }

        // destroy previous buffer
        vkDestroyBuffer(api_ctx_->device, handle_.buf, nullptr);
        vkFreeMemory(api_ctx_->device, mem_, nullptr);
    }

    handle_.buf = new_buf;
    handle_.generation = g_GenCounter++;
    mem_ = buffer_mem;
}

void Ren::Buffer::Free() {
    assert(mapped_offset_ == 0xffffffff && !mapped_ptr_);
    if (handle_.buf != VK_NULL_HANDLE) {
        api_ctx_->bufs_to_destroy[api_ctx_->backend_frame].push_back(handle_.buf);
        api_ctx_->mem_to_free[api_ctx_->backend_frame].push_back(mem_);

        handle_ = {};
        size_ = 0;
        LinearAlloc::Clear();
    }
}

uint32_t Ren::Buffer::AlignMapOffset(const uint32_t offset) {
    const uint32_t align_to = uint32_t(api_ctx_->device_properties.limits.nonCoherentAtomSize);
    return offset - (offset % align_to);
}

uint8_t *Ren::Buffer::MapRange(const uint8_t dir, const uint32_t offset, const uint32_t size, const bool persistent) {
    assert(mapped_offset_ == 0xffffffff && !mapped_ptr_);
    assert(offset + size <= size_);
    assert(type_ == eBufType::Stage);
    assert(offset == AlignMapOffset(offset));
    assert((offset + size) == size_ || (offset + size) == AlignMapOffset(offset + size));

#ifndef NDEBUG
    for (auto it = std::begin(flushed_ranges_); it != std::end(flushed_ranges_);) {
        if (offset + size >= it->range.first && offset < it->range.first + it->range.second) {
            const WaitResult res = it->fence.ClientWaitSync(0);
            assert(res == WaitResult::Success);
            it = flushed_ranges_.erase(it);
        } else {
            ++it;
        }
    }
#endif

    void *mapped = nullptr;
    const VkResult res = vkMapMemory(api_ctx_->device, mem_, VkDeviceSize(offset), VkDeviceSize(size), 0, &mapped);
    assert(res == VK_SUCCESS && "Failed to map memory!");

    mapped_ptr_ = reinterpret_cast<uint8_t *>(mapped);
    mapped_offset_ = offset;
    return reinterpret_cast<uint8_t *>(mapped);
}

void Ren::Buffer::FlushMappedRange(uint32_t offset, const uint32_t size) {
    assert(offset == AlignMapOffset(offset));
    assert((offset + size) == size_ || (offset + size) == AlignMapOffset(offset + size));

    // offset argument is relative to mapped range
    offset += mapped_offset_;

    VkMappedMemoryRange range = {VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};
    range.memory = mem_;
    range.offset = VkDeviceSize(offset);
    range.size = VkDeviceSize(size);
    range.pNext = nullptr;

    VkResult res = vkFlushMappedMemoryRanges(api_ctx_->device, 1, &range);
    assert(res == VK_SUCCESS && "Failed to flush memory range!");

    res = vkInvalidateMappedMemoryRanges(api_ctx_->device, 1, &range);
    assert(res == VK_SUCCESS && "Failed to invalidate memory range!");

#ifndef NDEBUG
    // flushed_ranges_.emplace_back(std::make_pair(offset, size),
    //                             SyncFence{glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0)});
#endif
}

void Ren::Buffer::Unmap() {
    assert(mapped_offset_ != 0xffffffff && mapped_ptr_);
    vkUnmapMemory(api_ctx_->device, mem_);
    mapped_ptr_ = nullptr;
    mapped_offset_ = 0xffffffff;
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
                             const uint32_t size, void *_cmd_buf) {
    VkCommandBuffer cmd_buf = reinterpret_cast<VkCommandBuffer>(_cmd_buf);

    VkPipelineStageFlags src_stages = 0, dst_stages = 0;
    SmallVector<VkBufferMemoryBarrier, 2> barriers;

    if (src.resource_state != eResState::Undefined && src.resource_state != eResState::CopySrc) {
        auto &new_barrier = barriers.emplace_back();
        new_barrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
        new_barrier.srcAccessMask = VKAccessFlagsForState(src.resource_state);
        new_barrier.dstAccessMask = VKAccessFlagsForState(eResState::CopySrc);
        new_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        new_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        new_barrier.buffer = src.handle().buf;
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
        new_barrier.buffer = dst.handle().buf;
        new_barrier.offset = VkDeviceSize{dst_offset};
        new_barrier.size = VkDeviceSize{size};

        src_stages |= VKPipelineStagesForState(dst.resource_state);
        dst_stages |= VKPipelineStagesForState(eResState::CopyDst);
    }

    if (!barriers.empty()) {
        vkCmdPipelineBarrier(cmd_buf, src_stages, dst_stages, 0, 0, nullptr, uint32_t(barriers.size()),
                             barriers.cdata(), 0, nullptr);
    }

    VkBufferCopy region_to_copy = {};
    region_to_copy.srcOffset = VkDeviceSize{src_offset};
    region_to_copy.dstOffset = VkDeviceSize{dst_offset};
    region_to_copy.size = VkDeviceSize{size};

    vkCmdCopyBuffer(cmd_buf, src.handle().buf, dst.handle().buf, 1, &region_to_copy);

    src.resource_state = eResState::CopySrc;
    dst.resource_state = eResState::CopyDst;
}
