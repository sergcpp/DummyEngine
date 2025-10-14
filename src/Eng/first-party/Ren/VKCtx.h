#pragma once

#include "Common.h"
#include "MemoryAllocator.h"
#include "SmallVector.h"
#include "Storage.h"
#include "VK.h"

namespace Ren {
class ILog;

class Texture;
using TexRef = StrongRef<Texture, NamedStorage<Texture>>;

struct ApiContext {
    void *vulkan_module = {};
    VkInstance instance = {};
    VkDebugReportCallbackEXT debug_callback = {};
    VkSurfaceKHR surface = {};
    VkPhysicalDevice physical_device = {};
    VkPhysicalDeviceLimits phys_device_limits = {};
    VkPhysicalDeviceProperties device_properties = {};
    VkPhysicalDeviceMemoryProperties mem_properties = {};
    VkPipelineCache pipeline_cache = {};
    uint32_t present_family_index = 0, graphics_family_index = 0, compute_family_index = 0;

    VkDevice device = {};
    VkExtent2D res = {};
    VkSurfaceFormatKHR surface_format = {};
    VkPresentModeKHR present_mode = {};
    SmallVector<VkImage, MaxFramesInFlight> present_images;
    SmallVector<VkImageView, MaxFramesInFlight> present_image_views;
    SmallVector<TexRef, MaxFramesInFlight> present_image_refs;
    VkSwapchainKHR swapchain = {};

    uint32_t active_present_image = 0;

    VkQueue present_queue = {}, graphics_queue = {}, compute_queue = {};

    VkCommandPool command_pool = {}, temp_command_pool = {};
    VkCommandBuffer draw_cmd_buf[MaxFramesInFlight] = {};
    VkCommandBuffer curr_cmd_buf = {};

    VkSemaphore image_avail_semaphores[MaxFramesInFlight] = {};
    VkSemaphore render_finished_semaphores[MaxFramesInFlight] = {};
    bool render_finished_semaphore_is_set[MaxFramesInFlight] = {};
    VkFence in_flight_fences[MaxFramesInFlight] = {};

    VkQueryPool query_pools[MaxFramesInFlight] = {};
    uint32_t query_counts[MaxFramesInFlight] = {};
    uint64_t query_results[MaxFramesInFlight][MaxTimestampQueries] = {};

    int backend_frame = 0;

    uint32_t max_combined_image_samplers = 0;

