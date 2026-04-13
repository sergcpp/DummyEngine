#include "../Context.h"

#include <mutex>

#include "../DescriptorPool.h"
#include "../ResizableBuffer.h"
#include "VKCtx.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)
#endif

namespace Ren {
bool ignore_optick_errors = false;
std::mutex g_device_mtx;
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
    ignore |= (location == 0x00000000804d79d3); // requiredSubgroupSizeStages is ignored
    ignore |= (location == 0x00000000a5625282); // cooperative matrix type must be A Type
    ignore |= (location == 0x0000000024b5c69f); // forcing vertexPipelineStoresAndAtomics to VK_TRUE
    if (!ignore) {
        ctx->log()->Error("%s: %s\n", pLayerPrefix, pMessage);
    }
    return VK_FALSE;
}

const std::pair<uint32_t, const char *> KnownVendors[] = {
    {0x1002, "AMD"}, {0x10DE, "NVIDIA"}, {0x8086, "INTEL"}, {0x13B5, "ARM"}};

static const uint32_t PipelineCacheVersion = 1;

struct pipeline_cache_header_t {
    uint32_t version;
    uint32_t data_size;
    uint32_t data_hash;
    uint32_t vendor_id;
    uint32_t device_id;
    uint32_t driver_version;
    uint32_t driver_abi;
    uint8_t uuid[VK_UUID_SIZE];
};

uint32_t adler32(uint32_t h, Span<const uint8_t> buffer) {
    uint32_t s1 = h & 0xffff;
    uint32_t s2 = (h >> 16) & 0xffff;
    for (ptrdiff_t n = 0; n < buffer.size(); n++) {
        s1 = (s1 + buffer[n]) % 65521;
        s2 = (s2 + s1) % 65521;
    }
    return (s2 << 16) | s1;
}

uint32_t adler32(Span<const uint8_t> buffer) { return adler32(1, buffer); }

} // namespace Ren

Ren::Context::Context() {
    for (int i = 0; i < Ren::MaxFramesInFlight; ++i) {
        in_flight_frontend_frame[i] = -1;
    }
}

Ren::Context::~Context() {
    std::lock_guard<std::mutex> _(g_device_mtx);

    for (const ImageHandle img : api_->present_image_handles) {
        images_.Erase(img);
    }
    api_->present_image_handles.clear();
    ReleaseAll();

    if (api_ && api_->device) {
        api_->vkDeviceWaitIdle(api_->device);

        for (int i = 0; i < MaxFramesInFlight; ++i) {
            api_->backend_frame = (api_->backend_frame + 1) % Ren::MaxFramesInFlight;

            default_descr_alloc_[api_->backend_frame] = {};
            DestroyDeferredResources(*api_, api_->backend_frame);

            api_->vkDestroyFence(api_->device, api_->in_flight_fences[api_->backend_frame], nullptr);
            api_->vkDestroySemaphore(api_->device, api_->render_finished_semaphores[api_->backend_frame], nullptr);
            api_->vkDestroySemaphore(api_->device, api_->image_avail_semaphores[api_->backend_frame], nullptr);

            api_->vkDestroyQueryPool(api_->device, api_->query_pools[api_->backend_frame], nullptr);
        }

        default_mem_allocs_ = {};

        api_->vkDestroyPipelineCache(api_->device, api_->pipeline_cache, nullptr);

        api_->vkFreeCommandBuffers(api_->device, api_->command_pool, MaxFramesInFlight, &api_->draw_cmd_buf[0]);

        api_->vkDestroyCommandPool(api_->device, api_->command_pool, nullptr);
        api_->vkDestroyCommandPool(api_->device, api_->temp_command_pool, nullptr);

        for (size_t i = 0; i < api_->present_image_views.size(); ++i) {
            api_->vkDestroyImageView(api_->device, api_->present_image_views[i], nullptr);
            // api_->vkDestroyImage(api_->device, api_->present_images[i], nullptr);
        }
        if (api_->swapchain) {
            api_->vkDestroySwapchainKHR(api_->device, api_->swapchain, nullptr);
        }
        api_->vkDestroyDevice(api_->device, nullptr);
        if (api_->surface) {
            api_->vkDestroySurfaceKHR(api_->instance, api_->surface, nullptr);
        }
        if (api_->debug_callback) {
            api_->vkDestroyDebugReportCallbackEXT(api_->instance, api_->debug_callback, nullptr);
        }

#if defined(VK_USE_PLATFORM_XLIB_KHR)
        if (g_dpy) {
            XCloseDisplay(g_dpy); // has to be done before instance destruction
                                  // (https://github.com/KhronosGroup/Vulkan-LoaderAndValidationLayers/issues/1894)
        }
#endif
        api_->vkDestroyInstance(api_->instance, nullptr);
    }
}

