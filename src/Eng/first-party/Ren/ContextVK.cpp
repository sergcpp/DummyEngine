#include "Context.h"

#include "DescriptorPool.h"
#include "VKCtx.h"

#if defined(VK_USE_PLATFORM_WIN32_KHR)
#include <Windows.h>
#endif

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)
#endif

namespace Ren {
bool ignore_optick_errors = false;
#if defined(__linux__)
extern Display *g_dpy;
#endif

VKAPI_ATTR VkBool32 VKAPI_ATTR DebugReportCallback(const VkDebugReportFlagsEXT flags,
                                                   const VkDebugReportObjectTypeEXT objectType, const uint64_t object,
                                                   const size_t location, const int32_t messageCode,
                                                   const char *pLayerPrefix, const char *pMessage, void *pUserData) {
    auto *ctx = reinterpret_cast<const Context *>(pUserData);

    bool ignore = ignore_optick_errors && (location == 0x45e90123 || location == 0xffffffff9cacd67a);
    ignore |= (location == 0x0000000079de34d4); // dynamic rendering support is incomplete
    if (!ignore) {
        ctx->log()->Error("%s: %s\n", pLayerPrefix, pMessage);
    }
    return VK_FALSE;
}

const std::pair<uint32_t, const char *> KnownVendors[] = {
    {0x1002, "AMD"}, {0x10DE, "NVIDIA"}, {0x8086, "INTEL"}, {0x13B5, "ARM"}};
} // namespace Ren

Ren::Context::Context() = default;

Ren::Context::~Context() {
    api_ctx_->present_image_refs.clear();
    ReleaseAll();

    if (api_ctx_ && api_ctx_->device) {
        vkDeviceWaitIdle(api_ctx_->device);

        for (int i = 0; i < MaxFramesInFlight; ++i) {
            api_ctx_->backend_frame = i; // default_descr_alloc_'s destructors rely on this

            default_descr_alloc_[i].reset();
            DestroyDeferredResources(api_ctx_.get(), i);

            vkDestroyFence(api_ctx_->device, api_ctx_->in_flight_fences[i], nullptr);
            vkDestroySemaphore(api_ctx_->device, api_ctx_->render_finished_semaphores[i], nullptr);
            vkDestroySemaphore(api_ctx_->device, api_ctx_->image_avail_semaphores[i], nullptr);

            vkDestroyQueryPool(api_ctx_->device, api_ctx_->query_pools[i], nullptr);
        }

        default_memory_allocs_.reset();

        vkFreeCommandBuffers(api_ctx_->device, api_ctx_->command_pool, 1, &api_ctx_->setup_cmd_buf);
        vkFreeCommandBuffers(api_ctx_->device, api_ctx_->command_pool, MaxFramesInFlight, &api_ctx_->draw_cmd_buf[0]);

        for (int i = 0; i < StageBufferCount; ++i) {
            default_stage_bufs_.fences[i].ClientWaitSync();
            default_stage_bufs_.fences[i] = {};
            default_stage_bufs_.bufs[i] = {};
        }

        vkDestroyCommandPool(api_ctx_->device, api_ctx_->command_pool, nullptr);
        vkDestroyCommandPool(api_ctx_->device, api_ctx_->temp_command_pool, nullptr);

        for (size_t i = 0; i < api_ctx_->present_image_views.size(); ++i) {
            vkDestroyImageView(api_ctx_->device, api_ctx_->present_image_views[i], nullptr);
            // vkDestroyImage(api_ctx_->device, api_ctx_->present_images[i], nullptr);
        }

        vkDestroySwapchainKHR(api_ctx_->device, api_ctx_->swapchain, nullptr);

        vkDestroyDevice(api_ctx_->device, nullptr);
        vkDestroySurfaceKHR(api_ctx_->instance, api_ctx_->surface, nullptr);
        if (api_ctx_->debug_callback) {
            vkDestroyDebugReportCallbackEXT(api_ctx_->instance, api_ctx_->debug_callback, nullptr);
        }

#if defined(VK_USE_PLATFORM_XLIB_KHR)
        if (g_dpy) {
            XCloseDisplay(g_dpy); // has to be done before instance destruction
                                  // (https://github.com/KhronosGroup/Vulkan-LoaderAndValidationLayers/issues/1894)
        }
#endif
        vkDestroyInstance(api_ctx_->instance, nullptr);
    }
}

Ren::DescrMultiPoolAlloc *Ren::Context::default_descr_alloc() const {
    return default_descr_alloc_[api_ctx_->backend_frame].get();
}

