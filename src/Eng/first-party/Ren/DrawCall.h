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
class Texture;

enum class eBindTarget : uint16_t {
    Tex,
    TexSampled,
    Sampler,
    UBuf,
    UTBuf,
    SBufRO,
    SBufRW,
    STBufRO,
    STBufRW,
    ImageRO,
    ImageRW,
    AccStruct,
    _Count
};

#if defined(REN_GL_BACKEND)
uint32_t GLBindTarget(const Texture &tex, int view);
extern int g_param_buf_binding;
#endif

struct OpaqueHandle {
    union {
        const Texture *tex;
        const Buffer *buf;
#if defined(REN_VK_BACKEND)
        const AccStructureVK *acc_struct;
#endif
        void *ptr;
    };
    const Sampler *sampler = nullptr;
    int view_index = 0;

    OpaqueHandle() = default;
    OpaqueHandle(const Texture &_tex, int _view_index = 0) : tex(&_tex), view_index(_view_index) {}
    OpaqueHandle(const Texture &_tex, const Sampler &_sampler, int _view_index = 0)
        : tex(&_tex), sampler(&_sampler), view_index(_view_index) {}
    OpaqueHandle(const Buffer &_buf, int _view_index = 0) : buf(&_buf), view_index(_view_index) {}
    OpaqueHandle(const Sampler &_sampler) : ptr(nullptr), sampler(&_sampler) {}
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
    Binding(eBindTarget _trg, int _loc, OpaqueHandle _handle) : trg(_trg), loc(_loc), handle(_handle) {}
    Binding(eBindTarget _trg, int _loc, size_t _offset, OpaqueHandle _handle)
        : trg(_trg), loc(_loc), offset(uint16_t(_offset)), handle(_handle) {}
    Binding(eBindTarget _trg, int _loc, size_t _offset, size_t _size, OpaqueHandle _handle)
        : trg(_trg), loc(_loc), offset(uint16_t(_offset)), size(uint16_t(_size)), handle(_handle) {}
};
static_assert(sizeof(Binding) == sizeof(void *) + 8 + 8 + sizeof(void *));

#if defined(REN_VK_BACKEND)
[[nodiscard]] VkDescriptorSet PrepareDescriptorSet(ApiContext *api_ctx, VkDescriptorSetLayout layout,
                                                   Span<const Binding> bindings, DescrMultiPoolAlloc *descr_alloc,
                                                   ILog *log);
#endif

void DispatchCompute(CommandBuffer cmd_buf, const Pipeline &comp_pipeline, Vec3u grp_count,
                     Span<const Binding> bindings, const void *uniform_data, int uniform_data_len,
                     DescrMultiPoolAlloc *descr_alloc, ILog *log);
void DispatchCompute(const Pipeline &comp_pipeline, Vec3u grp_count, Span<const Binding> bindings,
                     const void *uniform_data, int uniform_data_len, DescrMultiPoolAlloc *descr_alloc, ILog *log);

void DispatchComputeIndirect(CommandBuffer cmd_buf, const Pipeline &comp_pipeline, const Buffer &indir_buf,
                             uint32_t indir_buf_offset, Span<const Binding> bindings, const void *uniform_data,
                             int uniform_data_len, DescrMultiPoolAlloc *descr_alloc, ILog *log);
void DispatchComputeIndirect(const Pipeline &comp_pipeline, const Buffer &indir_buf, uint32_t indir_buf_offset,
                             Span<const Binding> bindings, const void *uniform_data, int uniform_data_len,
                             DescrMultiPoolAlloc *descr_alloc, ILog *log);
} // namespace Ren