#include "Renderer.h"

#include <Ren/Camera.h>
#include <Ren/Context.h>
#include <Ren/GL.h>
#include <Sys/Log.h>

namespace RendererInternal {
const char fillz_vs[] = R"(
#version 300 es

/*
UNIFORMS
	uMVPMatrix : 0
*/

layout(location = 0) in vec3 aVertexPosition;

uniform mat4 uMVPMatrix;

void main() {
    gl_Position = uMVPMatrix * vec4(aVertexPosition, 1.0);
} 
)";

    const char fillz_fs[] = R"(
#version 300 es

#ifdef GL_ES
	precision mediump float;
#endif

void main() {
}
)";

    const char shadow_vs[] = R"(
#version 300 es

/*
UNIFORMS
	uMVPMatrix : 0
*/

layout(location = 0) in vec3 aVertexPosition;

uniform mat4 uMVPMatrix;

void main() {
    gl_Position = uMVPMatrix * vec4(aVertexPosition, 1.0);
} 
)";

    const char shadow_fs[] = R"(
#version 300 es
#ifdef GL_ES
    precision mediump float;
#endif

void main() {
    //gl_FragDepth = gl_FragCoord.z;
}
)";

    const char blit_vs[] = R"(
#version 300 es

layout(location = 0) in vec2 aVertexPosition;
layout(location = 3) in vec2 aVertexUVs;

out vec2 aVertexUVs_;

void main() {
    aVertexUVs_ = aVertexUVs;
    gl_Position = vec4(aVertexPosition, 0.5, 1.0);
} 
)";

    const char blit_fs[] = R"(
#version 300 es
#ifdef GL_ES
	precision mediump float;
#endif

/*
UNIFORMS
    s_texture : 3
*/
        
uniform sampler2D s_texture;

in vec2 aVertexUVs_;

out vec4 outColor;

void main() {
    outColor = texelFetch(s_texture, ivec2(aVertexUVs_), 0);
}
)";

    const char blit_ms_vs[] = R"(
#version 310 es

layout(location = 0) in vec2 aVertexPosition;
layout(location = 3) in vec2 aVertexUVs;

out vec2 aVertexUVs_;

void main() {
    aVertexUVs_ = aVertexUVs;
    gl_Position = vec4(aVertexPosition, 0.5, 1.0);
} 
)";

    const char blit_ms_fs[] = R"(
#version 310 es
#extension GL_ARB_texture_multisample : enable

#ifdef GL_ES
	precision mediump float;
#endif

/*
UNIFORMS
    s_texture : 3
    uTexSize : 5
*/
        
layout(location = 14) uniform mediump sampler2DMS s_texture;

in vec2 aVertexUVs_;

out vec4 outColor;

void main() {
    outColor = texelFetch(s_texture, ivec2(aVertexUVs_), 0);
}
    )";

    const char blit_combine_fs[] = R"(
#version 310 es
#ifdef GL_ES
	precision mediump float;
#endif

/*
UNIFORMS
    s_texture : 3
    s_blured_texture : 4
    uTexSize : 5
*/
        
uniform sampler2D s_texture;
uniform sampler2D s_blured_texture;
uniform vec2 uTexSize;
layout(location = 14) uniform float gamma;
layout(location = 15) uniform float exposure;

in vec2 aVertexUVs_;

out vec4 outColor;

void main() {
    vec3 c0 = texelFetch(s_texture, ivec2(aVertexUVs_), 0).xyz;
    vec3 c1 = 0.1 * texture(s_blured_texture, aVertexUVs_ / uTexSize).xyz;
            
    c0 += c1;
    c0 = vec3(1.0) - exp(-c0 * exposure);
    c0 = pow(c0, vec3(1.0/gamma));

    outColor = vec4(c0, 1.0);
}
)";

    const char blit_combine_ms_fs[] = R"(
#version 310 es
#extension GL_ARB_texture_multisample : enable

#ifdef GL_ES
	precision mediump float;
#endif

/*
UNIFORMS
    s_texture : 3
    s_blured_texture : 4
    uTexSize : 5
*/
        
uniform mediump sampler2DMS s_texture;
uniform sampler2D s_blured_texture;
uniform vec2 uTexSize;
layout(location = 14) uniform float gamma;
layout(location = 15) uniform float exposure;

in vec2 aVertexUVs_;

out vec4 outColor;

void main() {
    vec3 c0 = texelFetch(s_texture, ivec2(aVertexUVs_), 0).xyz;
	vec3 c1 = texelFetch(s_texture, ivec2(aVertexUVs_), 1).xyz;
	vec3 c2 = texelFetch(s_texture, ivec2(aVertexUVs_), 2).xyz;
	vec3 c3 = texelFetch(s_texture, ivec2(aVertexUVs_), 3).xyz;
    vec3 c4 = 0.1 * texture(s_blured_texture, aVertexUVs_ / uTexSize).xyz;
            
    c0 += c4;
    c1 += c4;
    c2 += c4;
    c3 += c4;

    //c0 = exposure * c0 / (c0 + vec3(1.0));
    //c1 = exposure * c1 / (c1 + vec3(1.0));
    //c2 = exposure * c2 / (c2 + vec3(1.0));
    //c3 = exposure * c3 / (c3 + vec3(1.0));

    c0 = vec3(1.0) - exp(-c0 * exposure);
    c1 = vec3(1.0) - exp(-c1 * exposure);
    c2 = vec3(1.0) - exp(-c2 * exposure);
    c3 = vec3(1.0) - exp(-c3 * exposure);

    c0 = pow(c0, vec3(1.0/gamma));
    c1 = pow(c1, vec3(1.0/gamma));
    c2 = pow(c2, vec3(1.0/gamma));
    c3 = pow(c3, vec3(1.0/gamma));

    outColor = vec4(0.25 * (c0 + c1 + c2 + c3), 1.0);
}
)";

