#pragma once

#include <cstdint>

#include <vector>

#include <Ren/Fwd.h>

#include <Ren/Buffer.h>
#include <Ren/HashMap32.h>
#include <Ren/Log.h>
#include <Ren/SmallVector.h>
#include <Ren/SparseArray.h>
#include <Ren/Texture.h>

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

    operator bool() {
        return type != eRpResType::Undefined;
    }
};

const int MaxInOutCountPerPass = 16;

class RpBuilder;

class RenderPassBase {
  private:
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

inline bool operator==(const RpBufDesc &lhs, const RpBufDesc &rhs) {
    return lhs.size == rhs.size && lhs.type == rhs.type && lhs.access == rhs.access &&
           lhs.freq == rhs.freq;
}

struct RpAllocBuf {
    union {
        struct {
            uint8_t read_count;
            uint8_t write_count;
        };
        uint16_t _generation;
    };
    std::string name;
    RpBufDesc desc;
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
    std::string name;
    Ren::Tex2DParams desc;
    Ren::Tex2DRef ref;
};

class RpBuilder {
    Ren::Context &ctx_;
    ShaderLoader &sh_;

    Ren::SparseArray<RpAllocBuf> buffers_;
    Ren::HashMap32<std::string, uint32_t> name_to_buffer_;

    Ren::SparseArray<RpAllocTex> textures_;
    Ren::HashMap32<std::string, uint32_t> name_to_texture_;

  public:
    RpBuilder(Ren::Context &ctx, ShaderLoader &sh) : ctx_(ctx), sh_(sh) {}

    Ren::Context &ctx() { return ctx_; }
    Ren::ILog *log();
    ShaderLoader &sh() { return sh_; }

    RpResource ReadBuffer(RpResource handle, RenderPassBase &pass);
    RpResource ReadBuffer(const char *name, RenderPassBase &pass);

    RpResource ReadTexture(RpResource handle, RenderPassBase &pass);
    RpResource ReadTexture(const char *name, RenderPassBase &pass);

    RpResource WriteBuffer(RpResource handle, RenderPassBase &pass);
    RpResource WriteBuffer(const char *name, RenderPassBase &pass);
    RpResource WriteBuffer(const char *name, const RpBufDesc &desc, RenderPassBase &pass);

    RpResource WriteTexture(RpResource handle, RenderPassBase &pass);
    RpResource WriteTexture(const char *name, RenderPassBase &pass);
    RpResource WriteTexture(const char *name, const Ren::Tex2DParams &p,
                            RenderPassBase &pass);

    RpAllocBuf &GetReadBuffer(RpResource handle);
    RpAllocTex &GetReadTexture(RpResource handle);

    RpAllocBuf &GetWriteBuffer(RpResource handle);
    RpAllocTex &GetWriteTexture(RpResource handle);

    void Reset();
    void Compile(RenderPassBase *first_pass);
    void Execute(RenderPassBase *first_pass);

    Ren::SmallVector<Ren::SamplerRef, 64> temp_samplers;
};
