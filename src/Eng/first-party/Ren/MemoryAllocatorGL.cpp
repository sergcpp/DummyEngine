#include "MemoryAllocator.h"

void Ren::MemAllocation::Release() {}

Ren::MemoryAllocator::MemoryAllocator(const std::string_view name, ApiContext *api_ctx,
                                      const uint32_t initial_block_size, uint32_t mem_type_index,
                                      const float growth_factor, const uint32_t max_pool_size)
    : name_(name), api_ctx_(api_ctx), growth_factor_(growth_factor), mem_type_index_(mem_type_index),
      max_pool_size_(max_pool_size) {
    assert(growth_factor_ > 1);
    AllocateNewPool(initial_block_size);
}

Ren::MemoryAllocator::~MemoryAllocator() = default;

bool Ren::MemoryAllocator::AllocateNewPool(const uint32_t size) { return true; }

Ren::MemAllocation Ren::MemoryAllocator::Allocate(const uint32_t alignment, const uint32_t size) {
    return {};
}

void Ren::MemoryAllocator::Free(const uint32_t block) {
    alloc_.Free(block);
    assert(alloc_.IntegrityCheck());
}

void Ren::MemoryAllocators::Print(ILog *log) {}