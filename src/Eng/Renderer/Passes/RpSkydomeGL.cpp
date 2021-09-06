#include "RpSkydome.h"

#include <Ren/Context.h>
#include <Ren/GL.h>

#include "../../Renderer/PrimDraw.h"

#include "../Renderer_Structs.h"
#include "../assets/shaders/internal/skydome_interface.glsl"

void RpSkydome::DrawSkydome(RpBuilder &builder, RpAllocBuf &vtx_buf1, RpAllocBuf &vtx_buf2, RpAllocBuf &ndx_buf,
                            RpAllocTex &color_tex, RpAllocTex &spec_tex, RpAllocTex &depth_tex) {
    Ren::RastState rast_state = pipeline_.rast_state();
    rast_state.viewport[2] = view_state_->act_res[0];
    rast_state.viewport[3] = view_state_->act_res[1];
    rast_state.ApplyChanged(builder.rast_state());
    builder.rast_state() = rast_state;

    RpAllocBuf &unif_shared_data_buf = builder.GetReadBuffer(shared_data_buf_);
    RpAllocTex &env_tex = builder.GetReadTexture(env_tex_);

    glBindBufferBase(GL_UNIFORM_BUFFER, REN_UB_SHARED_DATA_LOC, GLuint(unif_shared_data_buf.ref->id()));

#if defined(REN_DIRECT_DRAWING)
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
#else
    glBindFramebuffer(GL_FRAMEBUFFER, GLuint(framebuf_[builder.ctx().backend_frame()].id()));
#endif
    glUseProgram(pipeline_.prog()->id());

    glBindVertexArray(pipeline_.vtx_input()->gl_vao());

    Ren::Mat4f translate_matrix;
    translate_matrix = Ren::Translate(translate_matrix, draw_cam_pos_);

    Ren::Mat4f scale_matrix;
    scale_matrix = Ren::Scale(scale_matrix, Ren::Vec3f{5000.0f, 5000.0f, 5000.0f});

    const Ren::Mat4f world_from_object = translate_matrix * scale_matrix;
    glUniformMatrix4fv(Skydome::U_M_MATRIX_LOC, 1, GL_FALSE, Ren::ValuePtr(world_from_object));

    ren_glBindTextureUnit_Comp(GL_TEXTURE_CUBE_MAP, Skydome::ENV_TEX_SLOT, env_tex.ref->id());

    const Ren::Mesh *skydome_mesh = prim_draw_.skydome_mesh();
    glDrawElementsBaseVertex(GL_TRIANGLES, GLsizei(skydome_mesh->indices_buf().size / sizeof(uint32_t)),
                             GL_UNSIGNED_INT, (void *)uintptr_t(skydome_mesh->indices_buf().offset),
                             GLint(skydome_mesh->attribs_buf1().offset / 16));

    glDepthFunc(GL_LESS);

    glDisable(GL_STENCIL_TEST);

    glEnable(GL_MULTISAMPLE);
}

RpSkydome::~RpSkydome() = default;