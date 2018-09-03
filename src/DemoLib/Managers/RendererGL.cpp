#include "Renderer.h"

#include <Ren/Camera.h>
#include <Ren/GL.h>

namespace RendererConstants {
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

void Renderer::DrawObjects(const Ren::Camera &cam, const std::vector<SceneObject> &objects) {
    using namespace Ren;
    using namespace RendererConstants;

    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    for (const auto &obj : objects) {
        if (obj.flags & (HasDrawable | HasTransform)) {
            const auto &ref = obj.dr->mesh;
            const auto &tr = obj.tr;

            auto m = ref.get();
            auto mat = m->strip(0).mat.get();
            auto p = mat->program();

            glUseProgram(p->prog_id());

            glBindBuffer(GL_ARRAY_BUFFER, m->attribs_buf_id());
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m->indices_buf_id());

            glUniform1f(p->uniform(U_MODE).loc, (float)1);
            glUniform1i(p->uniform(U_TEX).loc, DIFFUSEMAP_SLOT);
            glUniform1i(p->uniform(U_NORM_TEX).loc, NORMALMAP_SLOT);

            Mat4f world_from_object = Mat4f{ 1.0f };

            Mat4f view_from_world = cam.view_matrix(),
                  proj_from_view = cam.projection_matrix();

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

                //if (view_mode_ == DiagUVs1 || view_mode_ == DiagUVs2) {
                //    BindTexture(DIFFUSEMAP_SLOT, checker_tex_->tex_id());
                //} else {
                    BindTexture(DIFFUSEMAP_SLOT, mat->texture(0)->tex_id());
                //}
                BindTexture(NORMALMAP_SLOT, mat->texture(1)->tex_id());

                glDrawElements(GL_TRIANGLES, s->num_indices, GL_UNSIGNED_INT, (void *)uintptr_t(s->offset));
                ++s;
            }

            Ren::CheckError();
        }
    }
}
