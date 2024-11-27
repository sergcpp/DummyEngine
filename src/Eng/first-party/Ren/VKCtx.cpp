#include "VKCtx.h"

#include <regex>

#if defined(__linux__) || defined(__APPLE__)
#include <dlfcn.h>
#endif

#include "Config.h"
#include "Log.h"
#include "SmallVector.h"
#include "Texture.h"

namespace Ren {
#if defined(__linux__)
Display *g_dpy = nullptr;
Window g_win = {};
#elif defined(__APPLE__)
void *g_metal_layer = nullptr;
#endif
} // namespace Ren

bool Ren::ApiContext::Load(ILog *log) {
#if defined(_WIN32)
    static_assert(sizeof(void *) == sizeof(HMODULE), "!");
    vulkan_module = LoadLibrary("vulkan-1.dll");
    if (!vulkan_module) {
        log->Error("Failed to load vulkan-1.dll");
        return false;
    }
#define LOAD_VK_FUN(x)                                                                                                 \
    x = (PFN_##x)GetProcAddress((HMODULE)vulkan_module, #x);                                                           \
    if (!(x)) {                                                                                                        \
        log->Error("Failed to load %s", #x);                                                                           \
        return false;                                                                                                  \
    }
#elif defined(__linux__)
    vulkan_module = dlopen("libvulkan.so.1", RTLD_LAZY);
    if (!vulkan_module) {
        log->Error("Failed to load libvulkan.so");
        return false;
    }

#define LOAD_VK_FUN(x)                                                                                                 \
    x = (PFN_##x)dlsym(vulkan_module, #x);                                                                             \
    if (!(x)) {                                                                                                        \
        log->Error("Failed to load %s", #x);                                                                           \
        return false;                                                                                                  \
    }
#else

#if defined(VK_USE_PLATFORM_IOS_MVK)
    vulkan_module = dlopen("libMoltenVK.dylib", RTLD_LAZY);
    if (!vulkan_module) {
        log->Error("Failed to load libMoltenVK.dylib");
        return false;
    }
#else
    vulkan_module = dlopen("libvulkan.dylib", RTLD_LAZY);
    if (!vulkan_module) {
        log->Error("Failed to load libvulkan.dylib");
        return false;
    }
#endif

#define LOAD_VK_FUN(x)                                                                                                 \
    x = (PFN_##x)dlsym(vulkan_module, #x);                                                                             \
    if (!(x)) {                                                                                                        \
        log->Error("Failed to load %s", #x);                                                                           \
        return false;                                                                                                  \
    }
#endif

    LOAD_VK_FUN(vkCreateInstance)
    LOAD_VK_FUN(vkDestroyInstance)

    LOAD_VK_FUN(vkEnumerateInstanceLayerProperties)
    LOAD_VK_FUN(vkEnumerateInstanceExtensionProperties)

    LOAD_VK_FUN(vkGetInstanceProcAddr)
    LOAD_VK_FUN(vkGetDeviceProcAddr)

    LOAD_VK_FUN(vkEnumeratePhysicalDevices);
    LOAD_VK_FUN(vkGetPhysicalDeviceProperties);
    LOAD_VK_FUN(vkGetPhysicalDeviceFeatures);
    LOAD_VK_FUN(vkGetPhysicalDeviceQueueFamilyProperties);

    LOAD_VK_FUN(vkCreateDevice)
    LOAD_VK_FUN(vkDestroyDevice)

    LOAD_VK_FUN(vkEnumerateDeviceExtensionProperties)

#if defined(VK_USE_PLATFORM_WIN32_KHR)
    LOAD_VK_FUN(vkCreateWin32SurfaceKHR)
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
    LOAD_VK_FUN(vkCreateXlibSurfaceKHR)
#elif defined(VK_USE_PLATFORM_IOS_MVK)
    LOAD_VK_FUN(vkCreateIOSSurfaceMVK)
#elif defined(VK_USE_PLATFORM_MACOS_MVK)
    LOAD_VK_FUN(vkCreateMacOSSurfaceMVK)
#endif
    LOAD_VK_FUN(vkDestroySurfaceKHR)

    LOAD_VK_FUN(vkGetPhysicalDeviceSurfaceSupportKHR)
    LOAD_VK_FUN(vkGetPhysicalDeviceSurfaceCapabilitiesKHR)
    LOAD_VK_FUN(vkGetPhysicalDeviceSurfaceFormatsKHR)
    LOAD_VK_FUN(vkGetPhysicalDeviceSurfacePresentModesKHR)

    LOAD_VK_FUN(vkCreateSwapchainKHR)
    LOAD_VK_FUN(vkDestroySwapchainKHR)
    LOAD_VK_FUN(vkGetDeviceQueue)
    LOAD_VK_FUN(vkCreateCommandPool)
    LOAD_VK_FUN(vkDestroyCommandPool)
    LOAD_VK_FUN(vkAllocateCommandBuffers)
    LOAD_VK_FUN(vkFreeCommandBuffers)

    LOAD_VK_FUN(vkGetSwapchainImagesKHR)

    LOAD_VK_FUN(vkCreateFence)
    LOAD_VK_FUN(vkWaitForFences)
    LOAD_VK_FUN(vkResetFences)
    LOAD_VK_FUN(vkDestroyFence)
    LOAD_VK_FUN(vkGetFenceStatus)

    LOAD_VK_FUN(vkBeginCommandBuffer)
    LOAD_VK_FUN(vkEndCommandBuffer)

    LOAD_VK_FUN(vkQueueSubmit)
    LOAD_VK_FUN(vkQueueWaitIdle)
    LOAD_VK_FUN(vkResetCommandBuffer)
    LOAD_VK_FUN(vkCreateImageView)
    LOAD_VK_FUN(vkDestroyImageView)

    LOAD_VK_FUN(vkAcquireNextImageKHR)
    LOAD_VK_FUN(vkQueuePresentKHR)

    LOAD_VK_FUN(vkGetPhysicalDeviceMemoryProperties)
    LOAD_VK_FUN(vkGetPhysicalDeviceFormatProperties)
    LOAD_VK_FUN(vkGetPhysicalDeviceImageFormatProperties)

    LOAD_VK_FUN(vkCreateImage)
    LOAD_VK_FUN(vkDestroyImage)

    LOAD_VK_FUN(vkGetImageMemoryRequirements)
    LOAD_VK_FUN(vkAllocateMemory)
    LOAD_VK_FUN(vkFreeMemory)
    LOAD_VK_FUN(vkBindImageMemory)

    LOAD_VK_FUN(vkCreateRenderPass)
    LOAD_VK_FUN(vkDestroyRenderPass)

    LOAD_VK_FUN(vkCreateFramebuffer)
    LOAD_VK_FUN(vkDestroyFramebuffer)

    LOAD_VK_FUN(vkCreateBuffer)
    LOAD_VK_FUN(vkGetBufferMemoryRequirements)
    LOAD_VK_FUN(vkBindBufferMemory)
    LOAD_VK_FUN(vkDestroyBuffer)
    LOAD_VK_FUN(vkGetBufferMemoryRequirements)

    LOAD_VK_FUN(vkCreateBufferView)
    LOAD_VK_FUN(vkDestroyBufferView)

    LOAD_VK_FUN(vkMapMemory)
    LOAD_VK_FUN(vkUnmapMemory)
    LOAD_VK_FUN(vkFlushMappedMemoryRanges)
    LOAD_VK_FUN(vkInvalidateMappedMemoryRanges)

    LOAD_VK_FUN(vkCreateShaderModule)
    LOAD_VK_FUN(vkDestroyShaderModule)
    LOAD_VK_FUN(vkCreateDescriptorSetLayout)
    LOAD_VK_FUN(vkDestroyDescriptorSetLayout)

    LOAD_VK_FUN(vkCreatePipelineLayout)
    LOAD_VK_FUN(vkDestroyPipelineLayout)

    LOAD_VK_FUN(vkCreateGraphicsPipelines)
    LOAD_VK_FUN(vkCreateComputePipelines)
    LOAD_VK_FUN(vkDestroyPipeline)

    LOAD_VK_FUN(vkCreateSemaphore)
    LOAD_VK_FUN(vkDestroySemaphore)
    LOAD_VK_FUN(vkCreateSampler)
    LOAD_VK_FUN(vkDestroySampler)

    LOAD_VK_FUN(vkCreateDescriptorPool)
    LOAD_VK_FUN(vkDestroyDescriptorPool)
    LOAD_VK_FUN(vkResetDescriptorPool)

    LOAD_VK_FUN(vkAllocateDescriptorSets)
    LOAD_VK_FUN(vkFreeDescriptorSets)
    LOAD_VK_FUN(vkUpdateDescriptorSets)

    LOAD_VK_FUN(vkCreateQueryPool)
    LOAD_VK_FUN(vkDestroyQueryPool)
    LOAD_VK_FUN(vkGetQueryPoolResults)

    LOAD_VK_FUN(vkCmdPipelineBarrier)
    LOAD_VK_FUN(vkCmdBeginRenderPass)
    LOAD_VK_FUN(vkCmdBindPipeline)
    LOAD_VK_FUN(vkCmdSetViewport)
    LOAD_VK_FUN(vkCmdSetScissor)
    LOAD_VK_FUN(vkCmdBindDescriptorSets)
    LOAD_VK_FUN(vkCmdBindVertexBuffers)
    LOAD_VK_FUN(vkCmdBindIndexBuffer)
    LOAD_VK_FUN(vkCmdDraw)
    LOAD_VK_FUN(vkCmdDrawIndexed)
    LOAD_VK_FUN(vkCmdEndRenderPass)
    LOAD_VK_FUN(vkCmdCopyBufferToImage)
    LOAD_VK_FUN(vkCmdCopyImageToBuffer)
    LOAD_VK_FUN(vkCmdCopyBuffer)
    LOAD_VK_FUN(vkCmdFillBuffer)
    LOAD_VK_FUN(vkCmdUpdateBuffer)
    LOAD_VK_FUN(vkCmdPushConstants)
    LOAD_VK_FUN(vkCmdBlitImage)
    LOAD_VK_FUN(vkCmdClearColorImage)
    LOAD_VK_FUN(vkCmdClearDepthStencilImage)
    LOAD_VK_FUN(vkCmdClearAttachments)
    LOAD_VK_FUN(vkCmdCopyImage)
    LOAD_VK_FUN(vkCmdDispatch)
    LOAD_VK_FUN(vkCmdDispatchIndirect)
    LOAD_VK_FUN(vkCmdResetQueryPool)
    LOAD_VK_FUN(vkCmdWriteTimestamp)

#undef LOAD_VK_FUN

    return true;
}

bool Ren::ApiContext::LoadInstanceFunctions(ILog *log) {
#define LOAD_INSTANCE_FUN(x)                                                                                           \
    x = (PFN_##x)vkGetInstanceProcAddr(instance, #x);                                                                  \
    if (!(x)) {                                                                                                        \
        log->Error("Failed to load %s", #x);                                                                           \
        return false;                                                                                                  \
    }
#define LOAD_OPTIONAL_INSTANCE_FUN(x) x = (PFN_##x)vkGetInstanceProcAddr(instance, #x);

    LOAD_INSTANCE_FUN(vkCreateDebugReportCallbackEXT)
    LOAD_INSTANCE_FUN(vkDestroyDebugReportCallbackEXT)
    LOAD_INSTANCE_FUN(vkDebugReportMessageEXT)

    LOAD_INSTANCE_FUN(vkCreateAccelerationStructureKHR)
    LOAD_INSTANCE_FUN(vkDestroyAccelerationStructureKHR)
    LOAD_INSTANCE_FUN(vkGetAccelerationStructureBuildSizesKHR)
    LOAD_INSTANCE_FUN(vkGetAccelerationStructureDeviceAddressKHR)

    LOAD_INSTANCE_FUN(vkCmdBeginDebugUtilsLabelEXT)
    LOAD_INSTANCE_FUN(vkCmdEndDebugUtilsLabelEXT)
    LOAD_INSTANCE_FUN(vkSetDebugUtilsObjectNameEXT)

    LOAD_INSTANCE_FUN(vkCmdSetDepthBias)

    LOAD_INSTANCE_FUN(vkCmdBuildAccelerationStructuresKHR)
    LOAD_INSTANCE_FUN(vkCmdWriteAccelerationStructuresPropertiesKHR)
    LOAD_INSTANCE_FUN(vkCmdCopyAccelerationStructureKHR)
    LOAD_INSTANCE_FUN(vkCmdTraceRaysKHR)
    LOAD_INSTANCE_FUN(vkCmdTraceRaysIndirectKHR)

    LOAD_INSTANCE_FUN(vkDeviceWaitIdle)

    LOAD_INSTANCE_FUN(vkGetPhysicalDeviceProperties2KHR)
    LOAD_INSTANCE_FUN(vkGetPhysicalDeviceFeatures2KHR)
    LOAD_INSTANCE_FUN(vkGetBufferDeviceAddressKHR)

    // allowed to fail
    LOAD_OPTIONAL_INSTANCE_FUN(vkCreateRayTracingPipelinesKHR)
    LOAD_OPTIONAL_INSTANCE_FUN(vkGetRayTracingShaderGroupHandlesKHR)

    LOAD_OPTIONAL_INSTANCE_FUN(vkCmdBeginRenderingKHR)
    LOAD_OPTIONAL_INSTANCE_FUN(vkCmdEndRenderingKHR)

    LOAD_OPTIONAL_INSTANCE_FUN(vkGetPhysicalDeviceCooperativeMatrixPropertiesKHR)

#undef LOAD_INSTANCE_FUN

    return true;
}

bool Ren::ApiContext::InitVkInstance(const char *enabled_layers[], const int enabled_layers_count,
                                     int &validation_level, ILog *log) {
    if (validation_level) { // Find validation layer
        uint32_t layers_count = 0;
        vkEnumerateInstanceLayerProperties(&layers_count, nullptr);

        if (!layers_count) {
            log->Error("Failed to find any layer in your system");
            return false;
        }

        SmallVector<VkLayerProperties, 16> layers_available(layers_count);
        vkEnumerateInstanceLayerProperties(&layers_count, &layers_available[0]);

        bool found_validation = false;
        for (uint32_t i = 0; i < layers_count; i++) {
            if (strcmp(layers_available[i].layerName, "VK_LAYER_KHRONOS_validation") == 0) {
                found_validation = true;
            }
        }

        if (!found_validation) {
            log->Warning("Could not find validation layer");
            validation_level = 0;
        }
    }

    SmallVector<const char *, 16> desired_extensions;

    desired_extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
#if defined(VK_USE_PLATFORM_WIN32_KHR)
    desired_extensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
    desired_extensions.push_back(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_MACOS_MVK)
    desired_extensions.push_back(VK_MVK_MACOS_SURFACE_EXTENSION_NAME);
#endif

    const int number_required_extensions = int(desired_extensions.size());

    desired_extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
    desired_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    desired_extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

    if (validation_level) {
        desired_extensions.push_back(VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME);
    }

    const int number_optional_extensions = int(desired_extensions.size()) - number_required_extensions;

    { // Find required extensions
        uint32_t ext_count = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &ext_count, nullptr);

        SmallVector<VkExtensionProperties, 16> extensions_available(ext_count);
        vkEnumerateInstanceExtensionProperties(nullptr, &ext_count, &extensions_available[0]);

        uint32_t found_required_extensions = 0;
        for (uint32_t i = 0; i < ext_count; i++) {
            for (int j = 0; j < number_required_extensions; j++) {
                if (strcmp(extensions_available[i].extensionName, desired_extensions[j]) == 0) {
                    found_required_extensions++;
                }
            }
        }
        if (found_required_extensions != number_required_extensions) {
            log->Error("Not all required extensions were found!");
            log->Error("\tRequested:");
            for (int i = 0; i < number_required_extensions; ++i) {
                log->Error("\t\t%s", desired_extensions[i]);
            }
            log->Error("\tFound:");
            for (uint32_t i = 0; i < ext_count; i++) {
                for (int j = 0; j < number_required_extensions; j++) {
                    if (strcmp(extensions_available[i].extensionName, desired_extensions[j]) == 0) {
                        log->Error("\t\t%s", desired_extensions[i]);
                    }
                }
            }
            return false;
        }
    }

    VkApplicationInfo app_info = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app_info.pApplicationName = "Dummy";
    app_info.engineVersion = 1;
    app_info.apiVersion = VK_API_VERSION_1_1;

    VkInstanceCreateInfo instance_info = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    instance_info.pApplicationInfo = &app_info;
    if (validation_level) {
        instance_info.enabledLayerCount = enabled_layers_count;
        instance_info.ppEnabledLayerNames = enabled_layers;
    }
    instance_info.enabledExtensionCount = number_required_extensions + number_optional_extensions;
    instance_info.ppEnabledExtensionNames = desired_extensions.data();

    static const VkValidationFeatureEnableEXT enabled_validation_features[] = {
        VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT, VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT,
        VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_RESERVE_BINDING_SLOT_EXT,
        // VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT,
        //  VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT
    };

    VkValidationFeaturesEXT validation_features = {VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT};
    validation_features.enabledValidationFeatureCount = uint32_t(std::size(enabled_validation_features));
    validation_features.pEnabledValidationFeatures = enabled_validation_features;

    if (validation_level > 1) {
        instance_info.pNext = &validation_features;
    }

    const VkResult res = vkCreateInstance(&instance_info, nullptr, &instance);
    if (res != VK_SUCCESS) {
        log->Error("Failed to create vulkan instance");
        return false;
    }

    return true;
}

bool Ren::ApiContext::InitVkSurface(ILog *log) {
#if defined(VK_USE_PLATFORM_WIN32_KHR)
    HWND window = GetActiveWindow();
    if (!window) {
        return true;
    }

    VkWin32SurfaceCreateInfoKHR surface_create_info = {VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
    surface_create_info.hinstance = GetModuleHandle(NULL);
    surface_create_info.hwnd = window;

    VkResult res = vkCreateWin32SurfaceKHR(instance, &surface_create_info, nullptr, &surface);
    if (res != VK_SUCCESS) {
        log->Error("Could not create surface");
        return false;
    }
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
    if (!g_dpy || !g_win) {
        return true;
    }

    VkXlibSurfaceCreateInfoKHR surface_create_info = {VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR};
    surface_create_info.dpy = g_dpy;
    surface_create_info.window = g_win;

    VkResult res = vkCreateXlibSurfaceKHR(instance, &surface_create_info, nullptr, &surface);
    if (res != VK_SUCCESS) {
        log->Error("Could not create surface");
        return false;
    }
#elif defined(VK_USE_PLATFORM_IOS_MVK)
    VkIOSSurfaceCreateInfoMVK surface_create_info = {VK_STRUCTURE_TYPE_IOS_SURFACE_CREATE_INFO_MVK};
    surface_create_info.pView = ctx.view_from_world;

    VkResult res = vkCreateIOSSurfaceMVK(ctx.instance, &surface_create_info, nullptr, &ctx.surface);
    if (res != VK_SUCCESS) {
        log->Error("Could not create surface");
        return false;
    }
#elif defined(VK_USE_PLATFORM_MACOS_MVK)
    VkMacOSSurfaceCreateInfoMVK surface_create_info = {VK_STRUCTURE_TYPE_MACOS_SURFACE_CREATE_INFO_MVK};
    surface_create_info.pView = g_metal_layer;

    VkResult res = vkCreateMacOSSurfaceMVK(instance, &surface_create_info, nullptr, &surface);
    if (res != VK_SUCCESS) {
        log->Error("Could not create surface");
        return false;
    }
#endif

    return true;
}

bool Ren::ApiContext::InitVkDevice(const char *enabled_layers[], const int enabled_layers_count,
                                   const int validation_level, ILog *log) {
    VkDeviceQueueCreateInfo queue_create_infos[2] = {{}, {}};
    int infos_count = 0;
    const float queue_priorities[] = {1.0f};

    if (present_family_index != 0xffffffff) {
        // present queue
        queue_create_infos[infos_count] = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        queue_create_infos[infos_count].queueFamilyIndex = present_family_index;
        queue_create_infos[infos_count].queueCount = 1;
        queue_create_infos[infos_count].pQueuePriorities = queue_priorities;
        ++infos_count;
    }

    if (graphics_family_index != present_family_index) {
        // graphics queue
        queue_create_infos[infos_count] = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        queue_create_infos[infos_count].queueFamilyIndex = graphics_family_index;
        queue_create_infos[infos_count].queueCount = 1;
        queue_create_infos[infos_count].pQueuePriorities = queue_priorities;
        ++infos_count;
    }

    VkDeviceCreateInfo device_info = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    device_info.queueCreateInfoCount = infos_count;
    device_info.pQueueCreateInfos = queue_create_infos;
    if (validation_level) {
        device_info.enabledLayerCount = enabled_layers_count;
        device_info.ppEnabledLayerNames = enabled_layers;
    }
    SmallVector<const char *, 16> device_extensions;

    device_extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    device_extensions.push_back(VK_KHR_MAINTENANCE3_EXTENSION_NAME);
    // device_extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    device_extensions.push_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);

#if defined(VK_USE_PLATFORM_MACOS_MVK)
    device_extensions.push_back(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
#endif

    if (this->raytracing_supported) {
        device_extensions.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
        device_extensions.push_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
        device_extensions.push_back(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
        device_extensions.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
        device_extensions.push_back(VK_KHR_SPIRV_1_4_EXTENSION_NAME);
        device_extensions.push_back(VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME);
        if (this->ray_query_supported) {
            device_extensions.push_back(VK_KHR_RAY_QUERY_EXTENSION_NAME);
        }
    }

    if (this->dynamic_rendering_supported) {
        device_extensions.push_back(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
        device_extensions.push_back(VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME); // required for dynamic rendering
        device_extensions.push_back(VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME);   // required for depth stencil resolve
    }

    if (this->renderpass_loadstore_none_supported) {
        device_extensions.push_back(VK_EXT_LOAD_STORE_OP_NONE_EXTENSION_NAME);
    }

    if (this->subgroup_size_control_supported) {
        device_extensions.push_back(VK_EXT_SUBGROUP_SIZE_CONTROL_EXTENSION_NAME);
    }

    if (this->fp16_supported) {
        device_extensions.push_back(VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME);
        device_extensions.push_back(VK_KHR_16BIT_STORAGE_EXTENSION_NAME);
    }

    if (this->shader_buf_int64_atomics_supported) {
        device_extensions.push_back(VK_KHR_SHADER_ATOMIC_INT64_EXTENSION_NAME);
    }

    if (this->coop_matrix_supported) {
        device_extensions.push_back(VK_KHR_VULKAN_MEMORY_MODEL_EXTENSION_NAME);
        device_extensions.push_back(VK_KHR_COOPERATIVE_MATRIX_EXTENSION_NAME);
    }

    device_info.enabledExtensionCount = uint32_t(device_extensions.size());
    device_info.ppEnabledExtensionNames = device_extensions.cdata();

    VkPhysicalDeviceFeatures features = {};
    features.shaderClipDistance = VK_TRUE;
    features.samplerAnisotropy = VK_TRUE;
    features.imageCubeArray = VK_TRUE;
    features.fillModeNonSolid = VK_TRUE;
    features.fragmentStoresAndAtomics = VK_TRUE;
    features.geometryShader = VK_TRUE;
    features.shaderInt64 = this->shader_int64_supported ? VK_TRUE : VK_FALSE;
    device_info.pEnabledFeatures = &features;
    void **pp_next = const_cast<void **>(&device_info.pNext);

    /*VkPhysicalDeviceFeatures2KHR feat2 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR};
    feat2.features.shaderClipDistance = VK_TRUE;
    feat2.features.samplerAnisotropy = VK_TRUE;
    feat2.features.imageCubeArray = VK_TRUE;
    device_info.pNext = &feat2;
    void **pp_next = const_cast<void **>(&feat2.pNext);*/

    VkPhysicalDeviceDescriptorIndexingFeaturesEXT indexing_features = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT};
    indexing_features.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
    indexing_features.descriptorBindingPartiallyBound = VK_TRUE;
    indexing_features.runtimeDescriptorArray = VK_TRUE;
    (*pp_next) = &indexing_features;
    pp_next = &indexing_features.pNext;

    VkPhysicalDeviceBufferDeviceAddressFeaturesKHR device_address_features = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_KHR};
    device_address_features.bufferDeviceAddress = VK_TRUE;

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rt_pipeline_features = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR};
    rt_pipeline_features.rayTracingPipeline = VK_TRUE;
    rt_pipeline_features.rayTracingPipelineTraceRaysIndirect = VK_TRUE;

    VkPhysicalDeviceRayQueryFeaturesKHR rt_query_features = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR};
    rt_query_features.rayQuery = VK_TRUE;

    VkPhysicalDeviceAccelerationStructureFeaturesKHR acc_struct_features = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR};
    acc_struct_features.accelerationStructure = VK_TRUE;

    if (this->raytracing_supported) {
        (*pp_next) = &device_address_features;
        pp_next = &device_address_features.pNext;

        (*pp_next) = &rt_pipeline_features;
        pp_next = &rt_pipeline_features.pNext;

        (*pp_next) = &acc_struct_features;
        pp_next = &acc_struct_features.pNext;

        if (this->ray_query_supported) {
            (*pp_next) = &rt_query_features;
            pp_next = &rt_query_features.pNext;
        }
    }

    VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamic_rendering_features = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR};
    dynamic_rendering_features.dynamicRendering = VK_TRUE;
    if (this->dynamic_rendering_supported) {
        (*pp_next) = &dynamic_rendering_features;
        pp_next = &dynamic_rendering_features.pNext;
    }

    VkPhysicalDeviceSubgroupSizeControlFeaturesEXT subgroup_size_control_features = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_FEATURES_EXT};
    subgroup_size_control_features.subgroupSizeControl = VK_TRUE;
    if (this->subgroup_size_control_supported) {
        (*pp_next) = &subgroup_size_control_features;
        pp_next = &subgroup_size_control_features.pNext;
    }

    VkPhysicalDeviceShaderFloat16Int8FeaturesKHR shader_fp16_features = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES_KHR};
    shader_fp16_features.shaderFloat16 = VK_TRUE;

    VkPhysicalDevice16BitStorageFeaturesKHR storage_fp16_features = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES_KHR};
    storage_fp16_features.storageBuffer16BitAccess = VK_TRUE;

    if (this->fp16_supported) {
        (*pp_next) = &shader_fp16_features;
        pp_next = &shader_fp16_features.pNext;

        (*pp_next) = &storage_fp16_features;
        pp_next = &storage_fp16_features.pNext;
    }

    VkPhysicalDeviceShaderAtomicInt64Features atomic_int64_features = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_INT64_FEATURES_KHR};
    atomic_int64_features.shaderBufferInt64Atomics = VK_TRUE;

    if (this->shader_buf_int64_atomics_supported) {
        (*pp_next) = &atomic_int64_features;
        pp_next = &atomic_int64_features.pNext;
    }

    VkPhysicalDeviceVulkanMemoryModelFeatures mem_model_features = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_MEMORY_MODEL_FEATURES};
    mem_model_features.vulkanMemoryModel = VK_TRUE;
    mem_model_features.vulkanMemoryModelDeviceScope = VK_TRUE;

    VkPhysicalDeviceCooperativeMatrixFeaturesKHR coop_matrix_features = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COOPERATIVE_MATRIX_FEATURES_KHR};
    coop_matrix_features.cooperativeMatrix = VK_TRUE;

    if (this->coop_matrix_supported) {
        (*pp_next) = &mem_model_features;
        pp_next = &mem_model_features.pNext;

        (*pp_next) = &coop_matrix_features;
        pp_next = &coop_matrix_features.pNext;
    }

