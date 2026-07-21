#pragma once

#include <cstdint>

#include "Fwd.h"
#include "math/Vec.h"
#include "utils/Span.h"

namespace Ren {
struct ApiContext;
class DescrMultiPoolAlloc;
class ILog;
struct BindlessDescriptors;

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
    BindlessDescriptors,
    _Count
};

#if defined(REN_GL_BACKEND)
uint32_t GLBindTarget(const ImageCold &img, int view);
extern int g_param_buf_binding;
#endif

struct OpaqueHandle {
    union {
        const BufferROHandle buf;
        const ImageROHandle img;
        const BindlessDescriptors *bindless;
#if defined(REN_VK_BACKEND)
        const AccStructROHandle acc_struct;
#endif
        void *ptr;
    };
    const SamplerROHandle sampler = {};
    int view_index = 0;
#ifndef NDEBUG
    bool readonly = false;
#endif

    OpaqueHandle(const BufferROHandle _buf, const int _view_index = 0) : buf(_buf), view_index(_view_index) {
#ifndef NDEBUG
        readonly = true;
#endif
    }
    OpaqueHandle(const BufferRWHandle _buf, const int _view_index = 0) : buf(_buf), view_index(_view_index) {}

    OpaqueHandle(const ImageROHandle _img, const int _view_index = 0) : img(_img), view_index(_view_index) {
#ifndef NDEBUG
        readonly = true;
#endif
    }
    OpaqueHandle(const ImageRWHandle _img, const int _view_index = 0) : img(_img), view_index(_view_index) {}
    OpaqueHandle(const ImageROHandle _img, const SamplerROHandle _sampler, const int _view_index = 0)
        : img(_img), sampler(_sampler), view_index(_view_index) {
#ifndef NDEBUG
        readonly = true;
#endif
    }
    OpaqueHandle(const ImageRWHandle _img, const SamplerROHandle _sampler, const int _view_index = 0)
        : img(_img), sampler(_sampler), view_index(_view_index) {}

    OpaqueHandle(const SamplerROHandle _sampler) : ptr(nullptr), sampler(_sampler) {}
    OpaqueHandle(const BindlessDescriptors &_bindless) : bindless(&_bindless) {}
#if defined(REN_VK_BACKEND)
    OpaqueHandle(const AccStructROHandle _acc_struct) : acc_struct(_acc_struct) {
#ifndef NDEBUG
        readonly = true;
#endif
    }
#endif
};

struct Binding {
    eBindTarget trg;
    uint16_t loc = 0;
    uint16_t offset = 0;
    uint16_t size = 0;
    OpaqueHandle handle;

    Binding(const eBindTarget _trg, const int _loc, OpaqueHandle _handle) : trg(_trg), loc(_loc), handle(_handle) {}
    Binding(const eBindTarget _trg, const int _loc, const size_t _offset, OpaqueHandle _handle)
        : trg(_trg), loc(_loc), offset(uint16_t(_offset)), handle(_handle) {}
    Binding(const eBindTarget _trg, const int _loc, const size_t _offset, const size_t _size, OpaqueHandle _handle)
        : trg(_trg), loc(_loc), offset(uint16_t(_offset)), size(uint16_t(_size)), handle(_handle) {}
};
#ifndef NDEBUG
static_assert(sizeof(Binding) == 8 + sizeof(void *) + 8 + 8);
#else
static_assert(sizeof(Binding) == 8 + sizeof(void *) + 8);
#endif

#if defined(REN_VK_BACKEND)
[[nodiscard]] VkDescriptorSet PrepareDescriptorSet(const ApiContext &api, const StoragesRef &storages,
                                                   VkDescriptorSetLayout layout, Span<const Binding> bindings,
                                                   DescrMultiPoolAlloc &descr_alloc, ILog *log);
#endif

void DispatchCompute(CommandBuffer cmd_buf, PipelineHandle pipeline, const StoragesRef &storages, Vec3u grp_count,
                     Span<const Binding> bindings, const void *uniform_data, int uniform_data_len,
                     DescrMultiPoolAlloc &descr_alloc, ILog *log);
void DispatchCompute(PipelineHandle pipeline, const StoragesRef &storages, Vec3u grp_count,
                     Span<const Binding> bindings, const void *uniform_data, int uniform_data_len,
                     DescrMultiPoolAlloc &descr_alloc, ILog *log);

void DispatchComputeIndirect(CommandBuffer cmd_buf, PipelineHandle pipeline, const StoragesRef &storages,
                             BufferROHandle indir_buf, uint32_t indir_buf_offset, Span<const Binding> bindings,
                             const void *uniform_data, int uniform_data_len, DescrMultiPoolAlloc &descr_alloc,
                             ILog *log);
} // namespace Ren