bool Ren::Context::Init(const int w, const int h, ILog *log, int validation_level, const bool novsync,
                        const bool nohwrt, const bool nosubgroup, std::string_view preferred_device) {
    api_ = std::make_unique<ApiContext>();
    if (!api_->Load(log)) {
        return false;
    }

    w_ = w;
    h_ = h;
    novsync_ = novsync; // stored for swapchain reinitialization
    log_ = log;

    std::lock_guard<std::mutex> _(g_device_mtx);

    const char *enabled_layers[] = {"VK_LAYER_KHRONOS_validation", "VK_LAYER_KHRONOS_synchronization2"};
    const int enabled_layers_count = validation_level ? int(std::size(enabled_layers)) : 0;

    if (!api_->InitVkInstance(enabled_layers, enabled_layers_count, validation_level, log)) {
        return false;
    }
    if (!api_->LoadInstanceFunctions(log)) {
        return false;
    }

    validation_level_ = validation_level;

    if (validation_level) { // Sebug debug report callback
        VkDebugReportCallbackCreateInfoEXT callback_create_info = {VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT};
        callback_create_info.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT |
                                     VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
        callback_create_info.pfnCallback = DebugReportCallback;
        callback_create_info.pUserData = this;

        const VkResult res =
            api_->vkCreateDebugReportCallbackEXT(api_->instance, &callback_create_info, nullptr, &api_->debug_callback);
        if (res != VK_SUCCESS) {
            log->Warning("Failed to create debug report callback");
        }
    }

    // Create platform-specific surface
    if (!api_->InitVkSurface(log)) {
        return false;
    }

    if (!api_->ChooseVkPhysicalDevice(preferred_device, log)) {
        return false;
    }

    if (!api_->InitVkDevice(enabled_layers, enabled_layers_count, validation_level, log)) {
        return false;
    }

    // Workaround for a buggy linux AMD driver, make sure vkGetBufferDeviceAddressKHR is not NULL
    auto dev_vkGetBufferDeviceAddressKHR =
        (PFN_vkGetBufferDeviceAddressKHR)api_->vkGetDeviceProcAddr(api_->device, "vkGetBufferDeviceAddressKHR");
    if (!dev_vkGetBufferDeviceAddressKHR || nohwrt) {
        api_->raytracing_supported = api_->ray_query_supported = false;
    }

    if (api_->subgroup_size_control_supported) {
        VkPhysicalDeviceSubgroupSizeControlFeaturesEXT subgroup_size_control_features = {
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_FEATURES_EXT};

        VkPhysicalDeviceFeatures2 feat2 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
        feat2.pNext = &subgroup_size_control_features;

        api_->vkGetPhysicalDeviceFeatures2KHR(api_->physical_device, &feat2);

        api_->subgroup_size_control_supported &= (subgroup_size_control_features.subgroupSizeControl == VK_TRUE);
        api_->subgroup_size_control_supported &= (subgroup_size_control_features.computeFullSubgroups == VK_TRUE);
    }

    if (api_->present_family_index != 0xffffffff && !api_->InitSwapChain(w, h, novsync, log)) {
        return false;
    }

    const uint32_t family_index =
        api_->present_family_index != 0xffffffff ? api_->present_family_index : api_->graphics_family_index;
    if (!api_->InitCommandBuffers(family_index, log)) {
        return false;
    }

    if (api_->present_family_index != 0xffffffff) {
        api_->vkGetDeviceQueue(api_->device, api_->present_family_index, 0, &api_->present_queue);
    }
    api_->vkGetDeviceQueue(api_->device, api_->graphics_family_index, 0, &api_->graphics_queue);

    if (api_->present_family_index != 0xffffffff && !api_->InitPresentImageViews(log)) {
        return false;
    }

    log_->Info("============================================================================");
    log_->Info("Device info:");

    log_->Info("\tVulkan version\t: %u.%u", VK_API_VERSION_MAJOR(api_->device_properties.apiVersion),
               VK_API_VERSION_MINOR(api_->device_properties.apiVersion));

    auto it = std::find_if(
        std::begin(KnownVendors), std::end(KnownVendors),
        [this](const std::pair<uint32_t, const char *> v) { return api_->device_properties.vendorID == v.first; });
    if (it != std::end(KnownVendors)) {
        log_->Info("\tVendor\t\t: %s", it->second);
    }
    log_->Info("\tName\t\t: %s", api_->device_properties.deviceName);
    log_->Info("============================================================================");

    capabilities.hwrt = (api_->raytracing_supported && api_->ray_query_supported);

    CheckDeviceCapabilities();

    capabilities.subgroup &= !nosubgroup;

    default_mem_allocs_ =
        std::make_unique<MemAllocators>("Default Allocs", api_.get(), 1 * 1024 * 1024 /* initial_block_size */,
                                        1.5f /* growth_factor */, 128 * 1024 * 1024 /* max_pool_size */);

    InitDefaultBuffers();

    image_atlas_ =
        ImageAtlasArray{api_.get(),       "Image Atlas",     ImageAtlasWidth,
                        ImageAtlasHeight, ImageAtlasLayers,  1,
                        eFormat::RGBA8,   eFilter::Bilinear, Bitmask(eImgUsage::Transfer) | eImgUsage::Sampled};

    for (size_t i = 0; i < api_->present_images.size(); ++i) {
        char name_buf[24];
        snprintf(name_buf, sizeof(name_buf), "Present Image [%i]", int(i));

        ImgParams params;
        params.w = w;
        params.h = h;
        if (api_->surface_format.format == VK_FORMAT_R8G8B8A8_UNORM) {
            params.format = eFormat::RGBA8;
        } else if (api_->surface_format.format == VK_FORMAT_R8G8B8A8_SRGB) {
            params.format = eFormat::RGBA8_srgb;
        } else if (api_->surface_format.format == VK_FORMAT_B8G8R8A8_UNORM) {
            params.format = eFormat::BGRA8;
        } else if (api_->surface_format.format == VK_FORMAT_B8G8R8A8_SRGB) {
            params.format = eFormat::BGRA8_srgb;
        }
        params.usage = Bitmask(eImgUsage::RenderTarget);
        params.flags |= eImgFlags::NoOwnership;

        ImageHandle new_img = images_.Emplace();
        const auto &[img_main, img_cold] = images_[new_img];

        img_main.img = api_->present_images[i];
        img_main.views.push_back(api_->present_image_views[i]);

        img_cold.name = Ren::String{name_buf};
        img_cold.params = params;

        api_->present_image_handles.push_back(new_img);
    }

    for (int i = 0; i < MaxFramesInFlight; ++i) {
        const int PoolStep = 8;
        const int MaxImgSamplerCount = 32;
        const int MaxImgCount = 16;
        const int MaxSamplerCount = 16;
        const int MaxStoreImgCount = 6;
        const int MaxUbufCount = 8;
        const int MaxUTbufCount = 16;
        const int MaxSbufCount = 16;
        const int MaxSTbufCount = 4;
        const int MaxAccCount = 1;
        const int InitialSetsCount = 16;

        default_descr_alloc_[i] = std::make_unique<DescrMultiPoolAlloc>(
            *api_, PoolStep, MaxImgSamplerCount, MaxImgCount, MaxSamplerCount, MaxStoreImgCount, MaxUbufCount,
            MaxUTbufCount, MaxSbufCount, MaxSTbufCount, MaxAccCount, InitialSetsCount);
    }

    VkPhysicalDeviceProperties device_properties = {};
    api_->vkGetPhysicalDeviceProperties(api_->physical_device, &device_properties);

    api_->phys_device_limits = device_properties.limits;
    api_->max_combined_image_samplers = std::min(std::min(device_properties.limits.maxPerStageDescriptorSampledImages,
                                                          device_properties.limits.maxPerStageDescriptorSamplers) -
                                                     10,
                                                 16384u);
    capabilities.max_combined_image_samplers = api_->max_combined_image_samplers;

    // SWRT is temporarily works with bindless textures only
    capabilities.swrt = (api_->max_combined_image_samplers >= 16384u);

    return true;
}

