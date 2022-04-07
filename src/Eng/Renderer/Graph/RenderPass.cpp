#include "RenderPass.h"

#include "GraphBuilder.h"

RpResource RenderPass::AddTransferInput(const Ren::WeakBufferRef &buf) {
    return builder_.ReadBuffer(buf, Ren::eResState::CopySrc, Ren::eStageBits::Transfer, *this);
}

RpResource RenderPass::AddTransferOutput(const char *name, const RpBufDesc &desc) {
    return builder_.WriteBuffer(name, desc, Ren::eResState::CopyDst, Ren::eStageBits::Transfer, *this);
}

RpResource RenderPass::AddTransferOutput(const Ren::WeakBufferRef &buf) {
    return builder_.WriteBuffer(buf, Ren::eResState::CopyDst, Ren::eStageBits::Transfer, *this);
}

RpResource RenderPass::AddTransferImageInput(const Ren::WeakTex2DRef &tex) {
    return builder_.ReadTexture(tex, Ren::eResState::CopySrc, Ren::eStageBits::Transfer, *this);
}

RpResource RenderPass::AddTransferImageInput(const RpResource handle) {
    return builder_.ReadTexture(handle, Ren::eResState::CopySrc, Ren::eStageBits::Transfer, *this);
}

RpResource RenderPass::AddTransferImageOutput(const char *name, const Ren::Tex2DParams &params) {
    return builder_.WriteTexture(name, params, Ren::eResState::CopyDst, Ren::eStageBits::Transfer, *this);
}

RpResource RenderPass::AddTransferImageOutput(const Ren::WeakTex2DRef &tex) {
    return builder_.WriteTexture(tex, Ren::eResState::CopyDst, Ren::eStageBits::Transfer, *this);
}

RpResource RenderPass::AddTransferImageOutput(const RpResource handle) {
    return builder_.WriteTexture(handle, Ren::eResState::CopyDst, Ren::eStageBits::Transfer, *this);
}

RpResource RenderPass::AddStorageReadonlyInput(RpResource handle, Ren::eStageBits stages) {
    return builder_.ReadBuffer(handle, Ren::eResState::ShaderResource, stages, *this);
}

RpResource RenderPass::AddStorageReadonlyInput(const Ren::WeakBufferRef &buf, const Ren::eStageBits stages) {
    return builder_.ReadBuffer(buf, Ren::eResState::ShaderResource, stages, *this);
}

RpResource RenderPass::AddStorageOutput(const char *name, const RpBufDesc &desc, const Ren::eStageBits stages) {
    return builder_.WriteBuffer(name, desc, Ren::eResState::UnorderedAccess, stages, *this);
}

RpResource RenderPass::AddStorageOutput(const RpResource handle, const Ren::eStageBits stages) {
    return builder_.WriteBuffer(handle, Ren::eResState::UnorderedAccess, stages, *this);
}

RpResource RenderPass::AddStorageOutput(const Ren::WeakBufferRef &buf, const Ren::eStageBits stages) {
    return builder_.WriteBuffer(buf, Ren::eResState::UnorderedAccess, stages, *this);
}

RpResource RenderPass::AddStorageImageOutput(const char *name, const Ren::Tex2DParams &params,
                                              const Ren::eStageBits stages) {
    return builder_.WriteTexture(name, params, Ren::eResState::UnorderedAccess, stages, *this);
}

RpResource RenderPass::AddStorageImageOutput(const RpResource handle, const Ren::eStageBits stages) {
    return builder_.WriteTexture(handle, Ren::eResState::UnorderedAccess, stages, *this);
}

RpResource RenderPass::AddStorageImageOutput(const Ren::WeakTex2DRef &tex, const Ren::eStageBits stages) {
    return builder_.WriteTexture(tex, Ren::eResState::UnorderedAccess, stages, *this);
}

RpResource RenderPass::AddColorOutput(const char *name, const Ren::Tex2DParams &params) {
    return builder_.WriteTexture(name, params, Ren::eResState::RenderTarget, Ren::eStageBits::ColorAttachment, *this);
}

