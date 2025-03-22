#pragma once

#include <cstdint>

#include <variant>

#include "Bitmask.h"
#include "Fwd.h"
#include "Span.h"

namespace Ren {
struct ApiContext;

enum class eStage : uint16_t {
    VertexInput,
    VertexShader,
    TessCtrlShader,
    TessEvalShader,
    GeometryShader,
    FragmentShader,
    ComputeShader,
    RayTracingShader,
    ColorAttachment,
    DepthAttachment,
    DrawIndirect,
    Transfer,
    AccStructureBuild
};

const Bitmask<eStage> AllStages = Bitmask<eStage>{eStage::VertexInput} | eStage::VertexShader | eStage::TessCtrlShader |
                                  eStage::TessEvalShader | eStage::GeometryShader | eStage::FragmentShader |
                                  eStage::ComputeShader | eStage::RayTracingShader | eStage::ColorAttachment |
                                  eStage::DepthAttachment | eStage::DrawIndirect | eStage::Transfer |
                                  eStage::AccStructureBuild;

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
Bitmask<eStage> StagesForState(eResState state);

class Buffer;
class Texture;

struct TransitionInfo {
    std::variant<const Texture *, const Buffer *> p_res;

    eResState old_state = eResState::Undefined;
    eResState new_state = eResState::Undefined;

    bool update_internal_state = false;

    TransitionInfo() = default;
    TransitionInfo(const Buffer *_p_buf, const eResState _new_state)
        : p_res(_p_buf), new_state(_new_state), update_internal_state(true) {}
    TransitionInfo(const Texture *_p_tex, const eResState _new_state)
        : p_res(_p_tex), new_state(_new_state), update_internal_state(true) {}
};

void TransitionResourceStates(ApiContext *api_ctx, CommandBuffer cmd_buf, Bitmask<eStage> src_stages_mask,
                              Bitmask<eStage> dst_stages_mask, Span<const TransitionInfo> transitions);
} // namespace Ren