#include "ResourceDX.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <d3d12.h>

#include "../SmallVector.h"
#include "BufferDX.h"
#include "TextureAtlasDX.h"
#include "TextureDX.h"

namespace Ray {
namespace Dx {
const D3D12_RESOURCE_STATES g_resource_states[] = {
    D3D12_RESOURCE_STATE_COMMON,                            // Undefined
    D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,        // VertexBuffer
    D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,        // UniformBuffer
    D3D12_RESOURCE_STATE_INDEX_BUFFER,                      // IndexBuffer
    D3D12_RESOURCE_STATE_RENDER_TARGET,                     // RenderTarget
    D3D12_RESOURCE_STATE_UNORDERED_ACCESS,                  // UnorderedAccess
    D3D12_RESOURCE_STATE_DEPTH_READ,                        // DepthRead
    D3D12_RESOURCE_STATE_DEPTH_WRITE,                       // DepthWrite
    D3D12_RESOURCE_STATE_DEPTH_READ,                        // StencilTestDepthFetch
    D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,               // ShaderResource
    D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT,                 // IndirectArgument
    D3D12_RESOURCE_STATE_COPY_DEST,                         // CopyDst
    D3D12_RESOURCE_STATE_COPY_SOURCE,                       // CopySrc
    D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, // BuildASRead
    D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, // BuildASWrite
    D3D12_RESOURCE_STATE_GENERIC_READ                       // RayTracing
};
static_assert(COUNT_OF(g_resource_states) == int(eResState::_Count), "!");

} // namespace Dx
} // namespace Ray

D3D12_RESOURCE_STATES Ray::Dx::DXResourceState(const eResState state) { return g_resource_states[int(state)]; }

void Ray::Dx::TransitionResourceStates(ID3D12GraphicsCommandList *cmd_buf, const eStageBits src_stages_mask,
                                       const eStageBits dst_stages_mask, Span<const TransitionInfo> transitions) {
    SmallVector<D3D12_RESOURCE_BARRIER, 64> barriers;

    for (const TransitionInfo &tr : transitions) {
        if (tr.type == eResType::Tex2D && tr.p_tex->ready()) {
            eResState old_state = tr.old_state;
            if (old_state == eResState::Undefined) {
                // take state from resource itself
                old_state = tr.p_tex->resource_state;
                if (old_state == tr.new_state && old_state != eResState::UnorderedAccess) {
                    // transition is not needed
                    continue;
                }
            }

            auto &new_barrier = barriers.emplace_back();
            if (old_state != tr.new_state) {
                new_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                new_barrier.Transition.pResource = tr.p_tex->handle().img;
                new_barrier.Transition.StateBefore = DXResourceState(old_state);
                new_barrier.Transition.StateAfter = DXResourceState(tr.new_state);
                new_barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            } else {
                new_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
                new_barrier.UAV.pResource = tr.p_tex->handle().img;
            }

            if (tr.update_internal_state) {
                tr.p_tex->resource_state = tr.new_state;
            }
        } else if (tr.type == eResType::Tex3D) {
            eResState old_state = tr.old_state;
            if (old_state == eResState::Undefined) {
                // take state from resource itself
                old_state = tr.p_3dtex->resource_state;
                if (old_state == tr.new_state && old_state != eResState::UnorderedAccess) {
                    // transition is not needed
                    continue;
                }
            }

            auto &new_barrier = barriers.emplace_back();
            if (old_state != tr.new_state) {
                new_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                new_barrier.Transition.pResource = tr.p_3dtex->handle().img;
                new_barrier.Transition.StateBefore = DXResourceState(old_state);
                new_barrier.Transition.StateAfter = DXResourceState(tr.new_state);
                new_barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            } else {
                new_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
                new_barrier.UAV.pResource = tr.p_3dtex->handle().img;
            }

            if (tr.update_internal_state) {
                tr.p_3dtex->resource_state = tr.new_state;
            }
        } else if (tr.type == eResType::Buffer && *tr.p_buf) {
            eResState old_state = tr.old_state;
            if (old_state == eResState::Undefined) {
                // take state from resource itself
                old_state = tr.p_buf->resource_state;
                if (old_state == tr.new_state && old_state != eResState::UnorderedAccess) {
                    // transition is not needed
                    continue;
                }
            }

            auto &new_barrier = barriers.emplace_back();
            if (old_state != tr.new_state) {
                new_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                new_barrier.Transition.pResource = tr.p_buf->dx_resource();
                new_barrier.Transition.StateBefore = DXResourceState(old_state);
                new_barrier.Transition.StateAfter = DXResourceState(tr.new_state);
                new_barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            } else {
                new_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
                new_barrier.UAV.pResource = tr.p_buf->dx_resource();
            }

            if (tr.update_internal_state) {
                tr.p_buf->resource_state = tr.new_state;
            }
        } else if (tr.type == eResType::TexAtlas && tr.p_tex_arr->page_count()) {
            eResState old_state = tr.old_state;
            if (old_state == eResState::Undefined) {
                // take state from resource itself
                old_state = tr.p_tex_arr->resource_state;
                if (old_state == tr.new_state && old_state != eResState::UnorderedAccess) {
                    // transition is not needed
                    continue;
                }
            }

            auto &new_barrier = barriers.emplace_back();
            if (old_state != tr.new_state) {
                new_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                new_barrier.Transition.pResource = tr.p_tex_arr->dx_resource();
                new_barrier.Transition.StateBefore = DXResourceState(old_state);
                new_barrier.Transition.StateAfter = DXResourceState(tr.new_state);
                new_barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            } else {
                new_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
                new_barrier.UAV.pResource = tr.p_tex_arr->dx_resource();
            }

            if (tr.update_internal_state) {
                tr.p_tex_arr->resource_state = tr.new_state;
            }
        }
    }

    if (!barriers.empty()) {
        cmd_buf->ResourceBarrier(UINT(barriers.size()), barriers.data());
    }
}