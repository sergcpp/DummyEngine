#include "Renderer.h"

#include <Ren/Camera.h>
#include <Ren/Context.h>
#include <Ren/GL.h>

namespace RendererInternal {
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
            //gl_FragDepth = gl_FragCoord.z;
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

    const char blit_ms_fs_shader[] = R"(
        #extension GL_ARB_texture_multisample : require

        #ifdef GL_ES
	        precision mediump float;
        #endif

        /*
        UNIFORMS
            s_texture : 0
        */
        
        uniform sampler2DMS s_texture;

        varying vec2 aVertexUVs_;

        void main(void) {
            gl_FragColor = texelFetch(s_texture, ivec2(aVertexUVs_), 0) * 0.25 +
					       texelFetch(s_texture, ivec2(aVertexUVs_), 1) * 0.25 +
					       texelFetch(s_texture, ivec2(aVertexUVs_), 2) * 0.25 +
					       texelFetch(s_texture, ivec2(aVertexUVs_), 3) * 0.25;
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
    const int U_MV_MATR = 1;

    const int U_SH_MVP_MATR = 2;

    const int U_TEX = 3;
    const int U_NORM_TEX = 4;
    const int U_SHADOW_TEX = 5;

    const int U_SUN_DIR = 10;
    const int U_SUN_COL = 11;

    const int DIFFUSEMAP_SLOT = 0;
    const int NORMALMAP_SLOT = 1;
    const int SHADOWMAP_SLOT = 2;

    inline void BindTexture(int slot, uint32_t tex) {
        glActiveTexture((GLenum)(GL_TEXTURE0 + slot));
        glBindTexture(GL_TEXTURE_2D, (GLuint)tex);
    }

    const bool DEPTH_PREPASS = false;
}

void Renderer::InitShadersInternal() {
    using namespace RendererInternal;

    Ren::eProgLoadStatus status;
    fill_depth_prog_ = ctx_.LoadProgramGLSL("fill_depth", fillz_vs_shader, fillz_fs_shader, &status);
    assert(status == Ren::ProgCreatedFromData);

    shadow_prog_ = ctx_.LoadProgramGLSL("shadow", shadow_vs_shader, shadow_fs_shader, &status);
    assert(status == Ren::ProgCreatedFromData);

    blit_prog_ = ctx_.LoadProgramGLSL("blit", blit_vs_shader, blit_fs_shader, &status);
    assert(status == Ren::ProgCreatedFromData);

    blit_ms_prog_ = ctx_.LoadProgramGLSL("blit_ms", blit_vs_shader, blit_ms_fs_shader, &status);
    assert(status == Ren::ProgCreatedFromData);
}

void Renderer::DrawObjectsInternal(const DrawableItem *drawables, size_t drawable_count, const Ren::Mat4f shadow_transforms[4],
                                   const DrawableItem *shadow_drawables[4], size_t shadow_drawable_count[4], const Environment &env) {
    using namespace Ren;
    using namespace RendererInternal;

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);

    glClearColor(env.sky_col[0], env.sky_col[1], env.sky_col[2], 1.0f);
    //glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glDisable(GL_CULL_FACE);

    const Ren::Program *cur_program = nullptr;
    const Ren::Material *cur_mat = nullptr;
    const Ren::Mesh *cur_mesh = nullptr;
    const Ren::Mat4f *cur_clip_from_object = nullptr,
                     *cur_world_from_object = nullptr,
                     *cur_sh_clip_from_object[4] = { nullptr };

    int32_t viewport_before[4];
    glGetIntegerv(GL_VIEWPORT, viewport_before);

