#pragma once

#include <string>

#include "Buffer.h"
#include "FreelistAlloc.h"
#include "SmallVector.h"

#if defined(USE_VK_RENDER)
#include "VK.h"
#endif

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)
#endif

namespace Ren {
class Buffer;
class MemoryAllocator;

struct MemAllocation {
    uint32_t offset = 0xffffffff, block = 0xffffffff;
    uint16_t pool = 0xffff;
    MemoryAllocator *owner = nullptr;

    MemAllocation() = default;
    MemAllocation(const MemAllocation &rhs) = delete;
    MemAllocation(MemAllocation &&rhs) noexcept
        : offset(rhs.offset), block(rhs.block), pool(rhs.pool), owner(std::exchange(rhs.owner, nullptr)) {}

    MemAllocation &operator=(const MemAllocation &rhs) = delete;
    MemAllocation &operator=(MemAllocation &&rhs) noexcept {
        Release();

        offset = std::exchange(rhs.offset, 0xffffffff);
        block = std::exchange(rhs.block, 0xffffffff);
        pool = std::exchange(rhs.pool, 0xffff);
        owner = std::exchange(rhs.owner, nullptr);

        return (*this);
    }

    ~MemAllocation() { Release(); }

    void Release();
};

class MemoryAllocator {
    std::string name_;
    ApiContext *api_ctx_ = nullptr;
    float growth_factor_;
    uint32_t max_pool_size_;

    struct MemPool {
#if defined(USE_VK_RENDER)
        VkDeviceMemory mem;
#endif
        uint32_t size = 0xffffffff;
    };

    uint32_t mem_type_index_;
    FreelistAlloc alloc_;
    SmallVector<MemPool, 8> pools_;

    bool AllocateNewPool(uint32_t size);

  public:
    MemoryAllocator(std::string_view name, ApiContext *api_ctx, uint32_t initial_block_size, uint32_t mem_type_index,
                    float growth_factor, uint32_t max_pool_size);
    ~MemoryAllocator();

    MemoryAllocator(const MemoryAllocator &rhs) = delete;
    MemoryAllocator(MemoryAllocator &&rhs) = default;

    MemoryAllocator &operator=(const MemoryAllocator &rhs) = delete;
    MemoryAllocator &operator=(MemoryAllocator &&rhs) = default;

#if defined(USE_VK_RENDER)
    VkDeviceMemory mem(int i) const { return pools_[i].mem; }
#endif
    uint32_t mem_type_index() const { return mem_type_index_; }

    MemAllocation Allocate(uint32_t size, uint32_t alignment, const char *tag);
    void Free(uint32_t block);

    void Print(ILog *log) const {
        // for (const auto &block : blocks_) {
        //     block.alloc.PrintNode(0, "", true, log);
        // }
    }
};

class MemoryAllocators {
    char name_[16];
    ApiContext *api_ctx_;
    uint32_t initial_block_size_;
    float growth_factor_;
    uint32_t max_pool_size_;
    SmallVector<MemoryAllocator, 4> allocators_;

  public:
    MemoryAllocators(const char name[16], ApiContext *api_ctx, const uint32_t initial_block_size,
                     const float growth_factor, const uint32_t max_pool_size)
        : api_ctx_(api_ctx), initial_block_size_(initial_block_size), growth_factor_(growth_factor),
          max_pool_size_(max_pool_size) {
        strcpy(name_, name);
    }

    MemAllocation Allocate(uint32_t size, uint32_t alignment, uint32_t mem_type_index, const char *tag) {
        int alloc_index = -1;
        for (int i = 0; i < int(allocators_.size()); ++i) {
            if (allocators_[i].mem_type_index() == mem_type_index) {
                alloc_index = i;
                break;
            }
        }

        if (alloc_index == -1) {
            char name[32];
            snprintf(name, sizeof(name), "%s (type %i)", name_, int(mem_type_index));
            alloc_index = int(allocators_.size());
            allocators_.emplace_back(name, api_ctx_, initial_block_size_, mem_type_index, growth_factor_,
                                     max_pool_size_);
        }

        return allocators_[alloc_index].Allocate(size, alignment, tag);
    }

    void Print(ILog *log);
};
} // namespace Ren

#ifdef _MSC_VER
#pragma warning(pop)
#endif
