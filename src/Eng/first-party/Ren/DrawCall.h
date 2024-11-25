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
    Tex2D,
    Tex2DSampled,
    Tex2DArraySampled,
    Tex2DMs,
    TexCubeArray,
    Tex3DSampled,
    Sampler,
    UBuf,
    UTBuf,
    SBufRO,
    SBufRW,
    STBufRO,
    STBufRW,
    Image2D,
    Image2DArray,
    AccStruct,
    _Count
};

#if defined(REN_GL_BACKEND)
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
#if defined(REN_VK_BACKEND)
        const AccStructureVK *acc_struct;
#endif
    };
    const Sampler *sampler = nullptr;
    int view_index = 0;

    OpaqueHandle() = default;
    OpaqueHandle(const Texture1D &_tex) : tex_buf(&_tex) {}
    OpaqueHandle(const Texture2D &_tex, int _view_index = 0) : tex(&_tex), view_index(_view_index) {}
    OpaqueHandle(const Texture2D &_tex, const Sampler &_sampler, int _view_index = 0)
        : tex(&_tex), sampler(&_sampler), view_index(_view_index) {}
    OpaqueHandle(const Texture3D &_tex) : tex3d(&_tex) {}
    OpaqueHandle(const Buffer &_buf) : buf(&_buf) {}
    OpaqueHandle(const ProbeStorage &_probes) : cube_arr(&_probes) {}
    OpaqueHandle(const Texture2DArray &_tex2d_arr) : tex2d_arr(&_tex2d_arr) {}
    OpaqueHandle(const Sampler &_sampler) : sampler(&_sampler) {}
#if defined(REN_VK_BACKEND)
    OpaqueHandle(const AccStructureVK &_acc_struct) : acc_struct(&_acc_struct) {}
#endif
};

struct Binding {
    eBindTarget trg;
    uint16_t loc = 0;
    uint16_t offset = 0;
    uint16_t size = 0;
    OpaqueHandle handle;

    Binding() = default;
    Binding(eBindTarget _trg, int _loc, OpaqueHandle _handle)
        : trg(_trg), loc(_loc), handle(_handle) {}
    Binding(eBindTarget _trg, int _loc, size_t _offset, OpaqueHandle _handle)
        : trg(_trg), loc(_loc), offset(uint16_t(_offset)), handle(_handle) {}
    Binding(eBindTarget _trg, int _loc, size_t _offset, size_t _size, OpaqueHandle _handle)
        : trg(_trg), loc(_loc), offset(uint16_t(_offset)), size(uint16_t(_size)), handle(_handle) {}
};
static_assert(sizeof(Binding) == sizeof(void *) + 8 + 8 + sizeof(void *), "!");

#if defined(REN_VK_BACKEND)
[[nodiscard]] VkDescriptorSet PrepareDescriptorSet(ApiContext *api_ctx, VkDescriptorSetLayout layout,
                                                   Span<const Binding> bindings, DescrMultiPoolAlloc *descr_alloc,
                                                   ILog *log);
#endif

void DispatchCompute(const Pipeline &comp_pipeline, Vec3u grp_count, Span<const Binding> bindings,
                     const void *uniform_data, int uniform_data_len, DescrMultiPoolAlloc *descr_alloc, ILog *log);
void DispatchComputeIndirect(const Pipeline &comp_pipeline, const Buffer &indir_buf, uint32_t indir_buf_offset,
                             Span<const Binding> bindings, const void *uniform_data, int uniform_data_len,
                             DescrMultiPoolAlloc *descr_alloc, ILog *log);
} // namespace Ren