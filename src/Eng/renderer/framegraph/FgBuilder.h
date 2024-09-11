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

#include "FgResource.h"

namespace Eng {
class ShaderLoader;

struct fg_write_node_t {
    int16_t node_index;
    int16_t slot_index;
};
static_assert(sizeof(fg_write_node_t) == 4, "!");

struct FgAllocBuf {
    union {
        struct {
            uint8_t read_count;
            uint8_t write_count;
        };
        uint16_t _generation;
    };

    Ren::eStageBits used_in_stages;
    Ren::SmallVector<fg_write_node_t, 32> written_in_nodes;
    Ren::SmallVector<fg_write_node_t, 32> read_in_nodes;

    std::string name;
    bool external = false;
    FgBufDesc desc;
    Ren::WeakBufferRef ref;
    Ren::BufferRef strong_ref;
    Ren::Tex1DRef tbos[4];
};

struct FgAllocTex {
    union {
        struct {
            uint8_t read_count;
            uint8_t write_count;
        };
        uint16_t _generation;
    };

    Ren::eStageBits used_in_stages;
    Ren::SmallVector<fg_write_node_t, 32> written_in_nodes;
    Ren::SmallVector<fg_write_node_t, 32> read_in_nodes;

    std::string name;
    bool transient = false; // unused for now
    bool external = false;
    int alias_of = -1;
    int history_of = -1;
    int history_index = -1;
    Ren::Tex2DParams desc;
    Ren::WeakTex2DRef ref;
    Ren::Tex2DRef strong_ref;
    const Ren::Texture2DArray *arr = nullptr;
    const Ren::Texture3D *tex3d = nullptr;
};

class FgNode;

class FgBuilder {
    Ren::Context &ctx_;
    Eng::ShaderLoader &sh_;

    Ren::RastState rast_state_;

    Ren::SparseArray<FgAllocBuf> buffers_;
    Ren::HashMap32<std::string, uint16_t> name_to_buffer_;

    Ren::SparseArray<FgAllocTex> textures_;
    Ren::HashMap32<std::string, uint16_t> name_to_texture_;

    void AllocateNeededResources(FgNode &node);
    void InsertResourceTransitions(FgNode &node);
    void HandleResourceTransition(const FgResource &res, Ren::SmallVectorImpl<Ren::TransitionInfo> &res_transitions,
                                  Ren::eStageBits &src_stages, Ren::eStageBits &dst_stages);
    void CheckResourceStates(FgNode &node);

    bool DependsOn_r(int16_t dst_node, int16_t src_node);
    int16_t FindPreviousWrittenInNode(FgResRef handle);
    void FindPreviousReadInNodes(FgResRef handle, Ren::SmallVectorImpl<int16_t> &out_nodes);
    void TraverseNodeDependencies_r(FgNode *node, int recursion_depth, std::vector<FgNode *> &out_node_stack);

    void PrepareAllocResources();
    void BuildAliases();
    void BuildResourceLinkedLists();

    static const int AllocBufSize = 4 * 1024 * 1024;
    std::unique_ptr<char[]> alloc_buf_;
    Sys::MonoAlloc<char> alloc_;
    std::vector<FgNode *> nodes_;
    std::vector<FgNode *> reordered_nodes_;
    std::vector<std::unique_ptr<void, void (*)(void *)>> nodes_data_;

    template <typename T> static void node_data_deleter(void *_ptr) {
        T *ptr = reinterpret_cast<T *>(_ptr);
        ptr->~T();
        // no deallocation is needed
    }

    std::vector<std::vector<int>> alias_chains_;

  public:
    FgBuilder(Ren::Context &ctx, Eng::ShaderLoader &sh)
        : ctx_(ctx), sh_(sh), alloc_buf_(new char[AllocBufSize]), alloc_(alloc_buf_.get(), AllocBufSize) {}

    Ren::Context &ctx() { return ctx_; }
    Ren::ILog *log();
    Eng::ShaderLoader &sh() { return sh_; }

    Ren::RastState &rast_state() { return rast_state_; }

    bool ready() const { return !reordered_nodes_.empty(); }

    FgNode &AddNode(std::string_view name);
    FgNode *FindNode(std::string_view name);
    FgNode *GetReorderedNode(const int i) { return reordered_nodes_[i]; }