void Ren::Context::Resize(const int w, const int h, const bool novsync) {
    if (w_ == w && h_ == h && novsync_ == novsync) {
        return;
    }

    w_ = w;
    h_ = h;
    novsync_ = novsync;

    api_->vkDeviceWaitIdle(api_->device);

    for (const ImageHandle img : api_->present_image_handles) {
        images_.Erase(img);
    }
    api_->present_image_handles.clear();

    for (size_t i = 0; i < api_->present_image_views.size(); ++i) {
        api_->vkDestroyImageView(api_->device, api_->present_image_views[i], nullptr);
        // api_->vkDestroyImage(api_->device, api_->present_images[i], nullptr);
    }

    api_->vkDestroySwapchainKHR(api_->device, api_->swapchain, nullptr);

    if (api_->present_family_index != 0xffffffff && !api_->InitSwapChain(w, h, novsync, log_)) {
        log_->Error("Swapchain initialization failed");
    }

    if (api_->present_family_index != 0xffffffff && !api_->InitPresentImageViews(log_)) {
        log_->Error("Image views initialization failed");
    }

    for (size_t i = 0; i < api_->present_images.size(); ++i) {
        char name_buf[24];
        snprintf(name_buf, sizeof(name_buf), "Present Image [%i]", int(i));

        ImgParams params;
        params.w = w;
        params.h = h;
        if (api_->surface_format.format == VK_FORMAT_R8G8B8A8_UNORM) {
            params.format = eFormat::RGBA8;
        } else if (api_->surface_format.format == VK_FORMAT_B8G8R8A8_UNORM) {
            params.format = eFormat::BGRA8;
        } else if (api_->surface_format.format == VK_FORMAT_B8G8R8A8_SRGB) {
            params.format = eFormat::BGRA8_srgb;
        }
        params.usage = Bitmask(eImgUsage::RenderTarget);
        params.flags |= eImgFlags::NoOwnership;

        ImageHandle new_img = images_.Emplace();
        const auto &[img_main, img_cold] = images_[new_img];

        img_main.img = api_->present_images[i];
        img_main.views.push_back(api_->present_image_views[i]);

        img_cold.name = Ren::String{name_buf};
        img_cold.params = params;

        api_->present_image_handles.push_back(new_img);
    }
}