const char blit_reduced_fs[] = R"(
#version 300 es
#ifdef GL_ES
	precision mediump float;
#endif

/*
UNIFORMS
    s_texture : 3
    uOffset : 4
*/
        
uniform sampler2D s_texture;
uniform vec2 uOffset;

in vec2 aVertexUVs_;

out vec4 outColor;

void main() {
    vec3 c0 = texture(s_texture, aVertexUVs_ + uOffset).xyz;
    outColor.r = 0.299 * c0.r + 0.587 * c0.g + 0.114 * c0.b;
}
)";

    const char blit_down_fs[] = R"(
#version 300 es

#ifdef GL_ES
	precision mediump float;
#endif

/*
UNIFORMS
    s_texture : 3
*/
        
uniform sampler2D s_texture;

in vec2 aVertexUVs_;

out vec4 outColor;

void main() {
    vec3 col = vec3(0.0);
    for (float j = -1.5; j < 2.0; j += 1.0) {
        for (float i = -1.5; i < 2.0; i += 1.0) {
            col += texelFetch(s_texture, ivec2(aVertexUVs_ + vec2(i, j)), 0).xyz;
        }
    }
    outColor = vec4((1.0/16.0) * col, 1.0);
}
    )";

const char blit_down_ms_fs[] = R"(
#version 310 es
#extension GL_ARB_texture_multisample : enable

#ifdef GL_ES
	precision mediump float;
#endif

/*
UNIFORMS
    s_texture : 3
*/
        
uniform mediump sampler2DMS s_texture;

in vec2 aVertexUVs_;

out vec4 outColor;

void main() {
    vec3 col = vec3(0.0);
    for (float j = -1.5; j < 2.0; j += 1.0) {
        for (float i = -1.5; i < 2.0; i += 1.0) {
            col += texelFetch(s_texture, ivec2(aVertexUVs_ + vec2(i, j)), 0).xyz;
        }
    }
    outColor = vec4((1.0/16.0) * col, 1.0);
}
    )";

const char blit_gauss_fs[] = R"(
#version 310 es
#extension GL_ARB_texture_multisample : enable

#ifdef GL_ES
	precision mediump float;
#endif

/*
UNIFORMS
    s_texture : 3
    vertical : 4
*/
        
uniform sampler2D s_texture;
uniform float vertical;

in vec2 aVertexUVs_;

out vec4 outColor;

