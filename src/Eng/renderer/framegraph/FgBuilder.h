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
#include <Ren/Image.h>
#include <Ren/Log.h>
#include <Ren/Pipeline.h>
#include <Ren/RastState.h>
#include <Ren/Sampler.h>
#include <Ren/utils/HashMap32.h>
#include <Ren/utils/SmallVector.h>
#include <Ren/utils/SparseArray.h>
#include <Sys/MonoAlloc.h>

#include "FgResource.h"

namespace Eng {
class FramebufferPool;
class ShaderLoader;
class PrimDraw;

struct fg_node_slot_t {
    int16_t node_index;
    int16_t slot_index;
};
static_assert(sizeof(fg_node_slot_t) == 4);

struct fg_node_range_t {
    int16_t first_write_node = SHRT_MAX;
    int16_t last_write_node = -1;
    int16_t first_read_node = SHRT_MAX;
    int16_t last_read_node = -1;

    [[nodiscard]] bool has_writer() const { return first_write_node <= last_write_node; }
    [[nodiscard]] bool has_reader() const { return first_read_node <= last_read_node; }
    [[nodiscard]] bool is_used() const { return has_writer() || has_reader(); }

    [[nodiscard]] bool can_alias() const {
        if (has_reader() && has_writer() && first_read_node <= first_write_node) {
            return false;
        }
        return true;
    }

    [[nodiscard]] int last_used_node() const {
        int16_t last_node = 0;
        if (has_writer()) {
            last_node = std::max(last_node, last_write_node);
        }
        if (has_reader()) {
            last_node = std::max(last_node, last_read_node);
        }
        return last_node;
    }

    [[nodiscard]] int first_used_node() const {
        int16_t first_node = SHRT_MAX;
        if (has_writer()) {
            first_node = std::min(first_node, first_write_node);
        }
        if (has_reader()) {
            first_node = std::min(first_node, first_read_node);
        }
        return first_node;
    }

    [[nodiscard]] int length() const { return last_used_node() - first_used_node(); }
};

struct FgBufDesc {
    Ren::eBufType type;
    uint32_t size;
    Ren::SmallVector<Ren::eFormat, 1> views;
};

inline bool operator==(const FgBufDesc &lhs, const FgBufDesc &rhs) {
    return lhs.size == rhs.size && lhs.type == rhs.type;
}

struct FgImageViewDesc {
    Ren::eFormat format;
    int mip_level;
    int mip_count;
    int base_layer;
    int layer_count;
};

struct FgImgDesc : Ren::ImgParams {
    Ren::SmallVector<FgImageViewDesc, 1> views;
};

struct FgAllocRes {
    Ren::String name;
    bool external = false;
    int alias_of = -1; // used in case of simple resource-to-resource aliasing
    uint16_t history_of = 0xffff;
    mutable uint16_t history_index = 0xffff;

    Ren::Bitmask<Ren::eStage> used_in_stages, aliased_in_stages;
    Ren::SmallVector<fg_node_slot_t, 32> written_in_nodes;
    mutable Ren::SmallVector<fg_node_slot_t, 32> read_in_nodes;
    Ren::SmallVector<FgResRef, 32> overlaps_with; // used in case of memory-level aliasing
    fg_node_range_t lifetime;
};

struct FgAllocBufMain {
    Ren::BufferHandle handle;
};

struct FgAllocBufCold : public FgAllocRes {
    FgBufDesc desc;
};

struct FgAllocImgMain {
    Ren::ImageHandle handle_to_use; // swapped with its history every frame
    Ren::ImageHandle handle_to_own; // stable handle
};

struct FgAllocImgCold : public FgAllocRes {
    FgImgDesc desc;
};

using FgImgROHandle = Ren::Handle<FgAllocImgMain, Ren::ROTag>;
using FgImgRWHandle = Ren::Handle<FgAllocImgMain, Ren::RWTag>;

class FgNode;

class FgContext {
  protected:
    Ren::Context &ctx_;
    ShaderLoader &sh_;

    mutable Ren::RastState rast_state_;

    Ren::SparseDualStorage<FgAllocBufMain, FgAllocBufCold> buffers_;
    Ren::HashMap32<Ren::String, uint16_t> name_to_buffer_;
    Ren::SparseDualStorage<FgAllocImgMain, FgAllocImgCold> images_;
    Ren::HashMap32<Ren::String, uint16_t> name_to_image_;

