#include "Resource.h"

namespace Ren {
const eStageBits g_stage_bits_per_state[] = {
    {},                        // Undefined
    eStageBits::VertexInput,   // VertexBuffer
    eStageBits::VertexShader | /* eStageBits::TessCtrlShader | eStageBits::TessEvalShader | eStageBits::GeometryShader
                                  |*/
        eStageBits::FragmentShader | eStageBits::ComputeShader, // UniformBuffer
    eStageBits::VertexInput,                                    // IndexBuffer
    eStageBits::ColorAttachment,                                // RenderTarget
    eStageBits::VertexShader | /* eStageBits::TessCtrlShader | eStageBits::TessEvalShader | eStageBits::GeometryShader
                                  |*/
        eStageBits::FragmentShader | eStageBits::ComputeShader, // UnorderedAccess
    eStageBits::DepthAttachment,                                // DepthRead
    eStageBits::DepthAttachment,                                // DepthWrite
    eStageBits::VertexShader | /* eStageBits::TessCtrlShader | eStageBits::TessEvalShader | eStageBits::GeometryShader
                                  |*/
        eStageBits::FragmentShader | eStageBits::ComputeShader, // ShaderResource
    eStageBits::DrawIndirect,                                   // IndirectArgument
    eStageBits::Transfer,                                       // CopyDst
    eStageBits::Transfer,                                       // CopySrc
    eStageBits::AccStructureBuild,                              // BuildASRead
    eStageBits::AccStructureBuild,                              // BuildASWrite
    eStageBits::RayTracingShader                                // RayTracing
};
static_assert(sizeof(g_stage_bits_per_state) / sizeof(g_stage_bits_per_state[0]) == int(eResState::_Count), "!");
} // namespace Ren

Ren::eStageBits Ren::StageBitsForState(eResState state) { return g_stage_bits_per_state[int(state)]; }

#if !defined(USE_VK_RENDER)
void Ren::TransitionResourceStates(void *_cmd_buf, const eStageBits src_stages_mask, const eStageBits dst_stages_mask,
                                   const TransitionInfo *transitions, const int transitions_count) {
    for (int i = 0; i < transitions_count; i++) {
        if (transitions[i].p_tex) {
            eResState old_state = transitions[i].old_state;
            if (old_state == Ren::eResState::Undefined) {
                // take state from resource itself
                old_state = transitions[i].p_tex->resource_state;
                if (old_state != eResState::Undefined && old_state == transitions[i].new_state) {
                    // transition is not needed
                    continue;
                }
            }

            if (transitions[i].update_internal_state) {
                transitions[i].p_tex->resource_state = transitions[i].new_state;
            }
        } else if (transitions[i].p_buf) {
            eResState old_state = transitions[i].old_state;
            if (old_state == Ren::eResState::Undefined) {
                // take state from resource itself
                old_state = transitions[i].p_buf->resource_state;
                if (old_state == transitions[i].new_state) {
                    // transition is not needed
                    continue;
                }
            }

            if (transitions[i].update_internal_state) {
                transitions[i].p_buf->resource_state = transitions[i].new_state;
            }
        }
    }
}
#endif