void main() {
    if(vertical < 1.0) {
        outColor += texelFetch(s_texture, ivec2(aVertexUVs_) - ivec2(4, 0), 0) * 0.05;
	    outColor += texelFetch(s_texture, ivec2(aVertexUVs_) - ivec2(3, 0), 0) * 0.09;
	    outColor += texelFetch(s_texture, ivec2(aVertexUVs_) - ivec2(2, 0), 0) * 0.12;
	    outColor += texelFetch(s_texture, ivec2(aVertexUVs_) - ivec2(1, 0), 0) * 0.15;
	    outColor += texelFetch(s_texture, ivec2(aVertexUVs_), 0) * 0.16;
	    outColor += texelFetch(s_texture, ivec2(aVertexUVs_) + ivec2(1, 0), 0) * 0.15;
	    outColor += texelFetch(s_texture, ivec2(aVertexUVs_) + ivec2(2, 0), 0) * 0.12;
	    outColor += texelFetch(s_texture, ivec2(aVertexUVs_) + ivec2(3, 0), 0) * 0.09;
	    outColor += texelFetch(s_texture, ivec2(aVertexUVs_) + ivec2(4, 0), 0) * 0.05;
    } else {
        outColor += texelFetch(s_texture, ivec2(aVertexUVs_) - ivec2(0, 4), 0) * 0.05;
	    outColor += texelFetch(s_texture, ivec2(aVertexUVs_) - ivec2(0, 3), 0) * 0.09;
	    outColor += texelFetch(s_texture, ivec2(aVertexUVs_) - ivec2(0, 2), 0) * 0.12;
	    outColor += texelFetch(s_texture, ivec2(aVertexUVs_) - ivec2(0, 1), 0) * 0.15;
	    outColor += texelFetch(s_texture, ivec2(aVertexUVs_), 0) * 0.16;
	    outColor += texelFetch(s_texture, ivec2(aVertexUVs_) + ivec2(0, 1), 0) * 0.15;
	    outColor += texelFetch(s_texture, ivec2(aVertexUVs_) + ivec2(0, 2), 0) * 0.12;
	    outColor += texelFetch(s_texture, ivec2(aVertexUVs_) + ivec2(0, 3), 0) * 0.09;
	    outColor += texelFetch(s_texture, ivec2(aVertexUVs_) + ivec2(0, 4), 0) * 0.05;
    }
}
)";

    struct MatricesBlock {
        Ren::Mat4f uMVPMatrix;
        Ren::Mat4f uMVMatrix;
        Ren::Mat4f uShadowMatrix[4];
    };
    static_assert(sizeof(MatricesBlock) == 384, "!");

    const Ren::Vec2f poisson_disk[] = {
        { -0.705374f, -0.668203f }, { -0.780145f, 0.486251f  }, { 0.566637f, 0.605213f   }, { 0.488876f, -0.783441f  },
        { -0.613392f, 0.617481f  }, { 0.170019f, -0.040254f  }, { -0.299417f, 0.791925f  }, { 0.645680f, 0.493210f   },
        { -0.651784f, 0.717887f  }, { 0.421003f, 0.027070f   }, { -0.817194f, -0.271096f }, { 0.977050f, -0.108615f  },
        { 0.063326f, 0.142369f   }, { 0.203528f, 0.214331f   }, { -0.667531f, 0.326090f  }, { -0.098422f, -0.295755f },
        { -0.885922f, 0.215369f  }, { 0.039766f, -0.396100f  }, { 0.751946f, 0.453352f   }, { 0.078707f, -0.715323f  },
        { -0.075838f, -0.529344f }, { 0.724479f, -0.580798f  }, { 0.222999f, -0.215125f  }, { -0.467574f, -0.405438f },
        { -0.248268f, -0.814753f }, { 0.354411f, -0.887570f  }, { 0.175817f, 0.382366f   }, { 0.487472f, -0.063082f  },
        { -0.084078f, 0.898312f  }, { 0.470016f, 0.217933f   }, { -0.696890f, -0.549791f }, { -0.149693f, 0.605762f  },
        { 0.034211f, 0.979980f   }, { 0.503098f, -0.308878f  }, { -0.016205f, -0.872921f }, { 0.385784f, -0.393902f  },
        { -0.146886f, -0.859249f }, { 0.643361f, 0.164098f   }, { 0.634388f, -0.049471f  }, { -0.688894f, 0.007843f  },
        { 0.464034f, -0.188818f  }, { -0.440840f, 0.137486f  }, { 0.364483f, 0.511704f   }, { 0.034028f, 0.325968f   },
        { 0.099094f, -0.308023f  }, { 0.693960f, -0.366253f  }, { 0.678884f, -0.204688f  }, { 0.001801f, 0.780328f   },
        { 0.145177f, -0.898984f  }, { 0.062655f, -0.611866f  }, { 0.315226f, -0.604297f  }, { -0.371868f, 0.882138f  },
        { 0.200476f, 0.494430f   }, { -0.494552f, -0.711051f }, { 0.612476f, 0.705252f   }, { -0.578845f, -0.768792f },
        { -0.772454f, -0.090976f }, { 0.504440f, 0.372295f   }, { 0.155736f, 0.065157f   }, { 0.391522f, 0.849605f   },
        { -0.620106f, -0.328104f }, { 0.789239f, -0.419965f  }, { -0.545396f, 0.538133f  }, { -0.178564f, -0.596057f }
    };

    const GLuint A_POS = 0;
    const GLuint A_NORMAL = 1;
    const GLuint A_TANGENT = 2;
    const GLuint A_UVS1 = 3;
    const GLuint A_UVS2 = 4;

    const GLuint A_INDICES = 3;
    const GLuint A_WEIGHTS = 4;

    const int U_MATRICES = 0;

    const int U_MVP_MATR = 0;
    const int U_MV_MATR = 1;

    const int U_SH_MVP_MATR = 2;

    const int U_TEX = 3;
    const int U_NORM_TEX = 4;
    const int U_SHADOW_TEX = 5;
    const int U_LM_DIR_TEX = 6;
    const int U_LM_INDIR_TEX = 7;
    const int U_LM_INDIR_SH_TEX = 8;

    const int U_SUN_DIR = 12;
    const int U_SUN_COL = 13;

    const int U_GAMMA = 14;
    const int U_EXPOSURE = 15;

    const int DIFFUSEMAP_SLOT = 0;
    const int NORMALMAP_SLOT = 1;
    const int SHADOWMAP_SLOT = 2;
    const int LM_DIRECT_SLOT = 3;
    const int LM_INDIR_SLOT = 4;
    const int LM_INDIR_SH_SLOT = 5;

    inline void BindTexture(int slot, uint32_t tex) {
        glActiveTexture((GLenum)(GL_TEXTURE0 + slot));
        glBindTexture(GL_TEXTURE_2D, (GLuint)tex);
    }

    const bool DEPTH_PREPASS = true;
}

void Renderer::InitRendererInternal() {
    using namespace RendererInternal;

    LOGI("Compiling fill_depth");
    Ren::eProgLoadStatus status;
    fill_depth_prog_ = ctx_.LoadProgramGLSL("fill_depth", fillz_vs, fillz_fs, &status);
    assert(status == Ren::ProgCreatedFromData);
    LOGI("Compiling shadow");
    shadow_prog_ = ctx_.LoadProgramGLSL("shadow", shadow_vs, shadow_fs, &status);
    assert(status == Ren::ProgCreatedFromData);
    LOGI("Compiling blit");
    blit_prog_ = ctx_.LoadProgramGLSL("blit", blit_vs, blit_fs, &status);
    assert(status == Ren::ProgCreatedFromData);
    LOGI("Compiling blit_combine");
    blit_combine_prog_ = ctx_.LoadProgramGLSL("blit_combine", blit_vs, blit_combine_fs, &status);
    assert(status == Ren::ProgCreatedFromData);
    LOGI("Compiling blit_combine_ms");
    blit_combine_ms_prog_ = ctx_.LoadProgramGLSL("blit_combine_ms", blit_ms_vs, blit_combine_ms_fs, &status);
    assert(status == Ren::ProgCreatedFromData);
    LOGI("Compiling blit_ms");
    blit_ms_prog_ = ctx_.LoadProgramGLSL("blit_ms", blit_ms_vs, blit_ms_fs, &status);
    assert(status == Ren::ProgCreatedFromData);
    LOGI("Compiling blit_red");
    blit_red_prog_ = ctx_.LoadProgramGLSL("blit_red", blit_vs, blit_reduced_fs, &status);
    assert(status == Ren::ProgCreatedFromData);
    LOGI("Compiling blit_down");
    blit_down_prog_ = ctx_.LoadProgramGLSL("blit_down", blit_vs, blit_down_fs, &status);
    assert(status == Ren::ProgCreatedFromData);
    LOGI("Compiling blit_down_ms");
    blit_down_ms_prog_ = ctx_.LoadProgramGLSL("blit_down_ms", blit_ms_vs, blit_down_ms_fs, &status);
    assert(status == Ren::ProgCreatedFromData);
    LOGI("Compiling blit_gauss");
    blit_gauss_prog_ = ctx_.LoadProgramGLSL("blit_gauss", blit_vs, blit_gauss_fs, &status);
    assert(status == Ren::ProgCreatedFromData);

    {
        GLuint matrices_ubo;

        glGenBuffers(1, &matrices_ubo);
        glBindBuffer(GL_UNIFORM_BUFFER, matrices_ubo);
        glBufferData(GL_UNIFORM_BUFFER, sizeof(MatricesBlock), NULL, GL_STATIC_DRAW);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);

        unif_matrices_block_ = (uint32_t)matrices_ubo;
    }
}