bool Ren::Context::Init(const int w, const int h, ILog *log, const int validation_level, const char *preferred_device) {
    if (!LoadVulkan(log)) {
        return false;
    }

    w_ = w;
    h_ = h;
    log_ = log;

    api_ctx_.reset(new ApiContext);

    const char *enabled_layers[] = {"VK_LAYER_KHRONOS_validation", "VK_LAYER_KHRONOS_synchronization2"};
    const int enabled_layers_count = validation_level ? COUNT_OF(enabled_layers) : 0;

    if (!InitVkInstance(api_ctx_->instance, enabled_layers, enabled_layers_count, validation_level, log)) {
        return false;
    }

    if (validation_level) { // Sebug debug report callback
        VkDebugReportCallbackCreateInfoEXT callback_create_info = {VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT};
        callback_create_info.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT |
                                     VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
        callback_create_info.pfnCallback = DebugReportCallback;
        callback_create_info.pUserData = this;

        const VkResult res = vkCreateDebugReportCallbackEXT(api_ctx_->instance, &callback_create_info, nullptr,
                                                            &api_ctx_->debug_callback);
        if (res != VK_SUCCESS) {
            log->Error("Failed to create debug report callback");
            return false;
        }
    }

    // Create platform-specific surface
    if (!InitVkSurface(api_ctx_->surface, api_ctx_->instance, log)) {
        return false;
    }

    if (!ChooseVkPhysicalDevice(api_ctx_->physical_device, api_ctx_->device_properties, api_ctx_->mem_properties,
                                api_ctx_->present_family_index, api_ctx_->graphics_family_index,
                                api_ctx_->raytracing_supported, api_ctx_->ray_query_supported,
                                api_ctx_->dynamic_rendering_supported, preferred_device, api_ctx_->instance,
                                api_ctx_->surface, log)) {
        return false;
    }

    if (!InitVkDevice(api_ctx_->device, api_ctx_->physical_device, api_ctx_->present_family_index,
                      api_ctx_->graphics_family_index, api_ctx_->raytracing_supported, api_ctx_->ray_query_supported,
                      api_ctx_->dynamic_rendering_supported, enabled_layers, enabled_layers_count, log)) {
        return false;
    }

    // Workaround for a buggy linux AMD driver, make sure vkGetBufferDeviceAddressKHR is not NULL
    auto dev_vkGetBufferDeviceAddressKHR =
        (PFN_vkGetBufferDeviceAddressKHR)vkGetDeviceProcAddr(api_ctx_->device, "vkGetBufferDeviceAddressKHR");
    if (!dev_vkGetBufferDeviceAddressKHR) {
        api_ctx_->raytracing_supported = api_ctx_->ray_query_supported = false;
    }

    if (api_ctx_->present_family_index != 0xffffffff &&
        !InitSwapChain(api_ctx_->swapchain, api_ctx_->surface_format, api_ctx_->res, api_ctx_->present_mode, w, h,
                       api_ctx_->device, api_ctx_->physical_device, api_ctx_->present_family_index,
                       api_ctx_->graphics_family_index, api_ctx_->surface, log)) {
        return false;
    }

    const uint32_t family_index =
        api_ctx_->present_family_index != 0xffffffff ? api_ctx_->present_family_index : api_ctx_->graphics_family_index;
    if (!InitCommandBuffers(api_ctx_->command_pool, api_ctx_->temp_command_pool, api_ctx_->setup_cmd_buf,
                            api_ctx_->draw_cmd_buf, api_ctx_->image_avail_semaphores,
                            api_ctx_->render_finished_semaphores, api_ctx_->in_flight_fences, api_ctx_->query_pools,
                            api_ctx_->device, family_index, log)) {
        return false;
    }

    if (api_ctx_->present_family_index != 0xffffffff) {
        vkGetDeviceQueue(api_ctx_->device, api_ctx_->present_family_index, 0, &api_ctx_->present_queue);
    }
    vkGetDeviceQueue(api_ctx_->device, api_ctx_->graphics_family_index, 0, &api_ctx_->graphics_queue);

    if (api_ctx_->present_family_index != 0xffffffff &&
        !InitPresentImageViews(api_ctx_->present_images, api_ctx_->present_image_views, api_ctx_->device,
                               api_ctx_->swapchain, api_ctx_->surface_format, api_ctx_->setup_cmd_buf,
                               api_ctx_->present_queue, log)) {
        return false;
    }

    RegisterAsMainThread();

    log_->Info("===========================================");
    log_->Info("Device info:");

    log_->Info("\tVulkan version\t: %i.%i", VK_API_VERSION_MAJOR(api_ctx_->device_properties.apiVersion),
               VK_API_VERSION_MINOR(api_ctx_->device_properties.apiVersion));

    auto it = std::find_if(
        std::begin(KnownVendors), std::end(KnownVendors),
        [this](const std::pair<uint32_t, const char *> v) { return api_ctx_->device_properties.vendorID == v.first; });
    if (it != std::end(KnownVendors)) {
        log_->Info("\tVendor\t\t: %s", it->second);
    }
    log_->Info("\tName\t\t: %s", api_ctx_->device_properties.deviceName);

    log_->Info("===========================================");

    capabilities.raytracing = api_ctx_->raytracing_supported;
    capabilities.ray_query = api_ctx_->ray_query_supported;
    capabilities.dynamic_rendering = api_ctx_->dynamic_rendering_supported;
    CheckDeviceCapabilities();

    default_memory_allocs_.reset(new MemoryAllocators(
        "Default Allocs", api_ctx_.get(), 32 * 1024 * 1024 /* initial_block_size */, 1.5f /* growth_factor */));

    InitDefaultBuffers();

    texture_atlas_ = TextureAtlasArray{api_ctx_.get(),     TextureAtlasWidth,       TextureAtlasHeight,
                                       TextureAtlasLayers, eTexFormat::RawRGBA8888, eTexFilter::BilinearNoMipmap};

    for (size_t i = 0; i < api_ctx_->present_images.size(); ++i) {
        char name_buf[24];
        sprintf(name_buf, "Present Image [%i]", int(i));

        Tex2DParams params;
        params.w = w;
        params.h = h;
        if (api_ctx_->surface_format.format == VK_FORMAT_R8G8B8A8_UNORM) {
            params.format = eTexFormat::RawRGBA8888;
        } else if (api_ctx_->surface_format.format == VK_FORMAT_R8G8B8A8_SRGB) {
            params.format = eTexFormat::RawRGBA8888;
            params.flags |= eTexFlagBits::SRGB;
        } else if (api_ctx_->surface_format.format == VK_FORMAT_B8G8R8A8_UNORM) {
            params.format = eTexFormat::RawBGRA8888;
        } else if (api_ctx_->surface_format.format == VK_FORMAT_B8G8R8A8_SRGB) {
            params.format = eTexFormat::RawBGRA8888;
            params.flags |= eTexFlagBits::SRGB;
        }
        params.usage = eTexUsageBits::RenderTarget;
        params.flags |= eTexFlagBits::NoOwnership;

        api_ctx_->present_image_refs.emplace_back(textures_.Add(name_buf, api_ctx_.get(), api_ctx_->present_images[i],
                                                                api_ctx_->present_image_views[i], VkSampler{}, params,
                                                                log_));
    }

    for (int i = 0; i < MaxFramesInFlight; ++i) {
        const int PoolStep = 4;
        const int MaxImgSamplerCount = 32;
        const int MaxStoreImgCount = 6;
        const int MaxUbufCount = 8;
        const int MaxSbufCount = 16;
        const int MaxTbufCount = 16;
        const int MaxAccCount = 1;
        const int InitialSetsCount = 16;

        default_descr_alloc_[i].reset(new DescrMultiPoolAlloc(api_ctx_.get(), PoolStep, MaxImgSamplerCount,
                                                              MaxStoreImgCount, MaxUbufCount, MaxSbufCount,
                                                              MaxTbufCount, MaxAccCount, InitialSetsCount));
    }

    VkPhysicalDeviceProperties device_properties = {};
    vkGetPhysicalDeviceProperties(api_ctx_->physical_device, &device_properties);

    api_ctx_->phys_device_limits = device_properties.limits;
    api_ctx_->max_combined_image_samplers =
        std::min(std::min(device_properties.limits.maxPerStageDescriptorSampledImages,
                          device_properties.limits.maxPerStageDescriptorSamplers) -
                     10,
                 16384u);
    capabilities.max_combined_image_samplers = api_ctx_->max_combined_image_samplers;

    // SWRT is temporarily works with bindless textures only
    capabilities.swrt = (api_ctx_->max_combined_image_samplers >= 16384u);

    return true;
}