    std::string GetResourceDebugInfo(const FgResource &res) const;

    template <typename T, class... Args> T *AllocNodeData(Args &&...args) {
        char *mem = alloc_.allocate(sizeof(T) + alignof(T));
        auto *new_data = reinterpret_cast<T *>(mem + alignof(T) - (uintptr_t(mem) % alignof(T)));
        alloc_.construct(new_data, std::forward<Args>(args)...);
        nodes_data_.push_back(std::unique_ptr<T, void (*)(void *)>(new_data, node_data_deleter<T>));
        return new_data;
    }

    FgResRef ReadBuffer(FgResRef handle, Ren::eResState desired_state, Ren::eStageBits stages, FgNode &node);
    FgResRef ReadBuffer(const Ren::WeakBufferRef &ref, Ren::eResState desired_state, Ren::eStageBits stages,
                        FgNode &node, int slot_index = -1);
    FgResRef ReadBuffer(const Ren::WeakBufferRef &ref, const Ren::WeakTex1DRef &tbo, Ren::eResState desired_state,
                        Ren::eStageBits stages, FgNode &node);

    FgResRef ReadTexture(FgResRef handle, Ren::eResState desired_state, Ren::eStageBits stages, FgNode &node);
    FgResRef ReadTexture(std::string_view name, Ren::eResState desired_state, Ren::eStageBits stages, FgNode &node);
    FgResRef ReadTexture(const Ren::WeakTex2DRef &ref, Ren::eResState desired_state, Ren::eStageBits stages,
                         FgNode &node);
    FgResRef ReadTexture(const Ren::Texture2DArray *ref, Ren::eResState desired_state, Ren::eStageBits stages,
                         FgNode &node);
    FgResRef ReadTexture(const Ren::Texture3D *ref, Ren::eResState desired_state, Ren::eStageBits stages, FgNode &node);

    FgResRef ReadHistoryTexture(FgResRef handle, Ren::eResState desired_state, Ren::eStageBits stages, FgNode &node);
    FgResRef ReadHistoryTexture(std::string_view name, Ren::eResState desired_state, Ren::eStageBits stages,
                                FgNode &node);

    FgResRef WriteBuffer(FgResRef handle, Ren::eResState desired_state, Ren::eStageBits stages, FgNode &node);
    FgResRef WriteBuffer(std::string_view name, const FgBufDesc &desc, Ren::eResState desired_state,
                         Ren::eStageBits stages, FgNode &node);
    FgResRef WriteBuffer(const Ren::WeakBufferRef &ref, Ren::eResState desired_state, Ren::eStageBits stages,
                         FgNode &node);

    FgResRef WriteTexture(FgResRef handle, Ren::eResState desired_state, Ren::eStageBits stages, FgNode &node);
    FgResRef WriteTexture(std::string_view name, Ren::eResState desired_state, Ren::eStageBits stages, FgNode &node);
    FgResRef WriteTexture(std::string_view name, const Ren::Tex2DParams &p, Ren::eResState desired_state,
                          Ren::eStageBits stages, FgNode &node);
    FgResRef WriteTexture(const Ren::WeakTex2DRef &ref, Ren::eResState desired_state, Ren::eStageBits stages,
                          FgNode &node, int slot_index = -1);
    FgResRef WriteTexture(const Ren::Texture2DArray *ref, Ren::eResState desired_state, Ren::eStageBits stages,
                          FgNode &node);

    FgResRef MakeTextureResource(const Ren::WeakTex2DRef &ref);

    FgAllocBuf &GetReadBuffer(FgResRef handle);
    FgAllocTex &GetReadTexture(FgResRef handle);

    FgAllocBuf &GetWriteBuffer(FgResRef handle);
    FgAllocTex &GetWriteTexture(FgResRef handle);

    void Reset();
    void Compile(Ren::Span<const FgResRef> backbuffer_sources = {});
    void Execute();

    Ren::SmallVector<Ren::SamplerRef, 64> temp_samplers;

    struct node_timing_t {
        std::string_view name;
        int query_beg, query_end;
    };

    Ren::SmallVector<node_timing_t, 256> node_timings_[Ren::MaxFramesInFlight];
};
} // namespace Eng