void Ren::Context::CheckDeviceCapabilities() {
    { // Depth-stencil
        VkImageFormatProperties props;
        const VkResult res = api_->vkGetPhysicalDeviceImageFormatProperties(
            api_->physical_device, VK_FORMAT_D24_UNORM_S8_UINT, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, 0, &props);

        capabilities.depth24_stencil8_format = (res == VK_SUCCESS);
    }
    { // BC4-compressed 3D texture
        VkImageFormatProperties props;
        const VkResult res = api_->vkGetPhysicalDeviceImageFormatProperties(
            api_->physical_device, VK_FORMAT_BC4_UNORM_BLOCK, VK_IMAGE_TYPE_3D, VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_SAMPLED_BIT, 0, &props);

        capabilities.bc4_3d_texture_format = (res == VK_SUCCESS);
    }
    {
        VkImageFormatProperties props;
        const VkResult res = api_->vkGetPhysicalDeviceImageFormatProperties(
            api_->physical_device, VK_FORMAT_R5G6B5_UNORM_PACK16, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, 0, &props);

        capabilities.rgb565_render_target = (res == VK_SUCCESS);
    }

    VkPhysicalDeviceProperties2 prop2 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
    void **pp_next = const_cast<void **>(&prop2.pNext);

    VkPhysicalDeviceSubgroupProperties subgroup_props = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES};
    (*pp_next) = &subgroup_props;
    pp_next = &subgroup_props.pNext;

    if (capabilities.hwrt) {
        (*pp_next) = &api_->rt_props;
        pp_next = &api_->rt_props.pNext;

        (*pp_next) = &api_->acc_props;
        pp_next = &api_->acc_props.pNext;
    }

    api_->vkGetPhysicalDeviceProperties2KHR(api_->physical_device, &prop2);

    capabilities.subgroup = (subgroup_props.supportedStages & VK_SHADER_STAGE_COMPUTE_BIT) != 0;
    capabilities.subgroup &= (subgroup_props.supportedOperations & VK_SUBGROUP_FEATURE_BASIC_BIT) != 0;
    capabilities.subgroup &= (subgroup_props.supportedOperations & VK_SUBGROUP_FEATURE_BALLOT_BIT) != 0;
    capabilities.subgroup &= (subgroup_props.supportedOperations & VK_SUBGROUP_FEATURE_SHUFFLE_BIT) != 0;
    capabilities.subgroup &= (subgroup_props.supportedOperations & VK_SUBGROUP_FEATURE_VOTE_BIT) != 0;
    capabilities.subgroup &= (subgroup_props.supportedOperations & VK_SUBGROUP_FEATURE_ARITHMETIC_BIT) != 0;
    capabilities.subgroup &= (subgroup_props.supportedOperations & VK_SUBGROUP_FEATURE_QUAD_BIT) != 0;
}

