#pragma once

#include <cstdint>

#include <variant>

#include "Fwd.h"
#include "Span.h"

namespace Ren {
struct ApiContext;

enum class eStageBits : uint16_t {
    None = 0,
    VertexInput = (1u << 0u),
    VertexShader = (1u << 1u),
    TessCtrlShader = (1u << 2u),
    TessEvalShader = (1u << 3u),
    GeometryShader = (1u << 4u),
    FragmentShader = (1u << 5u),
    ComputeShader = (1u << 6u),
    RayTracingShader = (1u << 7u),
    ColorAttachment = (1u << 8u),
    DepthAttachment = (1u << 9u),
    DrawIndirect = (1u << 10u),
    Transfer = (1u << 11u),
    AccStructureBuild = (1u << 12u)
};
inline eStageBits operator|(const eStageBits lhs, const eStageBits rhs) {
    return eStageBits(uint16_t(lhs) | uint16_t(rhs));
}
inline eStageBits operator&(const eStageBits lhs, const eStageBits rhs) {
    return eStageBits(uint16_t(lhs) & uint16_t(rhs));
}
inline eStageBits operator|=(eStageBits &lhs, const eStageBits rhs) {
    lhs = eStageBits(uint16_t(lhs) | uint16_t(rhs));
    return lhs;
}
inline eStageBits operator&=(eStageBits &lhs, const eStageBits rhs) {
    lhs = eStageBits(uint16_t(lhs) & uint16_t(rhs));
    return lhs;
}

const eStageBits AllStages = eStageBits::VertexInput | eStageBits::VertexShader | eStageBits::TessCtrlShader |
                             eStageBits::TessEvalShader | eStageBits::GeometryShader | eStageBits::FragmentShader |
                             eStageBits::ComputeShader | eStageBits::RayTracingShader | eStageBits::ColorAttachment |
                             eStageBits::DepthAttachment | eStageBits::DrawIndirect | eStageBits::Transfer |
                             eStageBits::AccStructureBuild;

enum class eResState : uint8_t {
    Undefined,
    Discarded,
    VertexBuffer,
    UniformBuffer,
    IndexBuffer,
    RenderTarget,
    UnorderedAccess,
    DepthRead,
    DepthWrite,
    StencilTestDepthFetch,
    ShaderResource,
    IndirectArgument,
    CopyDst,
    CopySrc,
    BuildASRead,
    BuildASWrite,
    RayTracing,
    _Count
};

#if defined(REN_VK_BACKEND)
int VKImageLayoutForState(eResState state);
uint32_t VKAccessFlagsForState(eResState state);
uint32_t VKPipelineStagesForState(eResState state);
#endif
bool IsRWState(eResState state);
eStageBits StageBitsForState(eResState state);

class Buffer;
class Texture;
class Texture2DArray;

struct TransitionInfo {
    std::variant<const Texture *, const Buffer *, const Texture2DArray *> p_res;

    eResState old_state = eResState::Undefined;
    eResState new_state = eResState::Undefined;

    bool update_internal_state = false;

    TransitionInfo() = default;
    TransitionInfo(const Buffer *_p_buf, const eResState _new_state)
        : p_res(_p_buf), new_state(_new_state), update_internal_state(true) {}
    TransitionInfo(const Texture *_p_tex, const eResState _new_state)
        : p_res(_p_tex), new_state(_new_state), update_internal_state(true) {}
    TransitionInfo(const Texture2DArray *_p_tex2darr, const eResState _new_state)
        : p_res(_p_tex2darr), new_state(_new_state), update_internal_state(true) {}
};

void TransitionResourceStates(ApiContext *api_ctx, CommandBuffer cmd_buf, eStageBits src_stages_mask,
                              eStageBits dst_stages_mask, Span<const TransitionInfo> transitions);
} // namespace Ren