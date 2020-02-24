#pragma once

#include <cstdint>

#include <vector>

#include <Ren/Fwd.h>
#include <Ren/HashMap32.h>
#include <Ren/Log.h>
#include <Ren/SparseArray.h>

class ShaderLoader;

namespace Graph {
enum class eResourceType : uint8_t { Undefined, Buffer };

struct ResourceHandle {
    eResourceType type = eResourceType::Undefined;
    union {
        struct {
            uint8_t read_count;
            uint8_t write_count;
        };
        uint16_t _generation = 0;
    };
    uint32_t index = 0xffffffff;

    ResourceHandle() = default;
    ResourceHandle(eResourceType _type, uint16_t __generation, uint32_t _index)
        : type(_type), _generation(__generation), index(_index) {}
};

const int MaxInOutCountPerPass = 16;

class RpBuilder;

class RenderPassBase {
  protected:
    friend class RpBuilder;

    ResourceHandle input_[MaxInOutCountPerPass];
    int input_count_ = 0;
    ResourceHandle output_[MaxInOutCountPerPass];
    int output_count_ = 0;

    uint32_t ref_count_ = 0;

  public:
    virtual ~RenderPassBase() {}

    virtual void Execute(RpBuilder &builder) = 0;

    virtual const char* name() const = 0;

    RenderPassBase *p_next = nullptr;
};

struct BufferDesc {
    uint32_t size;
    Ren::eBufferType type;
    Ren::eBufferAccessType access;
    Ren::eBufferAccessFreq freq;
};

struct AllocatedBuffer {
    union {
        struct {
            uint8_t read_count;
            uint8_t write_count;
        };
        uint16_t _generation;
    };
    Ren::BufferRef ref;
};

class RpBuilder {
    Ren::Context &ctx_;
    ShaderLoader &sh_;

    Ren::SparseArray<AllocatedBuffer> buffers_;
    Ren::HashMap32<const char *, uint32_t> name_to_buffer_;

  public:
    RpBuilder(Ren::Context &ctx, ShaderLoader &sh) : ctx_(ctx), sh_(sh) {}

    Ren::Context &ctx() { return ctx_; }
    Ren::ILog* log();
    ShaderLoader &sh() { return sh_; }

    ResourceHandle CreateBuffer(const char *name, const BufferDesc &desc);
    ResourceHandle ReadBuffer(ResourceHandle handle);
    ResourceHandle WriteBuffer(ResourceHandle handle, RenderPassBase &pass);

    AllocatedBuffer &GetReadBuffer(ResourceHandle handle);
    AllocatedBuffer &GetWriteBuffer(ResourceHandle handle);

    void Reset();
    void Compile(RenderPassBase *first_pass);
    void Execute(RenderPassBase *first_pass);
};
} // namespace Graph