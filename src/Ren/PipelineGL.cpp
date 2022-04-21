#include "Pipeline.h"

Ren::Pipeline::~Pipeline() = default;

Ren::Pipeline &Ren::Pipeline::operator=(Pipeline &&rhs) noexcept = default;

void Ren::Pipeline::Destroy() {}

bool Ren::Pipeline::Init(ApiContext *api_ctx, const RastState &rast_state, ProgramRef prog,
                         const VertexInput *vtx_input, const RenderPass *render_pass, uint32_t subpass_index,
                         ILog *log) {
    type_ = ePipelineType::Graphics;
    rast_state_ = rast_state;
    prog_ = std::move(prog);
    vtx_input_ = vtx_input;
    return true;
}

bool Ren::Pipeline::Init(ApiContext *api_ctx, ProgramRef prog, ILog *log) {
    Destroy();

    type_ = ePipelineType::Compute;
    prog_ = std::move(prog);

    return true;
}