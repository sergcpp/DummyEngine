#include "SubPass.h"

#include "GraphBuilder.h"

RpResRef RpSubpass::AddTransferInput(const Ren::WeakBufferRef &buf) {
    return builder_.ReadBuffer(buf, Ren::eResState::CopySrc, Ren::eStageBits::Transfer, *this);
}

RpResRef RpSubpass::AddTransferOutput(const char *name, const RpBufDesc &desc) {
    return builder_.WriteBuffer(name, desc, Ren::eResState::CopyDst, Ren::eStageBits::Transfer, *this);
}

RpResRef RpSubpass::AddTransferOutput(const Ren::WeakBufferRef &buf) {
    return builder_.WriteBuffer(buf, Ren::eResState::CopyDst, Ren::eStageBits::Transfer, *this);
}

RpResRef RpSubpass::AddTransferImageInput(const Ren::WeakTex2DRef &tex) {
    return builder_.ReadTexture(tex, Ren::eResState::CopySrc, Ren::eStageBits::Transfer, *this);
}

RpResRef RpSubpass::AddTransferImageInput(const RpResRef handle) {
    return builder_.ReadTexture(handle, Ren::eResState::CopySrc, Ren::eStageBits::Transfer, *this);
}

RpResRef RpSubpass::AddTransferImageOutput(const char *name, const Ren::Tex2DParams &params) {
    return builder_.WriteTexture(name, params, Ren::eResState::CopyDst, Ren::eStageBits::Transfer, *this);
}

RpResRef RpSubpass::AddTransferImageOutput(const Ren::WeakTex2DRef &tex) {
    return builder_.WriteTexture(tex, Ren::eResState::CopyDst, Ren::eStageBits::Transfer, *this);
}

RpResRef RpSubpass::AddTransferImageOutput(const RpResRef handle) {
    return builder_.WriteTexture(handle, Ren::eResState::CopyDst, Ren::eStageBits::Transfer, *this);
}

RpResRef RpSubpass::AddStorageReadonlyInput(RpResRef handle, Ren::eStageBits stages) {
    return builder_.ReadBuffer(handle, Ren::eResState::ShaderResource, stages, *this);
}

RpResRef RpSubpass::AddStorageReadonlyInput(const Ren::WeakBufferRef &buf, const Ren::eStageBits stages) {
    return builder_.ReadBuffer(buf, Ren::eResState::ShaderResource, stages, *this);
}

RpResRef RpSubpass::AddStorageReadonlyInput(const Ren::WeakBufferRef& buf, const Ren::WeakTex1DRef& tbo,
                                            const Ren::eStageBits stages) {
    return builder_.ReadBuffer(buf, tbo, Ren::eResState::ShaderResource, stages, *this);
}

RpResRef RpSubpass::AddStorageOutput(const char *name, const RpBufDesc &desc, const Ren::eStageBits stages) {
    return builder_.WriteBuffer(name, desc, Ren::eResState::UnorderedAccess, stages, *this);
}

RpResRef RpSubpass::AddStorageOutput(const RpResRef handle, const Ren::eStageBits stages) {
    return builder_.WriteBuffer(handle, Ren::eResState::UnorderedAccess, stages, *this);
}

RpResRef RpSubpass::AddStorageOutput(const Ren::WeakBufferRef &buf, const Ren::eStageBits stages) {
    return builder_.WriteBuffer(buf, Ren::eResState::UnorderedAccess, stages, *this);
}

RpResRef RpSubpass::AddStorageImageOutput(const char *name, const Ren::Tex2DParams &params,
                                          const Ren::eStageBits stages) {
    return builder_.WriteTexture(name, params, Ren::eResState::UnorderedAccess, stages, *this);
}

RpResRef RpSubpass::AddStorageImageOutput(const RpResRef handle, const Ren::eStageBits stages) {
    return builder_.WriteTexture(handle, Ren::eResState::UnorderedAccess, stages, *this);
}

RpResRef RpSubpass::AddStorageImageOutput(const Ren::WeakTex2DRef &tex, const Ren::eStageBits stages) {
    return builder_.WriteTexture(tex, Ren::eResState::UnorderedAccess, stages, *this);
}

RpResRef RpSubpass::AddColorOutput(const char *name, const Ren::Tex2DParams &params) {
    return builder_.WriteTexture(name, params, Ren::eResState::RenderTarget, Ren::eStageBits::ColorAttachment, *this);
}

RpResRef RpSubpass::AddColorOutput(const RpResRef handle) {
    return builder_.WriteTexture(handle, Ren::eResState::RenderTarget, Ren::eStageBits::ColorAttachment, *this);
}

RpResRef RpSubpass::AddColorOutput(const Ren::WeakTex2DRef &tex) {
    return builder_.WriteTexture(tex, Ren::eResState::RenderTarget, Ren::eStageBits::ColorAttachment, *this);
}

