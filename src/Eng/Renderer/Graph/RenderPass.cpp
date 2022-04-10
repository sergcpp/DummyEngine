#include "RenderPass.h"

#include "GraphBuilder.h"

RpResRef RenderPass::AddTransferInput(const Ren::WeakBufferRef &buf) {
    return builder_.ReadBuffer(buf, Ren::eResState::CopySrc, Ren::eStageBits::Transfer, *this);
}

RpResRef RenderPass::AddTransferOutput(const char *name, const RpBufDesc &desc) {
    return builder_.WriteBuffer(name, desc, Ren::eResState::CopyDst, Ren::eStageBits::Transfer, *this);
}

RpResRef RenderPass::AddTransferOutput(const Ren::WeakBufferRef &buf) {
    return builder_.WriteBuffer(buf, Ren::eResState::CopyDst, Ren::eStageBits::Transfer, *this);
}

RpResRef RenderPass::AddTransferImageInput(const Ren::WeakTex2DRef &tex) {
    return builder_.ReadTexture(tex, Ren::eResState::CopySrc, Ren::eStageBits::Transfer, *this);
}

RpResRef RenderPass::AddTransferImageInput(const RpResRef handle) {
    return builder_.ReadTexture(handle, Ren::eResState::CopySrc, Ren::eStageBits::Transfer, *this);
}

RpResRef RenderPass::AddTransferImageOutput(const char *name, const Ren::Tex2DParams &params) {
    return builder_.WriteTexture(name, params, Ren::eResState::CopyDst, Ren::eStageBits::Transfer, *this);
}

RpResRef RenderPass::AddTransferImageOutput(const Ren::WeakTex2DRef &tex) {
    return builder_.WriteTexture(tex, Ren::eResState::CopyDst, Ren::eStageBits::Transfer, *this);
}

RpResRef RenderPass::AddTransferImageOutput(const RpResRef handle) {
    return builder_.WriteTexture(handle, Ren::eResState::CopyDst, Ren::eStageBits::Transfer, *this);
}

RpResRef RenderPass::AddStorageReadonlyInput(RpResRef handle, Ren::eStageBits stages) {
    return builder_.ReadBuffer(handle, Ren::eResState::ShaderResource, stages, *this);
}

RpResRef RenderPass::AddStorageReadonlyInput(const Ren::WeakBufferRef &buf, const Ren::eStageBits stages) {
    return builder_.ReadBuffer(buf, Ren::eResState::ShaderResource, stages, *this);
}

RpResRef RenderPass::AddStorageOutput(const char *name, const RpBufDesc &desc, const Ren::eStageBits stages) {
    return builder_.WriteBuffer(name, desc, Ren::eResState::UnorderedAccess, stages, *this);
}

RpResRef RenderPass::AddStorageOutput(const RpResRef handle, const Ren::eStageBits stages) {
    return builder_.WriteBuffer(handle, Ren::eResState::UnorderedAccess, stages, *this);
}

RpResRef RenderPass::AddStorageOutput(const Ren::WeakBufferRef &buf, const Ren::eStageBits stages) {
    return builder_.WriteBuffer(buf, Ren::eResState::UnorderedAccess, stages, *this);
}

RpResRef RenderPass::AddStorageImageOutput(const char *name, const Ren::Tex2DParams &params,
                                              const Ren::eStageBits stages) {
    return builder_.WriteTexture(name, params, Ren::eResState::UnorderedAccess, stages, *this);
}

RpResRef RenderPass::AddStorageImageOutput(const RpResRef handle, const Ren::eStageBits stages) {
    return builder_.WriteTexture(handle, Ren::eResState::UnorderedAccess, stages, *this);
}

RpResRef RenderPass::AddStorageImageOutput(const Ren::WeakTex2DRef &tex, const Ren::eStageBits stages) {
    return builder_.WriteTexture(tex, Ren::eResState::UnorderedAccess, stages, *this);
}

RpResRef RenderPass::AddColorOutput(const char *name, const Ren::Tex2DParams &params) {
    return builder_.WriteTexture(name, params, Ren::eResState::RenderTarget, Ren::eStageBits::ColorAttachment, *this);
}

