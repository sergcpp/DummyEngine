#include "RenderPass.h"

Ren::RenderPass &Ren::RenderPass::operator=(RenderPass &&rhs) noexcept = default;

bool Ren::RenderPass::Init(ApiContext *api_ctx, const RenderTargetInfo &_depth_rt, Span<const RenderTargetInfo> _rts,
                           ILog *log) {
    depth_rt = _depth_rt;
    color_rts.assign(std::begin(_rts), std::end(_rts));
    return true;
}

void Ren::RenderPass::Destroy() {}

bool Ren::RenderPass::Setup(ApiContext *api_ctx, const RenderTarget &_depth_rt, Span<const RenderTarget> _color_rts,
                            ILog *log) {
    return true;
}

bool Ren::RenderPass::Setup(ApiContext *api_ctx, const RenderTargetInfo &_depth_rt, Span<const RenderTargetInfo> _color_rts,
                            ILog *log) {
    return true;
}
