#pragma once

#include <cstdint>

#include "Fwd.h"
#include "MVec.h"
#include "Span.h"

namespace Ren {
struct ApiContext;
class Buffer;
class DescrMultiPoolAlloc;
class ILog;
class Pipeline;
class ProbeStorage;
class Texture1D;
class Texture2D;
class Texture3D;
class Texture2DArray;

enum class eBindTarget : uint16_t {
    Tex2DSampled,
    Tex2DArraySampled,
    Tex2DMs,
    TexCubeArray,
    Tex3DSampled,
    TBuf,
    UBuf,
    SBufRO,
    SBufRW,
    Image2D,
    Image2DArray,
    AccStruct,
    _Count
};

#if defined(USE_GL_RENDER)
uint32_t GLBindTarget(eBindTarget binding);
extern int g_param_buf_binding;
#endif

struct OpaqueHandle {
    union {
        const Texture2D *tex;
        const Texture3D *tex3d;
        const Buffer *buf;
        const Texture1D *tex_buf;
        const ProbeStorage *cube_arr;
        const Texture2DArray *tex2d_arr;
#if defined(USE_VK_RENDER)
        const AccStructureVK *acc_struct;
#endif
    };
    OpaqueHandle() = default;
    OpaqueHandle(const Texture1D &_tex) : tex_buf(&_tex) {}
    OpaqueHandle(const Texture2D &_tex) : tex(&_tex) {}
    OpaqueHandle(const Texture3D &_tex) : tex3d(&_tex) {}
    OpaqueHandle(const Buffer &_buf) : buf(&_buf) {}
    OpaqueHandle(const ProbeStorage &_probes) : cube_arr(&_probes) {}
    OpaqueHandle(const Texture2DArray &_tex2d_arr) : tex2d_arr(&_tex2d_arr) {}
#if defined(USE_VK_RENDER)
    OpaqueHandle(const AccStructureVK &_acc_struct) : acc_struct(&_acc_struct) {}
#endif
};

struct Binding {
    eBindTarget trg;
    uint16_t loc = 0;
    uint16_t offset = 0;
    uint16_t size = 0;
    OpaqueHandle handle;
    const Sampler *sampler = nullptr;

    Binding() = default;
    Binding(eBindTarget _trg, int _loc, OpaqueHandle _handle, const Sampler *_sampler = nullptr)
        : trg(_trg), loc(_loc), handle(_handle), sampler(_sampler) {}
    Binding(eBindTarget _trg, int _loc, size_t _offset, OpaqueHandle _handle)
        : trg(_trg), loc(_loc), offset(uint16_t(_offset)), handle(_handle) {}
    Binding(eBindTarget _trg, uint16_t _loc, size_t _offset, size_t _size, OpaqueHandle _handle)
        : trg(_trg), loc(_loc), offset(uint16_t(_offset)), size(uint16_t(_size)), handle(_handle) {}
};
static_assert(sizeof(Binding) == sizeof(void *) + 8 + sizeof(void *), "!");

#if defined(USE_VK_RENDER)
VkDescriptorSet PrepareDescriptorSet(ApiContext *api_ctx, VkDescriptorSetLayout layout, Span<const Binding> bindings,
                                     DescrMultiPoolAlloc *descr_alloc, ILog *log);
#endif

void DispatchCompute(const Pipeline &comp_pipeline, Vec3u grp_count, Span<const Binding> bindings,
                     const void *uniform_data, int uniform_data_len, DescrMultiPoolAlloc *descr_alloc, ILog *log);
void DispatchComputeIndirect(const Pipeline &comp_pipeline, const Buffer &indir_buf, uint32_t indir_buf_offset,
                             Span<const Binding> bindings, const void *uniform_data, int uniform_data_len,
                             DescrMultiPoolAlloc *descr_alloc, ILog *log);
} // namespace Ren