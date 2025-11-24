#include "Context.h"

#include <mutex>

#include "DescriptorPool.h"
#include "VKCtx.h"

#if defined(VK_USE_PLATFORM_WIN32_KHR)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#endif

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

    api_ctx_->present_image_refs.clear();
    ReleaseAll();

    if (api_ctx_ && api_ctx_->device) {
        api_ctx_->vkDeviceWaitIdle(api_ctx_->device);

        for (int i = 0; i < MaxFramesInFlight; ++i) {
            api_ctx_->backend_frame = (api_ctx_->backend_frame + 1) % Ren::MaxFramesInFlight;

            default_descr_alloc_[api_ctx_->backend_frame] = {};
            DestroyDeferredResources(api_ctx_.get(), api_ctx_->backend_frame);

            api_ctx_->vkDestroyFence(api_ctx_->device, api_ctx_->in_flight_fences[api_ctx_->backend_frame], nullptr);
            api_ctx_->vkDestroySemaphore(api_ctx_->device,
                                         api_ctx_->render_finished_semaphores[api_ctx_->backend_frame], nullptr);
            api_ctx_->vkDestroySemaphore(api_ctx_->device, api_ctx_->image_avail_semaphores[api_ctx_->backend_frame],
                                         nullptr);

            api_ctx_->vkDestroyQueryPool(api_ctx_->device, api_ctx_->query_pools[api_ctx_->backend_frame], nullptr);
        }

        default_mem_allocs_ = {};

        api_ctx_->vkDestroyPipelineCache(api_ctx_->device, api_ctx_->pipeline_cache, nullptr);

        api_ctx_->vkFreeCommandBuffers(api_ctx_->device, api_ctx_->command_pool, MaxFramesInFlight,
                                       &api_ctx_->draw_cmd_buf[0]);

        api_ctx_->vkDestroyCommandPool(api_ctx_->device, api_ctx_->command_pool, nullptr);
        api_ctx_->vkDestroyCommandPool(api_ctx_->device, api_ctx_->temp_command_pool, nullptr);

        for (size_t i = 0; i < api_ctx_->present_image_views.size(); ++i) {
            api_ctx_->vkDestroyImageView(api_ctx_->device, api_ctx_->present_image_views[i], nullptr);
            // vkDestroyImage(api_ctx_->device, api_ctx_->present_images[i], nullptr);
        }
        if (api_ctx_->swapchain) {
            api_ctx_->vkDestroySwapchainKHR(api_ctx_->device, api_ctx_->swapchain, nullptr);
        }
        api_ctx_->vkDestroyDevice(api_ctx_->device, nullptr);
        if (api_ctx_->surface) {
            api_ctx_->vkDestroySurfaceKHR(api_ctx_->instance, api_ctx_->surface, nullptr);
        }
        if (api_ctx_->debug_callback) {
            api_ctx_->vkDestroyDebugReportCallbackEXT(api_ctx_->instance, api_ctx_->debug_callback, nullptr);
        }

#if defined(VK_USE_PLATFORM_XLIB_KHR)
        if (g_dpy) {
            XCloseDisplay(g_dpy); // has to be done before instance destruction
                                  // (https://github.com/KhronosGroup/Vulkan-LoaderAndValidationLayers/issues/1894)
        }
#endif
        api_ctx_->vkDestroyInstance(api_ctx_->instance, nullptr);
    }
}

Ren::DescrMultiPoolAlloc &Ren::Context::default_descr_alloc() const {
    return *default_descr_alloc_[api_ctx_->backend_frame];
}