    std::unique_ptr<FramebufferPool> framebuffers_;

    FgContext(ShaderLoader &sh);
    ~FgContext();

  public:
    [[nodiscard]] Ren::Context &ren_ctx() const { return ctx_; }
    [[nodiscard]] ShaderLoader &sh() const { return sh_; }
    [[nodiscard]] Ren::RastState &rast_state() const { return rast_state_; }

    [[nodiscard]] const Ren::StoragesRef &storages() const;
    [[nodiscard]] FramebufferPool *framebuffers() const { return framebuffers_.get(); }

    [[nodiscard]] Ren::CommandBuffer cmd_buf() const;
    [[nodiscard]] Ren::ILog *log() const;
    [[nodiscard]] Ren::DescrMultiPoolAlloc &descr_alloc() const;

    [[nodiscard]] int backend_frame() const;

    [[nodiscard]] Ren::BufferROHandle AccessROBuffer(FgBufROHandle handle) const;
    [[nodiscard]] Ren::ImageROHandle AccessROImage(FgImgROHandle handle) const;

    [[nodiscard]] Ren::BufferHandle AccessRWBuffer(FgBufRWHandle handle) const;
    [[nodiscard]] Ren::ImageHandle AccessRWImage(FgImgRWHandle handle) const;

    Ren::FramebufferHandle FindOrCreateFramebuffer(Ren::RenderPassROHandle render_pass,
                                                   const Ren::FramebufferAttachment &depth,
                                                   const Ren::FramebufferAttachment &stencil,
                                                   Ren::Span<const Ren::FramebufferAttachment> color_attachments) const;
    Ren::FramebufferHandle FindOrCreateFramebuffer(Ren::RenderPassROHandle render_pass, Ren::ImageRWHandle depth,
                                                   Ren::ImageRWHandle stencil,
                                                   Ren::Span<const Ren::ImageRWHandle> color_attachments) const;
};

class FgBuilder : public FgContext {
    PrimDraw &prim_draw_; // used to clear rendertargets

    void InsertResourceTransitions(FgNode &node);
    void HandleResourceTransition(const FgResource &res, Ren::SmallVectorImpl<Ren::TransitionInfo> &res_transitions,
                                  Ren::Bitmask<Ren::eStage> &src_stages, Ren::Bitmask<Ren::eStage> &dst_stages);
    void CheckResourceStates(FgNode &node);

    FgBufRWHandle FindBuffer(const Ren::String &name) {
        const uint16_t *index = name_to_buffer_.Find(name);
        if (!index) {
            return {};
        }
        return FgBufRWHandle{*index, buffers_.GetGeneration(*index)};
    }
    FgImgRWHandle FindImage(const Ren::String &name) {
        const uint16_t *index = name_to_image_.Find(name);
        if (!index) {
            return {};
        }
        return FgImgRWHandle{*index, images_.GetGeneration(*index)};
    }

    bool DependsOn_r(int16_t dst_node, int16_t src_node);
    int16_t FindPreviousWrittenInNode(const FgResource &res);
    int16_t FindPreviousWrittenInNode(FgBufRWHandle res);
    int16_t FindPreviousWrittenInNode(FgImgRWHandle res);
    void FindPreviousReadInNodes(const FgResource &res, Ren::SmallVectorImpl<int16_t> &out_nodes);
    void TraverseNodeDependencies_r(FgNode *node, int recursion_depth, std::vector<FgNode *> &out_node_stack);

    void PrepareAllocResources();
    void PrepareResourceLifetimes();
    void AllocateNeededResources_Simple();
    bool AllocateNeededResources_MemHeaps();
    void ClearResources_Simple();
    void ClearResources_MemHeaps();
    void ReleaseMemHeaps();
    void BuildResourceLinkedLists();

    void ClearBuffer_AsTransfer(Ren::BufferHandle buf, Ren::CommandBuffer cmd_buf);
    void ClearBuffer_AsStorage(Ren::BufferHandle buf, Ren::CommandBuffer cmd_buf);

    void ClearImage_AsTransfer(Ren::ImageHandle img, Ren::CommandBuffer cmd_buf);
    void ClearImage_AsStorage(Ren::ImageHandle img, Ren::CommandBuffer cmd_buf);
    void ClearImage_AsTarget(Ren::ImageHandle img, Ren::CommandBuffer cmd_buf);

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

