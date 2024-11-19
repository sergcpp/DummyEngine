#include "SceneData.h"

#if defined(USE_VK_RENDER)
#include <Ren/DescriptorPool.h>
#include <Ren/VKCtx.h>
#endif

Eng::PersistentGpuData::PersistentGpuData() = default;

Eng::PersistentGpuData::~PersistentGpuData() { Release(); }

void Eng::PersistentGpuData::Release() {
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
#elif defined(USE_GL_RENDER)
    textures_buf = {};
#endif
    pipelines.clear();
    instance_buf = {};
    materials_buf = {};
    vertex_buf1 = vertex_buf2 = skin_vertex_buf = delta_buf = indices_buf = {};
    stoch_lights_buf = stoch_lights_nodes_buf = {};
    rt_tlas_buf = rt_sh_tlas_buf = {};
    hwrt = {};
    swrt = {};

    rt_tlas = {};
    rt_sh_tlas = {};

    probe_ray_data = {};
    probe_irradiance = {};
    probe_distance = {};
    probe_offset = {};
    probe_volumes.clear();
}