void Renderer::CheckInitVAOs() {
    using namespace RendererInternal;

    GLuint vertex_buf = ctx_.default_vertex_buf()->buf_id(),
           indices_buf = ctx_.default_indices_buf()->buf_id();

    if (vertex_buf != last_vertex_buffer_ || indices_buf != last_index_buffer_) {
        GLuint shadow_pass_vao;
        glGenVertexArrays(1, &shadow_pass_vao);
        glBindVertexArray(shadow_pass_vao);

        glBindBuffer(GL_ARRAY_BUFFER, vertex_buf);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indices_buf);

        int stride = 13 * sizeof(float);
        glEnableVertexAttribArray(A_POS);
        glVertexAttribPointer(A_POS, 3, GL_FLOAT, GL_FALSE, stride, (void *)0);

        glBindVertexArray(0);

        shadow_pass_vao_ = (uint32_t)shadow_pass_vao;

        GLuint depth_pass_vao;
        glGenVertexArrays(1, &depth_pass_vao);
        glBindVertexArray(depth_pass_vao);

        glBindBuffer(GL_ARRAY_BUFFER, vertex_buf);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indices_buf);

        glEnableVertexAttribArray(A_POS);
        glVertexAttribPointer(A_POS, 3, GL_FLOAT, GL_FALSE, stride, (void *)0);

        glBindVertexArray(0);
        depth_pass_vao_ = (uint32_t)depth_pass_vao;

        GLuint draw_pass_vao;
        glGenVertexArrays(1, &draw_pass_vao);
        glBindVertexArray(draw_pass_vao);

        glBindBuffer(GL_ARRAY_BUFFER, vertex_buf);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indices_buf);

        glEnableVertexAttribArray(A_POS);
        glVertexAttribPointer(A_POS, 3, GL_FLOAT, GL_FALSE, stride, (void *)0);

        glEnableVertexAttribArray(A_NORMAL);
        glVertexAttribPointer(A_NORMAL, 3, GL_FLOAT, GL_FALSE, stride, (void *)(3 * sizeof(float)));

        glEnableVertexAttribArray(A_TANGENT);
        glVertexAttribPointer(A_TANGENT, 3, GL_FLOAT, GL_FALSE, stride, (void *)(6 * sizeof(float)));

        glEnableVertexAttribArray(A_UVS1);
        glVertexAttribPointer(A_UVS1, 2, GL_FLOAT, GL_FALSE, stride, (void *)(9 * sizeof(float)));

        glEnableVertexAttribArray(A_UVS2);
        glVertexAttribPointer(A_UVS2, 2, GL_FLOAT, GL_FALSE, stride, (void *)(11 * sizeof(float)));

        glBindVertexArray(0);
        draw_pass_vao_ = (uint32_t)draw_pass_vao;

        last_vertex_buffer_ = (uint32_t)vertex_buf;
        last_index_buffer_ = (uint32_t)indices_buf;
    }
}

void Renderer::DestroyRendererInternal() {
    {
        GLuint matrices_ubo = (GLuint)unif_matrices_block_;
        glDeleteBuffers(1, &matrices_ubo);
    }

    {
        GLuint shadow_pass_vao = (GLuint)shadow_pass_vao_;
        glDeleteVertexArrays(1, &shadow_pass_vao);

        GLuint depth_pass_vao = (GLuint)depth_pass_vao_;
        glDeleteVertexArrays(1, &depth_pass_vao);
    }
}

