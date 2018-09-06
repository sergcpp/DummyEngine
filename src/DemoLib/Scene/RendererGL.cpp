#include "Renderer.h"

#include <Ren/Camera.h>
#include <Ren/Context.h>
#include <Ren/GL.h>

namespace RendererConstants {
    const char fillz_vs_shader[] = R"(
        /*
        ATTRIBUTES
	        aVertexPosition : 0
        UNIFORMS
	        uMVPMatrix : 0
        */

        attribute vec3 aVertexPosition;

        uniform mat4 uMVPMatrix;

        void main(void) {
            gl_Position = uMVPMatrix * vec4(aVertexPosition, 1.0);
        } 
    )";

    const char fillz_fs_shader[] = R"(
        #ifdef GL_ES
	        precision mediump float;
        #endif

        void main(void) {
        }
    )";

    const char shadow_vs_shader[] = R"(
        /*
        ATTRIBUTES
	        aVertexPosition : 0
        UNIFORMS
	        uMVPMatrix : 0
        */

        attribute vec3 aVertexPosition;
        uniform mat4 uMVPMatrix;

        void main(void) {
            gl_Position = uMVPMatrix * vec4(aVertexPosition, 1.0);
        } 
    )";

    const char shadow_fs_shader[] = R"(
        #ifdef GL_ES
            precision mediump float;
        #endif

        void main(void) {
	        gl_FragColor.r = gl_FragCoord.z;
        }
    )";

    const char blit_vs_shader[] = R"(
        /*
        ATTRIBUTES
	        aVertexPosition : 0
            aVertexUVs : 1
        */

        attribute vec2 aVertexPosition;
        attribute vec2 aVertexUVs;

        varying vec2 aVertexUVs_;

        void main(void) {
            aVertexUVs_ = aVertexUVs;
            gl_Position = vec4(aVertexPosition, 0.5, 1.0);
        } 
    )";

    const char blit_fs_shader[] = R"(
        #ifdef GL_ES
	        precision mediump float;
        #endif

        /*
        UNIFORMS
            s_texture : 0
        */
        
        uniform sampler2D s_texture;

        varying vec2 aVertexUVs_;

        void main(void) {
            gl_FragColor = texture2D(s_texture, aVertexUVs_);
        }
    )";

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

    const bool DEPTH_PREPASS = false;
}

void Renderer::InitShadersInternal() {
    using namespace RendererConstants;

    Ren::eProgLoadStatus status;
    fill_depth_prog_ = ctx_.LoadProgramGLSL("fill_depth", fillz_vs_shader, fillz_fs_shader, &status);
    assert(status == Ren::ProgCreatedFromData);

    shadow_prog_ = ctx_.LoadProgramGLSL("shadow", shadow_vs_shader, shadow_fs_shader, &status);
    assert(status == Ren::ProgCreatedFromData);

    blit_prog_ = ctx_.LoadProgramGLSL("blit", blit_vs_shader, blit_fs_shader, &status);
    assert(status == Ren::ProgCreatedFromData);
}

