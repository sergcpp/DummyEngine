#include "RenderPass.h"

Ren::RenderPass &Ren::RenderPass::operator=(RenderPass &&rhs) noexcept = default;

void Ren::RenderPass::Destroy() {}

bool Ren::RenderPass::Setup(ApiContext *api_ctx, const RenderTarget color_rts[], int color_rts_count,
                            RenderTarget depth_rt, ILog *log) {
    return true;
}

bool Ren::RenderPass::Setup(ApiContext *api_ctx, const RenderTargetInfo rts[], int color_rts_count,
                            RenderTargetInfo depth_rt, ILog *log) {
    return true;
}