RpResRef RpSubpass::AddColorOutput(const char *name) {
    return builder_.WriteTexture(name, Ren::eResState::RenderTarget, Ren::eStageBits::ColorAttachment, *this);
}

RpResRef RpSubpass::AddDepthOutput(const char *name, const Ren::Tex2DParams &params) {
    return builder_.WriteTexture(name, params, Ren::eResState::DepthWrite, Ren::eStageBits::DepthAttachment, *this);
}

RpResRef RpSubpass::AddDepthOutput(const Ren::WeakTex2DRef &tex) {
    return builder_.WriteTexture(tex, Ren::eResState::DepthWrite, Ren::eStageBits::DepthAttachment, *this);
}

RpResRef RpSubpass::ReplaceColorOutput(const int slot_index, const Ren::WeakTex2DRef &tex) {
    return builder_.WriteTexture(tex, Ren::eResState::RenderTarget, Ren::eStageBits::ColorAttachment, *this,
                                 slot_index);
}

RpResRef RpSubpass::AddUniformBufferInput(const RpResRef handle, const Ren::eStageBits stages) {
    return builder_.ReadBuffer(handle, Ren::eResState::UniformBuffer, stages, *this);
}

RpResRef RpSubpass::AddTextureInput(const RpResRef handle, const Ren::eStageBits stages) {
    return builder_.ReadTexture(handle, Ren::eResState::ShaderResource, stages, *this);
}

RpResRef RpSubpass::AddTextureInput(const Ren::WeakTex2DRef &tex, Ren::eStageBits stages) {
    return builder_.ReadTexture(tex, Ren::eResState::ShaderResource, stages, *this);
}

RpResRef RpSubpass::AddTextureInput(const char *name, Ren::eStageBits stages) {
    return builder_.ReadTexture(name, Ren::eResState::ShaderResource, stages, *this);
}

RpResRef RpSubpass::AddHistoryTextureInput(RpResRef handle, Ren::eStageBits stages) {
    return builder_.ReadHistoryTexture(handle, Ren::eResState::ShaderResource, stages, *this);
}

RpResRef RpSubpass::AddHistoryTextureInput(const char* name, Ren::eStageBits stages) {
    return builder_.ReadHistoryTexture(name, Ren::eResState::ShaderResource, stages, *this);
}

RpResRef RpSubpass::AddCustomTextureInput(const RpResRef handle, const Ren::eResState desired_state,
                                          const Ren::eStageBits stages) {
    return builder_.ReadTexture(handle, desired_state, stages, *this);
}

RpResRef RpSubpass::AddVertexBufferInput(const RpResRef handle) {
    return builder_.ReadBuffer(handle, Ren::eResState::VertexBuffer, Ren::eStageBits::VertexInput, *this);
}

RpResRef RpSubpass::AddVertexBufferInput(const Ren::WeakBufferRef &buf) {
    return builder_.ReadBuffer(buf, Ren::eResState::VertexBuffer, Ren::eStageBits::VertexInput, *this);
}

RpResRef RpSubpass::AddIndexBufferInput(const RpResRef handle) {
    return builder_.ReadBuffer(handle, Ren::eResState::IndexBuffer, Ren::eStageBits::VertexInput, *this);
}

RpResRef RpSubpass::AddIndexBufferInput(const Ren::WeakBufferRef &buf) {
    return builder_.ReadBuffer(buf, Ren::eResState::IndexBuffer, Ren::eStageBits::VertexInput, *this);
}

RpResRef RpSubpass::AddIndirectBufferInput(const RpResRef handle) {
    return builder_.ReadBuffer(handle, Ren::eResState::IndirectArgument, Ren::eStageBits::DrawIndirect, *this);
}

RpResRef RpSubpass::AddIndirectBufferInput(const Ren::WeakBufferRef &buf) {
    return builder_.ReadBuffer(buf, Ren::eResState::IndirectArgument, Ren::eStageBits::DrawIndirect, *this);
}

RpResRef RpSubpass::AddASBuildReadonlyInput(const RpResRef handle) {
    return builder_.ReadBuffer(handle, Ren::eResState::BuildASRead, Ren::eStageBits::AccStructureBuild, *this);
}

RpResRef RpSubpass::AddASBuildOutput(const Ren::WeakBufferRef &buf) {
    return builder_.WriteBuffer(buf, Ren::eResState::BuildASWrite, Ren::eStageBits::AccStructureBuild, *this);
}

RpResRef RpSubpass::AddASBuildOutput(const char *name, const RpBufDesc &desc) {
    return builder_.WriteBuffer(name, desc, Ren::eResState::BuildASWrite, Ren::eStageBits::AccStructureBuild, *this);
}
