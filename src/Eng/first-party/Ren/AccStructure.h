#pragma once

#include <cstdint>

#include "Buffer.h"
#include "FreelistAlloc.h"
#include "Fwd.h"
#include "Resource.h"

namespace Ren {
struct ApiContext;

class IAccStructure {
  public:
    virtual ~IAccStructure() {}

    virtual void Free() {}
    virtual void FreeImmediate() { Free(); }
};

#if defined(USE_VK_RENDER)
class AccStructureVK : public IAccStructure {
    ApiContext *api_ctx_ = nullptr;
    VkAccelerationStructureKHR handle_ = {};

  public:
    FreelistAlloc::Allocation mem_alloc;

    AccStructureVK() = default;
    ~AccStructureVK() override { Free(); }

    AccStructureVK(const AccStructureVK &rhs) = delete;
    AccStructureVK(AccStructureVK &&rhs) = delete;

    AccStructureVK &operator=(const AccStructureVK &rhs) = delete;
    AccStructureVK &operator=(AccStructureVK &&rhs) = delete;

    [[nodiscard]] const VkAccelerationStructureKHR &vk_handle() const {
        return handle_;
    } // needs to be reference as we take it's address later
    [[nodiscard]] VkDeviceAddress vk_device_address() const;

    [[nodiscard]] bool Init(ApiContext *api_ctx, VkAccelerationStructureKHR handle);

    void Free() override;
    void FreeImmediate() override;

    eResState resource_state = eResState::Undefined;
};
#endif

class AccStructureSW : public IAccStructure {
  public:
    AccStructureSW(const uint32_t _mesh_index, const SubAllocation _nodes_alloc, const SubAllocation _prim_alloc)
        : mesh_index(_mesh_index), nodes_alloc(_nodes_alloc), prim_alloc(_prim_alloc) {}

    uint32_t mesh_index;
    SubAllocation nodes_alloc, prim_alloc;
};
} // namespace Ren