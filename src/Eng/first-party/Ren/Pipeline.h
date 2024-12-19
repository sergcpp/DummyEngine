#pragma once

#include "Buffer.h"
#include "Fwd.h"
#include "Program.h"
#include "RastState.h"
#include "RenderPass.h"
#include "Span.h"
#include "TextureParams.h"
#include "VertexInput.h"

namespace Ren {
struct ApiContext;
class RenderPass;
struct RenderTarget;
struct RenderTargetInfo;

enum class ePipelineType : uint8_t { Undefined, Graphics, Compute, Raytracing };

struct DispatchIndirectCommand {
    uint32_t num_groups_x;
    uint32_t num_groups_y;
    uint32_t num_groups_z;
};

struct TraceRaysIndirectCommand {
    uint32_t width;
    uint32_t height;
    uint32_t depth;
};

class Pipeline : public RefCounter {
    ApiContext *api_ctx_ = nullptr;
    ePipelineType type_ = ePipelineType::Undefined;
    RastState rast_state_;
    RenderPassRef render_pass_;
    ProgramRef prog_;
    VertexInputRef vtx_input_;
#if defined(REN_VK_BACKEND)
    VkPipelineLayout layout_ = {};
    VkPipeline handle_ = {};

    SmallVector<VkRayTracingShaderGroupCreateInfoKHR, 4> rt_shader_groups_;

    VkStridedDeviceAddressRegionKHR rgen_region_ = {};
    VkStridedDeviceAddressRegionKHR miss_region_ = {};
    VkStridedDeviceAddressRegionKHR hit_region_ = {};
    VkStridedDeviceAddressRegionKHR call_region_ = {};

    Buffer rt_sbt_buf_;
#endif

    void Destroy();

  public:
    Pipeline() = default;
    Pipeline(ApiContext *api_ctx, ProgramRef prog, ILog *log, int subgroup_size = -1) {
        Init(api_ctx, std::move(prog), log, subgroup_size);
    }
    Pipeline(ApiContext *api_ctx, const RastState &rast_state, ProgramRef prog, VertexInputRef vtx_input,
             RenderPassRef render_pass, uint32_t subpass_index, ILog *log) {
        Init(api_ctx, rast_state, prog, vtx_input, render_pass, subpass_index, log);
    }
    Pipeline(const Pipeline &rhs) = delete;
    Pipeline(Pipeline &&rhs) noexcept { (*this) = std::move(rhs); }
    ~Pipeline();

    Pipeline &operator=(const Pipeline &rhs) = delete;
    Pipeline &operator=(Pipeline &&rhs) noexcept;

    operator bool() const { return api_ctx_ != nullptr; }

    bool operator==(const Pipeline &rhs) const {
        return vtx_input_ == rhs.vtx_input_ && prog_ == rhs.prog_ && render_pass_ == rhs.render_pass_ &&
               rast_state_ == rhs.rast_state_;
    }
    bool operator!=(const Pipeline &rhs) const {
        return vtx_input_ != rhs.vtx_input_ || prog_ != rhs.prog_ || render_pass_ != rhs.render_pass_ ||
               rast_state_ != rhs.rast_state_;
    }
    bool operator<(const Pipeline &rhs) const {
        return LessThan(rhs.rast_state_, rhs.prog_, rhs.vtx_input_, rhs.render_pass_);
    }

    bool Equals(const RastState &rast_state, const ProgramRef &prog, const VertexInputRef &vtx_input,
                const RenderPassRef &render_pass) const {
        return vtx_input_ == vtx_input && prog_ == prog && render_pass_ == render_pass && rast_state_ == rast_state;
    }
    bool LessThan(const RastState &rast_state, const ProgramRef &prog, const VertexInputRef &vtx_input,
                  const RenderPassRef &render_pass) const {
        if (vtx_input_ < vtx_input) {
            return true;
        } else if (vtx_input_ == vtx_input) {
            if (prog_ < prog) {
                return true;
            } else if (prog_ == prog) {
                if (render_pass_ < render_pass) {
                    return true;
                } else if (render_pass_ == render_pass) {
                    return rast_state_ < rast_state;
                }
            }
        }
        return false;
    }

    ePipelineType type() const { return type_; }
    const RastState &rast_state() const { return rast_state_; }
    const RenderPassRef &render_pass() const { return render_pass_; }
    const ProgramRef &prog() const { return prog_; }
    const VertexInputRef &vtx_input() const { return vtx_input_; }

#if defined(REN_VK_BACKEND)
    VkPipelineLayout layout() const { return layout_; }
    VkPipeline handle() const { return handle_; }

    const VkStridedDeviceAddressRegionKHR *rgen_table() const { return &rgen_region_; }
    const VkStridedDeviceAddressRegionKHR *miss_table() const { return &miss_region_; }
    const VkStridedDeviceAddressRegionKHR *hit_table() const { return &hit_region_; }
    const VkStridedDeviceAddressRegionKHR *call_table() const { return &call_region_; }
#endif

    bool Init(ApiContext *api_ctx, const RastState &rast_state, ProgramRef prog, VertexInputRef vtx_input,
              RenderPassRef render_pass, uint32_t subpass_index, ILog *log);
    bool Init(ApiContext *api_ctx, ProgramRef prog, ILog *log, int subgroup_size = -1);
};

using PipelineRef = StrongRef<Pipeline, SortedStorage<Pipeline>>;
using WeakPipelineRef = WeakRef<Pipeline, SortedStorage<Pipeline>>;
using PipelineStorage = SortedStorage<Pipeline>;
} // namespace Ren
