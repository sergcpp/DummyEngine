#include "AccStructure.h"

#include "VKCtx.h"

void Ren::AccStructureVK::Destroy() {
    if (handle_) {
        api_ctx_->acc_structs_to_destroy[api_ctx_->backend_frame].push_back(handle_);
        handle_ = {};
    }
}

VkDeviceAddress Ren::AccStructureVK::vk_device_address() const {
    VkAccelerationStructureDeviceAddressInfoKHR info = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR};
    info.accelerationStructure = handle_;
    return api_ctx_->vkGetAccelerationStructureDeviceAddressKHR(api_ctx_->device, &info);
}

bool Ren::AccStructureVK::Init(ApiContext *api_ctx, VkAccelerationStructureKHR handle) {
    Destroy();

    api_ctx_ = api_ctx;
    handle_ = handle;
    return true;
}