#if defined(VK_USE_PLATFORM_MACOS_MVK)
    VkPhysicalDevicePortabilitySubsetFeaturesKHR subset_features = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PORTABILITY_SUBSET_FEATURES_KHR};
    subset_features.mutableComparisonSamplers = VK_TRUE;
    (*pp_next) = &subset_features;
    pp_next = &subset_features.pNext;
#endif

    const VkResult res = vkCreateDevice(physical_device, &device_info, nullptr, &device);
    if (res != VK_SUCCESS) {
        log->Error("Failed to create logical device!");
        return false;
    }

    return true;
}

bool Ren::ApiContext::ChooseVkPhysicalDevice(std::string_view preferred_device, ILog *log) {
    uint32_t physical_device_count = 0;
    vkEnumeratePhysicalDevices(instance, &physical_device_count, nullptr);

    SmallVector<VkPhysicalDevice, 4> physical_devices(physical_device_count);
    vkEnumeratePhysicalDevices(instance, &physical_device_count, &physical_devices[0]);

    int best_score = 0;

    for (uint32_t i = 0; i < physical_device_count; i++) {
        VkPhysicalDeviceProperties device_properties = {};
        vkGetPhysicalDeviceProperties(physical_devices[i], &device_properties);

        bool acc_struct_supported = false, raytracing_supported = false, ray_query_supported = false,
             dynamic_rendering_supported = false, renderpass_loadstore_none_supported = false,
             subgroup_size_control_supported = false, shader_fp16_supported = false, storage_fp16_supported = false,
             shader_int64_supported = false, shader_buf_int64_atomics_supported = false, coop_matrix_supported = false;

        { // check for swapchain support
            uint32_t extension_count;
            vkEnumerateDeviceExtensionProperties(physical_devices[i], nullptr, &extension_count, nullptr);

            SmallVector<VkExtensionProperties, 16> available_extensions(extension_count);
            vkEnumerateDeviceExtensionProperties(physical_devices[i], nullptr, &extension_count,
                                                 &available_extensions[0]);

            bool swapchain_supported = false, anisotropy_supported = false;

            for (uint32_t j = 0; j < extension_count; j++) {
                const VkExtensionProperties &ext = available_extensions[j];

                if (strcmp(ext.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
                    swapchain_supported = true;
                } else if (strcmp(ext.extensionName, VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME) == 0) {
                    acc_struct_supported = true;
                } else if (strcmp(ext.extensionName, VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME) == 0) {
                    raytracing_supported = true;
                } else if (strcmp(ext.extensionName, VK_KHR_RAY_QUERY_EXTENSION_NAME) == 0) {
                    ray_query_supported = true;
                } else if (strcmp(ext.extensionName, VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME) == 0) {
                    // dynamic_rendering_supported = true;
                } else if (strcmp(ext.extensionName, VK_EXT_LOAD_STORE_OP_NONE_EXTENSION_NAME) == 0) {
                    renderpass_loadstore_none_supported = true;
                } else if (strcmp(ext.extensionName, VK_EXT_SUBGROUP_SIZE_CONTROL_EXTENSION_NAME) == 0) {
                    subgroup_size_control_supported = true;
                } else if (strcmp(ext.extensionName, VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME) == 0) {
                    shader_fp16_supported = true;
                } else if (strcmp(ext.extensionName, VK_KHR_16BIT_STORAGE_EXTENSION_NAME) == 0) {
                    storage_fp16_supported = true;
                } else if (strcmp(ext.extensionName, VK_KHR_SHADER_ATOMIC_INT64_EXTENSION_NAME) == 0) {
                    shader_buf_int64_atomics_supported = true;
                } else if (strcmp(ext.extensionName, VK_KHR_COOPERATIVE_MATRIX_EXTENSION_NAME) == 0) {
                    coop_matrix_supported = true;
                }
            }

            VkPhysicalDeviceFeatures supported_features;
            vkGetPhysicalDeviceFeatures(physical_devices[i], &supported_features);

            anisotropy_supported = (supported_features.samplerAnisotropy == VK_TRUE);

            if (!swapchain_supported || !anisotropy_supported) {
                continue;
            }

            VkPhysicalDeviceFeatures2KHR device_features2 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR};

            VkPhysicalDeviceShaderAtomicInt64Features atomic_int64_features = {
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_INT64_FEATURES_KHR};
            if (shader_buf_int64_atomics_supported) {
                device_features2.pNext = &atomic_int64_features;
            }
            vkGetPhysicalDeviceFeatures2KHR(physical_devices[i], &device_features2);

            shader_int64_supported = (device_features2.features.shaderInt64 == VK_TRUE);
            shader_buf_int64_atomics_supported &= (atomic_int64_features.shaderBufferInt64Atomics == VK_TRUE);

            if (shader_fp16_supported) {
                VkPhysicalDeviceShaderFloat16Int8Features fp16_features = {
                    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES};

                VkPhysicalDeviceFeatures2 prop2 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
                prop2.pNext = &fp16_features;

                vkGetPhysicalDeviceFeatures2KHR(physical_devices[i], &prop2);

                shader_fp16_supported &= (fp16_features.shaderFloat16 != 0);
            }

            if (coop_matrix_supported) {
                VkPhysicalDeviceCooperativeMatrixFeaturesKHR coop_matrix_features = {
                    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COOPERATIVE_MATRIX_FEATURES_KHR};

                VkPhysicalDeviceFeatures2 prop2 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
                prop2.pNext = &coop_matrix_features;

                vkGetPhysicalDeviceFeatures2KHR(physical_devices[i], &prop2);

                uint32_t props_count = 0;
                vkGetPhysicalDeviceCooperativeMatrixPropertiesKHR(physical_devices[i], &props_count, nullptr);

                SmallVector<VkCooperativeMatrixPropertiesKHR, 16> coop_matrix_props(
                    props_count, {VK_STRUCTURE_TYPE_COOPERATIVE_MATRIX_PROPERTIES_KHR});

                vkGetPhysicalDeviceCooperativeMatrixPropertiesKHR(physical_devices[i], &props_count,
                                                                  coop_matrix_props.data());

                bool found = false;
                for (const VkCooperativeMatrixPropertiesKHR &p : coop_matrix_props) {
                    if (p.AType == VK_COMPONENT_TYPE_FLOAT16_KHR && p.BType == VK_COMPONENT_TYPE_FLOAT16_KHR &&
                        p.CType == VK_COMPONENT_TYPE_FLOAT16_KHR && p.ResultType == VK_COMPONENT_TYPE_FLOAT16_KHR &&
                        p.MSize == 16 && p.NSize == 8 && p.KSize == 8 && p.scope == VK_SCOPE_SUBGROUP_KHR) {
                        found = true;
                        break;
                    }
                }
                coop_matrix_supported &= found;
            }
        }

        uint32_t queue_family_count;
        vkGetPhysicalDeviceQueueFamilyProperties(physical_devices[i], &queue_family_count, nullptr);

        SmallVector<VkQueueFamilyProperties, 8> queue_family_properties(queue_family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(physical_devices[i], &queue_family_count, &queue_family_properties[0]);

        uint32_t present_family_index = 0xffffffff, graphics_family_index = 0xffffffff;

        for (uint32_t j = 0; j < queue_family_count; j++) {
            VkBool32 supports_present = VK_FALSE;
            if (surface) {
                vkGetPhysicalDeviceSurfaceSupportKHR(physical_devices[i], j, surface, &supports_present);
            }

            if (supports_present && queue_family_properties[j].queueCount > 0 &&
                queue_family_properties[j].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                present_family_index = j;
                graphics_family_index = j;
                break;
            } else if (supports_present && queue_family_properties[j].queueCount > 0 &&
                       present_family_index == 0xffffffff) {
                present_family_index = j;
            } else if (queue_family_properties[j].queueCount > 0 &&
                       (queue_family_properties[j].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
                       graphics_family_index == 0xffffffff) {
                graphics_family_index = j;
            }
        }

        if (graphics_family_index != 0xffffffff) {
            int score = 0;

            if (device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                score += 1000;
            }

            score += int(device_properties.limits.maxImageDimension2D);

            if (present_family_index == graphics_family_index) {
                // prefer same present and graphics queue family for performance
                score += 500;
            }

            if (acc_struct_supported && raytracing_supported) {
                score += 500;
            }

            if (dynamic_rendering_supported) {
                score += 100;
            }

            if (!preferred_device.empty() && MatchDeviceNames(device_properties.deviceName, preferred_device.data())) {
                // preferred device found
                score += 100000;
            }

            if (score > best_score) {
                best_score = score;

                physical_device = physical_devices[i];

                uint32_t _supported_stages_mask = 0xffffffff;
                if (!raytracing_supported) {
                    _supported_stages_mask &= ~VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
                }

                this->device_properties = device_properties;
                this->present_family_index = present_family_index;
                this->graphics_family_index = graphics_family_index;
                this->raytracing_supported = (acc_struct_supported && raytracing_supported);
                this->ray_query_supported = ray_query_supported;
                this->dynamic_rendering_supported = dynamic_rendering_supported;
                this->renderpass_loadstore_none_supported = renderpass_loadstore_none_supported;
                this->subgroup_size_control_supported = subgroup_size_control_supported;
                this->supported_stages_mask = _supported_stages_mask;
                this->fp16_supported = (shader_fp16_supported && storage_fp16_supported);
                this->shader_int64_supported = shader_int64_supported;
                this->shader_buf_int64_atomics_supported = shader_buf_int64_atomics_supported;
                this->coop_matrix_supported = coop_matrix_supported;
            }
        }
    }

    if (!physical_device) {
        log->Error("No physical device detected that can render and present!");
        return false;
    }

    vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_properties);

    return true;
}

bool Ren::ApiContext::InitSwapChain(int w, int h, ILog *log) {
    { // choose surface format
        uint32_t format_count = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count, nullptr);

        SmallVector<VkSurfaceFormatKHR, 32> surface_formats(format_count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count, &surface_formats[0]);

        if (format_count == 1 && surface_formats[0].format == VK_FORMAT_UNDEFINED) {
            surface_format.format = VK_FORMAT_B8G8R8A8_UNORM; // VK_FORMAT_R8G8B8A8_UNORM;
            surface_format.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        } else {
            surface_format = surface_formats[0];

            for (uint32_t i = 0; i < format_count; i++) {
                const VkSurfaceFormatKHR &fmt = surface_formats[i];

                if (fmt.format == VK_FORMAT_B8G8R8A8_UNORM && fmt.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                    surface_format = fmt;
                    break;
                }
            }
        }
    }

    { // choose present mode
        uint32_t present_mode_count = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, nullptr);

        SmallVector<VkPresentModeKHR, 8> present_modes(present_mode_count);
        vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, &present_modes[0]);

        // fifo mode is guaranteed to be supported
        present_mode = VK_PRESENT_MODE_FIFO_KHR;
        for (uint32_t i = 0; i < present_mode_count; i++) {
            if (present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
                present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
                break;
            } else if (present_modes[i] == VK_PRESENT_MODE_FIFO_RELAXED_KHR) {
                // gives less latency than fifo mode, use it
                present_mode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
            }
        }
    }

    VkSurfaceCapabilitiesKHR surface_capabilities = {};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &surface_capabilities);

    { // choose resolution
        VkExtent2D surface_resolution = surface_capabilities.currentExtent;
        if (surface_resolution.width == 0xffffffff) {
            // can use any value, use native window resolution
            res = VkExtent2D{uint32_t(w), uint32_t(h)};
        } else {
            VkExtent2D actual_extent = VkExtent2D{uint32_t(w), uint32_t(h)};

            actual_extent.width = std::min(std::max(actual_extent.width, surface_capabilities.minImageExtent.width),
                                           surface_capabilities.maxImageExtent.width);
            actual_extent.height = std::min(std::max(actual_extent.height, surface_capabilities.minImageExtent.height),
                                            surface_capabilities.maxImageExtent.height);

            res = actual_extent;
        }
    }

    uint32_t desired_image_count = 1;
    if (present_mode == VK_PRESENT_MODE_FIFO_KHR || present_mode == VK_PRESENT_MODE_FIFO_RELAXED_KHR) {
        desired_image_count = 2;
    } else if (present_mode == VK_PRESENT_MODE_MAILBOX_KHR) {
        desired_image_count = 3;
    }

    if (desired_image_count < surface_capabilities.minImageCount) {
        desired_image_count = surface_capabilities.minImageCount;
    } else if (surface_capabilities.maxImageCount != 0 && desired_image_count > surface_capabilities.maxImageCount) {
        desired_image_count = surface_capabilities.maxImageCount;
    }

    VkSurfaceTransformFlagBitsKHR pre_transform = surface_capabilities.currentTransform;
    if (surface_capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) {
        pre_transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    }

    VkSwapchainCreateInfoKHR swap_chain_create_info = {VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    swap_chain_create_info.surface = surface;
    swap_chain_create_info.minImageCount = desired_image_count;
    swap_chain_create_info.imageFormat = surface_format.format;
    swap_chain_create_info.imageColorSpace = surface_format.colorSpace;
    swap_chain_create_info.imageExtent = res;
    swap_chain_create_info.imageArrayLayers = 1;
    swap_chain_create_info.imageUsage =
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT /*| VK_IMAGE_USAGE_STORAGE_BIT*/;

    uint32_t queue_fam_indices[] = {present_family_index, graphics_family_index};

    if (queue_fam_indices[0] != queue_fam_indices[1]) {
        swap_chain_create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swap_chain_create_info.queueFamilyIndexCount = 2;
        swap_chain_create_info.pQueueFamilyIndices = queue_fam_indices;
    } else {
        swap_chain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    swap_chain_create_info.preTransform = pre_transform;
    swap_chain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swap_chain_create_info.presentMode = present_mode;
    swap_chain_create_info.clipped = VK_TRUE;
    swap_chain_create_info.oldSwapchain = VK_NULL_HANDLE;

    VkResult res = vkCreateSwapchainKHR(device, &swap_chain_create_info, nullptr, &swapchain);
    if (res != VK_SUCCESS) {
        log->Error("Failed to create swapchain!");
        return false;
    }

    return true;
}

bool Ren::ApiContext::InitCommandBuffers(uint32_t family_index, ILog *log) {
    VkCommandPoolCreateInfo cmd_pool_create_info = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    cmd_pool_create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    cmd_pool_create_info.queueFamilyIndex = family_index;

    VkResult res = vkCreateCommandPool(device, &cmd_pool_create_info, nullptr, &command_pool);
    if (res != VK_SUCCESS) {
        log->Error("Failed to create command pool!");
        return false;
    }

    { // create pool for temporary commands
        VkCommandPoolCreateInfo tmp_cmd_pool_create_info = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        tmp_cmd_pool_create_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        tmp_cmd_pool_create_info.queueFamilyIndex = family_index;

        res = vkCreateCommandPool(device, &tmp_cmd_pool_create_info, nullptr, &temp_command_pool);
        if (res != VK_SUCCESS) {
            log->Error("Failed to create command pool!");
            return false;
        }
    }

    VkCommandBufferAllocateInfo cmd_buf_alloc_info = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cmd_buf_alloc_info.commandPool = command_pool;
    cmd_buf_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmd_buf_alloc_info.commandBufferCount = 1;

    res = vkAllocateCommandBuffers(device, &cmd_buf_alloc_info, &setup_cmd_buf);
    if (res != VK_SUCCESS) {
        log->Error("Failed to create command buffer!");
        return false;
    }

    cmd_buf_alloc_info.commandBufferCount = MaxFramesInFlight;
    res = vkAllocateCommandBuffers(device, &cmd_buf_alloc_info, draw_cmd_buf);
    if (res != VK_SUCCESS) {
        log->Error("Failed to create command buffer!");
        return false;
    }

    { // create fences
        VkSemaphoreCreateInfo sem_info = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};

        VkFenceCreateInfo fence_info = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (int i = 0; i < MaxFramesInFlight; i++) {
            res = vkCreateSemaphore(device, &sem_info, nullptr, &image_avail_semaphores[i]);
            if (res != VK_SUCCESS) {
                log->Error("Failed to create semaphore!");
                return false;
            }
            res = vkCreateSemaphore(device, &sem_info, nullptr, &render_finished_semaphores[i]);
            if (res != VK_SUCCESS) {
                log->Error("Failed to create semaphore!");
                return false;
            }
            res = vkCreateFence(device, &fence_info, nullptr, &in_flight_fences[i]);
            if (res != VK_SUCCESS) {
                log->Error("Failed to create fence!");
                return false;
            }
        }
    }

    { // create query pools
        VkQueryPoolCreateInfo pool_info = {VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
        pool_info.queryCount = MaxTimestampQueries;
        pool_info.queryType = VK_QUERY_TYPE_TIMESTAMP;

        for (int i = 0; i < MaxFramesInFlight; ++i) {
            res = vkCreateQueryPool(device, &pool_info, nullptr, &query_pools[i]);
            if (res != VK_SUCCESS) {
                log->Error("Failed to create query pool!");
                return false;
            }
        }
    }

    return true;
}

bool Ren::ApiContext::InitPresentImageViews(ILog *log) {
    uint32_t image_count;
    vkGetSwapchainImagesKHR(device, swapchain, &image_count, nullptr);

    present_images.resize(image_count);
    vkGetSwapchainImagesKHR(device, swapchain, &image_count, &present_images[0]);

#ifdef ENABLE_GPU_DEBUG
    for (uint32_t i = 0; i < image_count; ++i) {
        const std::string name = "PresentImage[" + std::to_string(i) + "]";
        VkDebugUtilsObjectNameInfoEXT name_info = {VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
        name_info.objectType = VK_OBJECT_TYPE_IMAGE;
        name_info.objectHandle = uint64_t(present_images[i]);
        name_info.pObjectName = name.c_str();
        vkSetDebugUtilsObjectNameEXT(device, &name_info);
    }
#endif

    VkImageViewCreateInfo present_images_view_create_info = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    present_images_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    present_images_view_create_info.format = surface_format.format;
    present_images_view_create_info.components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                                                  VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
    present_images_view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    present_images_view_create_info.subresourceRange.baseMipLevel = 0;
    present_images_view_create_info.subresourceRange.levelCount = 1;
    present_images_view_create_info.subresourceRange.baseArrayLayer = 0;
    present_images_view_create_info.subresourceRange.layerCount = 1;

    VkCommandBufferBeginInfo begin_info = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    VkFenceCreateInfo fence_create_info = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};

    VkFence submit_fence = {};
    vkCreateFence(device, &fence_create_info, nullptr, &submit_fence);

    present_image_views.resize(image_count);
    for (uint32_t i = 0; i < image_count; i++) {
        present_images_view_create_info.image = present_images[i];

        vkBeginCommandBuffer(setup_cmd_buf, &begin_info);

        VkImageMemoryBarrier layout_transition_barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        layout_transition_barrier.srcAccessMask = 0;
        layout_transition_barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        layout_transition_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        layout_transition_barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        layout_transition_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        layout_transition_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        layout_transition_barrier.image = present_images[i];

        VkImageSubresourceRange resource_range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        layout_transition_barrier.subresourceRange = resource_range;

        vkCmdPipelineBarrier(setup_cmd_buf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0,
                             nullptr, 0, nullptr, 1, &layout_transition_barrier);

        vkEndCommandBuffer(setup_cmd_buf);

        VkPipelineStageFlags wait_stage_mask[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        VkSubmitInfo submit_info = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submit_info.waitSemaphoreCount = 0;
        submit_info.pWaitSemaphores = nullptr;
        submit_info.pWaitDstStageMask = wait_stage_mask;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &setup_cmd_buf;
        submit_info.signalSemaphoreCount = 0;
        submit_info.pSignalSemaphores = nullptr;

        VkResult res = vkQueueSubmit(present_queue, 1, &submit_info, submit_fence);
        if (res != VK_SUCCESS) {
            log->Error("vkQueueSubmit failed");
            return false;
        }

        vkWaitForFences(device, 1, &submit_fence, VK_TRUE, UINT64_MAX);
        vkResetFences(device, 1, &submit_fence);

        vkResetCommandBuffer(setup_cmd_buf, 0);

        res = vkCreateImageView(device, &present_images_view_create_info, nullptr, &present_image_views[i]);
        if (res != VK_SUCCESS) {
            log->Error("vkCreateImageView failed");
            return false;
        }
    }

    vkDestroyFence(device, submit_fence, nullptr);

    return true;
}

VkCommandBuffer Ren::ApiContext::BegSingleTimeCommands() {
    VkCommandBufferAllocateInfo alloc_info = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandPool = temp_command_pool;
    alloc_info.commandBufferCount = 1;

    VkCommandBuffer command_buf = {};
    VkResult res = vkAllocateCommandBuffers(device, &alloc_info, &command_buf);
    assert(res == VK_SUCCESS);

    VkCommandBufferBeginInfo begin_info = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    res = vkBeginCommandBuffer(command_buf, &begin_info);
    assert(res == VK_SUCCESS);

    return command_buf;
}

void Ren::ApiContext::EndSingleTimeCommands(VkCommandBuffer command_buf) {
    VkResult res = vkEndCommandBuffer(command_buf);
    assert(res == VK_SUCCESS);

    VkSubmitInfo submit_info = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buf;

    res = vkQueueSubmit(graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
    assert(res == VK_SUCCESS);
    res = vkQueueWaitIdle(graphics_queue);
    assert(res == VK_SUCCESS);

    vkFreeCommandBuffers(device, temp_command_pool, 1, &command_buf);
}

void Ren::ApiContext::EndSingleTimeCommands(VkCommandBuffer command_buf, VkFence fence_to_insert) {
    vkEndCommandBuffer(command_buf);

    VkSubmitInfo submit_info = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buf;

    vkQueueSubmit(graphics_queue, 1, &submit_info, fence_to_insert);
}

Ren::ApiContext::~ApiContext() {
    if (vulkan_module) {
#if defined(_WIN32)
        FreeLibrary((HMODULE)vulkan_module);
#else
        dlclose(vulkan_module);
#endif
        vulkan_module = {};
    }
}

bool Ren::MatchDeviceNames(const char *name, const char *pattern) {
    std::regex match_name(pattern);
    return std::regex_search(name, match_name) || strcmp(name, pattern) == 0;
}

bool Ren::ReadbackTimestampQueries(ApiContext *api_ctx, int i) {
    VkQueryPool query_pool = api_ctx->query_pools[i];
    const uint32_t query_count = uint32_t(api_ctx->query_counts[i]);
    if (!query_count) {
        // nothing to readback
        return true;
    }

    const VkResult res = api_ctx->vkGetQueryPoolResults(
        api_ctx->device, query_pool, 0, query_count, query_count * sizeof(uint64_t), api_ctx->query_results[i],
        sizeof(uint64_t), VK_QUERY_RESULT_WAIT_BIT | VK_QUERY_RESULT_64_BIT);
    api_ctx->query_counts[api_ctx->backend_frame] = 0;

    return (res == VK_SUCCESS);
}

void Ren::DestroyDeferredResources(ApiContext *api_ctx, int i) {
    for (VkImageView view : api_ctx->image_views_to_destroy[i]) {
        api_ctx->vkDestroyImageView(api_ctx->device, view, nullptr);
    }
    api_ctx->image_views_to_destroy[i].clear();
    for (VkImage img : api_ctx->images_to_destroy[i]) {
        api_ctx->vkDestroyImage(api_ctx->device, img, nullptr);
    }
    api_ctx->images_to_destroy[i].clear();
    for (VkSampler sampler : api_ctx->samplers_to_destroy[i]) {
        api_ctx->vkDestroySampler(api_ctx->device, sampler, nullptr);
    }
    api_ctx->samplers_to_destroy[i].clear();

    api_ctx->allocations_to_free[i].clear();
    api_ctx->allocators_to_release[i].clear();

    for (VkBufferView view : api_ctx->buf_views_to_destroy[i]) {
        api_ctx->vkDestroyBufferView(api_ctx->device, view, nullptr);
    }
    api_ctx->buf_views_to_destroy[i].clear();
    for (VkBuffer buf : api_ctx->bufs_to_destroy[i]) {
        api_ctx->vkDestroyBuffer(api_ctx->device, buf, nullptr);
    }
    api_ctx->bufs_to_destroy[i].clear();

    for (VkDeviceMemory mem : api_ctx->mem_to_free[i]) {
        api_ctx->vkFreeMemory(api_ctx->device, mem, nullptr);
    }
    api_ctx->mem_to_free[i].clear();

    for (VkRenderPass rp : api_ctx->render_passes_to_destroy[i]) {
        api_ctx->vkDestroyRenderPass(api_ctx->device, rp, nullptr);
    }
    api_ctx->render_passes_to_destroy[i].clear();

    for (VkFramebuffer fb : api_ctx->framebuffers_to_destroy[i]) {
        api_ctx->vkDestroyFramebuffer(api_ctx->device, fb, nullptr);
    }
    api_ctx->framebuffers_to_destroy[i].clear();

    for (VkDescriptorPool pool : api_ctx->descriptor_pools_to_destroy[i]) {
        api_ctx->vkDestroyDescriptorPool(api_ctx->device, pool, nullptr);
    }
    api_ctx->descriptor_pools_to_destroy[i].clear();

    for (VkPipelineLayout pipe_layout : api_ctx->pipeline_layouts_to_destroy[i]) {
        api_ctx->vkDestroyPipelineLayout(api_ctx->device, pipe_layout, nullptr);
    }
    api_ctx->pipeline_layouts_to_destroy[i].clear();

    for (VkPipeline pipe : api_ctx->pipelines_to_destroy[i]) {
        api_ctx->vkDestroyPipeline(api_ctx->device, pipe, nullptr);
    }
    api_ctx->pipelines_to_destroy[i].clear();

    for (VkAccelerationStructureKHR acc_struct : api_ctx->acc_structs_to_destroy[i]) {
        api_ctx->vkDestroyAccelerationStructureKHR(api_ctx->device, acc_struct, nullptr);
    }
    api_ctx->acc_structs_to_destroy[i].clear();
}

void Ren::_SubmitCurrentCommandsWaitForCompletionAndResume(ApiContext *api_ctx) {
    // Finish command buffer
    api_ctx->vkEndCommandBuffer(api_ctx->draw_cmd_buf[api_ctx->backend_frame]);

    { // Submit commands
        VkSubmitInfo submit_info = {VK_STRUCTURE_TYPE_SUBMIT_INFO};

        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &api_ctx->draw_cmd_buf[api_ctx->backend_frame];

        VkResult res = api_ctx->vkQueueSubmit(api_ctx->graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
        assert(res == VK_SUCCESS);
    }

    // Wait for completion
    api_ctx->vkDeviceWaitIdle(api_ctx->device);

    // Restart command buffer
    VkCommandBufferBeginInfo begin_info = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    api_ctx->vkBeginCommandBuffer(api_ctx->draw_cmd_buf[api_ctx->backend_frame], &begin_info);
}
