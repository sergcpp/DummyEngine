#include "PrimDraw.h"

#include <Ren/Context.h>
#include <Ren/Framebuffer.h>
#include <Ren/GL.h>
#include <Ren/ProbeStorage.h>

#include "Renderer_Structs.h"

namespace PrimDrawInternal {
extern const int SphereIndicesCount;
} // namespace PrimDrawInternal

Eng::PrimDraw::~PrimDraw() = default;

void Eng::PrimDraw::DrawPrim(const ePrim prim, const RenderTarget &rt, Ren::Program *p,
                             Ren::Span<const Ren::Binding> bindings, Ren::Span<const Uniform> uniforms) {
    using namespace PrimDrawInternal;

    glBindFramebuffer(GL_FRAMEBUFFER, rt.fb->id());
    // glViewport(rt.viewport[0], rt.viewport[1], rt.viewport[2], rt.viewport[3]);

    for (const auto &b : bindings) {
        if (b.trg == Ren::eBindTarget::UBuf) {
            if (b.offset) {
                assert(b.size != 0);
                glBindBufferRange(GL_UNIFORM_BUFFER, b.loc, b.handle.buf->id(), b.offset, b.size);
            } else {
                glBindBufferBase(GL_UNIFORM_BUFFER, b.loc, b.handle.buf->id());
            }
        } else if (b.trg == Ren::eBindTarget::TexCubeArray) {
            ren_glBindTextureUnit_Comp(Ren::GLBindTarget(b.trg), GLuint(b.loc),
                                       GLuint(b.handle.cube_arr ? b.handle.cube_arr->handle().id : 0));
        } else if (b.trg == Ren::eBindTarget::TBuf) {
            ren_glBindTextureUnit_Comp(Ren::GLBindTarget(b.trg), GLuint(b.loc), GLuint(b.handle.tex_buf->id()));
        } else {
            ren_glBindTextureUnit_Comp(Ren::GLBindTarget(b.trg), GLuint(b.loc), GLuint(b.handle.tex->id()));
        }
    }

    glUseProgram(p->id());

    for (const auto &u : uniforms) {
        if (u.type == Ren::eType::Float32) {
            if (u.size == 1) {
                glUniform1f(GLint(u.loc), u.fdata[0]);
            } else if (u.size == 2) {
                glUniform2f(GLint(u.loc), u.fdata[0], u.fdata[1]);
            } else if (u.size == 3) {
                glUniform3f(GLint(u.loc), u.fdata[0], u.fdata[1], u.fdata[2]);
            } else if (u.size == 4) {
                glUniform4f(GLint(u.loc), u.fdata[0], u.fdata[1], u.fdata[2], u.fdata[3]);
            } else {
                assert(u.size % 4 == 0);
                glUniformMatrix4fv(GLint(u.loc), 1, GL_FALSE, u.pfdata);
            }
        } else if (u.type == Ren::eType::Int32) {
            if (u.size == 1) {
                glUniform1i(GLint(u.loc), u.idata[0]);
            } else if (u.size == 2) {
                glUniform2i(GLint(u.loc), u.idata[0], u.idata[1]);
            } else if (u.size == 3) {
                glUniform3i(GLint(u.loc), u.idata[0], u.idata[1], u.idata[2]);
            } else if (u.size == 4) {
                glUniform4i(GLint(u.loc), u.idata[0], u.idata[1], u.idata[2], u.idata[3]);
            } else {
                assert(u.size % 4 == 0);
                glUniform4iv(GLint(u.loc), GLsizei(u.size / 4), u.pidata);
            }
        }
    }

    if (prim == ePrim::Quad) {
        glBindVertexArray(fs_quad_vtx_input_.gl_vao());
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, (const GLvoid *)uintptr_t(quad_ndx_.offset));
    } else if (prim == ePrim::Sphere) {
        glBindVertexArray(sphere_vtx_input_.gl_vao());
        glDrawElements(GL_TRIANGLES, GLsizei(SphereIndicesCount), GL_UNSIGNED_SHORT,
                       (void *)uintptr_t(sphere_ndx_.offset));
    }

#ifndef NDEBUG
    Ren::ResetGLState();
#endif
}

