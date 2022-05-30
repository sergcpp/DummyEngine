#pragma once

#include "Buffer.h"
#include "Fwd.h"
#include "Program.h"
#include "RastState.h"
#include "Span.h"
#include "TextureParams.h"

namespace Ren {
struct ApiContext;
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
    const RenderPass *render_pass_ = nullptr;
    SmallVector<eTexFormat, 4> color_formats_;
    eTexFormat depth_format_ = eTexFormat::Undefined;
    ProgramRef prog_;
    const VertexInput *vtx_input_ = nullptr;
#if defined(USE_VK_RENDER)
    VkPipelineLayout layout_ = VK_NULL_HANDLE;
    VkPipeline handle_ = VK_NULL_HANDLE;

    SmallVector<VkRayTracingShaderGroupCreateInfoKHR, 4> rt_shader_groups_;

    VkStridedDeviceAddressRegionKHR rgen_region_ = {};
    VkStridedDeviceAddressRegionKHR miss_region_ = {};
    VkStridedDeviceAddressRegionKHR hit_region_ = {};
    VkStridedDeviceAddressRegionKHR call_region_ = {};

    Buffer rt_sbt_buf_;
#endif

    void Destroy();

    bool Init(ApiContext *api_ctx, const RastState &rast_state, ProgramRef prog, const VertexInput *vtx_input,
              const RenderPass *render_pass, Span<const RenderTargetInfo> color_attachments,
              RenderTargetInfo depth_attachment, uint32_t subpass_index, ILog *log);

  public:
    Pipeline() = default;
    Pipeline(const Pipeline &rhs) = delete;
    Pipeline(Pipeline &&rhs) noexcept { (*this) = std::move(rhs); }
    ~Pipeline();

    Pipeline &operator=(const Pipeline &rhs) = delete;
    Pipeline &operator=(Pipeline &&rhs) noexcept;

    operator bool() const { return api_ctx_ != nullptr; }

    ePipelineType type() const { return type_; }
    const RastState &rast_state() const { return rast_state_; }
    const RenderPass *render_pass() const { return render_pass_; }
    const ProgramRef &prog() const { return prog_; }
    const VertexInput *vtx_input() const { return vtx_input_; }

    const SmallVectorImpl<eTexFormat> &color_formats() const { return color_formats_; }
    eTexFormat depth_format() const { return depth_format_; }


#if defined(USE_VK_RENDER)
    VkPipelineLayout layout() const { return layout_; }
    VkPipeline handle() const { return handle_; }

    const VkStridedDeviceAddressRegionKHR *rgen_table() const { return &rgen_region_; }
    const VkStridedDeviceAddressRegionKHR *miss_table() const { return &miss_region_; }
    const VkStridedDeviceAddressRegionKHR *hit_table() const { return &hit_region_; }
    const VkStridedDeviceAddressRegionKHR *call_table() const { return &call_region_; }
#endif

    bool Init(ApiContext *api_ctx, const RastState &rast_state, ProgramRef prog, const VertexInput *vtx_input,
              const RenderPass *render_pass, uint32_t subpass_index, ILog *log);
    bool Init(ApiContext *api_ctx, const RastState &rast_state, ProgramRef prog, const VertexInput *vtx_input,
              Span<const RenderTarget> color_attachments, RenderTarget depth_attachment, uint32_t subpass_index,
              ILog *log);
    bool Init(ApiContext *api_ctx, ProgramRef prog, ILog *log);
};

using PipelineRef = StrongRef<Pipeline, SparseArray<Pipeline>>;
using WeakPipelineRef = WeakRef<Pipeline, SparseArray<Pipeline>>;
using PipelineStorage = SparseArray<Pipeline>;

} // namespace Ren
