#pragma once

#if defined(USE_VK_RENDER)
#include "VKCtx.h"
#endif

namespace Ren {
class IAccStructure {
  public:
    virtual ~IAccStructure() {}
};

#if defined(USE_VK_RENDER)
class AccStructureVK : public IAccStructure {
    ApiContext *api_ctx_ = nullptr;
    VkAccelerationStructureKHR handle_ = VK_NULL_HANDLE;

    void Destroy();
  public:
    AccStructureVK() = default;
    ~AccStructureVK() override { Destroy(); }

    AccStructureVK(const AccStructureVK &rhs) = delete;
    AccStructureVK(AccStructureVK &&rhs) = delete;

    AccStructureVK &operator=(const AccStructureVK &rhs) = delete;
    AccStructureVK &operator=(AccStructureVK &&rhs) = delete;

    VkAccelerationStructureKHR vk_handle() const { return handle_; }
    VkDeviceAddress vk_device_address() const;

    bool Init(ApiContext *api_ctx, VkAccelerationStructureKHR handle);

    eResState resource_state = eResState::Undefined;
};
#endif
}