bool Ren::Context::Init(const int w, const int h, ILog *log, int validation_level, const bool nohwrt,
                        const bool nosubgroup, std::string_view preferred_device) {
    api_ctx_ = std::make_unique<ApiContext>();
    if (!api_ctx_->Load(log)) {
        return false;
    }

    w_ = w;
    h_ = h;
    log_ = log;

    std::lock_guard<std::mutex> _(g_device_mtx);

    const char *enabled_layers[] = {"VK_LAYER_KHRONOS_validation", "VK_LAYER_KHRONOS_synchronization2"};
    const int enabled_layers_count = validation_level ? int(std::size(enabled_layers)) : 0;

    if (!api_ctx_->InitVkInstance(enabled_layers, enabled_layers_count, validation_level, log)) {
        return false;
    }
    if (!api_ctx_->LoadInstanceFunctions(log)) {
        return false;
    }

    validation_level_ = validation_level;

    if (validation_level) { // Sebug debug report callback
        VkDebugReportCallbackCreateInfoEXT callback_create_info = {VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT};
        callback_create_info.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT |
                                     VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
        callback_create_info.pfnCallback = DebugReportCallback;
        callback_create_info.pUserData = this;

        const VkResult res = api_ctx_->vkCreateDebugReportCallbackEXT(api_ctx_->instance, &callback_create_info,
                                                                      nullptr, &api_ctx_->debug_callback);
        if (res != VK_SUCCESS) {
            log->Warning("Failed to create debug report callback");
        }
    }

    // Create platform-specific surface
    if (!api_ctx_->InitVkSurface(log)) {
        return false;
    }

    if (!api_ctx_->ChooseVkPhysicalDevice(preferred_device, log)) {
        return false;
    }

    if (!api_ctx_->InitVkDevice(enabled_layers, enabled_layers_count, validation_level, log)) {
        return false;
    }

    // Workaround for a buggy linux AMD driver, make sure vkGetBufferDeviceAddressKHR is not NULL
    auto dev_vkGetBufferDeviceAddressKHR =
        (PFN_vkGetBufferDeviceAddressKHR)api_ctx_->vkGetDeviceProcAddr(api_ctx_->device, "vkGetBufferDeviceAddressKHR");
    if (!dev_vkGetBufferDeviceAddressKHR || nohwrt) {
        api_ctx_->raytracing_supported = api_ctx_->ray_query_supported = false;
    }

    if (api_ctx_->subgroup_size_control_supported) {
        VkPhysicalDeviceSubgroupSizeControlFeaturesEXT subgroup_size_control_features = {
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_FEATURES_EXT};

        VkPhysicalDeviceFeatures2 feat2 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
        feat2.pNext = &subgroup_size_control_features;

        api_ctx_->vkGetPhysicalDeviceFeatures2KHR(api_ctx_->physical_device, &feat2);

        api_ctx_->subgroup_size_control_supported &= (subgroup_size_control_features.subgroupSizeControl == VK_TRUE);
    }

    if (api_ctx_->present_family_index != 0xffffffff && !api_ctx_->InitSwapChain(w, h, log)) {
        return false;
    }

    const uint32_t family_index =
        api_ctx_->present_family_index != 0xffffffff ? api_ctx_->present_family_index : api_ctx_->graphics_family_index;
    if (!api_ctx_->InitCommandBuffers(family_index, log)) {
        return false;
    }

    if (api_ctx_->present_family_index != 0xffffffff) {
        api_ctx_->vkGetDeviceQueue(api_ctx_->device, api_ctx_->present_family_index, 0, &api_ctx_->present_queue);
    }
    api_ctx_->vkGetDeviceQueue(api_ctx_->device, api_ctx_->graphics_family_index, 0, &api_ctx_->graphics_queue);

    if (api_ctx_->present_family_index != 0xffffffff && !api_ctx_->InitPresentImageViews(log)) {
        return false;
    }

    log_->Info("============================================================================");
    log_->Info("Device info:");

    log_->Info("\tVulkan version\t: %u.%u", VK_API_VERSION_MAJOR(api_ctx_->device_properties.apiVersion),
               VK_API_VERSION_MINOR(api_ctx_->device_properties.apiVersion));

    auto it = std::find_if(
        std::begin(KnownVendors), std::end(KnownVendors),
        [this](const std::pair<uint32_t, const char *> v) { return api_ctx_->device_properties.vendorID == v.first; });
    if (it != std::end(KnownVendors)) {
        log_->Info("\tVendor\t\t: %s", it->second);
    }
    log_->Info("\tName\t\t: %s", api_ctx_->device_properties.deviceName);
    log_->Info("============================================================================");

    capabilities.hwrt = (api_ctx_->raytracing_supported && api_ctx_->ray_query_supported);

    CheckDeviceCapabilities();

    capabilities.subgroup &= !nosubgroup;

    default_mem_allocs_ =
        std::make_unique<MemAllocators>("Default Allocs", api_ctx_.get(), 1 * 1024 * 1024 /* initial_block_size */,
                                        1.5f /* growth_factor */, 128 * 1024 * 1024 /* max_pool_size */);

    InitDefaultBuffers();

    image_atlas_ =
        ImageAtlasArray{api_ctx_.get(),   "Image Atlas",     ImageAtlasWidth,
                        ImageAtlasHeight, ImageAtlasLayers,  1,
                        eFormat::RGBA8,   eFilter::Bilinear, Bitmask(eImgUsage::Transfer) | eImgUsage::Sampled};

    for (size_t i = 0; i < api_ctx_->present_images.size(); ++i) {
        char name_buf[24];
        snprintf(name_buf, sizeof(name_buf), "Present Image [%i]", int(i));

        ImgParams params;
        params.w = w;
        params.h = h;
        if (api_ctx_->surface_format.format == VK_FORMAT_R8G8B8A8_UNORM) {
            params.format = eFormat::RGBA8;
        } else if (api_ctx_->surface_format.format == VK_FORMAT_R8G8B8A8_SRGB) {
            params.format = eFormat::RGBA8_srgb;
        } else if (api_ctx_->surface_format.format == VK_FORMAT_B8G8R8A8_UNORM) {
            params.format = eFormat::BGRA8;
        } else if (api_ctx_->surface_format.format == VK_FORMAT_B8G8R8A8_SRGB) {
            params.format = eFormat::BGRA8_srgb;
        }
        params.usage = Bitmask(eImgUsage::RenderTarget);
        params.flags |= eImgFlags::NoOwnership;

        api_ctx_->present_image_refs.emplace_back(images_.Insert(
            name_buf, api_ctx_.get(),
            ImgHandle{api_ctx_->present_images[i], api_ctx_->present_image_views[i], VkImageView{}, VkSampler{}, 0},
            params, MemAllocation{}, log_));
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
            api_ctx_.get(), PoolStep, MaxImgSamplerCount, MaxImgCount, MaxSamplerCount, MaxStoreImgCount, MaxUbufCount,
            MaxUTbufCount, MaxSbufCount, MaxSTbufCount, MaxAccCount, InitialSetsCount);
    }

    VkPhysicalDeviceProperties device_properties = {};
    api_ctx_->vkGetPhysicalDeviceProperties(api_ctx_->physical_device, &device_properties);

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
    if (w_ == w && h_ == h) {
        return;
    }

    w_ = w;
    h_ = h;

    api_ctx_->vkDeviceWaitIdle(api_ctx_->device);
    api_ctx_->present_image_refs.clear();

    for (size_t i = 0; i < api_ctx_->present_image_views.size(); ++i) {
        api_ctx_->vkDestroyImageView(api_ctx_->device, api_ctx_->present_image_views[i], nullptr);
        // vkDestroyImage(api_ctx_->device, api_ctx_->present_images[i], nullptr);
    }

    api_ctx_->vkDestroySwapchainKHR(api_ctx_->device, api_ctx_->swapchain, nullptr);

    if (api_ctx_->present_family_index != 0xffffffff && !api_ctx_->InitSwapChain(w, h, log_)) {
        log_->Error("Swapchain initialization failed");
    }

    if (api_ctx_->present_family_index != 0xffffffff && !api_ctx_->InitPresentImageViews(log_)) {
        log_->Error("Image views initialization failed");
    }

    for (size_t i = 0; i < api_ctx_->present_images.size(); ++i) {
        char name_buf[24];
        snprintf(name_buf, sizeof(name_buf), "Present Image [%i]", int(i));

        ImgParams params;
        params.w = w;
        params.h = h;
        if (api_ctx_->surface_format.format == VK_FORMAT_R8G8B8A8_UNORM) {
            params.format = eFormat::RGBA8;
        } else if (api_ctx_->surface_format.format == VK_FORMAT_B8G8R8A8_UNORM) {
            params.format = eFormat::BGRA8;
        } else if (api_ctx_->surface_format.format == VK_FORMAT_B8G8R8A8_SRGB) {
            params.format = eFormat::BGRA8_srgb;
        }
        params.usage = Bitmask(eImgUsage::RenderTarget);
        params.flags |= eImgFlags::NoOwnership;

        ImgRef ref = images_.FindByName(name_buf);
        if (ref) {
            ref->Init(
                ImgHandle{api_ctx_->present_images[i], api_ctx_->present_image_views[i], VkImageView{}, VkSampler{}, 0},
                params, MemAllocation{}, log_);
            api_ctx_->present_image_refs.emplace_back(std::move(ref));
        } else {
            api_ctx_->present_image_refs.emplace_back(images_.Insert(
                name_buf, api_ctx_.get(),
                ImgHandle{api_ctx_->present_images[i], api_ctx_->present_image_views[i], VkImageView{}, VkSampler{}, 0},
                params, MemAllocation{}, log_));
        }
    }
}

