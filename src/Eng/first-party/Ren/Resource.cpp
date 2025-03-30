#include "Resource.h"

namespace Ren {
const Bitmask<eStage> g_stage_bits_per_state[] = {
    {},                             // Undefined
    AllStages,                      // Discarded
    eStage::VertexInput,            // VertexBuffer
    Bitmask{eStage::VertexShader} | /* eStageBits::TessCtrlShader | eStageBits::TessEvalShader |
                                   eStageBits::GeometryShader
                                  |*/
        eStage::FragmentShader | eStage::ComputeShader | eStage::RayTracingShader, // UniformBuffer
    eStage::VertexInput,                                                           // IndexBuffer
    eStage::ColorAttachment,                                                       // RenderTarget
    Bitmask{
        eStage::VertexShader} | /* eStageBits::TessCtrlShader | eStageBits::TessEvalShader | eStageBits::GeometryShader
                                  |*/
        eStage::FragmentShader |
        eStage::ComputeShader | eStage::RayTracingShader,      // UnorderedAccess
    eStage::DepthAttachment,                                   // DepthRead
    eStage::DepthAttachment,                                   // DepthWrite
    Bitmask{eStage::DepthAttachment} | eStage::FragmentShader, // StencilTestDepthFetch
    Bitmask{
        eStage::VertexShader} | /* eStageBits::TessCtrlShader | eStageBits::TessEvalShader | eStageBits::GeometryShader
                                  |*/
        eStage::FragmentShader |
        eStage::ComputeShader | eStage::RayTracingShader, // ShaderResource
    eStage::DrawIndirect,                                 // IndirectArgument
    eStage::Transfer,                                     // CopyDst
    eStage::Transfer,                                     // CopySrc
    eStage::AccStructureBuild,                            // BuildASRead
    eStage::AccStructureBuild,                            // BuildASWrite
    eStage::RayTracingShader                              // RayTracing
};
static_assert(std::size(g_stage_bits_per_state) == int(eResState::_Count));
} // namespace Ren

Ren::Bitmask<Ren::eStage> Ren::StagesForState(const eResState state) { return g_stage_bits_per_state[int(state)]; }

bool Ren::IsRWState(const eResState state) {
    return state == eResState::RenderTarget || state == eResState::UnorderedAccess || state == eResState::DepthWrite ||
           state == eResState::CopyDst || state == eResState::BuildASWrite;
}