#pragma once

#include <cstdint>

#include "Fwd.h"
#include "Resource.h"

namespace Ren {
struct ApiContext;

class IAccStructure {
  public:
    virtual ~IAccStructure() {}

    uint32_t geo_index = 0, geo_count = 0;
};

#if defined(USE_VK_RENDER)
class AccStructureVK : public IAccStructure {
    ApiContext *api_ctx_ = nullptr;
    VkAccelerationStructureKHR handle_ = {};

    void Destroy();

  public:
    AccStructureVK() = default;
    ~AccStructureVK() override { Destroy(); }

    AccStructureVK(const AccStructureVK &rhs) = delete;
    AccStructureVK(AccStructureVK &&rhs) = delete;

    AccStructureVK &operator=(const AccStructureVK &rhs) = delete;
    AccStructureVK &operator=(AccStructureVK &&rhs) = delete;

    const VkAccelerationStructureKHR &vk_handle() const {
        return handle_;
    } // needs to be reference as we take it's address later
    VkDeviceAddress vk_device_address() const;

    bool Init(ApiContext *api_ctx, VkAccelerationStructureKHR handle);

    eResState resource_state = eResState::Undefined;
};
#endif

class AccStructureSW : public IAccStructure {
  public:
    AccStructureSW(uint32_t _mesh_index)
        : mesh_index(_mesh_index) {}

    uint32_t mesh_index = 0;
};
} // namespace Ren