void Ren::Context::CheckDeviceCapabilities() {
    { // Depth-stencil
        VkImageFormatProperties props;
        const VkResult res = api_ctx_->vkGetPhysicalDeviceImageFormatProperties(
            api_ctx_->physical_device, VK_FORMAT_D24_UNORM_S8_UINT, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, 0, &props);

        capabilities.depth24_stencil8_format = (res == VK_SUCCESS);
    }
    { // BC4-compressed 3D texture
        VkImageFormatProperties props;
        const VkResult res = api_ctx_->vkGetPhysicalDeviceImageFormatProperties(
            api_ctx_->physical_device, VK_FORMAT_BC4_UNORM_BLOCK, VK_IMAGE_TYPE_3D, VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_SAMPLED_BIT, 0, &props);

        capabilities.bc4_3d_texture_format = (res == VK_SUCCESS);
    }
    {
        VkImageFormatProperties props;
        const VkResult res = api_ctx_->vkGetPhysicalDeviceImageFormatProperties(
            api_ctx_->physical_device, VK_FORMAT_R5G6B5_UNORM_PACK16, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, 0, &props);

        capabilities.rgb565_render_target = (res == VK_SUCCESS);
    }

    VkPhysicalDeviceProperties2 prop2 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
    void **pp_next = const_cast<void **>(&prop2.pNext);

    VkPhysicalDeviceSubgroupProperties subgroup_props = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES};
    (*pp_next) = &subgroup_props;
    pp_next = &subgroup_props.pNext;

    if (capabilities.hwrt) {
        (*pp_next) = &api_ctx_->rt_props;
        pp_next = &api_ctx_->rt_props.pNext;

        (*pp_next) = &api_ctx_->acc_props;
        pp_next = &api_ctx_->acc_props.pNext;
    }

    api_ctx_->vkGetPhysicalDeviceProperties2KHR(api_ctx_->physical_device, &prop2);

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

    VkResult res = api_ctx_->vkBeginCommandBuffer(cmd_buf, &begin_info);
    assert(res == VK_SUCCESS);
}