RpResource RenderPass::AddColorOutput(const RpResource handle) {
    return builder_.WriteTexture(handle, Ren::eResState::RenderTarget, Ren::eStageBits::ColorAttachment, *this);
}

RpResource RenderPass::AddColorOutput(const Ren::WeakTex2DRef &tex) {
    return builder_.WriteTexture(tex, Ren::eResState::RenderTarget, Ren::eStageBits::ColorAttachment, *this);
}

RpResource RenderPass::AddColorOutput(const char *name) {
    return builder_.WriteTexture(name, Ren::eResState::RenderTarget, Ren::eStageBits::ColorAttachment, *this);
}

RpResource RenderPass::AddDepthOutput(const char *name, const Ren::Tex2DParams &params) {
    return builder_.WriteTexture(name, params, Ren::eResState::DepthWrite, Ren::eStageBits::DepthAttachment, *this);
}

RpResource RenderPass::AddUniformBufferInput(RpResource handle, const Ren::eStageBits stages) {
    return builder_.ReadBuffer(handle, Ren::eResState::UniformBuffer, stages, *this);
}

RpResource RenderPass::AddTextureInput(RpResource handle, const Ren::eStageBits stages) {
    return builder_.ReadTexture(handle, Ren::eResState::ShaderResource, stages, *this);
}

RpResource RenderPass::AddTextureInput(const Ren::WeakTex2DRef &tex, Ren::eStageBits stages) {
    return builder_.ReadTexture(tex, Ren::eResState::ShaderResource, stages, *this);
}

RpResource RenderPass::AddTextureInput(const char *name, Ren::eStageBits stages) {
    return builder_.ReadTexture(name, Ren::eResState::ShaderResource, stages, *this);
}

RpResource RenderPass::AddCustomTextureInput(const RpResource handle, const Ren::eResState desired_state,
                                              const Ren::eStageBits stages) {
    return builder_.ReadTexture(handle, desired_state, stages, *this);
}

RpResource RenderPass::AddVertexBufferInput(RpResource handle) {
    return builder_.ReadBuffer(handle, Ren::eResState::VertexBuffer, Ren::eStageBits::VertexInput, *this);
}

RpResource RenderPass::AddVertexBufferInput(const Ren::WeakBufferRef &buf) {
    return builder_.ReadBuffer(buf, Ren::eResState::VertexBuffer, Ren::eStageBits::VertexInput, *this);
}

RpResource RenderPass::AddIndexBufferInput(RpResource handle) {
    return builder_.ReadBuffer(handle, Ren::eResState::IndexBuffer, Ren::eStageBits::VertexInput, *this);
}

RpResource RenderPass::AddIndexBufferInput(const Ren::WeakBufferRef &buf) {
    return builder_.ReadBuffer(buf, Ren::eResState::IndexBuffer, Ren::eStageBits::VertexInput, *this);
}

RpResource RenderPass::AddIndirectBufferInput(RpResource handle) {
    return builder_.ReadBuffer(handle, Ren::eResState::IndirectArgument, Ren::eStageBits::DrawIndirect, *this);
}

RpResource RenderPass::AddIndirectBufferInput(const Ren::WeakBufferRef &buf) {
    return builder_.ReadBuffer(buf, Ren::eResState::IndirectArgument, Ren::eStageBits::DrawIndirect, *this);
}

RpResource RenderPass::AddASBuildReadonlyInput(RpResource handle) {
    return builder_.ReadBuffer(handle, Ren::eResState::BuildASRead, Ren::eStageBits::AccStructureBuild, *this);
}

RpResource RenderPass::AddASBuildOutput(const Ren::WeakBufferRef &buf) {
    return builder_.WriteBuffer(buf, Ren::eResState::BuildASWrite, Ren::eStageBits::AccStructureBuild, *this);
}

RpResource RenderPass::AddASBuildOutput(const char *name, const RpBufDesc &desc) {
    return builder_.WriteBuffer(name, desc, Ren::eResState::BuildASWrite, Ren::eStageBits::AccStructureBuild, *this);
}