void Renderer::DrawObjectsInternal(const DrawableItem *drawables, size_t drawable_count, const Ren::Mat4f shadow_transforms[4],
                                   const DrawableItem *shadow_drawables[4], size_t shadow_drawable_count[4], const Environment &env) {
    using namespace Ren;
    using namespace RendererInternal;

    CheckInitVAOs();

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);

    glClearColor(env.sky_col[0], env.sky_col[1], env.sky_col[2], 1.0f);

    glDisable(GL_CULL_FACE);

    const Ren::Program *cur_program = nullptr;
    const Ren::Material *cur_mat = nullptr;
    const Ren::Mat4f *cur_clip_from_object = nullptr,
                     *cur_world_from_object = nullptr,
                     *cur_sh_clip_from_object[4] = { nullptr };
    const Ren::Texture2D *cur_lm_dir_tex = nullptr,
                         *cur_lm_indir_tex = nullptr,
                         *cur_lm_indir_sh_tex[4] = { nullptr };

    int32_t viewport_before[4];
    glGetIntegerv(GL_VIEWPORT, viewport_before);

    {   // draw shadow map
        bool fb_bound = false;

        glEnable(GL_POLYGON_OFFSET_FILL);

        glBindVertexArray(shadow_pass_vao_);

        for (int casc = 0; casc < 4; casc++) {
            if (shadow_drawable_count[casc]) {
                if (cur_program != shadow_prog_.get()) {
                    cur_program = shadow_prog_.get();
                    glUseProgram(cur_program->prog_id());
                }

                if (!fb_bound) {
                    glBindFramebuffer(GL_FRAMEBUFFER, shadow_buf_.fb);
                    glClear(GL_DEPTH_BUFFER_BIT);
                    fb_bound = true;
                }

                if (casc == 0) {
                    glViewport(0, 0, shadow_buf_.w / 2, shadow_buf_.h / 2);
                    glPolygonOffset(1.5f, 6.0f);
                } else if (casc == 1) {
                    glViewport(shadow_buf_.w / 2, 0, shadow_buf_.w / 2, shadow_buf_.h / 2);
                    glPolygonOffset(1.25f, 4.0f);
                } else if (casc == 2) {
                    glViewport(0, shadow_buf_.h / 2, shadow_buf_.w / 2, shadow_buf_.h / 2);
                } else {
                    glViewport(shadow_buf_.w / 2, shadow_buf_.h / 2, shadow_buf_.w / 2, shadow_buf_.h / 2);
                }

                const auto *shadow_dr_list = shadow_drawables[casc];

                for (size_t i = 0; i < shadow_drawable_count[casc]; i++) {
                    const auto &dr = shadow_dr_list[i];
                    
                    const Ren::Mat4f *clip_from_object = dr.clip_from_object;
                    const Ren::Mesh *mesh = dr.mesh;
                    const Ren::TriStrip *strip = dr.strip;

                    if (clip_from_object != cur_clip_from_object) {
                        glUniformMatrix4fv(cur_program->uniform(U_MVP_MATR).loc, 1, GL_FALSE, ValuePtr(clip_from_object));
                        cur_clip_from_object = clip_from_object;
                    }

                    glDrawElements(GL_TRIANGLES, strip->num_indices, GL_UNSIGNED_INT, (void *)uintptr_t(mesh->indices_offset() + strip->offset));
                }
            }
        }

        glPolygonOffset(0.0f, 0.0f);
        glDisable(GL_POLYGON_OFFSET_FILL);

        glBindVertexArray(0);
    }

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    // Bind main buffer for drawing
    glBindFramebuffer(GL_FRAMEBUFFER, clean_buf_.fb);
    //glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, clean_buf_.w, clean_buf_.h);
    glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

    if (DEPTH_PREPASS && !wireframe_mode_) {
        glDepthFunc(GL_LESS);

        cur_program = fill_depth_prog_.get();
        glUseProgram(cur_program->prog_id());

        glBindVertexArray(depth_pass_vao_);

        // fill depth
        for (size_t i = 0; i < drawable_count; i++) {
            const auto &dr = drawables[i];

            const Ren::Mat4f *clip_from_object = dr.clip_from_object;
            const Ren::Mesh *mesh = dr.mesh;
            const Ren::TriStrip *strip = dr.strip;

            if (clip_from_object != cur_clip_from_object) {
                glUniformMatrix4fv(cur_program->uniform(U_MVP_MATR).loc, 1, GL_FALSE, ValuePtr(clip_from_object));
                cur_clip_from_object = clip_from_object;
            }

            glDrawElements(GL_TRIANGLES, strip->num_indices, GL_UNSIGNED_INT, (void *)uintptr_t(mesh->indices_offset() + strip->offset));
        }

        glBindVertexArray(0);

        glDepthFunc(GL_EQUAL);
    }

