#include "ModlApp.h"

#include <Ren/GL.h>

#include <Ren/MMat.h>

namespace {
    const int A_POS = 0;
    const int A_NORMAL = 1;
    const int A_TANGENT = 2;
    const int A_UVS1 = 3;
    const int A_UVS2 = 4;

    const int A_INDICES = 3;
    const int A_WEIGHTS = 4;

    const int U_MVP_MATR = 0;
    const int U_M_PALETTE = 1;

    const int U_MODE = 2;
    const int U_TEX = 3;
    const int U_NORM_TEX = 4;

    const int U_COL = 1;

    const int DIFFUSEMAP_SLOT = 0;
    const int NORMALMAP_SLOT = 1;

    inline void BindTexture(int slot, uint32_t tex) {
        glActiveTexture((GLenum)(GL_TEXTURE0 + slot));
        glBindTexture(GL_TEXTURE_2D, (GLuint)tex);
    }
}

void ModlApp::DrawMeshSimple(Ren::MeshRef &ref) {
    using namespace Ren;

    auto m		= ref.get();
    auto mat	= m->strip(0).mat.get();
    auto p      = mat->program();

    glBindBuffer(GL_ARRAY_BUFFER, m->attribs_buf_id());
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m->indices_buf_id());

    if (view_mode_ == Material) {
        
    } else {
        p = diag_prog_;

        glUniform1f(p->uniform(U_MODE).loc, (float)view_mode_);
        glUniform1i(p->uniform(U_TEX).loc, DIFFUSEMAP_SLOT);
        glUniform1i(p->uniform(U_NORM_TEX).loc, NORMALMAP_SLOT);

        //glBindTexture(GL_TEXTURE0 + DIFFUSEMAP_SLOT, checker_tex_->tex_id());
    }

    glUseProgram(p->prog_id());

    Mat4f world_from_object = Mat4f{ 1.0f };

    world_from_object = Rotate(world_from_object, angle_x_, { 1, 0, 0 });
    world_from_object = Rotate(world_from_object, angle_y_, { 0, 1, 0 });

    Mat4f view_from_world = cam_.view_matrix(),
          proj_from_view = cam_.projection_matrix();

    Mat4f view_from_object = view_from_world * world_from_object,
          proj_from_object = proj_from_view * view_from_object;

    glUniformMatrix4fv(p->uniform(U_MVP_MATR).loc, 1, GL_FALSE, ValuePtr(proj_from_object));

    glUniform3f(p->uniform(U_COL).loc, 1.0f, 1.0f, 1.0f);

    int stride = sizeof(float) * 13;
    glEnableVertexAttribArray(p->attribute(A_POS).loc);
    glVertexAttribPointer(p->attribute(A_POS).loc, 3, GL_FLOAT, GL_FALSE, stride, (void *)0);

    glEnableVertexAttribArray(p->attribute(A_NORMAL).loc);
    glVertexAttribPointer(p->attribute(A_NORMAL).loc, 3, GL_FLOAT, GL_FALSE, stride, (void *)(3 * sizeof(float)));

    glEnableVertexAttribArray(p->attribute(A_TANGENT).loc);
    glVertexAttribPointer(p->attribute(A_TANGENT).loc, 3, GL_FLOAT, GL_FALSE, stride, (void *)(6 * sizeof(float)));

    glEnableVertexAttribArray(p->attribute(A_UVS1).loc);
    glVertexAttribPointer(p->attribute(A_UVS1).loc, 3, GL_FLOAT, GL_FALSE, stride, (void *)(9 * sizeof(float)));

    glEnableVertexAttribArray(p->attribute(A_UVS2).loc);
    glVertexAttribPointer(p->attribute(A_UVS2).loc, 3, GL_FLOAT, GL_FALSE, stride, (void *)(11 * sizeof(float)));

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    const Ren::TriStrip *s = &m->strip(0);
    while (s->offset != -1) {
        const Ren::Material *mat = s->mat.get();

        if (view_mode_ == DiagUVs1 || view_mode_ == DiagUVs2) {
            BindTexture(DIFFUSEMAP_SLOT, checker_tex_->tex_id());
        } else {
            BindTexture(DIFFUSEMAP_SLOT, mat->texture(0)->tex_id());
        }
        BindTexture(NORMALMAP_SLOT, mat->texture(1)->tex_id());

        glDrawElements(GL_TRIANGLES, s->num_indices, GL_UNSIGNED_INT, (void *)uintptr_t(s->offset));
        ++s;
    }

    Ren::CheckError();
}

void ModlApp::DrawMeshSkeletal(Ren::MeshRef &ref, float dt_s) {
    using namespace Ren;

    auto m	    = ref.get();
    auto mat	= m->strip(0).mat.get();
    auto p		= mat->program();

    Ren::Skeleton *skel = m->skel();
    if (!skel->anims.empty()) {
        skel->UpdateAnim(0, dt_s, nullptr);
        skel->ApplyAnim(0);
        skel->UpdateBones();
    }

    glBindBuffer(GL_ARRAY_BUFFER, m->attribs_buf_id());
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m->indices_buf_id());

    glUseProgram(p->prog_id());

    static float angle = 0;
    angle += 0.0002f;

    Mat4f world_from_object = Rotate(Mat4f{ 1.0f }, angle, { 0, 1, 0 }),
          view_from_world = cam_.view_matrix(),
          proj_from_view = cam_.projection_matrix();

    Mat4f view_from_object = view_from_world * world_from_object,
          proj_from_object = proj_from_view * view_from_object;

    glUniformMatrix4fv(p->uniform(U_MVP_MATR).loc, 1, GL_FALSE, ValuePtr(proj_from_object));

    size_t num_bones = skel->matr_palette.size();
    glUniformMatrix4fv(p->uniform(U_M_PALETTE).loc, (GLsizei)num_bones, GL_FALSE, ValuePtr(skel->matr_palette[0]));

    int stride = sizeof(float) * 16;
    glEnableVertexAttribArray(p->attribute(A_POS).loc);
    glVertexAttribPointer(p->attribute(A_POS).loc, 3, GL_FLOAT, GL_FALSE, stride, (void *)0);

    glEnableVertexAttribArray(p->attribute(A_NORMAL).loc);
    glVertexAttribPointer(p->attribute(A_NORMAL).loc, 3, GL_FLOAT, GL_FALSE, stride, (void *)(3 * sizeof(float)));

    glEnableVertexAttribArray(p->attribute(A_INDICES).loc);
    glVertexAttribPointer(p->attribute(A_INDICES).loc, 4, GL_FLOAT, GL_FALSE, stride, (void *) (0 + 8 * sizeof(float)));

    glEnableVertexAttribArray(p->attribute(A_WEIGHTS).loc);
    glVertexAttribPointer(p->attribute(A_WEIGHTS).loc, 4, GL_FLOAT, GL_FALSE, stride, (void *) (0 + 12 * sizeof(float)));

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    const Ren::TriStrip *s = &m->strip(0);
    while (s->offset != -1) {
        const Ren::Material *mat = s->mat.get();
        //R::BindTexture(0, mat->textures[0].tex_id);
        glDrawElements(GL_TRIANGLE_STRIP, s->num_indices, GL_UNSIGNED_SHORT,
                       (void *)uintptr_t(s->offset));
        ++s;
    }

	Ren::CheckError();
}

void ModlApp::ClearColorAndDepth(float r, float g, float b, float a) {
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}