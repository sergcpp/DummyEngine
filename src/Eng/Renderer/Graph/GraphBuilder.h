#pragma once

#include <cstdint>

#include <vector>

#include <Ren/Fwd.h>
#include <Ren/HashMap32.h>
#include <Ren/Log.h>
#include <Ren/SparseArray.h>

class ShaderLoader;

enum class eRpResType : uint8_t { Undefined, Buffer, Texture };

struct RpResource {
    eRpResType type = eRpResType::Undefined;
    union {
        struct {
            uint8_t read_count;
            uint8_t write_count;
        };
        uint16_t _generation = 0;
    };
    uint32_t index = 0xffffffff;

    RpResource() = default;
    RpResource(eRpResType _type, uint16_t __generation, uint32_t _index)
        : type(_type), _generation(__generation), index(_index) {}
};

const int MaxInOutCountPerPass = 16;

class RpBuilder;

class RenderPassBase {
  protected:
    friend class RpBuilder;

    RpResource input_[MaxInOutCountPerPass];
    int input_count_ = 0;
    RpResource output_[MaxInOutCountPerPass];
    int output_count_ = 0;

    uint32_t ref_count_ = 0;

  public:
    virtual ~RenderPassBase() {}

    virtual void Execute(RpBuilder &builder) = 0;

    virtual const char *name() const = 0;

    RenderPassBase *p_next = nullptr;
};

struct RpBufDesc {
    uint32_t size;
    Ren::eBufferType type;
    Ren::eBufferAccessType access;
    Ren::eBufferAccessFreq freq;
};

struct RpAllocBuf {
    union {
        struct {
            uint8_t read_count;
            uint8_t write_count;
        };
        uint16_t _generation;
    };
    Ren::BufferRef ref;
    Ren::Tex1DRef tbos[4];
};

struct RpAllocTex {
    union {
        struct {
            uint8_t read_count;
            uint8_t write_count;
        };
        uint16_t _generation;
    };
    Ren::Tex2DRef ref;
};

class RpBuilder {
    Ren::Context &ctx_;
    ShaderLoader &sh_;

    Ren::SparseArray<RpAllocBuf> buffers_;
    Ren::HashMap32<const char *, uint32_t> name_to_buffer_;

    Ren::SparseArray<RpAllocTex> textures_;
    Ren::HashMap32<const char *, uint32_t> name_to_texture_;

  public:
    RpBuilder(Ren::Context &ctx, ShaderLoader &sh) : ctx_(ctx), sh_(sh) {}

    Ren::Context &ctx() { return ctx_; }
    Ren::ILog *log();
    ShaderLoader &sh() { return sh_; }

    RpResource CreateBuffer(const char *name, const RpBufDesc &desc);
    RpResource CreateTexture(const char *name, const Ren::Tex2DParams &p);

    RpResource ReadBuffer(RpResource handle);
    RpResource ReadBuffer(const char *name);
    RpResource ReadTexture(RpResource handle);

    RpResource WriteBuffer(RpResource handle, RenderPassBase &pass);
    RpResource WriteTexture(RpResource handle, RenderPassBase &pass);

    RpAllocBuf &GetReadBuffer(RpResource handle);
    RpAllocTex &GetReadTexture(RpResource handle);

    RpAllocBuf &GetWriteBuffer(RpResource handle);
    RpAllocTex &GetWriteTexture(RpResource handle);

    void Reset();
    void Compile(RenderPassBase *first_pass);
    void Execute(RenderPassBase *first_pass);
};
