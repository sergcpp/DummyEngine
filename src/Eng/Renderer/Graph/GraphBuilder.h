#pragma once

#include <cstdint>

#include <unordered_set>
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
#include <Sys/MonoAlloc.h>

#include "Resource.h"

class ShaderLoader;

class RpBuilder;

struct rp_write_pass_t {
    int16_t pass_index;
    int16_t slot_index;
};
static_assert(sizeof(rp_write_pass_t) == 4, "!");

struct RpAllocBuf {
    union {
        struct {
            uint8_t read_count;
            uint8_t write_count;
        };
        uint16_t _generation;
    };

    Ren::eStageBits used_in_stages;
    Ren::SmallVector<rp_write_pass_t, 32> written_in_passes;
    Ren::SmallVector<rp_write_pass_t, 32> read_in_passes;

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
    Ren::SmallVector<rp_write_pass_t, 32> written_in_passes;
    Ren::SmallVector<rp_write_pass_t, 32> read_in_passes;

    std::string name;
    bool transient = false; // unused for now
    int alias_of = -1;
    int history_of = -1;
    int history_index = -1;
    Ren::Tex2DParams desc;
    Ren::WeakTex2DRef ref;
    Ren::Tex2DRef strong_ref;
};

class RpSubpass;

class RpBuilder {
    Ren::Context &ctx_;
    ShaderLoader &sh_;

    Ren::RastState rast_state_;

    Ren::SparseArray<RpAllocBuf> buffers_;
    Ren::HashMap32<std::string, uint16_t> name_to_buffer_;

    Ren::SparseArray<RpAllocTex> textures_;
    Ren::HashMap32<std::string, uint16_t> name_to_texture_;

    void AllocateNeededResources(RpSubpass &pass);
    void InsertResourceTransitions(RpSubpass &pass);
    void HandleResourceTransition(const RpResource &res, Ren::SmallVectorImpl<Ren::TransitionInfo> &res_transitions,
                                  Ren::eStageBits &src_stages, Ren::eStageBits &dst_stages);
    void CheckResourceStates(RpSubpass &pass);

    bool DependsOn_r(int16_t dst_pass, int16_t src_pass);
    int16_t FindPreviousWrittenInPass(RpResRef handle);
    void TraversePassDependencies(const RpSubpass *pass, int recursion_depth, std::vector<RpSubpass *> &out_pass_stack);

    void PrepareAllocResources();
    void BuildRenderPasses();
    void BuildTransientResources();
    void BuildAliases();
    void BuildResourceLinkedLists();

    static const int AllocBufSize = 4 * 1024 * 1024;
    std::unique_ptr<char[]> alloc_buf_;
    Sys::MonoAlloc<char> alloc_;
    std::vector<RpSubpass *> subpasses_;
    std::vector<RpSubpass *> reordered_subpasses_;
    std::vector<std::unique_ptr<void, void (*)(void *)>> subpass_data_;

    template <typename T> static void pass_data_deleter(void *_ptr) {
        T *ptr = reinterpret_cast<T *>(_ptr);
        ptr->~T();
        // no deallocation is needed
    }

    struct RenderPass {
        int subpass_beg, subpass_end;
    };
    std::vector<RenderPass> render_passes_;
    std::vector<std::vector<int>> alias_chains_;

  public:
    RpBuilder(Ren::Context &ctx, ShaderLoader &sh)
        : ctx_(ctx), sh_(sh), alloc_buf_(new char[AllocBufSize]), alloc_(alloc_buf_.get(), AllocBufSize) {}

    Ren::Context &ctx() { return ctx_; }
    Ren::ILog *log();
    ShaderLoader &sh() { return sh_; }

    Ren::RastState &rast_state() { return rast_state_; }

    bool ready() const { return !reordered_subpasses_.empty(); }

    RpSubpass &AddPass(const char *name);
    RpSubpass *FindPass(const char *name);

    template <typename T, class... Args> T *AllocPassData(Args &&...args) {
        auto *new_data = reinterpret_cast<T *>(alloc_.allocate(sizeof(T)));
        alloc_.construct(new_data, std::forward<Args>(args)...);
        subpass_data_.push_back(std::unique_ptr<T, void (*)(void *)>(new_data, pass_data_deleter<T>));
        return new_data;
    }

    RpResRef ReadBuffer(RpResRef handle, Ren::eResState desired_state, Ren::eStageBits stages, RpSubpass &pass);
    RpResRef ReadBuffer(const Ren::WeakBufferRef &ref, Ren::eResState desired_state, Ren::eStageBits stages,
                        RpSubpass &pass);

    RpResRef ReadTexture(RpResRef handle, Ren::eResState desired_state, Ren::eStageBits stages, RpSubpass &pass);
    RpResRef ReadTexture(const char *name, Ren::eResState desired_state, Ren::eStageBits stages, RpSubpass &pass);
    RpResRef ReadTexture(const Ren::WeakTex2DRef &ref, Ren::eResState desired_state, Ren::eStageBits stages,
                         RpSubpass &pass);

    RpResRef ReadHistoryTexture(RpResRef handle, Ren::eResState desired_state, Ren::eStageBits stages, RpSubpass &pass);

    RpResRef WriteBuffer(RpResRef handle, Ren::eResState desired_state, Ren::eStageBits stages, RpSubpass &pass);
    RpResRef WriteBuffer(const char *name, const RpBufDesc &desc, Ren::eResState desired_state, Ren::eStageBits stages,
                         RpSubpass &pass);
    RpResRef WriteBuffer(const Ren::WeakBufferRef &ref, Ren::eResState desired_state, Ren::eStageBits stages,
                         RpSubpass &pass);

    RpResRef WriteTexture(RpResRef handle, Ren::eResState desired_state, Ren::eStageBits stages, RpSubpass &pass);
    RpResRef WriteTexture(const char *name, Ren::eResState desired_state, Ren::eStageBits stages, RpSubpass &pass);
    RpResRef WriteTexture(const char *name, const Ren::Tex2DParams &p, Ren::eResState desired_state,
                          Ren::eStageBits stages, RpSubpass &pass);
    RpResRef WriteTexture(const Ren::WeakTex2DRef &ref, Ren::eResState desired_state, Ren::eStageBits stages,
                          RpSubpass &pass, int slot_index = -1);

    RpAllocBuf &GetReadBuffer(RpResRef handle);
    RpAllocTex &GetReadTexture(RpResRef handle);

    RpAllocBuf &GetWriteBuffer(RpResRef handle);
    RpAllocTex &GetWriteTexture(RpResRef handle);

    void Reset();
    void Compile(const RpResRef backbuffer_sources[] = nullptr, int backbuffer_sources_count = 0);
    void Execute();

    Ren::SmallVector<Ren::SamplerRef, 64> temp_samplers;

    struct pass_timing_t {
        std::string name;
        int query_beg, query_end;
    };

    Ren::SmallVector<pass_timing_t, 256> pass_timings_[Ren::MaxFramesInFlight];
};
