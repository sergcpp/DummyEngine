#pragma once

#include <cstdint>

#include <vector>

#include <Ren/Buffer.h>
#include <Ren/Common.h>
#include <Ren/Framebuffer.h>
#include <Ren/Fwd.h>
#include <Ren/HashMap32.h>
#include <Ren/Log.h>
#include <Ren/RastState.h>
#include <Ren/Sampler.h>
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
    Ren::eResState desired_state = Ren::eResState::Undefined;
    Ren::eStageBits stages = Ren::eStageBits::None;
    uint32_t index = 0xffffffff;

    Ren::eStageBits src_stages = Ren::eStageBits::None;
    Ren::eStageBits dst_stages = Ren::eStageBits::None;

    RpResource *next_use = nullptr;

    RpResource() = default;
    RpResource(eRpResType _type, uint16_t __generation, Ren::eResState _desired_state, Ren::eStageBits _stages,
               uint32_t _index)
        : type(_type), _generation(__generation), desired_state(_desired_state), stages(_stages), index(_index) {}

    operator bool() { return type != eRpResType::Undefined; }

    static bool LessThanTypeAndIndex(const RpResource &lhs, const RpResource &rhs) {
        if (lhs.type != rhs.type) {
            return lhs.type < rhs.type;
		}
        return lhs.index < rhs.index;
    }
};

class RpBuilder;

class RenderPassBase {
  private:
    friend class RpBuilder;

    Ren::SmallVector<RpResource, 16> input_;
    Ren::SmallVector<RpResource, 16> output_;
    uint32_t ref_count_ = 0;

  public:
    virtual ~RenderPassBase() {}

    virtual void Execute(RpBuilder &builder) = 0;

    virtual const char *name() const = 0;

    RenderPassBase *p_next = nullptr;
};

struct RpBufDesc {
    Ren::eBufType type;
    uint32_t size;
};

inline bool operator==(const RpBufDesc &lhs, const RpBufDesc &rhs) {
    return lhs.size == rhs.size && lhs.type == rhs.type;
}

struct RpAllocBuf {
    union {
        struct {
            uint8_t read_count;
            uint8_t write_count;
        };
        uint16_t _generation;
    };

    Ren::eStageBits used_in_stages;

    std::string name;
    RpBufDesc desc;
    Ren::WeakBufferRef ref;
    Ren::BufferRef strong_ref;
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

    Ren::eStageBits used_in_stages;

    std::string name;
    Ren::Tex2DParams desc;
    Ren::WeakTex2DRef ref;
    Ren::Tex2DRef strong_ref;
};

class RpBuilder {
    Ren::Context &ctx_;
    ShaderLoader &sh_;

    Ren::RastState rast_state_;

    Ren::SparseArray<RpAllocBuf> buffers_;
    Ren::HashMap32<std::string, uint32_t> name_to_buffer_;

    Ren::SparseArray<RpAllocTex> textures_;
    Ren::HashMap32<std::string, uint32_t> name_to_texture_;

    void AllocateNeededResources(RenderPassBase *pass);
    void InsertResourceTransitions(RenderPassBase *pass);
    void HandleResourceTransition(const RpResource &res, Ren::SmallVectorImpl<Ren::TransitionInfo> &res_transitions,
                                  Ren::eStageBits &src_stages, Ren::eStageBits &dst_stages);

  public:
    RpBuilder(Ren::Context &ctx, ShaderLoader &sh) : ctx_(ctx), sh_(sh) {}

    Ren::Context &ctx() { return ctx_; }
    Ren::ILog *log();
    ShaderLoader &sh() { return sh_; }

    Ren::RastState &rast_state() { return rast_state_; }

    RpResource ReadBuffer(RpResource handle, Ren::eResState desired_state, Ren::eStageBits stages,
                          RenderPassBase &pass);
    RpResource ReadBuffer(const char *name, Ren::eResState desired_state, Ren::eStageBits stages, RenderPassBase &pass);
    RpResource ReadBuffer(const Ren::WeakBufferRef &ref, Ren::eResState desired_state, Ren::eStageBits stages,
                          RenderPassBase &pass);

    RpResource ReadTexture(RpResource handle, Ren::eResState desired_state, Ren::eStageBits stages,
                           RenderPassBase &pass);
    RpResource ReadTexture(const char *name, Ren::eResState desired_state, Ren::eStageBits stages,
                           RenderPassBase &pass);
    RpResource ReadTexture(const Ren::WeakTex2DRef &ref, Ren::eResState desired_state, Ren::eStageBits stages,
                           RenderPassBase &pass);

    RpResource WriteBuffer(RpResource handle, Ren::eResState desired_state, Ren::eStageBits stages,
                           RenderPassBase &pass);
    RpResource WriteBuffer(const char *name, Ren::eResState desired_state, Ren::eStageBits stages,
                           RenderPassBase &pass);
    RpResource WriteBuffer(const char *name, const RpBufDesc &desc, Ren::eResState desired_state,
                           Ren::eStageBits stages, RenderPassBase &pass);
    RpResource WriteBuffer(const Ren::WeakBufferRef &ref, Ren::eResState desired_state, Ren::eStageBits stages,
                           RenderPassBase &pass);

    RpResource WriteTexture(RpResource handle, Ren::eResState desired_state, Ren::eStageBits stages,
                            RenderPassBase &pass);
    RpResource WriteTexture(const char *name, Ren::eResState desired_state, Ren::eStageBits stages,
                            RenderPassBase &pass);
    RpResource WriteTexture(const char *name, const Ren::Tex2DParams &p, Ren::eResState desired_state,
                            Ren::eStageBits stages, RenderPassBase &pass);
    RpResource WriteTexture(const Ren::WeakTex2DRef &ref, Ren::eResState desired_state, Ren::eStageBits stages,
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