void Ren::Context::Resize(const int w, const int h) {
    w_ = w;
    h_ = h;

    vkDeviceWaitIdle(api_ctx_->device);
    api_ctx_->present_image_refs.clear();

    for (size_t i = 0; i < api_ctx_->present_image_views.size(); ++i) {
        vkDestroyImageView(api_ctx_->device, api_ctx_->present_image_views[i], nullptr);
        // vkDestroyImage(api_ctx_->device, api_ctx_->present_images[i], nullptr);
    }

    vkDestroySwapchainKHR(api_ctx_->device, api_ctx_->swapchain, nullptr);

    if (!InitSwapChain(api_ctx_->swapchain, api_ctx_->surface_format, api_ctx_->res, api_ctx_->present_mode, w, h,
                       api_ctx_->device, api_ctx_->physical_device, api_ctx_->present_family_index,
                       api_ctx_->graphics_family_index, api_ctx_->surface, log_)) {
        log_->Error("Swapchain initialization failed");
    }

    if (!InitPresentImageViews(api_ctx_->present_images, api_ctx_->present_image_views, api_ctx_->device,
                               api_ctx_->swapchain, api_ctx_->surface_format, api_ctx_->setup_cmd_buf,
                               api_ctx_->present_queue, log_)) {
        log_->Error("Image views initialization failed");
    }

    for (size_t i = 0; i < api_ctx_->present_images.size(); ++i) {
        char name_buf[24];
        sprintf(name_buf, "Present Image [%i]", int(i));

        Tex2DParams params;
        params.w = w;
        params.h = h;
        if (api_ctx_->surface_format.format == VK_FORMAT_R8G8B8A8_UNORM) {
            params.format = eTexFormat::RawRGBA8888;
        } else if (api_ctx_->surface_format.format == VK_FORMAT_B8G8R8A8_UNORM) {
            params.format = eTexFormat::RawBGRA8888;
        } else if (api_ctx_->surface_format.format == VK_FORMAT_B8G8R8A8_SRGB) {
            params.format = eTexFormat::RawBGRA8888;
            params.flags |= eTexFlagBits::SRGB;
        }
        params.usage = eTexUsageBits::RenderTarget;
        params.flags |= eTexFlagBits::NoOwnership;

        Tex2DRef ref = textures_.FindByName(name_buf);
        if (ref) {
            ref->Init(api_ctx_->present_images[i], api_ctx_->present_image_views[i], VkSampler{}, params, log_);
            api_ctx_->present_image_refs.emplace_back(std::move(ref));
        } else {
            api_ctx_->present_image_refs.emplace_back(
                textures_.Add(name_buf, api_ctx_.get(), api_ctx_->present_images[i], api_ctx_->present_image_views[i],
                              VkSampler{}, params, log_));
        }
    }
}

