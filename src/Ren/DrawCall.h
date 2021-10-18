#pragma once

#include <cstdint>

#include "MVec.h"

namespace Ren {
class Buffer;
class DescrMultiPoolAlloc;
class ILog;
class Pipeline;
class ProbeStorage;
class Texture1D;
class Texture2D;

enum class eBindTarget : uint16_t { Tex2D, Tex2DMs, TexCubeArray, TBuf, UBuf, SBuf, Image, _Count };

#if defined(USE_GL_RENDER)
uint32_t GLBindTarget(eBindTarget binding);
extern int g_param_buf_binding;
#endif

struct OpaqueHandle {
    union {
        const Texture2D *tex;
        const Buffer *buf;
        const Texture1D *tex_buf;
        const ProbeStorage *cube_arr;
    };
    OpaqueHandle() = default;
    OpaqueHandle(const Ren::Texture2D &_tex) : tex(&_tex) {}
    OpaqueHandle(const Ren::Buffer &_buf) : buf(&_buf) {}
    OpaqueHandle(const Ren::Texture1D &_tex) : tex_buf(&_tex) {}
    OpaqueHandle(const ProbeStorage &_probes) : cube_arr(&_probes) {}
};

struct Binding {
    eBindTarget trg;
    uint16_t loc = 0;
    uint16_t offset = 0;
    uint16_t size = 0;
    OpaqueHandle handle;

    Binding() = default;
    Binding(eBindTarget _trg, uint16_t _loc, OpaqueHandle _handle) : trg(_trg), loc(_loc), handle(_handle) {}
    Binding(eBindTarget _trg, uint16_t _loc, size_t _offset, size_t _size, OpaqueHandle _handle)
        : trg(_trg), loc(_loc), offset(uint16_t(_offset)), size(uint16_t(_size)), handle(_handle) {}
};
static_assert(sizeof(Binding) == sizeof(void *) + 8, "!");

void DispatchCompute(const Pipeline &comp_pipeline, Vec3u grp_count, const Binding bindings[], const int bindings_count,
                     const void *uniform_data, int uniform_data_len, DescrMultiPoolAlloc *descr_alloc, ILog *log);
void DispatchComputeIndirect(const Pipeline &comp_pipeline, const Buffer &indir_buf, const Binding bindings[],
                             const int bindings_count, const void *uniform_data, int uniform_data_len,
                             DescrMultiPoolAlloc *descr_alloc, ILog *log);
} // namespace Ren