    std::vector<Ren::SmallVector<uint32_t, 4>> img_alias_chains_, buf_alias_chains_;
    std::vector<Ren::MemHeap> memory_heaps_;

    Ren::PipelineHandle pi_clear_image_[3][int(Ren::eFormat::_Count)];
    Ren::PipelineHandle pi_clear_buffer_;

  public:
    FgBuilder(ShaderLoader &sh, PrimDraw &prim_draw);
    ~FgBuilder() { Reset(); }

    bool ready() const { return !reordered_nodes_.empty(); }

    FgNode &AddNode(std::string_view name, eFgQueueType queue = eFgQueueType::Graphics);
    FgNode *FindNode(std::string_view name);
    FgNode *GetReorderedNode(const int i) { return reordered_nodes_[i]; }

    std::string GetResourceDebugInfo(const FgResource &res) const;
    void GetResourceFrameLifetime(const FgAllocBufCold &b, uint16_t out_lifetime[2][2]) const;
    void GetResourceFrameLifetime(const FgAllocImgCold &i, uint16_t out_lifetime[2][2]) const;

    const Ren::SparseDualStorage<FgAllocBufMain, FgAllocBufCold> &buffers() const { return buffers_; }
    const Ren::HashMap32<Ren::String, uint16_t> &name_to_buffer() const { return name_to_buffer_; }
    const Ren::SparseDualStorage<FgAllocImgMain, FgAllocImgCold> &images() const { return images_; }
    const Ren::HashMap32<Ren::String, uint16_t> &name_to_image() const { return name_to_image_; }

    template <typename T, class... Args> T *AllocTempData(Args &&...args) {
        char *mem = alloc_.allocate(sizeof(T) + alignof(T) - 1);
        auto *new_data = reinterpret_cast<T *>(mem + (alignof(T) - uintptr_t(mem) % alignof(T)) % alignof(T));
        alloc_.construct(new_data, std::forward<Args>(args)...);
        nodes_data_.push_back(std::unique_ptr<T, void (*)(void *)>(new_data, node_data_deleter<T>));
        return new_data;
    }

    FgBufROHandle ReadBuffer(FgBufROHandle handle, Ren::eResState desired_state, Ren::Bitmask<Ren::eStage> stages,
                             FgNode &node, int slot_index = -1);

    FgImgROHandle ReadImage(FgImgROHandle handle, Ren::eResState desired_state, Ren::Bitmask<Ren::eStage> stages,
                            FgNode &node);

    FgImgROHandle ReadHistoryImage(FgImgROHandle handle, Ren::eResState desired_state, Ren::Bitmask<Ren::eStage> stages,
                                   FgNode &node);
    FgImgROHandle ReadHistoryImage(std::string_view name, Ren::eResState desired_state,
                                   Ren::Bitmask<Ren::eStage> stages, FgNode &node);

    FgBufRWHandle WriteBuffer(std::string_view name, const FgBufDesc &desc, Ren::eResState desired_state,
                              Ren::Bitmask<Ren::eStage> stages, FgNode &node);
    FgBufRWHandle WriteBuffer(FgBufRWHandle handle, Ren::eResState desired_state, Ren::Bitmask<Ren::eStage> stages,
                              FgNode &node);

    FgImgRWHandle WriteImage(std::string_view name, const FgImgDesc &desc, Ren::eResState desired_state,
                             Ren::Bitmask<Ren::eStage> stages, FgNode &node);
    FgImgRWHandle WriteImage(FgImgRWHandle handle, Ren::eResState desired_state, Ren::Bitmask<Ren::eStage> stages,
                             FgNode &node, int slot_index = -1);

    FgBufRWHandle ImportResource(Ren::BufferHandle handle);
    FgImgRWHandle ImportResource(Ren::ImageHandle handle);

    void Reset();
    void Compile(Ren::Span<const std::variant<FgBufRWHandle, FgImgRWHandle>> backbuffer_sources = {});
    void Execute();

    struct node_timing_t {
        std::string_view name;
        int query_beg = -1, query_end = -1;
    };

    Ren::SmallVector<node_timing_t, 256> node_timings_[Ren::MaxFramesInFlight];
};
} // namespace Eng