#if !defined(__ANDROID__)
    if (wireframe_mode_) {
        glDepthFunc(GL_LEQUAL);
        glDepthMask(GL_FALSE);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    } else {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
#endif

    glBindVertexArray((GLuint)draw_pass_vao_);

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

            glBindBufferBase(GL_UNIFORM_BUFFER, p->uniform_block(U_MATRICES).loc, (GLuint)unif_matrices_block_);

            glUniform3fv(U_SUN_DIR, 1, Ren::ValuePtr(env.sun_dir));
            glUniform3fv(U_SUN_COL, 1, Ren::ValuePtr(env.sun_col));

            glUniform1f(U_GAMMA, 2.2f);

            if (clip_from_object == cur_clip_from_object) {
                //glUniformMatrix4fv(p->uniform(U_MVP_MATR).loc, 1, GL_FALSE, ValuePtr(clip_from_object));

                glBindBuffer(GL_UNIFORM_BUFFER, (GLuint)unif_matrices_block_);
                glBufferSubData(GL_UNIFORM_BUFFER, offsetof(MatricesBlock, uMVPMatrix), sizeof(Ren::Mat4f), ValuePtr(clip_from_object));
                glBindBuffer(GL_UNIFORM_BUFFER, 0);
            }

            if (world_from_object == cur_world_from_object) {
                //glUniformMatrix4fv(p->uniform(U_MV_MATR).loc, 1, GL_FALSE, ValuePtr(world_from_object));

                glBindBuffer(GL_UNIFORM_BUFFER, (GLuint)unif_matrices_block_);
                glBufferSubData(GL_UNIFORM_BUFFER, offsetof(MatricesBlock, uMVMatrix), sizeof(Ren::Mat4f), ValuePtr(world_from_object));
                glBindBuffer(GL_UNIFORM_BUFFER, 0);
            }

            BindTexture(SHADOWMAP_SLOT, shadow_buf_.depth_tex.GetValue());

            cur_program = p;
        }

        if (clip_from_object != cur_clip_from_object) {
            //glUniformMatrix4fv(p->uniform(U_MVP_MATR).loc, 1, GL_FALSE, ValuePtr(clip_from_object));

            glBindBuffer(GL_UNIFORM_BUFFER, (GLuint)unif_matrices_block_);
            glBufferSubData(GL_UNIFORM_BUFFER, offsetof(MatricesBlock, uMVPMatrix), sizeof(Ren::Mat4f), ValuePtr(clip_from_object));
            glBindBuffer(GL_UNIFORM_BUFFER, 0);

            cur_clip_from_object = clip_from_object;
        }

        if (world_from_object != cur_world_from_object) {
            //glUniformMatrix4fv(cur_program->uniform(U_MV_MATR).loc, 1, GL_FALSE, ValuePtr(world_from_object));

            glBindBuffer(GL_UNIFORM_BUFFER, (GLuint)unif_matrices_block_);
            glBufferSubData(GL_UNIFORM_BUFFER, offsetof(MatricesBlock, uMVMatrix), sizeof(Ren::Mat4f), ValuePtr(world_from_object));
            glBindBuffer(GL_UNIFORM_BUFFER, 0);

            cur_world_from_object = world_from_object;
        }

        {   // update shadow matrices
            for (int casc = 0; casc < 4; casc++) {
                const auto *_sh_clip_from_object = sh_clip_from_object[casc];
                if (_sh_clip_from_object && _sh_clip_from_object != cur_sh_clip_from_object[casc]) {
                    //glUniformMatrix4fv(p->uniform(U_SH_MVP_MATR).loc + casc, 1, GL_FALSE, ValuePtr(_sh_clip_from_object));

                    glBindBuffer(GL_UNIFORM_BUFFER, (GLuint)unif_matrices_block_);
                    glBufferSubData(GL_UNIFORM_BUFFER, offsetof(MatricesBlock, uShadowMatrix) + casc * sizeof(Ren::Mat4f), sizeof(Ren::Mat4f), ValuePtr(_sh_clip_from_object));
                    glBindBuffer(GL_UNIFORM_BUFFER, 0);

                    cur_sh_clip_from_object[casc] = _sh_clip_from_object;
                }
            }
        }

        if (mat != cur_mat) {
            BindTexture(DIFFUSEMAP_SLOT, mat->texture(0)->tex_id());
            BindTexture(NORMALMAP_SLOT, mat->texture(1)->tex_id());
            cur_mat = mat;
        }

        if (cur_lm_dir_tex != dr.lm_dir_tex) {
            cur_lm_dir_tex = dr.lm_dir_tex ? dr.lm_dir_tex : default_lightmap_.get();
            BindTexture(LM_DIRECT_SLOT, cur_lm_dir_tex->tex_id());
        }

        if (!dr.lm_indir_sh_tex[0]) {
            if (cur_lm_indir_tex != dr.lm_indir_tex) {
                if (dr.lm_indir_tex) {
                    cur_lm_indir_tex = dr.lm_indir_tex;
                } else if (cur_mat->texture(2)) {
                    cur_lm_indir_tex = cur_mat->texture(2).get();
                } else {
                    cur_lm_indir_tex = default_lightmap_.get();
                }
                BindTexture(LM_INDIR_SLOT, cur_lm_indir_tex->tex_id());
            }
        } else {
            cur_lm_indir_tex = default_lightmap_.get();
            BindTexture(LM_INDIR_SLOT, cur_lm_indir_tex->tex_id());
        }

        for (int sh_l = 0; sh_l < 4; sh_l++) {
            if (dr.lm_indir_sh_tex[sh_l] && cur_lm_indir_sh_tex[sh_l] != dr.lm_indir_sh_tex[sh_l]) {
                cur_lm_indir_sh_tex[sh_l] = dr.lm_indir_sh_tex[sh_l];
                BindTexture(LM_INDIR_SH_SLOT + sh_l, cur_lm_indir_sh_tex[sh_l]->tex_id());
            } else {
                BindTexture(LM_INDIR_SH_SLOT + sh_l, default_lightmap_->tex_id());
            }
        }

        glDrawElements(GL_TRIANGLES, strip->num_indices, GL_UNSIGNED_INT, (void *)uintptr_t(mesh->indices_offset() + strip->offset));
    }

    glBindVertexArray(0);

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    {   // prepare blured buffer
        glBindFramebuffer(GL_FRAMEBUFFER, blur_buf1_.fb);
        glViewport(0, 0, blur_buf1_.w, blur_buf1_.h);

        const float fs_quad_pos[] = { -1.0f, -1.0f,       1.0f, -1.0f,
                                      1.0f, 1.0f,         -1.0f, 1.0f };

        const float fs_quad_uvs[] = { 0.0f, 0.0f,               float(w_), 0.0f,
                                      float(w_), float(h_),     0.0f, float(h_) };

        const uint8_t fs_quad_indices[] = { 0, 1, 2,    0, 2, 3 };

        if (clean_buf_.msaa > 1) {
            cur_program = blit_down_ms_prog_.get();
        } else {
            cur_program = blit_down_prog_.get();
        }
        glUseProgram(cur_program->prog_id());

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

        glEnableVertexAttribArray(A_POS);
        glVertexAttribPointer(A_POS, 2, GL_FLOAT, GL_FALSE, 0, &fs_quad_pos[0]);

        glEnableVertexAttribArray(A_UVS1);
        glVertexAttribPointer(A_UVS1, 2, GL_FLOAT, GL_FALSE, 0, &fs_quad_uvs[0]);

        glUniform1i(cur_program->uniform(U_TEX).loc, DIFFUSEMAP_SLOT);

        if (clean_buf_.msaa > 1) {
            glActiveTexture((GLenum)(GL_TEXTURE0 + DIFFUSEMAP_SLOT));
            glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, clean_buf_.col_tex.GetValue());
        } else {
            BindTexture(DIFFUSEMAP_SLOT, clean_buf_.col_tex.GetValue());
        }

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, &fs_quad_indices[0]);

        ////////////////

        glBindFramebuffer(GL_FRAMEBUFFER, blur_buf2_.fb);
        glViewport(0, 0, blur_buf2_.w, blur_buf2_.h);

        const float fs_quad_uvs1[] = { 0.0f, 0.0f,                                   float(blur_buf2_.w), 0.0f,
                                       float(blur_buf2_.w), float(blur_buf2_.h),     0.0f, float(blur_buf2_.h) };

        cur_program = blit_gauss_prog_.get();
        glUseProgram(cur_program->prog_id());

        glEnableVertexAttribArray(A_POS);
        glVertexAttribPointer(A_POS, 2, GL_FLOAT, GL_FALSE, 0, &fs_quad_pos[0]);

        glEnableVertexAttribArray(A_UVS1);
        glVertexAttribPointer(A_UVS1, 2, GL_FLOAT, GL_FALSE, 0, &fs_quad_uvs1[0]);

        glUniform1i(cur_program->uniform(U_TEX).loc, DIFFUSEMAP_SLOT);
        glUniform1f(cur_program->uniform(4).loc, 0.5f);

        BindTexture(DIFFUSEMAP_SLOT, blur_buf1_.col_tex.GetValue());

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, &fs_quad_indices[0]);

        glUniform1f(cur_program->uniform(4).loc, 1.5f);

        glBindFramebuffer(GL_FRAMEBUFFER, blur_buf1_.fb);
        glViewport(0, 0, blur_buf1_.w, blur_buf1_.h);

        BindTexture(DIFFUSEMAP_SLOT, blur_buf2_.col_tex.GetValue());

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, &fs_quad_indices[0]);

        glDisableVertexAttribArray(A_POS);
        glDisableVertexAttribArray(A_UVS1);
    }

    {   // draw to small framebuffer
        glBindFramebuffer(GL_FRAMEBUFFER, reduced_buf_.fb);
        glViewport(0, 0, reduced_buf_.w, reduced_buf_.h);

        cur_program = blit_red_prog_.get();
        glUseProgram(cur_program->prog_id());

        const float fs_quad_pos[] = { -1.0f, -1.0f,       1.0f, -1.0f,
                                      1.0f, 1.0f,         -1.0f, 1.0f };

        const float fs_quad_uvs[] = { 0.0f, 0.0f,     1.0f, 0.0f,
                                      1.0f, 1.0f,     0.0f, 1.0f };

        const uint8_t fs_quad_indices[] = { 0, 1, 2,    0, 2, 3 };

        const Ren::Vec2f offset_step = { 1.0f / reduced_buf_.w, 1.0f / reduced_buf_.h };

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

        glEnableVertexAttribArray(A_POS);
        glVertexAttribPointer(A_POS, 2, GL_FLOAT, GL_FALSE, 0, &fs_quad_pos[0]);

        glEnableVertexAttribArray(A_UVS1);
        glVertexAttribPointer(A_UVS1, 2, GL_FLOAT, GL_FALSE, 0, &fs_quad_uvs[0]);

        glUniform1i(cur_program->uniform(U_TEX).loc, DIFFUSEMAP_SLOT);

        static int cur_offset = 0;
        glUniform2f(cur_program->uniform(4).loc, 0.5f * poisson_disk[cur_offset][0] * offset_step[0],
                                                 0.5f * poisson_disk[cur_offset][1] * offset_step[1]);
        cur_offset = cur_offset >= 63 ? 0 : (cur_offset + 1);

        BindTexture(DIFFUSEMAP_SLOT, blur_buf1_.col_tex.GetValue());

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, &fs_quad_indices[0]);

        glDisableVertexAttribArray(A_POS);
        glDisableVertexAttribArray(A_UVS1);

        reduced_pixels_.resize(4 * reduced_buf_.w * reduced_buf_.h);
        glReadPixels(0, 0, reduced_buf_.w, reduced_buf_.h, GL_RGBA, GL_FLOAT, &reduced_pixels_[0]);

        float cur_average = 0.0f;
        for (size_t i = 0; i < reduced_pixels_.size(); i += 4) {
            cur_average += reduced_pixels_[i];
        }

        float k = 1.0f / (reduced_pixels_.size() / 4);
        cur_average *= k;

        const float alpha = 1.0f / 64;
        reduced_average_ = alpha * cur_average + (1.0f - alpha) * reduced_average_;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(viewport_before[0], viewport_before[1], viewport_before[2], viewport_before[3]);

