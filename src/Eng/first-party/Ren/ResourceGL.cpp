#include "Resource.h"

#include "GL.h"
#include "Texture.h"

void Ren::TransitionResourceStates(Ren::ApiContext *api_context, CommandBuffer cmd_buf, const eStageBits src_stages_mask,
                                   const eStageBits dst_stages_mask, Span<const TransitionInfo> transitions) {
    GLbitfield mem_barrier_bits = 0;

    for (int i = 0; i < int(transitions.size()); i++) {
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

            if (old_state == eResState::UnorderedAccess) {
                mem_barrier_bits |= GL_SHADER_IMAGE_ACCESS_BARRIER_BIT;
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

            if (old_state == eResState::UnorderedAccess) {
                if (transitions[i].p_buf->type() == Ren::eBufType::VertexAttribs) {
                    mem_barrier_bits |= GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT;
                } else if (transitions[i].p_buf->type() == Ren::eBufType::VertexIndices) {
                    mem_barrier_bits |= GL_ELEMENT_ARRAY_BARRIER_BIT;
                } else if (transitions[i].p_buf->type() == Ren::eBufType::Uniform) {
                    mem_barrier_bits |= GL_UNIFORM_BARRIER_BIT;
                } else if (transitions[i].p_buf->type() == eBufType::Storage) {
                    mem_barrier_bits |= GL_SHADER_STORAGE_BARRIER_BIT;
                }
            }

            if (transitions[i].update_internal_state) {
                transitions[i].p_buf->resource_state = transitions[i].new_state;
            }
        }
    }

    if (mem_barrier_bits) {
        glMemoryBarrier(mem_barrier_bits);
    }
}