RpResRef RenderPass::AddColorOutput(const RpResRef handle) {
    return builder_.WriteTexture(handle, Ren::eResState::RenderTarget, Ren::eStageBits::ColorAttachment, *this);
}

RpResRef RenderPass::AddColorOutput(const Ren::WeakTex2DRef &tex) {
    return builder_.WriteTexture(tex, Ren::eResState::RenderTarget, Ren::eStageBits::ColorAttachment, *this);
}

RpResRef RenderPass::AddColorOutput(const char *name) {
    return builder_.WriteTexture(name, Ren::eResState::RenderTarget, Ren::eStageBits::ColorAttachment, *this);
}

RpResRef RenderPass::AddDepthOutput(const char *name, const Ren::Tex2DParams &params) {
    return builder_.WriteTexture(name, params, Ren::eResState::DepthWrite, Ren::eStageBits::DepthAttachment, *this);
}

RpResRef RenderPass::AddUniformBufferInput(const RpResRef handle, const Ren::eStageBits stages) {
    return builder_.ReadBuffer(handle, Ren::eResState::UniformBuffer, stages, *this);
}

RpResRef RenderPass::AddTextureInput(const RpResRef handle, const Ren::eStageBits stages) {
    return builder_.ReadTexture(handle, Ren::eResState::ShaderResource, stages, *this);
}

RpResRef RenderPass::AddTextureInput(const Ren::WeakTex2DRef &tex, Ren::eStageBits stages) {
    return builder_.ReadTexture(tex, Ren::eResState::ShaderResource, stages, *this);
}

RpResRef RenderPass::AddTextureInput(const char *name, Ren::eStageBits stages) {
    return builder_.ReadTexture(name, Ren::eResState::ShaderResource, stages, *this);
}

RpResRef RenderPass::AddCustomTextureInput(const RpResRef handle, const Ren::eResState desired_state,
                                              const Ren::eStageBits stages) {
    return builder_.ReadTexture(handle, desired_state, stages, *this);
}

RpResRef RenderPass::AddVertexBufferInput(const RpResRef handle) {
    return builder_.ReadBuffer(handle, Ren::eResState::VertexBuffer, Ren::eStageBits::VertexInput, *this);
}

RpResRef RenderPass::AddVertexBufferInput(const Ren::WeakBufferRef &buf) {
    return builder_.ReadBuffer(buf, Ren::eResState::VertexBuffer, Ren::eStageBits::VertexInput, *this);
}

RpResRef RenderPass::AddIndexBufferInput(const RpResRef handle) {
    return builder_.ReadBuffer(handle, Ren::eResState::IndexBuffer, Ren::eStageBits::VertexInput, *this);
}

RpResRef RenderPass::AddIndexBufferInput(const Ren::WeakBufferRef &buf) {
    return builder_.ReadBuffer(buf, Ren::eResState::IndexBuffer, Ren::eStageBits::VertexInput, *this);
}

RpResRef RenderPass::AddIndirectBufferInput(const RpResRef handle) {
    return builder_.ReadBuffer(handle, Ren::eResState::IndirectArgument, Ren::eStageBits::DrawIndirect, *this);
}

RpResRef RenderPass::AddIndirectBufferInput(const Ren::WeakBufferRef &buf) {
    return builder_.ReadBuffer(buf, Ren::eResState::IndirectArgument, Ren::eStageBits::DrawIndirect, *this);
}

RpResRef RenderPass::AddASBuildReadonlyInput(const RpResRef handle) {
    return builder_.ReadBuffer(handle, Ren::eResState::BuildASRead, Ren::eStageBits::AccStructureBuild, *this);
}

RpResRef RenderPass::AddASBuildOutput(const Ren::WeakBufferRef &buf) {
    return builder_.WriteBuffer(buf, Ren::eResState::BuildASWrite, Ren::eStageBits::AccStructureBuild, *this);
}

RpResRef RenderPass::AddASBuildOutput(const char *name, const RpBufDesc &desc) {
    return builder_.WriteBuffer(name, desc, Ren::eResState::BuildASWrite, Ren::eStageBits::AccStructureBuild, *this);
}