void Ren::Context::BegSingleTimeCommands(CommandBuffer cmd_buf) {
    VkCommandBufferBeginInfo begin_info = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    VkResult res = api_->vkBeginCommandBuffer(cmd_buf, &begin_info);
    assert(res == VK_SUCCESS);
}

Ren::CommandBuffer Ren::Context::BegTempSingleTimeCommands() { return api_->BegSingleTimeCommands(); }

Ren::SyncFence Ren::Context::EndSingleTimeCommands(CommandBuffer cmd_buf) {
    VkFenceCreateInfo fence_info = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    VkFence new_fence;
    const VkResult res = api_->vkCreateFence(api_->device, &fence_info, nullptr, &new_fence);
    if (res != VK_SUCCESS) {
        log_->Error("Failed to create fence!");
        return {};
    }

    api_->EndSingleTimeCommands(cmd_buf, new_fence);

    return SyncFence{api_.get(), new_fence};
}

void Ren::Context::EndTempSingleTimeCommands(CommandBuffer cmd_buf) { api_->EndSingleTimeCommands(cmd_buf); }

void Ren::Context::InsertReadbackMemoryBarrier(CommandBuffer cmd_buf) {
    VkMemoryBarrier mem_barrier = {VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    mem_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    mem_barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;

    api_->vkCmdPipelineBarrier(cmd_buf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &mem_barrier,
                               0, nullptr, 0, nullptr);
}

Ren::CommandBuffer Ren::Context::current_cmd_buf() { return api_->curr_cmd_buf; }

int Ren::Context::WriteTimestamp(const bool start) {
    VkCommandBuffer cmd_buf = api_->draw_cmd_buf[api_->backend_frame];

    api_->vkCmdWriteTimestamp(cmd_buf, start ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT : VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                              api_->query_pools[api_->backend_frame], api_->query_counts[api_->backend_frame]);

    const uint32_t query_index = api_->query_counts[api_->backend_frame]++;
    assert(api_->query_counts[api_->backend_frame] < MaxTimestampQueries);
    return int(query_index);
}

uint64_t Ren::Context::GetTimestampIntervalDurationUs(const int query_beg, const int query_end) const {
    return uint64_t(float(api_->query_results[api_->backend_frame][query_end] -
                          api_->query_results[api_->backend_frame][query_beg]) *
                    api_->phys_device_limits.timestampPeriod / 1000.0f);
}

void Ren::Context::WaitIdle() { api_->vkDeviceWaitIdle(api_->device); }

void Ren::Context::ResetAllocators() {
    default_mem_allocs_ = {};
    default_mem_allocs_ =
        std::make_unique<MemAllocators>("Default Allocs", api_.get(), 1 * 1024 * 1024 /* initial_block_size */,
                                        1.5f /* growth_factor */, 128 * 1024 * 1024 /* max_pool_size */);
}

uint64_t Ren::Context::device_id() const {
    return (uint64_t(api_->device_properties.vendorID) << 32) | api_->device_properties.deviceID;
}

bool Ren::Context::InitPipelineCache(Ren::Span<const uint8_t> data) {
    VkPipelineCacheCreateInfo cache_create_info = {VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO};

    if (data.size() > sizeof(pipeline_cache_header_t)) {
        pipeline_cache_header_t header = {};
        memcpy(&header, data.data(), sizeof(pipeline_cache_header_t));
        if (header.version == PipelineCacheVersion &&
            data.size() >= ptrdiff_t(sizeof(pipeline_cache_header_t) + header.data_size)) {
            const uint32_t orig_hash = header.data_hash;
            header.data_hash = 0;

            uint32_t data_hash =
                adler32(Span<const uint8_t>{(const uint8_t *)&header, sizeof(pipeline_cache_header_t)});
            data_hash = adler32(data_hash,
                                Span<const uint8_t>{data.data() + sizeof(pipeline_cache_header_t), header.data_size});
            if (data_hash == orig_hash && header.vendor_id == api_->device_properties.vendorID &&
                header.device_id == api_->device_properties.deviceID &&
                header.driver_version == api_->device_properties.driverVersion &&
                memcmp(header.uuid, api_->device_properties.pipelineCacheUUID, VK_UUID_SIZE) == 0) {
                cache_create_info.pInitialData = data.data() + sizeof(pipeline_cache_header_t);
                cache_create_info.initialDataSize = header.data_size;
            }
        }
    }

    DestroyPipelineCache();

    const VkResult res = api_->vkCreatePipelineCache(api_->device, &cache_create_info, nullptr, &api_->pipeline_cache);
    return res == VK_SUCCESS;
}

void Ren::Context::DestroyPipelineCache() {
    if (api_->pipeline_cache) {
        api_->vkDestroyPipelineCache(api_->device, api_->pipeline_cache, nullptr);
        api_->pipeline_cache = {};
    }
}

size_t Ren::Context::WritePipelineCache(Span<uint8_t> out_data) {
    Ren::Span<uint8_t> orig_data = out_data;
    if (!out_data.empty()) {
        out_data = Ren::Span<uint8_t>{out_data.data() + sizeof(pipeline_cache_header_t),
                                      out_data.size() - sizeof(pipeline_cache_header_t)};
    }
    size_t data_size = out_data.size();
    const VkResult res = api_->vkGetPipelineCacheData(api_->device, api_->pipeline_cache, &data_size, out_data.data());
    if (res == VK_SUCCESS && !orig_data.empty()) {
        pipeline_cache_header_t header = {};
        header.version = PipelineCacheVersion;
        header.data_size = uint32_t(data_size);
        header.data_hash = 0;
        header.vendor_id = api_->device_properties.vendorID;
        header.device_id = api_->device_properties.deviceID;
        header.driver_version = api_->device_properties.driverVersion;
        header.driver_abi = sizeof(void *);
        memcpy(header.uuid, api_->device_properties.pipelineCacheUUID, VK_UUID_SIZE);

        memcpy(orig_data.data(), &header, sizeof(pipeline_cache_header_t));
        header.data_hash = adler32(orig_data);
        memcpy(orig_data.data() + offsetof(pipeline_cache_header_t, data_hash), &header.data_hash, sizeof(uint32_t));
    }
    return res == VK_SUCCESS ? sizeof(pipeline_cache_header_t) + data_size : 0;
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif
