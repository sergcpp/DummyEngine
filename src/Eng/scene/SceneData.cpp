#include "SceneData.h"

#if defined(USE_VK_RENDER)
#include <Ren/DescriptorPool.h>
#include <Ren/VKCtx.h>
#endif

Eng::PersistentGpuData::PersistentGpuData() = default;

Eng::PersistentGpuData::~PersistentGpuData() { Clear(); }

void Eng::PersistentGpuData::Clear() {
#if defined(USE_VK_RENDER)
    if (textures_descr_pool) {
        Ren::ApiContext *api_ctx = textures_descr_pool->api_ctx();
        api_ctx->vkDestroyDescriptorSetLayout(api_ctx->device, textures_descr_layout, nullptr);
        textures_descr_layout = VK_NULL_HANDLE;
        api_ctx->vkDestroyDescriptorSetLayout(api_ctx->device, rt_textures_descr_layout, nullptr);
        rt_textures_descr_layout = VK_NULL_HANDLE;
        api_ctx->vkDestroyDescriptorSetLayout(api_ctx->device, rt_inline_textures_descr_layout, nullptr);
        rt_inline_textures_descr_layout = VK_NULL_HANDLE;
        for (auto &descr_set : textures_descr_sets) {
            descr_set.clear();
        }
        for (auto &descr_set : rt_textures_descr_sets) {
            descr_set = VK_NULL_HANDLE;
        }
    }
    textures_descr_pool = {};
    rt_textures_descr_pool = {};
    rt_inline_textures_descr_pool = {};
    instance_buf = {};
#elif defined(USE_GL_RENDER)
    textures_buf = {};
#endif
    materials_buf = {};
    pipelines.clear();
}