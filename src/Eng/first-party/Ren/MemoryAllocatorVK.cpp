#include "MemoryAllocator.h"

#include "VKCtx.h"

namespace Ren {
uint32_t FindMemoryType(const VkPhysicalDeviceMemoryProperties *mem_properties, uint32_t mem_type_bits,
                        VkMemoryPropertyFlags desired_mem_flags) {
    for (uint32_t i = 0; i < 32; i++) {
        const VkMemoryType mem_type = mem_properties->memoryTypes[i];
        if (mem_type_bits & 1u) {
            if ((mem_type.propertyFlags & desired_mem_flags) == desired_mem_flags) {
                return i;
            }
        }
        mem_type_bits = (mem_type_bits >> 1u);
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

Ren::MemoryAllocator::MemoryAllocator(const std::string_view name, ApiContext *api_ctx,
                                      const uint32_t initial_block_size, uint32_t mem_type_index,
                                      const float growth_factor, const uint32_t max_pool_size)
    : name_(name), api_ctx_(api_ctx), growth_factor_(growth_factor), mem_type_index_(mem_type_index),
      max_pool_size_(max_pool_size) {

    assert(growth_factor_ > 1.0f);
    AllocateNewPool(initial_block_size);
}

Ren::MemoryAllocator::~MemoryAllocator() {
    for (MemPool &pool : pools_) {
        api_ctx_->vkFreeMemory(api_ctx_->device, pool.mem, nullptr);
    }
}

bool Ren::MemoryAllocator::AllocateNewPool(const uint32_t size) {
    VkMemoryAllocateInfo buf_alloc_info = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    buf_alloc_info.allocationSize = VkDeviceSize(size);
    buf_alloc_info.memoryTypeIndex = mem_type_index_;

    VkDeviceMemory new_mem = {};
    const VkResult res = api_ctx_->vkAllocateMemory(api_ctx_->device, &buf_alloc_info, nullptr, &new_mem);
    if (res == VK_SUCCESS) {
        MemPool &new_pool = pools_.emplace_back();
        new_pool.mem = new_mem;
        new_pool.size = size;

        const uint16_t pool_ndx = alloc_.AddPool(size);
        assert(pool_ndx == pools_.size() - 1);
    }
    return res == VK_SUCCESS;
}

Ren::MemAllocation Ren::MemoryAllocator::Allocate(const uint32_t size, const uint32_t alignment, const char *tag) {
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

void Ren::MemoryAllocator::Free(const uint32_t block) {
    alloc_.Free(block);
    assert(alloc_.IntegrityCheck());
}

void Ren::MemoryAllocators::Print(ILog *log) {
    /*log->Info("=================================================================");
    log->Info("MemAllocs %s", name_.c_str());
    for (const auto &alloc : allocators_) {
        alloc.Print(log);
    }
    log->Info("=================================================================");*/
}