#pragma once

#include <memory>
#include <string>

#include "FreelistAlloc.h"
#include "SmallVector.h"

#if defined(REN_VK_BACKEND)
typedef uint32_t VkFlags;
typedef VkFlags VkMemoryPropertyFlags;
typedef struct VkDeviceMemory_T *VkDeviceMemory;
struct VkMemoryRequirements;
#endif

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)
#endif

namespace Ren {
struct ApiContext;
class MemAllocator;

struct MemHeap {
#if defined(REN_VK_BACKEND)
    VkDeviceMemory mem = {};
#elif defined(REN_GL_BACKEND)
    void *mem = nullptr;
#endif
    uint32_t size = 0xffffffff;
};

struct MemAllocation {
    uint32_t offset = 0xffffffff, block = 0xffffffff;
    uint16_t pool = 0xffff;
    MemAllocator *owner = nullptr;

    MemAllocation() = default;
    MemAllocation(const uint32_t _offset, const uint32_t _block, const uint16_t _pool)
        : offset(_offset), block(_block), pool(_pool) {}
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

    operator bool() const { return owner != nullptr; }

    ~MemAllocation() { Release(); }

    void Release();
};

class MemAllocator {
    std::string name_;
    ApiContext *api_ctx_ = nullptr;
    float growth_factor_;
    uint32_t max_pool_size_;

    uint32_t mem_type_index_;
    FreelistAlloc alloc_;
    SmallVector<MemHeap, 8> pools_;

    bool AllocateNewPool(uint32_t size);

  public:
    MemAllocator(std::string_view name, ApiContext *api_ctx, uint32_t initial_block_size, uint32_t mem_type_index,
                 float growth_factor, uint32_t max_pool_size);
    ~MemAllocator();

    MemAllocator(const MemAllocator &rhs) = delete;
    MemAllocator(MemAllocator &&rhs) = default;

    MemAllocator &operator=(const MemAllocator &rhs) = delete;
    MemAllocator &operator=(MemAllocator &&rhs) = default;

#if defined(REN_VK_BACKEND)
    [[nodiscard]] VkDeviceMemory mem(int i) const { return pools_[i].mem; }
#endif
    [[nodiscard]] uint32_t mem_type_index() const { return mem_type_index_; }

    MemAllocation Allocate(uint32_t alignment, uint32_t size);
    void Free(uint32_t block);
};

class MemAllocators {
    ApiContext *api_ctx_;
    std::string name_;
    uint32_t initial_block_size_;
    float growth_factor_;
    uint32_t max_pool_size_;
    std::unique_ptr<MemAllocator> allocators_[32];

  public:
    MemAllocators(std::string_view name, ApiContext *api_ctx, const uint32_t initial_block_size,
                  const float growth_factor, const uint32_t max_pool_size)
        : api_ctx_(api_ctx), name_(name), initial_block_size_(initial_block_size), growth_factor_(growth_factor),
          max_pool_size_(max_pool_size) {}

    MemAllocation Allocate(uint32_t alignment, uint32_t size, uint32_t mem_type_index);
#if defined(REN_VK_BACKEND)
    MemAllocation Allocate(const VkMemoryRequirements &mem_req, VkMemoryPropertyFlags desired_mem_flags);
#endif
};
} // namespace Ren

#ifdef _MSC_VER
#pragma warning(pop)
#endif
