#include "Resource.h"

#include "GL.h"
#include "Image.h"

void Ren::TransitionResourceStates(ApiContext *api_context, CommandBuffer cmd_buf,
                                   const Ren::Bitmask<Ren::eStage> src_stages_mask,
                                   const Ren::Bitmask<Ren::eStage> dst_stages_mask,
                                   Span<const TransitionInfo> transitions) {
    GLbitfield mem_barrier_bits = 0;

    for (const TransitionInfo &tr : transitions) {
        if (std::holds_alternative<const Image *>(tr.p_res)) {
            eResState old_state = tr.old_state;
            if (old_state == eResState::Undefined) {
                // take state from resource itself
                old_state = std::get<const Image *>(tr.p_res)->resource_state;
                if (old_state == tr.new_state && old_state != eResState::UnorderedAccess) {
                    // transition is not needed
                    continue;
                }
            }

            if (old_state == eResState::UnorderedAccess) {
                mem_barrier_bits |= GL_SHADER_IMAGE_ACCESS_BARRIER_BIT;
                if (tr.new_state == eResState::ShaderResource || tr.new_state == eResState::StencilTestDepthFetch) {
                    mem_barrier_bits |= GL_TEXTURE_FETCH_BARRIER_BIT;
                }
            }

            if (tr.update_internal_state) {
                std::get<const Image *>(tr.p_res)->resource_state = tr.new_state;
            }
        } else if (std::holds_alternative<const Buffer *>(tr.p_res)) {
            eResState old_state = tr.old_state;
            if (old_state == eResState::Undefined) {
                // take state from resource itself
                old_state = std::get<const Buffer *>(tr.p_res)->resource_state;
                if (old_state == tr.new_state && old_state != eResState::UnorderedAccess) {
                    // transition is not needed
                    continue;
                }
            }

            if (old_state == eResState::UnorderedAccess) {
                if (std::get<const Buffer *>(tr.p_res)->type() == eBufType::VertexAttribs) {
                    mem_barrier_bits |= GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT;
                } else if (std::get<const Buffer *>(tr.p_res)->type() == eBufType::VertexIndices) {
                    mem_barrier_bits |= GL_ELEMENT_ARRAY_BARRIER_BIT;
                } else if (std::get<const Buffer *>(tr.p_res)->type() == eBufType::Uniform) {
                    mem_barrier_bits |= GL_UNIFORM_BARRIER_BIT;
                } else if (std::get<const Buffer *>(tr.p_res)->type() == eBufType::Storage) {
                    mem_barrier_bits |= GL_SHADER_STORAGE_BARRIER_BIT;
                } else if (std::get<const Buffer *>(tr.p_res)->type() == eBufType::Texture) {
                    mem_barrier_bits |= GL_TEXTURE_FETCH_BARRIER_BIT;
                }
            }

            if (tr.update_internal_state) {
                std::get<const Buffer *>(tr.p_res)->resource_state = tr.new_state;
            }
        }
    }

    if (mem_barrier_bits) {
        glMemoryBarrier(mem_barrier_bits);
    }
}