    {   // draw shadow map
        bool fb_bound = false;

        for (int casc = 0; casc < 4; casc++) {
            if (shadow_drawable_count[casc]) {
                if (cur_program != shadow_prog_.get()) {
                    cur_program = shadow_prog_.get();
                    glUseProgram(cur_program->prog_id());

                    int stride = sizeof(float) * 13;
                    glEnableVertexAttribArray(cur_program->attribute(A_POS).loc);
                    glVertexAttribPointer(cur_program->attribute(A_POS).loc, 3, GL_FLOAT, GL_FALSE, stride, (void *)0);
                }

                if (!fb_bound) {
                    glBindFramebuffer(GL_FRAMEBUFFER, shadow_buf_.fb);
                    glClear(GL_DEPTH_BUFFER_BIT);
                    fb_bound = true;
                }

                if (casc == 0) {
                    glViewport(0, 0, shadow_buf_.w / 2, shadow_buf_.h / 2);
                } else if (casc == 1) {
                    glViewport(shadow_buf_.w / 2, 0, shadow_buf_.w / 2, shadow_buf_.h / 2);
                } else if (casc == 2) {
                    glViewport(0, shadow_buf_.h / 2, shadow_buf_.w / 2, shadow_buf_.h / 2);
                } else {
                    glViewport(shadow_buf_.w / 2, shadow_buf_.h / 2, shadow_buf_.w / 2, shadow_buf_.h / 2);
                }
                
                for (size_t i = 0; i < shadow_drawable_count[casc]; i++) {
                    const auto &dr = shadow_drawables[casc][i];

                    const Ren::Mat4f *clip_from_object = dr.clip_from_object;
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

                    if (clip_from_object != cur_clip_from_object) {
                        glUniformMatrix4fv(cur_program->uniform(U_MVP_MATR).loc, 1, GL_FALSE, ValuePtr(clip_from_object));
                        cur_clip_from_object = clip_from_object;
                    }

                    glDrawElements(GL_TRIANGLES, strip->num_indices, GL_UNSIGNED_INT, (void *)uintptr_t(strip->offset));
                }
            }
        }
    }

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    // Bind main buffer for drawing
    glBindFramebuffer(GL_FRAMEBUFFER, clean_buf_.fb);
    glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

    glViewport(0, 0, clean_buf_.w, clean_buf_.h);

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

            const Ren::Mat4f *clip_from_object = dr.clip_from_object;
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

            if (clip_from_object != cur_clip_from_object) {
                glUniformMatrix4fv(cur_program->uniform(U_MVP_MATR).loc, 1, GL_FALSE, ValuePtr(clip_from_object));
                cur_clip_from_object = clip_from_object;
            }