Ren::CommandBuffer Ren::Context::BegTempSingleTimeCommands() { return api_ctx_->BegSingleTimeCommands(); }

Ren::SyncFence Ren::Context::EndSingleTimeCommands(CommandBuffer cmd_buf) {
    VkFenceCreateInfo fence_info = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    VkFence new_fence;
    const VkResult res = api_ctx_->vkCreateFence(api_ctx_->device, &fence_info, nullptr, &new_fence);
    if (res != VK_SUCCESS) {
        log_->Error("Failed to create fence!");
        return {};
    }

    api_ctx_->EndSingleTimeCommands(cmd_buf, new_fence);

    return SyncFence{api_ctx_.get(), new_fence};
}

void Ren::Context::EndTempSingleTimeCommands(CommandBuffer cmd_buf) { api_ctx_->EndSingleTimeCommands(cmd_buf); }

void Ren::Context::InsertReadbackMemoryBarrier(CommandBuffer cmd_buf) {
    VkMemoryBarrier mem_barrier = {VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    mem_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    mem_barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;

    api_ctx_->vkCmdPipelineBarrier(cmd_buf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1,
                                   &mem_barrier, 0, nullptr, 0, nullptr);
}

Ren::CommandBuffer Ren::Context::current_cmd_buf() { return api_ctx_->curr_cmd_buf; }

int Ren::Context::WriteTimestamp(const bool start) {
    VkCommandBuffer cmd_buf = api_ctx_->draw_cmd_buf[api_ctx_->backend_frame];

    api_ctx_->vkCmdWriteTimestamp(
        cmd_buf, start ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT : VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        api_ctx_->query_pools[api_ctx_->backend_frame], api_ctx_->query_counts[api_ctx_->backend_frame]);

    const uint32_t query_index = api_ctx_->query_counts[api_ctx_->backend_frame]++;
    assert(api_ctx_->query_counts[api_ctx_->backend_frame] < MaxTimestampQueries);
    return int(query_index);
}

uint64_t Ren::Context::GetTimestampIntervalDurationUs(const int query_beg, const int query_end) const {
    return uint64_t(float(api_ctx_->query_results[api_ctx_->backend_frame][query_end] -
                          api_ctx_->query_results[api_ctx_->backend_frame][query_beg]) *
                    api_ctx_->phys_device_limits.timestampPeriod / 1000.0f);
}

void Ren::Context::WaitIdle() { api_ctx_->vkDeviceWaitIdle(api_ctx_->device); }

uint64_t Ren::Context::device_id() const {
    return (uint64_t(api_ctx_->device_properties.vendorID) << 32) | api_ctx_->device_properties.deviceID;
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
            if (data_hash == orig_hash && header.vendor_id == api_ctx_->device_properties.vendorID &&
                header.device_id == api_ctx_->device_properties.deviceID &&
                header.driver_version == api_ctx_->device_properties.driverVersion &&
                memcmp(header.uuid, api_ctx_->device_properties.pipelineCacheUUID, VK_UUID_SIZE) == 0) {
                cache_create_info.pInitialData = data.data() + sizeof(pipeline_cache_header_t);
                cache_create_info.initialDataSize = header.data_size;
            }
        }
    }

    const VkResult res =
        api_ctx_->vkCreatePipelineCache(api_ctx_->device, &cache_create_info, nullptr, &api_ctx_->pipeline_cache);
    return res == VK_SUCCESS;
}