    bool raytracing_supported = false, ray_query_supported = false;
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rt_props = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR};
    VkPhysicalDeviceAccelerationStructurePropertiesKHR acc_props = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR};

    bool renderpass_loadstore_none_supported = false;

    bool subgroup_size_control_supported = false;

    bool fp16_supported = false;

    bool shader_int64_supported = false;

    bool shader_buf_int64_atomics_supported = false;

    bool coop_matrix_supported = false;

    bool pageable_memory_supported = false;

    uint32_t supported_stages_mask = 0xffffffff;

    // resources scheduled for deferred destruction
    std::vector<VkImage> images_to_destroy[MaxFramesInFlight];
    std::vector<VkImageView> image_views_to_destroy[MaxFramesInFlight];
    std::vector<VkSampler> samplers_to_destroy[MaxFramesInFlight];
    std::vector<MemAllocation> allocations_to_free[MaxFramesInFlight];
    std::vector<std::unique_ptr<MemAllocators>> allocators_to_release[MaxFramesInFlight];
    std::vector<VkBuffer> bufs_to_destroy[MaxFramesInFlight];
    std::vector<VkBufferView> buf_views_to_destroy[MaxFramesInFlight];
    std::vector<VkDeviceMemory> mem_to_free[MaxFramesInFlight];
    std::vector<VkRenderPass> render_passes_to_destroy[MaxFramesInFlight];
    std::vector<VkFramebuffer> framebuffers_to_destroy[MaxFramesInFlight];
    std::vector<VkDescriptorPool> descriptor_pools_to_destroy[MaxFramesInFlight];
    std::vector<VkPipelineLayout> pipeline_layouts_to_destroy[MaxFramesInFlight];
    std::vector<VkPipeline> pipelines_to_destroy[MaxFramesInFlight];
    std::vector<VkAccelerationStructureKHR> acc_structs_to_destroy[MaxFramesInFlight];

    // main functions
    PFN_vkCreateInstance vkCreateInstance = {};
    PFN_vkDestroyInstance vkDestroyInstance = {};
    PFN_vkEnumerateInstanceLayerProperties vkEnumerateInstanceLayerProperties = {};
    PFN_vkEnumerateInstanceExtensionProperties vkEnumerateInstanceExtensionProperties = {};
    PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = {};
    PFN_vkGetDeviceProcAddr vkGetDeviceProcAddr = {};

    PFN_vkEnumeratePhysicalDevices vkEnumeratePhysicalDevices = {};
    PFN_vkGetPhysicalDeviceProperties vkGetPhysicalDeviceProperties = {};
    PFN_vkGetPhysicalDeviceFeatures vkGetPhysicalDeviceFeatures = {};
    PFN_vkGetPhysicalDeviceQueueFamilyProperties vkGetPhysicalDeviceQueueFamilyProperties = {};

    PFN_vkCreateDevice vkCreateDevice = {};
    PFN_vkDestroyDevice vkDestroyDevice = {};

    PFN_vkEnumerateDeviceExtensionProperties vkEnumerateDeviceExtensionProperties = {};

    PFN_vkBeginCommandBuffer vkBeginCommandBuffer = {};
    PFN_vkEndCommandBuffer vkEndCommandBuffer = {};

    PFN_vkQueueSubmit vkQueueSubmit = {};
    PFN_vkQueueWaitIdle vkQueueWaitIdle = {};
    PFN_vkResetCommandBuffer vkResetCommandBuffer = {};
    PFN_vkCreateImageView vkCreateImageView = {};
    PFN_vkDestroyImageView vkDestroyImageView = {};

    PFN_vkAcquireNextImageKHR vkAcquireNextImageKHR = {};
    PFN_vkQueuePresentKHR vkQueuePresentKHR = {};

    PFN_vkCmdPipelineBarrier vkCmdPipelineBarrier = {};
    PFN_vkCmdBeginRenderPass vkCmdBeginRenderPass = {};
    PFN_vkCmdBindPipeline vkCmdBindPipeline = {};
    PFN_vkCmdSetViewport vkCmdSetViewport = {};
    PFN_vkCmdSetScissor vkCmdSetScissor = {};
    PFN_vkCmdBindDescriptorSets vkCmdBindDescriptorSets = {};
    PFN_vkCmdBindVertexBuffers vkCmdBindVertexBuffers = {};
    PFN_vkCmdBindIndexBuffer vkCmdBindIndexBuffer = {};
    PFN_vkCmdDraw vkCmdDraw = {};
    PFN_vkCmdDrawIndexed vkCmdDrawIndexed = {};
    PFN_vkCmdEndRenderPass vkCmdEndRenderPass = {};
    PFN_vkCmdCopyBufferToImage vkCmdCopyBufferToImage = {};
    PFN_vkCmdCopyImageToBuffer vkCmdCopyImageToBuffer = {};
    PFN_vkCmdCopyBuffer vkCmdCopyBuffer = {};
    PFN_vkCmdFillBuffer vkCmdFillBuffer = {};
    PFN_vkCmdUpdateBuffer vkCmdUpdateBuffer = {};
    PFN_vkCmdPushConstants vkCmdPushConstants = {};
    PFN_vkCmdBlitImage vkCmdBlitImage = {};
    PFN_vkCmdClearColorImage vkCmdClearColorImage = {};
    PFN_vkCmdClearDepthStencilImage vkCmdClearDepthStencilImage = {};
    PFN_vkCmdClearAttachments vkCmdClearAttachments = {};
    PFN_vkCmdCopyImage vkCmdCopyImage = {};
    PFN_vkCmdDispatch vkCmdDispatch = {};
    PFN_vkCmdDispatchIndirect vkCmdDispatchIndirect = {};
    PFN_vkCmdResetQueryPool vkCmdResetQueryPool = {};
    PFN_vkCmdWriteTimestamp vkCmdWriteTimestamp = {};

    bool Load(ILog *log);

    // instance functions
    PFN_vkCreateDebugReportCallbackEXT vkCreateDebugReportCallbackEXT = {};
    PFN_vkDestroyDebugReportCallbackEXT vkDestroyDebugReportCallbackEXT = {};
    PFN_vkDebugReportMessageEXT vkDebugReportMessageEXT = {};

    PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR = {};
    PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR = {};
    PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR = {};
    PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR = {};

    PFN_vkCmdBeginDebugUtilsLabelEXT vkCmdBeginDebugUtilsLabelEXT = {};
    PFN_vkCmdEndDebugUtilsLabelEXT vkCmdEndDebugUtilsLabelEXT = {};
    PFN_vkSetDebugUtilsObjectNameEXT vkSetDebugUtilsObjectNameEXT = {};

    PFN_vkCmdSetDepthBias vkCmdSetDepthBias = {};

    PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR = {};
    PFN_vkCmdWriteAccelerationStructuresPropertiesKHR vkCmdWriteAccelerationStructuresPropertiesKHR = {};
    PFN_vkCmdCopyAccelerationStructureKHR vkCmdCopyAccelerationStructureKHR = {};
    PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR = {};
    PFN_vkCmdTraceRaysIndirectKHR vkCmdTraceRaysIndirectKHR = {};

    PFN_vkDeviceWaitIdle vkDeviceWaitIdle = {};

    PFN_vkGetPhysicalDeviceProperties2KHR vkGetPhysicalDeviceProperties2KHR = {};
    PFN_vkGetPhysicalDeviceFeatures2KHR vkGetPhysicalDeviceFeatures2KHR = {};
    PFN_vkGetBufferDeviceAddressKHR vkGetBufferDeviceAddressKHR = {};

    PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR = {};
    PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR = {};

    PFN_vkGetPhysicalDeviceMemoryProperties vkGetPhysicalDeviceMemoryProperties = {};
    PFN_vkGetPhysicalDeviceFormatProperties vkGetPhysicalDeviceFormatProperties = {};
    PFN_vkGetPhysicalDeviceImageFormatProperties vkGetPhysicalDeviceImageFormatProperties = {};
    PFN_vkGetPhysicalDeviceCooperativeMatrixPropertiesKHR vkGetPhysicalDeviceCooperativeMatrixPropertiesKHR = {};

    PFN_vkCreateImage vkCreateImage = {};
    PFN_vkDestroyImage vkDestroyImage = {};

    PFN_vkGetImageMemoryRequirements vkGetImageMemoryRequirements = {};
    PFN_vkAllocateMemory vkAllocateMemory = {};
    PFN_vkFreeMemory vkFreeMemory = {};
    PFN_vkBindImageMemory vkBindImageMemory = {};

    PFN_vkCreateRenderPass vkCreateRenderPass = {};
    PFN_vkDestroyRenderPass vkDestroyRenderPass = {};

    PFN_vkCreateFramebuffer vkCreateFramebuffer = {};
    PFN_vkDestroyFramebuffer vkDestroyFramebuffer = {};

    PFN_vkCreateBuffer vkCreateBuffer = {};
    PFN_vkBindBufferMemory vkBindBufferMemory = {};
    PFN_vkDestroyBuffer vkDestroyBuffer = {};
    PFN_vkGetBufferMemoryRequirements vkGetBufferMemoryRequirements = {};

    PFN_vkCreateBufferView vkCreateBufferView = {};
    PFN_vkDestroyBufferView vkDestroyBufferView = {};

    PFN_vkMapMemory vkMapMemory = {};
    PFN_vkUnmapMemory vkUnmapMemory = {};
    PFN_vkFlushMappedMemoryRanges vkFlushMappedMemoryRanges = {};
    PFN_vkInvalidateMappedMemoryRanges vkInvalidateMappedMemoryRanges = {};

    PFN_vkCreateShaderModule vkCreateShaderModule = {};
    PFN_vkDestroyShaderModule vkDestroyShaderModule = {};

    PFN_vkCreateDescriptorSetLayout vkCreateDescriptorSetLayout = {};
    PFN_vkDestroyDescriptorSetLayout vkDestroyDescriptorSetLayout = {};

    PFN_vkCreatePipelineLayout vkCreatePipelineLayout = {};
    PFN_vkDestroyPipelineLayout vkDestroyPipelineLayout = {};

    PFN_vkCreateGraphicsPipelines vkCreateGraphicsPipelines = {};
    PFN_vkCreateComputePipelines vkCreateComputePipelines = {};
    PFN_vkDestroyPipeline vkDestroyPipeline = {};

    PFN_vkCreateSemaphore vkCreateSemaphore = {};
    PFN_vkDestroySemaphore vkDestroySemaphore = {};
    PFN_vkCreateSampler vkCreateSampler = {};
    PFN_vkDestroySampler vkDestroySampler = {};

    PFN_vkCreateDescriptorPool vkCreateDescriptorPool = {};
    PFN_vkDestroyDescriptorPool vkDestroyDescriptorPool = {};
    PFN_vkResetDescriptorPool vkResetDescriptorPool = {};

    PFN_vkAllocateDescriptorSets vkAllocateDescriptorSets = {};
    PFN_vkFreeDescriptorSets vkFreeDescriptorSets = {};
    PFN_vkUpdateDescriptorSets vkUpdateDescriptorSets = {};

    PFN_vkCreateQueryPool vkCreateQueryPool = {};
    PFN_vkDestroyQueryPool vkDestroyQueryPool = {};
    PFN_vkGetQueryPoolResults vkGetQueryPoolResults = {};

    PFN_vkCreatePipelineCache vkCreatePipelineCache = {};
    PFN_vkGetPipelineCacheData vkGetPipelineCacheData = {};
    PFN_vkDestroyPipelineCache vkDestroyPipelineCache = {};

    PFN_vkCmdBeginRenderingKHR vkCmdBeginRenderingKHR = {};
    PFN_vkCmdEndRenderingKHR vkCmdEndRenderingKHR = {};

