#include "RpSkydome.h"

#include "../DebugMarker.h"
#include "../Renderer_Structs.h"

#include <Ren/Context.h>

namespace RpSkydomeInternal {
extern const float __skydome_positions[];
extern const int __skydome_vertices_count;
} // namespace RpSkydomeInternal

void RpSkydome::DrawSkydome(Graph::RpBuilder &builder) {
    using namespace RpSkydomeInternal;

    Ren::RastState rast_state;
    rast_state.cull_face.enabled = true;
    rast_state.cull_face.face = Ren::eCullFace::Back;
    rast_state.depth_test.enabled = true;
    rast_state.depth_test.func = Ren::eTestFunc::Always;
    rast_state.blend.enabled = false;

    rast_state.stencil.enabled = true;
    rast_state.stencil.mask = 0xff;
    rast_state.stencil.pass = Ren::eStencilOp::Replace;

    rast_state.viewport[2] = view_state_->act_res[0];
    rast_state.viewport[3] = view_state_->act_res[1];
    // Draw skydome without multisampling (not sure if it helps)
    rast_state.multisample = false;

    rast_state.Apply();

    Graph::AllocatedBuffer &unif_shared_data_buf = builder.GetReadBuffer(input_[0]);
    glBindBufferBase(GL_UNIFORM_BUFFER, REN_UB_SHARED_DATA_LOC,
                     (GLuint)unif_shared_data_buf.ref->id());

#if defined(REN_DIRECT_DRAWING)
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
#else
    glBindFramebuffer(GL_FRAMEBUFFER, GLuint(cached_fb_.id()));
#endif
    glUseProgram(skydome_prog_->id());

    glBindVertexArray(skydome_vao_.id());

    Ren::Mat4f translate_matrix;
    translate_matrix = Ren::Translate(translate_matrix, draw_cam_pos_);

    Ren::Mat4f scale_matrix;
    scale_matrix = Ren::Scale(scale_matrix, Ren::Vec3f{5000.0f, 5000.0f, 5000.0f});

    const Ren::Mat4f world_from_object = translate_matrix * scale_matrix;
    glUniformMatrix4fv(REN_U_M_MATRIX_LOC, 1, GL_FALSE, Ren::ValuePtr(world_from_object));

    ren_glBindTextureUnit_Comp(GL_TEXTURE_CUBE_MAP, REN_BASE0_TEX_SLOT,
                               env_->env_map->id());

    glDrawElements(
        GL_TRIANGLES, GLsizei(skydome_mesh_->indices_buf().size / sizeof(uint32_t)),
        GL_UNSIGNED_INT, (void *)uintptr_t(skydome_mesh_->indices_buf().offset));

    glDepthFunc(GL_LESS);

    glDisable(GL_STENCIL_TEST);

    glEnable(GL_MULTISAMPLE);
}
