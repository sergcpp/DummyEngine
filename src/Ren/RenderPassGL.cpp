#include "RenderPass.h"

Ren::RenderPass &Ren::RenderPass::operator=(RenderPass &&rhs) noexcept = default;

void Ren::RenderPass::Destroy() {}

bool Ren::RenderPass::Setup(ApiContext *api_ctx, Span<const RenderTarget> color_rts, RenderTarget depth_rt, ILog *log) {
    return true;
}

bool Ren::RenderPass::Setup(ApiContext *api_ctx, Span<const RenderTargetInfo> color_rts, RenderTargetInfo depth_rt,
                            ILog *log) {
    return true;
}