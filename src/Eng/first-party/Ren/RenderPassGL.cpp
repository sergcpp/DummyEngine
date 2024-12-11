#include "RenderPass.h"

Ren::RenderPass &Ren::RenderPass::operator=(RenderPass &&rhs) noexcept = default;

void Ren::RenderPass::Destroy() {}

bool Ren::RenderPass::Setup(ApiContext *api_ctx, RenderTarget depth_rt, Span<const RenderTarget> color_rts, ILog *log) {
    return true;
}

bool Ren::RenderPass::Setup(ApiContext *api_ctx, RenderTargetInfo depth_rt, Span<const RenderTargetInfo> color_rts,
                            ILog *log) {
    return true;
}