#if !defined(__ANDROID__)
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
#endif

    {   // Blit main framebuffer
        if (clean_buf_.msaa > 1) {
            cur_program = blit_combine_ms_prog_.get();
        } else {
            cur_program = blit_combine_prog_.get();
        }
        glUseProgram(cur_program->prog_id());

        const float fs_quad_pos[] = { -1.0f, -1.0f,       1.0f, -1.0f,
                                       1.0f, 1.0f,         -1.0f, 1.0f };

        const float fs_quad_uvs[] = { 0.0f, 0.0f,               float(w_), 0.0f,
                                      float(w_), float(h_),     0.0f, float(h_) };

        const uint8_t fs_quad_indices[] = { 0, 1, 2,    0, 2, 3 };

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

        glEnableVertexAttribArray(A_POS);
        glVertexAttribPointer(A_POS, 2, GL_FLOAT, GL_FALSE, 0, &fs_quad_pos[0]);

        glEnableVertexAttribArray(A_UVS1);
        glVertexAttribPointer(A_UVS1, 2, GL_FLOAT, GL_FALSE, 0, &fs_quad_uvs[0]);

        glUniform1i(cur_program->uniform(U_TEX).loc, DIFFUSEMAP_SLOT);
        glUniform1i(cur_program->uniform(U_TEX + 1).loc, DIFFUSEMAP_SLOT + 1);
        glUniform2f(cur_program->uniform(U_TEX + 2).loc, float(w_), float(h_));
        glUniform1f(U_GAMMA, 2.2f);

        float exposure = 0.7f / reduced_average_;

        glUniform1f(U_EXPOSURE, exposure);

        if (clean_buf_.msaa > 1) {
            glActiveTexture((GLenum)(GL_TEXTURE0 + DIFFUSEMAP_SLOT));
            glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, clean_buf_.col_tex.GetValue());
        } else {
            BindTexture(DIFFUSEMAP_SLOT, clean_buf_.col_tex.GetValue());
        }

        BindTexture(DIFFUSEMAP_SLOT + 1, blur_buf1_.col_tex.GetValue());

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, &fs_quad_indices[0]);

        glDisableVertexAttribArray(A_POS);
        glDisableVertexAttribArray(A_UVS1);
    }

