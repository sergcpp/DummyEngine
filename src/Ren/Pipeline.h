#pragma once

#include "Fwd.h"
#include "Program.h"
#include "RastState.h"

namespace Ren {
struct ApiContext;
class Pipeline : public RefCounter {
    ApiContext *api_ctx_ = nullptr;
    RastState rast_state_;
    const RenderPass *render_pass_;
    ProgramRef prog_;
    const VertexInput *vtx_input_;
#if defined(USE_VK_RENDER)
    VkPipelineLayout layout_ = VK_NULL_HANDLE;
    VkPipeline handle_ = VK_NULL_HANDLE;
#endif

    void Destroy();

  public:
    Pipeline() = default;
    Pipeline(const Pipeline &rhs) = delete;
    Pipeline(Pipeline &&rhs) noexcept { (*this) = std::move(rhs); }
    ~Pipeline();

    Pipeline &operator=(const Pipeline &rhs) = delete;
    Pipeline &operator=(Pipeline &&rhs) noexcept;

    operator bool() const { return api_ctx_ != nullptr; }

    const RastState &rast_state() const { return rast_state_; }
    const RenderPass *render_pass() const { return render_pass_;  }
    const ProgramRef &prog() const { return prog_; }
    const VertexInput *vtx_input() const { return vtx_input_; }

#if defined(USE_VK_RENDER)
    VkPipelineLayout layout() const { return layout_; }
    VkPipeline handle() const { return handle_; }
#endif

    bool Init(ApiContext *api_ctx, const RastState &rast_state, ProgramRef prog, const VertexInput *vtx_input,
              const RenderPass *render_pass, ILog *log);
};

using PipelineRef = StrongRef<Pipeline, SparseArray<Pipeline>>;
using WeakPipelineRef = WeakRef<Pipeline, SparseArray<Pipeline>>;
using PipelineStorage = SparseArray<Pipeline>;

} // namespace Ren