void Eng::PrimDraw::DrawPrim(ePrim prim, const Ren::ProgramRef &p, Ren::Span<const Ren::RenderTarget> color_rts,
                             Ren::RenderTarget depth_rt, const Ren::RastState &new_rast_state,
                             Ren::RastState &applied_rast_state, Ren::Span<const Ren::Binding> bindings,
                             const void *uniform_data, int uniform_data_len, int uniform_data_offset) {
    using namespace PrimDrawInternal;

    const Ren::Framebuffer *fb =
        FindOrCreateFramebuffer(nullptr, color_rts, new_rast_state.depth.test_enabled ? depth_rt : Ren::RenderTarget{},
                                new_rast_state.stencil.enabled ? depth_rt : Ren::RenderTarget{});

    new_rast_state.ApplyChanged(applied_rast_state);
    applied_rast_state = new_rast_state;

    glBindFramebuffer(GL_FRAMEBUFFER, fb->id());
    // glViewport(rt.viewport[0], rt.viewport[1], rt.viewport[2], rt.viewport[3]);

    for (const auto &b : bindings) {
        if (b.trg == Ren::eBindTarget::UBuf) {
            if (b.offset) {
                assert(b.size != 0);
                glBindBufferRange(GL_UNIFORM_BUFFER, b.loc, b.handle.buf->id(), b.offset, b.size);
            } else {
                glBindBufferBase(GL_UNIFORM_BUFFER, b.loc, b.handle.buf->id());
            }
        } else if (b.trg == Ren::eBindTarget::TexCubeArray) {
            ren_glBindTextureUnit_Comp(Ren::GLBindTarget(b.trg), GLuint(b.loc), GLuint(b.handle.cube_arr->handle().id));
        } else if (b.trg == Ren::eBindTarget::TBuf) {
            ren_glBindTextureUnit_Comp(Ren::GLBindTarget(b.trg), GLuint(b.loc), GLuint(b.handle.tex_buf->id()));
        } else if (b.trg == Ren::eBindTarget::Tex3D) {
            ren_glBindTextureUnit_Comp(Ren::GLBindTarget(b.trg), GLuint(b.loc), GLuint(b.handle.tex3d->id()));
        } else if (b.trg == Ren::eBindTarget::Tex2DArray) {
            ren_glBindTextureUnit_Comp(Ren::GLBindTarget(b.trg), GLuint(b.loc), GLuint(b.handle.tex2d_arr->id()));
        } else {
            ren_glBindTextureUnit_Comp(Ren::GLBindTarget(b.trg), GLuint(b.loc), GLuint(b.handle.tex->id()));
        }
    }

    glUseProgram(p->id());

    Ren::Buffer temp_stage_buffer, temp_unif_buffer;
    if (uniform_data) {
        temp_stage_buffer = Ren::Buffer("Temp upload buf", ctx_->api_ctx(), Ren::eBufType::Upload, uniform_data_len, 16);
        {
            uint8_t *stage_data = temp_stage_buffer.Map();
            memcpy(stage_data, uniform_data, uniform_data_len);
            temp_stage_buffer.Unmap();
        }
        temp_unif_buffer =
            Ren::Buffer("Temp uniform buf", ctx_->api_ctx(), Ren::eBufType::Uniform, uniform_data_len, 16);
        Ren::CopyBufferToBuffer(temp_stage_buffer, 0, temp_unif_buffer, 0, uniform_data_len, nullptr);

        glBindBufferBase(GL_UNIFORM_BUFFER, Eng::BIND_UB_UNIF_PARAM_BUF, temp_unif_buffer.id());
    }

    if (prim == ePrim::Quad) {
        glBindVertexArray(fs_quad_vtx_input_.gl_vao());
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, (const GLvoid *)uintptr_t(quad_ndx_.offset));
    } else if (prim == ePrim::Sphere) {
        glBindVertexArray(sphere_vtx_input_.gl_vao());
        glDrawElements(GL_TRIANGLES, GLsizei(SphereIndicesCount), GL_UNSIGNED_SHORT,
                       (void *)uintptr_t(sphere_ndx_.offset));
    }

#ifndef NDEBUG
    Ren::ResetGLState();
#endif
}