#if defined(VK_USE_PLATFORM_WIN32_KHR)
    PFN_vkCreateWin32SurfaceKHR vkCreateWin32SurfaceKHR = {};
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
    PFN_vkCreateXlibSurfaceKHR vkCreateXlibSurfaceKHR = {};
#elif defined(VK_USE_PLATFORM_IOS_MVK)
    PFN_vkCreateIOSSurfaceMVK vkCreateIOSSurfaceMVK = {};
#elif defined(VK_USE_PLATFORM_MACOS_MVK)
    PFN_vkCreateMacOSSurfaceMVK vkCreateMacOSSurfaceMVK = {};
#endif
    PFN_vkDestroySurfaceKHR vkDestroySurfaceKHR = {};
    PFN_vkGetPhysicalDeviceSurfaceSupportKHR vkGetPhysicalDeviceSurfaceSupportKHR = {};
    PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR vkGetPhysicalDeviceSurfaceCapabilitiesKHR = {};
    PFN_vkGetPhysicalDeviceSurfaceFormatsKHR vkGetPhysicalDeviceSurfaceFormatsKHR = {};
    PFN_vkGetPhysicalDeviceSurfacePresentModesKHR vkGetPhysicalDeviceSurfacePresentModesKHR = {};

    PFN_vkCreateSwapchainKHR vkCreateSwapchainKHR = {};
    PFN_vkDestroySwapchainKHR vkDestroySwapchainKHR = {};
    PFN_vkGetDeviceQueue vkGetDeviceQueue = {};
    PFN_vkCreateCommandPool vkCreateCommandPool = {};
    PFN_vkDestroyCommandPool vkDestroyCommandPool = {};
    PFN_vkAllocateCommandBuffers vkAllocateCommandBuffers = {};
    PFN_vkFreeCommandBuffers vkFreeCommandBuffers = {};

    PFN_vkGetSwapchainImagesKHR vkGetSwapchainImagesKHR = {};

    PFN_vkCreateFence vkCreateFence = {};
    PFN_vkWaitForFences vkWaitForFences = {};
    PFN_vkResetFences vkResetFences = {};
    PFN_vkDestroyFence vkDestroyFence = {};
    PFN_vkGetFenceStatus vkGetFenceStatus = {};

    bool LoadInstanceFunctions(ILog *log);

    bool InitVkInstance(const char *enabled_layers[], int enabled_layers_count, int &validation_level, ILog *log);
    bool InitVkSurface(ILog *log);
    bool ChooseVkPhysicalDevice(std::string_view preferred_device, ILog *log);
    bool InitVkDevice(const char *enabled_layers[], int enabled_layers_count, int validation_level, ILog *log);
    bool InitSwapChain(int w, int h, ILog *log);
    bool InitCommandBuffers(uint32_t family_index, ILog *log);
    bool InitPresentImageViews(ILog *log);

    VkCommandBuffer BegSingleTimeCommands();
    void EndSingleTimeCommands(VkCommandBuffer command_buf);
    void EndSingleTimeCommands(VkCommandBuffer command_buf, VkFence fence_to_insert);

    ~ApiContext();
};

inline VkDeviceSize AlignTo(VkDeviceSize size, VkDeviceSize alignment) {
    return alignment * ((size + alignment - 1) / alignment);
}

bool MatchDeviceNames(std::string_view name, std::string_view pattern);

class ILog;

bool ReadbackTimestampQueries(ApiContext *api_ctx, int i);

void DestroyDeferredResources(ApiContext *api_ctx, int i);

// Useful for synchronization debugging
void _SubmitCurrentCommandsWaitForCompletionAndResume(ApiContext *api_ctx);

} // namespace Ren