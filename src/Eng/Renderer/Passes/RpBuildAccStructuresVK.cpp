#include "RpBuildAccStructures.h"

void RpBuildAccStructuresExecutor::Execute(RpBuilder &builder) {
    RpAllocBuf &rt_obj_instances_buf = builder.GetReadBuffer(rt_obj_instances_buf_);
    RpAllocBuf &rt_tlas_buf = builder.GetWriteBuffer(rt_tlas_buf_);
    RpAllocBuf &rt_tlas_build_scratch_buf = builder.GetWriteBuffer(rt_tlas_build_scratch_buf_);

    Ren::Context &ctx = builder.ctx();
    Ren::ApiContext *api_ctx = ctx.api_ctx();

    auto *vk_tlas = reinterpret_cast<Ren::AccStructureVK *>(acc_struct_data_->rt_tlas[rt_index_]);

    VkAccelerationStructureGeometryInstancesDataKHR instances_data = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR};
    instances_data.data.deviceAddress = rt_obj_instances_buf.ref->vk_device_address();

    VkAccelerationStructureGeometryKHR tlas_geo = {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
    tlas_geo.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    tlas_geo.geometry.instances = instances_data;

    VkAccelerationStructureBuildGeometryInfoKHR tlas_build_info = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
    tlas_build_info.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    tlas_build_info.geometryCount = 1;
    tlas_build_info.pGeometries = &tlas_geo;
    tlas_build_info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    tlas_build_info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;

    tlas_build_info.srcAccelerationStructure = VK_NULL_HANDLE;
    tlas_build_info.dstAccelerationStructure = vk_tlas->vk_handle();

    tlas_build_info.scratchData.deviceAddress = rt_tlas_build_scratch_buf.ref->vk_device_address();

    /*VkAccelerationStructureCreateInfoKHR create_info = {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
    create_info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    create_info.buffer = acc_struct_data_->rt_tlas_buf->vk_handle();
    create_info.offset = 0;
    create_info.size = acc_struct_data_->rt_tlas_buf->size();

    VkAccelerationStructureKHR tlas_handle;
    VkResult res = vkCreateAccelerationStructureKHR(api_ctx->device, &create_info, nullptr, &tlas_handle);
    if (res != VK_SUCCESS) {
        ctx.log()->Error("[RpBuildAccStructures]: Failed to create acceleration structure!");
    }

    if (!vk_tlas->Init(api_ctx, tlas_handle)) {
        ctx.log()->Error("[SceneManager::InitHWAccStructures]: Failed to init TLAS!");
    }*/

    VkAccelerationStructureBuildRangeInfoKHR range_info = {};
    range_info.primitiveOffset = 0;
    range_info.primitiveCount = p_list_->rt_obj_instances[rt_index_].count;
    range_info.firstVertex = 0;
    range_info.transformOffset = 0;

    VkCommandBuffer cmd_buf = api_ctx->draw_cmd_buf[api_ctx->backend_frame];

    const VkAccelerationStructureBuildRangeInfoKHR *build_range = &range_info;
    vkCmdBuildAccelerationStructuresKHR(cmd_buf, 1, &tlas_build_info, &build_range);
}
