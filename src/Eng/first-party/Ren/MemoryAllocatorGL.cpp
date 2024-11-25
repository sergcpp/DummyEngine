#include "MemoryAllocator.h"

void Ren::MemAllocation::Release() {}

Ren::MemAllocator::MemAllocator(const std::string_view name, ApiContext *api_ctx, const uint32_t initial_block_size,
                                uint32_t mem_type_index, const float growth_factor, const uint32_t max_pool_size)
    : name_(name), api_ctx_(api_ctx), growth_factor_(growth_factor), mem_type_index_(mem_type_index),
      max_pool_size_(max_pool_size) {
    assert(growth_factor_ > 1);
    AllocateNewPool(initial_block_size);
}

Ren::MemAllocator::~MemAllocator() = default;

bool Ren::MemAllocator::AllocateNewPool(const uint32_t) { return true; }

Ren::MemAllocation Ren::MemAllocator::Allocate(const uint32_t, const uint32_t) { return {}; }

void Ren::MemAllocator::Free(const uint32_t block) {
    alloc_.Free(block);
    assert(alloc_.IntegrityCheck());
}
