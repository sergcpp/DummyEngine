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

    void Free();
    void FreeImmediate();

    eResState resource_state = eResState::Undefined;
};
#endif

class AccStructureSW : public IAccStructure {
  public:
    AccStructureSW(SubAllocation _mesh_alloc, SubAllocation _nodes_alloc, SubAllocation _prim_alloc)
        : mesh_alloc(_mesh_alloc), nodes_alloc(_nodes_alloc), prim_alloc(_prim_alloc) {}

    SubAllocation mesh_alloc, nodes_alloc, prim_alloc;
};
} // namespace Ren