void Renderer::DrawObjectsInternal(const DrawableItem *drawables, size_t drawable_count,
                                   const DrawableItem *shadow_drawables, size_t shadow_drawable_count) {
    using namespace Ren;
    using namespace RendererConstants;

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);

    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    const Ren::Program *cur_program = nullptr;
    const Ren::Material *cur_mat = nullptr;
    const Ren::Mesh *cur_mesh = nullptr;
    const Ren::Mat4f *cur_xform = nullptr;

    if (shadow_drawable_count) {
        cur_program = shadow_prog_.get();
        glUseProgram(cur_program->prog_id());

        int stride = sizeof(float) * 13;
        glEnableVertexAttribArray(cur_program->attribute(A_POS).loc);
        glVertexAttribPointer(cur_program->attribute(A_POS).loc, 3, GL_FLOAT, GL_FALSE, stride, (void *)0);

        int32_t viewport_before[4];

        glBindFramebuffer(GL_FRAMEBUFFER, shadow_buf_.fb);
        glGetIntegerv(GL_VIEWPORT, viewport_before);
        glViewport(0, 0, shadow_buf_.w, shadow_buf_.h);
        
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // draw shadow map
        for (size_t i = 0; i < shadow_drawable_count; i++) {
            const auto &dr = shadow_drawables[i];

            const Ren::Mat4f *xform = dr.xform;
            const Ren::Mesh *mesh = dr.mesh;
            const Ren::TriStrip *strip = dr.strip;

            if (mesh != cur_mesh) {
                glBindBuffer(GL_ARRAY_BUFFER, mesh->attribs_buf_id());
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->indices_buf_id());

                int stride = sizeof(float) * 13;
                glEnableVertexAttribArray(cur_program->attribute(A_POS).loc);
                glVertexAttribPointer(cur_program->attribute(A_POS).loc, 3, GL_FLOAT, GL_FALSE, stride, (void *)0);

                cur_mesh = mesh;
            }

            if (xform != cur_xform) {
                const auto &proj_from_object = *xform;

                glUniformMatrix4fv(cur_program->uniform(U_MVP_MATR).loc, 1, GL_FALSE, ValuePtr(proj_from_object));

                cur_xform = xform;
            }

            glDrawElements(GL_TRIANGLES, strip->num_indices, GL_UNSIGNED_INT, (void *)uintptr_t(strip->offset));
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(viewport_before[0], viewport_before[1], viewport_before[2], viewport_before[3]);
    }

    if (DEPTH_PREPASS && !wireframe_mode_) {
        glDepthFunc(GL_LESS);

        cur_program = fill_depth_prog_.get();
        glUseProgram(cur_program->prog_id());

        int stride = sizeof(float) * 13;
        glEnableVertexAttribArray(cur_program->attribute(A_POS).loc);
        glVertexAttribPointer(cur_program->attribute(A_POS).loc, 3, GL_FLOAT, GL_FALSE, stride, (void *)0);

        // fill depth
        for (size_t i = 0; i < drawable_count; i++) {
            const auto &dr = drawables[i];

            const Ren::Mat4f *xform = dr.xform;
            const Ren::Mesh *mesh = dr.mesh;
            const Ren::TriStrip *strip = dr.strip;

            if (mesh != cur_mesh) {
                glBindBuffer(GL_ARRAY_BUFFER, mesh->attribs_buf_id());
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->indices_buf_id());

                int stride = sizeof(float) * 13;
                glEnableVertexAttribArray(cur_program->attribute(A_POS).loc);
                glVertexAttribPointer(cur_program->attribute(A_POS).loc, 3, GL_FLOAT, GL_FALSE, stride, (void *)0);

                cur_mesh = mesh;
            }

            if (xform != cur_xform) {
                const auto &proj_from_object = *xform;

                glUniformMatrix4fv(cur_program->uniform(U_MVP_MATR).loc, 1, GL_FALSE, ValuePtr(proj_from_object));

                cur_xform = xform;
            }

            glDrawElements(GL_TRIANGLES, strip->num_indices, GL_UNSIGNED_INT, (void *)uintptr_t(strip->offset));
        }

        glDepthFunc(GL_EQUAL);
    }

    if (wireframe_mode_) {
        glDepthFunc(GL_LEQUAL);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    } else {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    // actual drawing
    for (size_t i = 0; i < drawable_count; i++) {
        const auto &dr = drawables[i];

        const Ren::Mat4f *xform = dr.xform;
        const Ren::Material *mat = dr.mat;
        const Ren::Mesh *mesh = dr.mesh;
        const Ren::TriStrip *strip = dr.strip;

        const auto *p = mat->program().get();

        if (p != cur_program) {
            glUseProgram(p->prog_id());

            if (mesh == cur_mesh) {
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
            }

            glUniform1i(p->uniform(U_TEX).loc, DIFFUSEMAP_SLOT);
            glUniform1i(p->uniform(U_NORM_TEX).loc, NORMALMAP_SLOT);

            glUniform3f(p->uniform(U_COL).loc, 1.0f, 1.0f, 1.0f);
            
            if (xform == cur_xform) {
                const auto &proj_from_object = *xform;
                glUniformMatrix4fv(p->uniform(U_MVP_MATR).loc, 1, GL_FALSE, ValuePtr(proj_from_object));
            }

            cur_program = p;
        }

        if (mesh != cur_mesh) {
            glBindBuffer(GL_ARRAY_BUFFER, mesh->attribs_buf_id());
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->indices_buf_id());

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

            cur_mesh = mesh;
        }

        if (xform != cur_xform) {
            const auto &proj_from_object = *xform;

            glUniformMatrix4fv(p->uniform(U_MVP_MATR).loc, 1, GL_FALSE, ValuePtr(proj_from_object));

            cur_xform = xform;
        }

        if (mat != cur_mat) {
            BindTexture(DIFFUSEMAP_SLOT, mat->texture(0)->tex_id());
            BindTexture(NORMALMAP_SLOT, mat->texture(1)->tex_id());
            cur_mat = mat;
        }

        glDrawElements(GL_TRIANGLES, strip->num_indices, GL_UNSIGNED_INT, (void *)uintptr_t(strip->offset));
    }

    if (debug_cull_ && culling_enabled_ && !depth_pixels_[0].empty()) {
        glUseProgram(0);

        glRasterPos2f(-1, -1);

        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glDrawPixels(256, 128, GL_RGBA, GL_UNSIGNED_BYTE, &depth_pixels_[0][0]);

        glRasterPos2f(-1 + 2 * float(256) / ctx_.w(), -1);
        glDrawPixels(256, 128, GL_RGBA, GL_UNSIGNED_BYTE, &depth_tiles_[0][0]);
    }

    if (debug_shadow_) {
        cur_program = blit_prog_.get();
        glUseProgram(cur_program->prog_id());

        glDisable(GL_CULL_FACE);

        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);

        float k = float(ctx_.w()) / ctx_.h();

        const float positions[] = { -1.0f, -1.0f,                       -1.0f + 0.25f, -1.0f,
                                    -1.0f + 0.25f, -1.0f + 0.25f * k,     -1.0f, -1.0f + 0.25f * k };

        const float uvs[] = { 0.0f, 0.0f,       1.0f, 0.0f,
                              1.0f, 1.0f,       0.0f, 1.0f };

        const uint8_t indices[] = { 0, 1, 2,    0, 2, 3 };

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

        glEnableVertexAttribArray(cur_program->attribute("aVertexPosition").loc);
        glVertexAttribPointer(cur_program->attribute("aVertexPosition").loc, 2, GL_FLOAT, GL_FALSE, 0, &positions[0]);

        glEnableVertexAttribArray(cur_program->attribute("aVertexUVs").loc);
        glVertexAttribPointer(cur_program->attribute("aVertexUVs").loc, 2, GL_FLOAT, GL_FALSE, 0, &uvs[0]);

        glUniform1i(cur_program->uniform("s_texture").loc, DIFFUSEMAP_SLOT);

        BindTexture(DIFFUSEMAP_SLOT, shadow_buf_.col_tex);

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, &indices[0]);
    }

#if 0
    glFinish();
#endif
}
