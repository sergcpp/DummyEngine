#include "MemoryAllocator.h"

#include "VKCtx.h"

namespace Ren {
uint32_t FindMemoryType(uint32_t search_from, const VkPhysicalDeviceMemoryProperties *mem_properties,
                        uint32_t mem_type_bits, VkMemoryPropertyFlags desired_mem_flags, VkDeviceSize desired_size) {
    for (uint32_t i = search_from; i < 32; i++) {
        const VkMemoryType mem_type = mem_properties->memoryTypes[i];
        if (mem_type.propertyFlags & VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD) {
            // skip for now
            continue;
        }
        if (mem_type_bits & (1u << i)) {
            if ((mem_type.propertyFlags & desired_mem_flags) == desired_mem_flags &&
                mem_properties->memoryHeaps[mem_type.heapIndex].size >= desired_size) {
                return i;
            }
        }
    }
    return 0xffffffff;
}
} // namespace Ren

void Ren::MemAllocation::Release() {
    if (owner) {
        owner->Free(block);
        owner = nullptr;
    }
}

Ren::MemAllocator::MemAllocator(const std::string_view name, ApiContext *api_ctx, const uint32_t initial_block_size,
                                uint32_t mem_type_index, const float growth_factor, const uint32_t max_pool_size)
    : name_(name), api_ctx_(api_ctx), growth_factor_(growth_factor), max_pool_size_(max_pool_size),
      mem_type_index_(mem_type_index) {

    assert(growth_factor_ > 1);
    AllocateNewPool(initial_block_size);
}

Ren::MemAllocator::~MemAllocator() {
    for (MemHeap &pool : pools_) {
        api_ctx_->vkFreeMemory(api_ctx_->device, pool.mem, nullptr);
    }
}

bool Ren::MemAllocator::AllocateNewPool(const uint32_t size) {
    VkMemoryAllocateInfo mem_alloc_info = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    mem_alloc_info.allocationSize = VkDeviceSize(size);
    mem_alloc_info.memoryTypeIndex = mem_type_index_;

    VkMemoryAllocateFlagsInfoKHR additional_flags = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO_KHR};
    if (api_ctx_->raytracing_supported) {
        additional_flags.flags |= VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;
        mem_alloc_info.pNext = &additional_flags;
    }

    VkDeviceMemory new_mem = {};
    const VkResult res = api_ctx_->vkAllocateMemory(api_ctx_->device, &mem_alloc_info, nullptr, &new_mem);
    if (res == VK_SUCCESS) {
        MemHeap &new_pool = pools_.emplace_back();
        new_pool.mem = new_mem;
        new_pool.size = size;

        const uint16_t pool_ndx = alloc_.AddPool(size);
        assert(pool_ndx == pools_.size() - 1);
    }
    return res == VK_SUCCESS;
}

Ren::MemAllocation Ren::MemAllocator::Allocate(const uint32_t alignment, const uint32_t size) {
    auto allocation = alloc_.Alloc(alignment, size);

    if (allocation.block == 0xffffffff) {
        const uint32_t required_size = FreelistAlloc::rounded_size(size + alignment);
        const bool res = AllocateNewPool(
            std::max(required_size, std::min(max_pool_size_, uint32_t(pools_.back().size * growth_factor_))));
        if (!res) {
            // allocation failed (out of memory)
            return {};
        }
        allocation = alloc_.Alloc(alignment, size);
    }

    assert((allocation.offset % alignment) == 0);
    assert(alloc_.IntegrityCheck());

    MemAllocation new_alloc = {};
    new_alloc.offset = allocation.offset;
    new_alloc.block = allocation.block;
    new_alloc.pool = allocation.pool;
    new_alloc.owner = this;

    return new_alloc;
}

void Ren::MemAllocator::Free(const uint32_t block) {
    alloc_.Free(block);
    assert(alloc_.IntegrityCheck());
}

Ren::MemAllocation Ren::MemAllocators::Allocate(uint32_t alignment, uint32_t size, uint32_t mem_type_index) {
    if (mem_type_index == 0xffffffff) {
        return {};
    }

    int alloc_index = -1;
    for (int i = 0; i < int(allocators_.size()); ++i) {
        if (allocators_[i]->mem_type_index() == mem_type_index) {
            alloc_index = i;
            break;
        }
    }

    if (alloc_index == -1) {
        const std::string name = name_ + " (type " + std::to_string(mem_type_index) + ")";
        alloc_index = int(allocators_.size());
        allocators_.emplace_back(std::make_unique<MemAllocator>(name, api_ctx_, initial_block_size_, mem_type_index,
                                                                growth_factor_, max_pool_size_));
    }

    return allocators_[alloc_index]->Allocate(alignment, size);
}

Ren::MemAllocation Ren::MemAllocators::Allocate(const VkMemoryRequirements &mem_req,
                                                const VkMemoryPropertyFlags desired_mem_flags) {
    uint32_t mem_type_index =
        FindMemoryType(0, &api_ctx_->mem_properties, mem_req.memoryTypeBits, desired_mem_flags, uint32_t(mem_req.size));
    while (mem_type_index != 0xffffffff) {
        MemAllocation alloc = Allocate(uint32_t(mem_req.alignment), uint32_t(mem_req.size), mem_type_index);
        if (alloc) {
            return alloc;
        }
        mem_type_index = FindMemoryType(mem_type_index + 1, &api_ctx_->mem_properties, mem_req.memoryTypeBits,
                                        desired_mem_flags, uint32_t(mem_req.size));
    }
    return {};
}