            glDrawElements(GL_TRIANGLES, strip->num_indices, GL_UNSIGNED_INT, (void *)uintptr_t(strip->offset));
        }

        glDepthFunc(GL_EQUAL);
    }

    if (wireframe_mode_) {
        glDepthFunc(GL_LEQUAL);
        glDepthMask(GL_FALSE);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    } else {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    // actual drawing
    for (size_t i = 0; i < drawable_count; i++) {
        const auto &dr = drawables[i];

        const Ren::Mat4f *clip_from_object = dr.clip_from_object,
                         *world_from_object = dr.world_from_object,
                         *const *sh_clip_from_object = dr.sh_clip_from_object;
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
            glUniform1i(p->uniform(U_SHADOW_TEX).loc, SHADOWMAP_SLOT);

            glUniform3fv(p->uniform(U_SUN_DIR).loc, 1, Ren::ValuePtr(env.sun_dir));
            glUniform3fv(p->uniform(U_SUN_COL).loc, 1, Ren::ValuePtr(env.sun_col));

            if (clip_from_object == cur_clip_from_object) {
                glUniformMatrix4fv(p->uniform(U_MVP_MATR).loc, 1, GL_FALSE, ValuePtr(clip_from_object));
            }

            if (world_from_object == cur_world_from_object) {
                glUniformMatrix4fv(p->uniform(U_MV_MATR).loc, 1, GL_FALSE, ValuePtr(world_from_object));
            }

            BindTexture(SHADOWMAP_SLOT, shadow_buf_.depth_tex.GetValue());

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

        if (clip_from_object != cur_clip_from_object) {
            glUniformMatrix4fv(p->uniform(U_MVP_MATR).loc, 1, GL_FALSE, ValuePtr(clip_from_object));
            cur_clip_from_object = clip_from_object;
        }

        if (world_from_object != cur_world_from_object) {
            glUniformMatrix4fv(cur_program->uniform(U_MV_MATR).loc, 1, GL_FALSE, ValuePtr(world_from_object));
            cur_world_from_object = world_from_object;
        }

        {   // update shadow matrices
            for (int casc = 0; casc < 4; casc++) {
                const auto *_sh_clip_from_object = sh_clip_from_object[casc];
                if (_sh_clip_from_object && _sh_clip_from_object != cur_sh_clip_from_object[casc]) {
                    glUniformMatrix4fv(p->uniform(U_SH_MVP_MATR).loc + casc, 1, GL_FALSE, ValuePtr(_sh_clip_from_object));
                    cur_sh_clip_from_object[casc] = _sh_clip_from_object;
                }
            }
        }

        if (mat != cur_mat) {
            BindTexture(DIFFUSEMAP_SLOT, mat->texture(0)->tex_id());
            BindTexture(NORMALMAP_SLOT, mat->texture(1)->tex_id());
            cur_mat = mat;
        }

        glDrawElements(GL_TRIANGLES, strip->num_indices, GL_UNSIGNED_INT, (void *)uintptr_t(strip->offset));
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(viewport_before[0], viewport_before[1], viewport_before[2], viewport_before[3]);

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    {   // Blit main framebuffer
        cur_program = blit_ms_prog_.get();
        glUseProgram(cur_program->prog_id());

        const float fs_quad_pos[] = { -1.0f, -1.0f,       1.0f, -1.0f,
                                       1.0f, 1.0f,         -1.0f, 1.0f };

        const float fs_quad_uvs[] = { 0.0f, 0.0f,               float(w_), 0.0f,
                                      float(w_), float(h_),     0.0f, float(h_) };

        const uint8_t fs_quad_indices[] = { 0, 1, 2,    0, 2, 3 };

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

        glEnableVertexAttribArray(cur_program->attribute("aVertexPosition").loc);
        glVertexAttribPointer(cur_program->attribute("aVertexPosition").loc, 2, GL_FLOAT, GL_FALSE, 0, &fs_quad_pos[0]);

        glEnableVertexAttribArray(cur_program->attribute("aVertexUVs").loc);
        glVertexAttribPointer(cur_program->attribute("aVertexUVs").loc, 2, GL_FLOAT, GL_FALSE, 0, &fs_quad_uvs[0]);

        glUniform1i(cur_program->uniform("s_texture").loc, DIFFUSEMAP_SLOT);

        glActiveTexture((GLenum)(GL_TEXTURE0 + DIFFUSEMAP_SLOT));
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, clean_buf_.col_tex.GetValue());

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, &fs_quad_indices[0]);
    }

    if (debug_cull_ && culling_enabled_ && !depth_pixels_[0].empty()) {
        glUseProgram(0);

        glRasterPos2f(-1, -1);
        glDrawPixels(256, 128, GL_RGBA, GL_UNSIGNED_BYTE, &depth_pixels_[0][0]);

        glRasterPos2f(-1 + 2 * float(256) / ctx_.w(), -1);
        glDrawPixels(256, 128, GL_RGBA, GL_UNSIGNED_BYTE, &depth_tiles_[0][0]);
    }

    if (debug_shadow_) {
        cur_program = blit_prog_.get();
        glUseProgram(cur_program->prog_id());

        //glDisable(GL_CULL_FACE);

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

        BindTexture(DIFFUSEMAP_SLOT, shadow_buf_.depth_tex.GetValue());

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, &indices[0]);
    }

#if 0
    glFinish();
#endif
}

void Renderer::BlitPixels(const void *data, int w, int h, const Ren::eTexColorFormat format) {
    glUseProgram(0);

    GLenum gl_format, gl_type;
    if (format == Ren::RawRGBA32F) {
        gl_format = GL_RGBA;
        gl_type = GL_FLOAT;
    }

    glRasterPos2f(-1, -1);
    glDrawPixels(w, h, gl_format, gl_type, data);
}