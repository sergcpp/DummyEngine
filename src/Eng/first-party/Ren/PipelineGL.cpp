#include "Pipeline.h"

Ren::Pipeline::~Pipeline() = default;

Ren::Pipeline &Ren::Pipeline::operator=(Pipeline &&rhs) noexcept = default;

void Ren::Pipeline::Destroy() {}

bool Ren::Pipeline::Init(ApiContext *api_ctx, const RastState &rast_state, ProgramRef prog, VertexInputRef vtx_input,
                         RenderPassRef render_pass, const uint32_t, ILog *log) {
    type_ = ePipelineType::Graphics;
    rast_state_ = rast_state;
    render_pass_ = std::move(render_pass);
    prog_ = std::move(prog);
    vtx_input_ = std::move(vtx_input);
    return true;
}

bool Ren::Pipeline::Init(ApiContext *api_ctx, ProgramRef prog, ILog *log, const int) {
    Destroy();

    type_ = ePipelineType::Compute;
    prog_ = std::move(prog);

    return true;
}