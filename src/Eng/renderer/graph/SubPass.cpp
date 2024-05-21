#include "SubPass.h"

#include "GraphBuilder.h"

Eng::RpResRef Eng::RpSubpass::AddTransferInput(const Ren::WeakBufferRef &buf) {
    return builder_.ReadBuffer(buf, Ren::eResState::CopySrc, Ren::eStageBits::Transfer, *this);
}

Eng::RpResRef Eng::RpSubpass::AddTransferInput(RpResRef handle) {
    return builder_.ReadBuffer(handle, Ren::eResState::CopySrc, Ren::eStageBits::Transfer, *this);
}

Eng::RpResRef Eng::RpSubpass::AddTransferOutput(std::string_view name, const RpBufDesc &desc) {
    return builder_.WriteBuffer(name, desc, Ren::eResState::CopyDst, Ren::eStageBits::Transfer, *this);
}

Eng::RpResRef Eng::RpSubpass::AddTransferOutput(const Ren::WeakBufferRef &buf) {
    return builder_.WriteBuffer(buf, Ren::eResState::CopyDst, Ren::eStageBits::Transfer, *this);
}

Eng::RpResRef Eng::RpSubpass::AddTransferOutput(RpResRef handle) {
    return builder_.WriteBuffer(handle, Ren::eResState::CopyDst, Ren::eStageBits::Transfer, *this);
}

Eng::RpResRef Eng::RpSubpass::AddTransferImageInput(const Ren::WeakTex2DRef &tex) {
    return builder_.ReadTexture(tex, Ren::eResState::CopySrc, Ren::eStageBits::Transfer, *this);
}

Eng::RpResRef Eng::RpSubpass::AddTransferImageInput(const RpResRef handle) {
    return builder_.ReadTexture(handle, Ren::eResState::CopySrc, Ren::eStageBits::Transfer, *this);
}

Eng::RpResRef Eng::RpSubpass::AddTransferImageOutput(std::string_view name, const Ren::Tex2DParams &params) {
    return builder_.WriteTexture(name, params, Ren::eResState::CopyDst, Ren::eStageBits::Transfer, *this);
}

Eng::RpResRef Eng::RpSubpass::AddTransferImageOutput(const Ren::WeakTex2DRef &tex) {
    return builder_.WriteTexture(tex, Ren::eResState::CopyDst, Ren::eStageBits::Transfer, *this);
}

Eng::RpResRef Eng::RpSubpass::AddTransferImageOutput(const RpResRef handle) {
    return builder_.WriteTexture(handle, Ren::eResState::CopyDst, Ren::eStageBits::Transfer, *this);
}

Eng::RpResRef Eng::RpSubpass::AddStorageReadonlyInput(RpResRef handle, Ren::eStageBits stages) {
    return builder_.ReadBuffer(handle, Ren::eResState::ShaderResource, stages, *this);
}

Eng::RpResRef Eng::RpSubpass::AddStorageReadonlyInput(const Ren::WeakBufferRef &buf, const Ren::eStageBits stages) {
    return builder_.ReadBuffer(buf, Ren::eResState::ShaderResource, stages, *this);
}

Eng::RpResRef Eng::RpSubpass::AddStorageReadonlyInput(const Ren::WeakBufferRef &buf, const Ren::WeakTex1DRef &tbo,
                                                      const Ren::eStageBits stages) {
    return builder_.ReadBuffer(buf, tbo, Ren::eResState::ShaderResource, stages, *this);
}

Eng::RpResRef Eng::RpSubpass::AddStorageOutput(std::string_view name, const RpBufDesc &desc, const Ren::eStageBits stages) {
    return builder_.WriteBuffer(name, desc, Ren::eResState::UnorderedAccess, stages, *this);
}

Eng::RpResRef Eng::RpSubpass::AddStorageOutput(const RpResRef handle, const Ren::eStageBits stages) {
    return builder_.WriteBuffer(handle, Ren::eResState::UnorderedAccess, stages, *this);
}

Eng::RpResRef Eng::RpSubpass::AddStorageOutput(const Ren::WeakBufferRef &buf, const Ren::eStageBits stages) {
    return builder_.WriteBuffer(buf, Ren::eResState::UnorderedAccess, stages, *this);
}

Eng::RpResRef Eng::RpSubpass::AddStorageImageOutput(std::string_view name, const Ren::Tex2DParams &params,
                                                    const Ren::eStageBits stages) {
    return builder_.WriteTexture(name, params, Ren::eResState::UnorderedAccess, stages, *this);
}

Eng::RpResRef Eng::RpSubpass::AddStorageImageOutput(const RpResRef handle, const Ren::eStageBits stages) {
    return builder_.WriteTexture(handle, Ren::eResState::UnorderedAccess, stages, *this);
}

Eng::RpResRef Eng::RpSubpass::AddStorageImageOutput(const Ren::WeakTex2DRef &tex, const Ren::eStageBits stages) {
    return builder_.WriteTexture(tex, Ren::eResState::UnorderedAccess, stages, *this);
}

Eng::RpResRef Eng::RpSubpass::AddStorageImageOutput(const Ren::Texture2DArray *tex, Ren::eStageBits stages) {
    return builder_.WriteTexture(tex, Ren::eResState::UnorderedAccess, stages, *this);
}

Eng::RpResRef Eng::RpSubpass::AddColorOutput(std::string_view name, const Ren::Tex2DParams &params) {
    return builder_.WriteTexture(name, params, Ren::eResState::RenderTarget, Ren::eStageBits::ColorAttachment, *this);
}

Eng::RpResRef Eng::RpSubpass::AddColorOutput(const RpResRef handle) {
    return builder_.WriteTexture(handle, Ren::eResState::RenderTarget, Ren::eStageBits::ColorAttachment, *this);
}

Eng::RpResRef Eng::RpSubpass::AddColorOutput(const Ren::WeakTex2DRef &tex) {
    return builder_.WriteTexture(tex, Ren::eResState::RenderTarget, Ren::eStageBits::ColorAttachment, *this);
}

Eng::RpResRef Eng::RpSubpass::AddColorOutput(std::string_view name) {
    return builder_.WriteTexture(name, Ren::eResState::RenderTarget, Ren::eStageBits::ColorAttachment, *this);
}

Eng::RpResRef Eng::RpSubpass::AddDepthOutput(std::string_view name, const Ren::Tex2DParams &params) {
    return builder_.WriteTexture(name, params, Ren::eResState::DepthWrite, Ren::eStageBits::DepthAttachment, *this);
}

Eng::RpResRef Eng::RpSubpass::AddDepthOutput(const RpResRef handle) {
    return builder_.WriteTexture(handle, Ren::eResState::DepthWrite, Ren::eStageBits::DepthAttachment, *this);
}

Eng::RpResRef Eng::RpSubpass::AddDepthOutput(const Ren::WeakTex2DRef &tex) {
    return builder_.WriteTexture(tex, Ren::eResState::DepthWrite, Ren::eStageBits::DepthAttachment, *this);
}

Eng::RpResRef Eng::RpSubpass::ReplaceTransferInput(const int slot_index, const Ren::WeakBufferRef &buf) {
    return builder_.ReadBuffer(buf, Ren::eResState::CopySrc, Ren::eStageBits::Transfer, *this, slot_index);
}

Eng::RpResRef Eng::RpSubpass::ReplaceColorOutput(const int slot_index, const Ren::WeakTex2DRef &tex) {
    return builder_.WriteTexture(tex, Ren::eResState::RenderTarget, Ren::eStageBits::ColorAttachment, *this,
                                 slot_index);
}

Eng::RpResRef Eng::RpSubpass::AddUniformBufferInput(const RpResRef handle, const Ren::eStageBits stages) {
    return builder_.ReadBuffer(handle, Ren::eResState::UniformBuffer, stages, *this);
}

Eng::RpResRef Eng::RpSubpass::AddTextureInput(const RpResRef handle, const Ren::eStageBits stages) {
    return builder_.ReadTexture(handle, Ren::eResState::ShaderResource, stages, *this);
}

Eng::RpResRef Eng::RpSubpass::AddTextureInput(const Ren::WeakTex2DRef &tex, const Ren::eStageBits stages) {
    return builder_.ReadTexture(tex, Ren::eResState::ShaderResource, stages, *this);
}

Eng::RpResRef Eng::RpSubpass::AddTextureInput(std::string_view name, const Ren::eStageBits stages) {
    return builder_.ReadTexture(name, Ren::eResState::ShaderResource, stages, *this);
}

Eng::RpResRef Eng::RpSubpass::AddTextureInput(const Ren::Texture2DArray *tex, const Ren::eStageBits stages) {
    return builder_.ReadTexture(tex, Ren::eResState::ShaderResource, stages, *this);
}

Eng::RpResRef Eng::RpSubpass::AddTextureInput(const Ren::Texture3D *tex, const Ren::eStageBits stages) {
    return builder_.ReadTexture(tex, Ren::eResState::ShaderResource, stages, *this);
}

Eng::RpResRef Eng::RpSubpass::AddHistoryTextureInput(RpResRef handle, const Ren::eStageBits stages) {
    return builder_.ReadHistoryTexture(handle, Ren::eResState::ShaderResource, stages, *this);
}

Eng::RpResRef Eng::RpSubpass::AddHistoryTextureInput(std::string_view name, const Ren::eStageBits stages) {
    return builder_.ReadHistoryTexture(name, Ren::eResState::ShaderResource, stages, *this);
}

Eng::RpResRef Eng::RpSubpass::AddCustomTextureInput(const RpResRef handle, const Ren::eResState desired_state,
                                                    const Ren::eStageBits stages) {
    return builder_.ReadTexture(handle, desired_state, stages, *this);
}

Eng::RpResRef Eng::RpSubpass::AddVertexBufferInput(const RpResRef handle) {
    return builder_.ReadBuffer(handle, Ren::eResState::VertexBuffer, Ren::eStageBits::VertexInput, *this);
}

Eng::RpResRef Eng::RpSubpass::AddVertexBufferInput(const Ren::WeakBufferRef &buf) {
    return builder_.ReadBuffer(buf, Ren::eResState::VertexBuffer, Ren::eStageBits::VertexInput, *this);
}

Eng::RpResRef Eng::RpSubpass::AddIndexBufferInput(const RpResRef handle) {
    return builder_.ReadBuffer(handle, Ren::eResState::IndexBuffer, Ren::eStageBits::VertexInput, *this);
}

Eng::RpResRef Eng::RpSubpass::AddIndexBufferInput(const Ren::WeakBufferRef &buf) {
    return builder_.ReadBuffer(buf, Ren::eResState::IndexBuffer, Ren::eStageBits::VertexInput, *this);
}

Eng::RpResRef Eng::RpSubpass::AddIndirectBufferInput(const RpResRef handle) {
    return builder_.ReadBuffer(handle, Ren::eResState::IndirectArgument, Ren::eStageBits::DrawIndirect, *this);
}

Eng::RpResRef Eng::RpSubpass::AddIndirectBufferInput(const Ren::WeakBufferRef &buf) {
    return builder_.ReadBuffer(buf, Ren::eResState::IndirectArgument, Ren::eStageBits::DrawIndirect, *this);
}

Eng::RpResRef Eng::RpSubpass::AddASBuildReadonlyInput(const RpResRef handle) {
    return builder_.ReadBuffer(handle, Ren::eResState::BuildASRead, Ren::eStageBits::AccStructureBuild, *this);
}

Eng::RpResRef Eng::RpSubpass::AddASBuildOutput(const Ren::WeakBufferRef &buf) {
    return builder_.WriteBuffer(buf, Ren::eResState::BuildASWrite, Ren::eStageBits::AccStructureBuild, *this);
}

Eng::RpResRef Eng::RpSubpass::AddASBuildOutput(std::string_view name, const RpBufDesc &desc) {
    return builder_.WriteBuffer(name, desc, Ren::eResState::BuildASWrite, Ren::eStageBits::AccStructureBuild, *this);
}
