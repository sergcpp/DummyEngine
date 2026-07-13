#include "SceneData.h"

#include <Ren/Context.h>
#include <Ren/ResizableBuffer.h>
#if defined(REN_VK_BACKEND)
#include <Ren/DescriptorPool.h>
#include <Ren/Vk/VKCtx.h>
#endif

Eng::PersistentGpuData::PersistentGpuData(Ren::Context &_ctx) : ctx(_ctx) {}

Eng::PersistentGpuData::~PersistentGpuData() {
    Release();

    if (trilinear_sampler) {
        ctx.ReleaseSampler(trilinear_sampler);
    }
    vertex_buf1.reset();
    vertex_buf2.reset();
    indices_buf.reset();
    skin_vertex_buf.reset();
    delta_buf.reset();
}

void Eng::PersistentGpuData::Release() {
    const Ren::ApiContext &api = ctx.api();
#if defined(REN_VK_BACKEND)
    if (textures_descr_pool) {
        api.vkDestroyDescriptorSetLayout(api.device, textures_descr_layout, nullptr);
        textures_descr_layout = VK_NULL_HANDLE;
        api.vkDestroyDescriptorSetLayout(api.device, rt_textures_descr_layout, nullptr);
        rt_textures_descr_layout = VK_NULL_HANDLE;
        api.vkDestroyDescriptorSetLayout(api.device, rt_inline_textures_descr_layout, nullptr);
        rt_inline_textures_descr_layout = VK_NULL_HANDLE;
        for (auto &descr_set : textures_descr_sets) {
            descr_set.clear();
        }
        for (auto &descr_set : rt_textures_descr_sets) {
            descr_set = VK_NULL_HANDLE;
        }
        api.allocators_to_release[api.backend_frame].push_back(std::move(mem_allocs));
    }
    textures_descr_pool = {};
    rt_textures_descr_pool = {};
    rt_inline_textures_descr_pool = {};
#elif defined(REN_GL_BACKEND)
    if (textures_buf) {
        ctx.ReleaseBuffer(textures_buf);
    }
    textures_buf = {};
#endif
    ctx.ReleaseBuffer(instances);
    instances = {};
    ctx.ReleaseBuffer(materials);
    materials = {};
    if (vertex_buf1) {
        vertex_buf1->Release(true /* immediately */);
        vertex_buf1 = {};
    }
    if (vertex_buf2) {
        vertex_buf2->Release(true /* immediately */);
        vertex_buf2 = {};
    }
    if (skin_vertex_buf) {
        skin_vertex_buf->Release(true /* immediately */);
        skin_vertex_buf = {};
    }
    if (delta_buf) {
        delta_buf->Release(true /* immediately */);
        delta_buf = {};
    }
    if (indices_buf) {
        indices_buf->Release(true /* immediately */);
        indices_buf = {};
    }
    if (stoch_lights) {
        ctx.ReleaseBuffer(stoch_lights);
    }
    stoch_lights = {};
    if (stoch_lights_nodes) {
        ctx.ReleaseBuffer(stoch_lights_nodes);
    }
    stoch_lights_nodes = {};
    for (Ren::BufferHandle &buf : rt_tlas_buf) {
        if (buf) {
            ctx.ReleaseBuffer(buf);
        }
        buf = {};
    }
    for (const Ren::BufferHandle b : hwrt.rt_blas_buffers) {
        ctx.ReleaseBuffer(b);
    }
    hwrt = {};
    if (swrt.rt_prim_indices_buf) {
        swrt.rt_prim_indices_buf->Release(true /* immediately */);
        swrt.rt_prim_indices_buf = {};
    }
    if (swrt.rt_blas_buf) {
        swrt.rt_blas_buf->Release(true /* immediately */);
        swrt.rt_blas_buf = {};
    }
    swrt = {};

    for (const Ren::AccStructHandle blas : rt_blases) {
        ctx.ReleaseAccStruct(blas);
    }
    rt_blases.clear();
    for (Ren::AccStructHandle &tlas : rt_tlases) {
        ctx.ReleaseAccStruct(tlas);
        tlas = {};
    }

    ctx.ReleaseImage(probe_irradiance, true /* immediately */);
    ctx.ReleaseImage(probe_distance, true /* immediately */);
    ctx.ReleaseImage(probe_offset, true /* immediately */);

    probe_irradiance = probe_distance = probe_offset = {};
    probe_volumes.clear();

    ctx.ReleaseBuffer(spatial_cache_entries, true /* immediately */);
    ctx.ReleaseBuffer(spatial_cache_voxels, true /* immediately */);

    spatial_cache_entries = spatial_cache_voxels = {};
}