void Ren::Context::CheckDeviceCapabilities() {
    VkImageFormatProperties props;
    const VkResult res = vkGetPhysicalDeviceImageFormatProperties(
        api_ctx_->physical_device, VK_FORMAT_D24_UNORM_S8_UINT, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, 0, &props);

    capabilities.depth24_stencil8_format = (res == VK_SUCCESS);

    if (capabilities.raytracing) {
        VkPhysicalDeviceProperties2 prop2 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
        prop2.pNext = &api_ctx_->rt_props;

        vkGetPhysicalDeviceProperties2KHR(api_ctx_->physical_device, &prop2);
    }
}

void Ren::Context::BegSingleTimeCommands(void *_cmd_buf) {
    VkCommandBuffer cmd_buf = reinterpret_cast<VkCommandBuffer>(_cmd_buf);

    VkCommandBufferBeginInfo begin_info = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    VkResult res = vkBeginCommandBuffer(cmd_buf, &begin_info);
    assert(res == VK_SUCCESS);
}

void *Ren::Context::BegTempSingleTimeCommands() {
    return Ren::BegSingleTimeCommands(api_ctx_->device, api_ctx_->temp_command_pool);
}

Ren::SyncFence Ren::Context::EndSingleTimeCommands(void *cmd_buf) {
    VkFenceCreateInfo fence_info = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    VkFence new_fence;
    const VkResult res = vkCreateFence(api_ctx_->device, &fence_info, nullptr, &new_fence);
    if (res != VK_SUCCESS) {
        log_->Error("Failed to create fence!");
        return {};
    }

    Ren::EndSingleTimeCommands(api_ctx_->device, api_ctx_->graphics_queue, reinterpret_cast<VkCommandBuffer>(cmd_buf),
                               api_ctx_->temp_command_pool, new_fence);

    return SyncFence{api_ctx_->device, new_fence};
}

void Ren::Context::EndTempSingleTimeCommands(void *cmd_buf) {
    Ren::EndSingleTimeCommands(api_ctx_->device, api_ctx_->graphics_queue, reinterpret_cast<VkCommandBuffer>(cmd_buf),
                               api_ctx_->temp_command_pool);
}

void *Ren::Context::current_cmd_buf() { return api_ctx_->draw_cmd_buf[api_ctx_->backend_frame]; }

int Ren::Context::WriteTimestamp(const bool start) {
    VkCommandBuffer cmd_buf = api_ctx_->draw_cmd_buf[api_ctx_->backend_frame];

    vkCmdWriteTimestamp(cmd_buf, start ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT : VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                        api_ctx_->query_pools[api_ctx_->backend_frame],
                        api_ctx_->query_counts[api_ctx_->backend_frame]);

    const uint32_t query_index = api_ctx_->query_counts[api_ctx_->backend_frame]++;
    assert(api_ctx_->query_counts[api_ctx_->backend_frame] < Ren::MaxTimestampQueries);
    return int(query_index);
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif
