#include "FgNode.h"

#include "FgBuilder.h"

Eng::FgResRef Eng::FgNode::AddTransferInput(const Ren::WeakBufferRef &buf) {
    return builder_.ReadBuffer(buf, Ren::eResState::CopySrc, Ren::eStageBits::Transfer, *this);
}

Eng::FgResRef Eng::FgNode::AddTransferInput(FgResRef handle) {
    return builder_.ReadBuffer(handle, Ren::eResState::CopySrc, Ren::eStageBits::Transfer, *this);
}

Eng::FgResRef Eng::FgNode::AddTransferOutput(std::string_view name, const FgBufDesc &desc) {
    return builder_.WriteBuffer(name, desc, Ren::eResState::CopyDst, Ren::eStageBits::Transfer, *this);
}

Eng::FgResRef Eng::FgNode::AddTransferOutput(const Ren::WeakBufferRef &buf) {
    return builder_.WriteBuffer(buf, Ren::eResState::CopyDst, Ren::eStageBits::Transfer, *this);
}

Eng::FgResRef Eng::FgNode::AddTransferOutput(FgResRef handle) {
    return builder_.WriteBuffer(handle, Ren::eResState::CopyDst, Ren::eStageBits::Transfer, *this);
}

Eng::FgResRef Eng::FgNode::AddTransferImageInput(const Ren::WeakTex2DRef &tex) {
    return builder_.ReadTexture(tex, Ren::eResState::CopySrc, Ren::eStageBits::Transfer, *this);
}

Eng::FgResRef Eng::FgNode::AddTransferImageInput(const FgResRef handle) {
    return builder_.ReadTexture(handle, Ren::eResState::CopySrc, Ren::eStageBits::Transfer, *this);
}

Eng::FgResRef Eng::FgNode::AddTransferImageOutput(std::string_view name, const Ren::Tex2DParams &params) {
    return builder_.WriteTexture(name, params, Ren::eResState::CopyDst, Ren::eStageBits::Transfer, *this);
}

Eng::FgResRef Eng::FgNode::AddTransferImageOutput(const Ren::WeakTex2DRef &tex) {
    return builder_.WriteTexture(tex, Ren::eResState::CopyDst, Ren::eStageBits::Transfer, *this);
}

Eng::FgResRef Eng::FgNode::AddTransferImageOutput(const FgResRef handle) {
    return builder_.WriteTexture(handle, Ren::eResState::CopyDst, Ren::eStageBits::Transfer, *this);
}

Eng::FgResRef Eng::FgNode::AddStorageReadonlyInput(FgResRef handle, Ren::eStageBits stages) {
    return builder_.ReadBuffer(handle, Ren::eResState::ShaderResource, stages, *this);
}

Eng::FgResRef Eng::FgNode::AddStorageReadonlyInput(const Ren::WeakBufferRef &buf, const Ren::eStageBits stages) {
    return builder_.ReadBuffer(buf, Ren::eResState::ShaderResource, stages, *this);
}

Eng::FgResRef Eng::FgNode::AddStorageReadonlyInput(const Ren::WeakBufferRef &buf, const Ren::WeakTex1DRef &tbo,
                                                      const Ren::eStageBits stages) {
    return builder_.ReadBuffer(buf, tbo, Ren::eResState::ShaderResource, stages, *this);
}

Eng::FgResRef Eng::FgNode::AddStorageOutput(std::string_view name, const FgBufDesc &desc,
                                            const Ren::eStageBits stages) {
    return builder_.WriteBuffer(name, desc, Ren::eResState::UnorderedAccess, stages, *this);
}

Eng::FgResRef Eng::FgNode::AddStorageOutput(const FgResRef handle, const Ren::eStageBits stages) {
    return builder_.WriteBuffer(handle, Ren::eResState::UnorderedAccess, stages, *this);
}

Eng::FgResRef Eng::FgNode::AddStorageOutput(const Ren::WeakBufferRef &buf, const Ren::eStageBits stages) {
    return builder_.WriteBuffer(buf, Ren::eResState::UnorderedAccess, stages, *this);
}

Eng::FgResRef Eng::FgNode::AddStorageImageOutput(std::string_view name, const Ren::Tex2DParams &params,
                                                    const Ren::eStageBits stages) {
    return builder_.WriteTexture(name, params, Ren::eResState::UnorderedAccess, stages, *this);
}

Eng::FgResRef Eng::FgNode::AddStorageImageOutput(const FgResRef handle, const Ren::eStageBits stages) {
    return builder_.WriteTexture(handle, Ren::eResState::UnorderedAccess, stages, *this);
}

Eng::FgResRef Eng::FgNode::AddStorageImageOutput(const Ren::WeakTex2DRef &tex, const Ren::eStageBits stages) {
    return builder_.WriteTexture(tex, Ren::eResState::UnorderedAccess, stages, *this);
}

Eng::FgResRef Eng::FgNode::AddStorageImageOutput(const Ren::Texture2DArray *tex, Ren::eStageBits stages) {
    return builder_.WriteTexture(tex, Ren::eResState::UnorderedAccess, stages, *this);
}

Eng::FgResRef Eng::FgNode::AddColorOutput(std::string_view name, const Ren::Tex2DParams &params) {
    return builder_.WriteTexture(name, params, Ren::eResState::RenderTarget, Ren::eStageBits::ColorAttachment, *this);
}

Eng::FgResRef Eng::FgNode::AddColorOutput(const FgResRef handle) {
    return builder_.WriteTexture(handle, Ren::eResState::RenderTarget, Ren::eStageBits::ColorAttachment, *this);
}

Eng::FgResRef Eng::FgNode::AddColorOutput(const Ren::WeakTex2DRef &tex) {
    return builder_.WriteTexture(tex, Ren::eResState::RenderTarget, Ren::eStageBits::ColorAttachment, *this);
}

Eng::FgResRef Eng::FgNode::AddColorOutput(std::string_view name) {
    return builder_.WriteTexture(name, Ren::eResState::RenderTarget, Ren::eStageBits::ColorAttachment, *this);
}

Eng::FgResRef Eng::FgNode::AddDepthOutput(std::string_view name, const Ren::Tex2DParams &params) {
    return builder_.WriteTexture(name, params, Ren::eResState::DepthWrite, Ren::eStageBits::DepthAttachment, *this);
}

Eng::FgResRef Eng::FgNode::AddDepthOutput(const FgResRef handle) {
    return builder_.WriteTexture(handle, Ren::eResState::DepthWrite, Ren::eStageBits::DepthAttachment, *this);
}

Eng::FgResRef Eng::FgNode::AddDepthOutput(const Ren::WeakTex2DRef &tex) {
    return builder_.WriteTexture(tex, Ren::eResState::DepthWrite, Ren::eStageBits::DepthAttachment, *this);
}

Eng::FgResRef Eng::FgNode::ReplaceTransferInput(const int slot_index, const Ren::WeakBufferRef &buf) {
    return builder_.ReadBuffer(buf, Ren::eResState::CopySrc, Ren::eStageBits::Transfer, *this, slot_index);
}

Eng::FgResRef Eng::FgNode::ReplaceColorOutput(const int slot_index, const Ren::WeakTex2DRef &tex) {
    return builder_.WriteTexture(tex, Ren::eResState::RenderTarget, Ren::eStageBits::ColorAttachment, *this,
                                 slot_index);
}

Eng::FgResRef Eng::FgNode::AddUniformBufferInput(const FgResRef handle, const Ren::eStageBits stages) {
    return builder_.ReadBuffer(handle, Ren::eResState::UniformBuffer, stages, *this);
}

Eng::FgResRef Eng::FgNode::AddTextureInput(const FgResRef handle, const Ren::eStageBits stages) {
    return builder_.ReadTexture(handle, Ren::eResState::ShaderResource, stages, *this);
}

Eng::FgResRef Eng::FgNode::AddTextureInput(const Ren::WeakTex2DRef &tex, const Ren::eStageBits stages) {
    return builder_.ReadTexture(tex, Ren::eResState::ShaderResource, stages, *this);
}

Eng::FgResRef Eng::FgNode::AddTextureInput(std::string_view name, const Ren::eStageBits stages) {
    return builder_.ReadTexture(name, Ren::eResState::ShaderResource, stages, *this);
}

Eng::FgResRef Eng::FgNode::AddTextureInput(const Ren::Texture2DArray *tex, const Ren::eStageBits stages) {
    return builder_.ReadTexture(tex, Ren::eResState::ShaderResource, stages, *this);
}

Eng::FgResRef Eng::FgNode::AddTextureInput(const Ren::Texture3D *tex, const Ren::eStageBits stages) {
    return builder_.ReadTexture(tex, Ren::eResState::ShaderResource, stages, *this);
}

Eng::FgResRef Eng::FgNode::AddHistoryTextureInput(FgResRef handle, const Ren::eStageBits stages) {
    return builder_.ReadHistoryTexture(handle, Ren::eResState::ShaderResource, stages, *this);
}

Eng::FgResRef Eng::FgNode::AddHistoryTextureInput(std::string_view name, const Ren::eStageBits stages) {
    return builder_.ReadHistoryTexture(name, Ren::eResState::ShaderResource, stages, *this);
}

Eng::FgResRef Eng::FgNode::AddCustomTextureInput(const FgResRef handle, const Ren::eResState desired_state,
                                                    const Ren::eStageBits stages) {
    return builder_.ReadTexture(handle, desired_state, stages, *this);
}

Eng::FgResRef Eng::FgNode::AddVertexBufferInput(const FgResRef handle) {
    return builder_.ReadBuffer(handle, Ren::eResState::VertexBuffer, Ren::eStageBits::VertexInput, *this);
}

Eng::FgResRef Eng::FgNode::AddVertexBufferInput(const Ren::WeakBufferRef &buf) {
    return builder_.ReadBuffer(buf, Ren::eResState::VertexBuffer, Ren::eStageBits::VertexInput, *this);
}

Eng::FgResRef Eng::FgNode::AddIndexBufferInput(const FgResRef handle) {
    return builder_.ReadBuffer(handle, Ren::eResState::IndexBuffer, Ren::eStageBits::VertexInput, *this);
}

Eng::FgResRef Eng::FgNode::AddIndexBufferInput(const Ren::WeakBufferRef &buf) {
    return builder_.ReadBuffer(buf, Ren::eResState::IndexBuffer, Ren::eStageBits::VertexInput, *this);
}

Eng::FgResRef Eng::FgNode::AddIndirectBufferInput(const FgResRef handle) {
    return builder_.ReadBuffer(handle, Ren::eResState::IndirectArgument, Ren::eStageBits::DrawIndirect, *this);
}

Eng::FgResRef Eng::FgNode::AddIndirectBufferInput(const Ren::WeakBufferRef &buf) {
    return builder_.ReadBuffer(buf, Ren::eResState::IndirectArgument, Ren::eStageBits::DrawIndirect, *this);
}

Eng::FgResRef Eng::FgNode::AddASBuildReadonlyInput(const FgResRef handle) {
    return builder_.ReadBuffer(handle, Ren::eResState::BuildASRead, Ren::eStageBits::AccStructureBuild, *this);
}

Eng::FgResRef Eng::FgNode::AddASBuildOutput(const Ren::WeakBufferRef &buf) {
    return builder_.WriteBuffer(buf, Ren::eResState::BuildASWrite, Ren::eStageBits::AccStructureBuild, *this);
}

Eng::FgResRef Eng::FgNode::AddASBuildOutput(std::string_view name, const FgBufDesc &desc) {
    return builder_.WriteBuffer(name, desc, Ren::eResState::BuildASWrite, Ren::eStageBits::AccStructureBuild, *this);
}