size_t Ren::Context::WritePipelineCache(Span<uint8_t> out_data) {
    Ren::Span<uint8_t> orig_data = out_data;
    if (!out_data.empty()) {
        out_data = Ren::Span<uint8_t>{out_data.data() + sizeof(pipeline_cache_header_t),
                                      out_data.size() - sizeof(pipeline_cache_header_t)};
    }
    size_t data_size = out_data.size();
    const VkResult res =
        api_ctx_->vkGetPipelineCacheData(api_ctx_->device, api_ctx_->pipeline_cache, &data_size, out_data.data());
    if (res == VK_SUCCESS && !orig_data.empty()) {
        pipeline_cache_header_t header = {};
        header.version = PipelineCacheVersion;
        header.data_size = uint32_t(data_size);
        header.data_hash = 0;
        header.vendor_id = api_ctx_->device_properties.vendorID;
        header.device_id = api_ctx_->device_properties.deviceID;
        header.driver_version = api_ctx_->device_properties.driverVersion;
        header.driver_abi = sizeof(void *);
        memcpy(header.uuid, api_ctx_->device_properties.pipelineCacheUUID, VK_UUID_SIZE);

        memcpy(orig_data.data(), &header, sizeof(pipeline_cache_header_t));
        header.data_hash = adler32(orig_data);
        memcpy(orig_data.data() + offsetof(pipeline_cache_header_t, data_hash), &header.data_hash, sizeof(uint32_t));
    }
    return res == VK_SUCCESS ? sizeof(pipeline_cache_header_t) + data_size : 0;
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif
