#pragma once

#include <climits>
#include <cstdint>

#include <unordered_set>
#include <variant>
#include <vector>

#include <Ren/Buffer.h>
#include <Ren/Common.h>
#include <Ren/Framebuffer.h>
#include <Ren/Fwd.h>
#include <Ren/HashMap32.h>
#include <Ren/Log.h>
#include <Ren/Pipeline.h>
#include <Ren/RastState.h>
#include <Ren/Sampler.h>
#include <Ren/SmallVector.h>
#include <Ren/SparseArray.h>
#include <Ren/Texture.h>
#include <Sys/MonoAlloc.h>

#include "FgResource.h"

namespace Eng {
class ShaderLoader;
class PrimDraw;

struct fg_node_slot_t {
    int16_t node_index;
    int16_t slot_index;
};
static_assert(sizeof(fg_node_slot_t) == 4, "!");

struct fg_node_range_t {
    int16_t first_write_node = SHRT_MAX;
    int16_t last_write_node = -1;
    int16_t first_read_node = SHRT_MAX;
    int16_t last_read_node = -1;

    bool has_writer() const { return first_write_node <= last_write_node; }
    bool has_reader() const { return first_read_node <= last_read_node; }
    bool is_used() const { return has_writer() || has_reader(); }

    bool can_alias() const {
        if (has_reader() && has_writer() && first_read_node <= first_write_node) {
            return false;
        }
        return true;
    }

    int last_used_node() const {
        int16_t last_node = 0;
        if (has_writer()) {
            last_node = std::max(last_node, last_write_node);
        }
        if (has_reader()) {
            last_node = std::max(last_node, last_read_node);
        }
        return last_node;
    }

    int first_used_node() const {
        int16_t first_node = SHRT_MAX;
        if (has_writer()) {
            first_node = std::min(first_node, first_write_node);
        }
        if (has_reader()) {
            first_node = std::min(first_node, first_read_node);
        }
        return first_node;
    }

    int length() const { return last_used_node() - first_used_node(); }
};

struct FgAllocRes {
    union {
        struct {
            uint8_t read_count;
            uint8_t write_count;
        };
        uint16_t _generation = 0;
    };

    std::string name;
    bool external = false;
    int alias_of = -1; // used in case of simple resource-to-resource aliasing
    int history_of = -1;
    int history_index = -1;

    Ren::eStageBits used_in_stages = {}, aliased_in_stages = {};
    Ren::SmallVector<fg_node_slot_t, 32> written_in_nodes;
    Ren::SmallVector<fg_node_slot_t, 32> read_in_nodes;
    Ren::SmallVector<FgResRef, 32> overlaps_with; // used in case of memory-level aliasing
    fg_node_range_t lifetime;
};

struct FgAllocBuf : public FgAllocRes {
    FgBufDesc desc;
    Ren::WeakBufRef ref;
    Ren::BufRef strong_ref;
};

struct FgAllocTex : public FgAllocRes {
    Ren::TexParams desc;
    Ren::WeakTexRef ref;
    Ren::TexRef strong_ref;
    // TODO: remove this once Texture/Texture2DArray will be merged into one class
    std::variant<std::monostate, const Ren::Texture2DArray *> _ref;
};

class FgNode;

class FgBuilder {
    Ren::Context &ctx_;
    ShaderLoader &sh_;
    PrimDraw &prim_draw_; // needed to clear rendertargets

    Ren::RastState rast_state_;

    Ren::SparseArray<FgAllocBuf> buffers_;
    Ren::HashMap32<std::string, uint16_t> name_to_buffer_;

    Ren::SparseArray<FgAllocTex> textures_;
    Ren::HashMap32<std::string, uint16_t> name_to_texture_;

    void InsertResourceTransitions(FgNode &node);
    void HandleResourceTransition(const FgResource &res, Ren::SmallVectorImpl<Ren::TransitionInfo> &res_transitions,
                                  Ren::eStageBits &src_stages, Ren::eStageBits &dst_stages);
    void CheckResourceStates(FgNode &node);

    bool DependsOn_r(int16_t dst_node, int16_t src_node);
    int16_t FindPreviousWrittenInNode(FgResRef handle);
    void FindPreviousReadInNodes(FgResRef handle, Ren::SmallVectorImpl<int16_t> &out_nodes);
    void TraverseNodeDependencies_r(FgNode *node, int recursion_depth, std::vector<FgNode *> &out_node_stack);

    void PrepareAllocResources();
    void PrepareResourceLifetimes();
    void AllocateNeededResources_Simple();
    bool AllocateNeededResources_MemHeaps();
    void ClearResources_Simple();
    void ClearResources_MemHeaps();
    void ReleaseMemHeaps();
    void BuildResourceLinkedLists();

    void ClearBuffer_AsTransfer(Ren::BufRef &buf, Ren::CommandBuffer cmd_buf);
    void ClearBuffer_AsStorage(Ren::BufRef &buf, Ren::CommandBuffer cmd_buf);

    void ClearImage_AsTransfer(Ren::TexRef &tex, Ren::CommandBuffer cmd_buf);
    void ClearImage_AsStorage(Ren::TexRef &tex, Ren::CommandBuffer cmd_buf);
    void ClearImage_AsTarget(Ren::TexRef &tex, Ren::CommandBuffer cmd_buf);

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

    std::vector<Ren::SmallVector<int, 4>> tex_alias_chains_, buf_alias_chains_;
    std::vector<Ren::MemHeap> memory_heaps_;

    Ren::PipelineRef pi_clear_image_[int(Ren::eTexFormat::_Count)];
    Ren::PipelineRef pi_clear_buffer_;

  public:
    FgBuilder(Ren::Context &ctx, ShaderLoader &sh, PrimDraw &prim_draw);
    ~FgBuilder() { Reset(); }

    Ren::Context &ctx() { return ctx_; }
    Ren::ILog *log();
    ShaderLoader &sh() { return sh_; }

    Ren::RastState &rast_state() { return rast_state_; }

    bool ready() const { return !reordered_nodes_.empty(); }

    FgNode &AddNode(std::string_view name);
    FgNode *FindNode(std::string_view name);
    FgNode *GetReorderedNode(const int i) { return reordered_nodes_[i]; }

    std::string GetResourceDebugInfo(const FgResource &res) const;
    void GetResourceFrameLifetime(const FgAllocBuf &b, uint16_t out_lifetime[2][2]) const;
    void GetResourceFrameLifetime(const FgAllocTex &t, uint16_t out_lifetime[2][2]) const;

    const Ren::SparseArray<FgAllocBuf> &buffers() const { return buffers_; }
    const Ren::SparseArray<FgAllocTex> &textures() const { return textures_; }

    template <typename T, class... Args> T *AllocNodeData(Args &&...args) {
        char *mem = alloc_.allocate(sizeof(T) + alignof(T));
        auto *new_data = reinterpret_cast<T *>(mem + alignof(T) - (uintptr_t(mem) % alignof(T)));
        alloc_.construct(new_data, std::forward<Args>(args)...);
        nodes_data_.push_back(std::unique_ptr<T, void (*)(void *)>(new_data, node_data_deleter<T>));
        return new_data;
    }

    FgResRef ReadBuffer(FgResRef handle, Ren::eResState desired_state, Ren::eStageBits stages, FgNode &node);
    FgResRef ReadBuffer(const Ren::WeakBufRef &ref, Ren::eResState desired_state, Ren::eStageBits stages, FgNode &node,
                        int slot_index = -1);

    FgResRef ReadTexture(FgResRef handle, Ren::eResState desired_state, Ren::eStageBits stages, FgNode &node);
    FgResRef ReadTexture(std::string_view name, Ren::eResState desired_state, Ren::eStageBits stages, FgNode &node);
    FgResRef ReadTexture(const Ren::WeakTexRef &ref, Ren::eResState desired_state, Ren::eStageBits stages,
                         FgNode &node);
    FgResRef ReadTexture(const Ren::Texture2DArray *ref, Ren::eResState desired_state, Ren::eStageBits stages,
                         FgNode &node);

    FgResRef ReadHistoryTexture(FgResRef handle, Ren::eResState desired_state, Ren::eStageBits stages, FgNode &node);
    FgResRef ReadHistoryTexture(std::string_view name, Ren::eResState desired_state, Ren::eStageBits stages,
                                FgNode &node);

    FgResRef WriteBuffer(FgResRef handle, Ren::eResState desired_state, Ren::eStageBits stages, FgNode &node);
    FgResRef WriteBuffer(std::string_view name, const FgBufDesc &desc, Ren::eResState desired_state,
                         Ren::eStageBits stages, FgNode &node);
    FgResRef WriteBuffer(const Ren::WeakBufRef &ref, Ren::eResState desired_state, Ren::eStageBits stages,
                         FgNode &node);

    FgResRef WriteTexture(FgResRef handle, Ren::eResState desired_state, Ren::eStageBits stages, FgNode &node);
    FgResRef WriteTexture(std::string_view name, Ren::eResState desired_state, Ren::eStageBits stages, FgNode &node);
    FgResRef WriteTexture(std::string_view name, const Ren::TexParams &p, Ren::eResState desired_state,
                          Ren::eStageBits stages, FgNode &node);
    FgResRef WriteTexture(const Ren::WeakTexRef &ref, Ren::eResState desired_state, Ren::eStageBits stages,
                          FgNode &node, int slot_index = -1);
    FgResRef WriteTexture(const Ren::Texture2DArray *ref, Ren::eResState desired_state, Ren::eStageBits stages,
                          FgNode &node);

    FgResRef MakeTextureResource(const Ren::WeakTexRef &ref);

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