#if !defined(__ANDROID__)
    glPixelZoom(1, 1);

    if (debug_cull_ && culling_enabled_ && !depth_pixels_[0].empty()) {
        glUseProgram(0);

        glRasterPos2f(-1, -1);
        glDrawPixels(256, 128, GL_RGBA, GL_UNSIGNED_BYTE, &depth_pixels_[0][0]);

        glRasterPos2f(-1 + 2 * float(256) / ctx_.w(), -1);
        glDrawPixels(256, 128, GL_RGBA, GL_UNSIGNED_BYTE, &depth_tiles_[0][0]);
    }
#endif

    if (debug_shadow_) {
        cur_program = blit_prog_.get();
        glUseProgram(cur_program->prog_id());

        float k = float(ctx_.w()) / ctx_.h();

        const float positions[] = { -1.0f, -1.0f,                       -1.0f + 0.25f, -1.0f,
                                    -1.0f + 0.25f, -1.0f + 0.25f * k,   -1.0f, -1.0f + 0.25f * k };

        const float uvs[] = { 0.0f, 0.0f,       1.0f, 0.0f,
                              1.0f, 1.0f,       0.0f, 1.0f };

        const uint8_t indices[] = { 0, 1, 2,    0, 2, 3 };

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

        glEnableVertexAttribArray(A_POS);
        glVertexAttribPointer(A_POS, 2, GL_FLOAT, GL_FALSE, 0, &positions[0]);

        glEnableVertexAttribArray(A_UVS1);
        glVertexAttribPointer(A_UVS1, 2, GL_FLOAT, GL_FALSE, 0, &uvs[0]);

        glUniform1i(cur_program->uniform(U_TEX).loc, DIFFUSEMAP_SLOT);

        BindTexture(DIFFUSEMAP_SLOT, shadow_buf_.depth_tex.GetValue());

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, &indices[0]);

        glDisableVertexAttribArray(A_POS);
        glDisableVertexAttribArray(A_UVS1);
    }

    if (debug_reduce_) {
        cur_program = blit_prog_.get();
        glUseProgram(cur_program->prog_id());

        float k = float(ctx_.w()) / ctx_.h();

        const float positions[] = { -1.0f, -1.0f,                      -1.0f + 0.5f, -1.0f,
                                    -1.0f + 0.5f, -1.0f + 0.25f * k,   -1.0f, -1.0f + 0.25f * k };

        const float uvs[] = {
            0.0f, 0.0f,                                   (float)reduced_buf_.w, 0.0f,
            (float)reduced_buf_.w, (float)reduced_buf_.h, 0.0f, (float)reduced_buf_.h
        };

        const uint8_t indices[] = { 0, 1, 2,    0, 2, 3 };

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

        glEnableVertexAttribArray(A_POS);
        glVertexAttribPointer(A_POS, 2, GL_FLOAT, GL_FALSE, 0, &positions[0]);

        glEnableVertexAttribArray(A_UVS1);
        glVertexAttribPointer(A_UVS1, 2, GL_FLOAT, GL_FALSE, 0, &uvs[0]);

        glUniform1i(cur_program->uniform(U_TEX).loc, DIFFUSEMAP_SLOT);

        BindTexture(DIFFUSEMAP_SLOT, reduced_buf_.col_tex.GetValue());

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, &indices[0]);

        glDisableVertexAttribArray(A_POS);
        glDisableVertexAttribArray(A_UVS1);
    }

#if 0
    glFinish();
#endif
}

void Renderer::BlitPixels(const void *data, int w, int h, const Ren::eTexColorFormat format) {
    using namespace RendererInternal;

    if (temp_tex_w_ != w || temp_tex_h_ != h || temp_tex_format_ != format) {
        if (temp_tex_w_ != 0 && temp_tex_h_ != 0) {
            GLuint gl_tex = (GLuint)temp_tex_;
            glDeleteTextures(1, &gl_tex);
        }

        GLuint new_tex;
        glGenTextures(1, &new_tex);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, new_tex);

        if (format == Ren::RawRGBA32F) {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, w, h, 0, GL_RGBA, GL_FLOAT, data);
        }

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        temp_tex_ = (uint32_t)new_tex;
        temp_tex_w_ = w;
        temp_tex_h_ = h;
        temp_tex_format_ = format;
    } else {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, temp_tex_);

        if (format == Ren::RawRGBA32F) {
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_FLOAT, data);
        }
    }

    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);

        Ren::Program *cur_program = blit_prog_.get();
        glUseProgram(cur_program->prog_id());

        const float fs_quad_pos[] = { -1.0f, -1.0f,       1.0f, -1.0f,
                                      1.0f, 1.0f,         -1.0f, 1.0f };

        const float fs_quad_uvs[] = { 0.0f, float(h),     float(w), float(h),
                                      float(w), 0.0f,     0.0f, 0.0f };

        const uint8_t fs_quad_indices[] = { 0, 1, 2,    0, 2, 3 };

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

        glEnableVertexAttribArray(A_POS);
        glVertexAttribPointer(A_POS, 2, GL_FLOAT, GL_FALSE, 0, &fs_quad_pos[0]);

        glEnableVertexAttribArray(A_UVS1);
        glVertexAttribPointer(A_UVS1, 2, GL_FLOAT, GL_FALSE, 0, &fs_quad_uvs[0]);

        glUniform1i(cur_program->uniform(U_TEX).loc, DIFFUSEMAP_SLOT);

        BindTexture(DIFFUSEMAP_SLOT, temp_tex_);

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, &fs_quad_indices[0]);

        glDisableVertexAttribArray(A_POS);
        glDisableVertexAttribArray(A_UVS1);
    }
}