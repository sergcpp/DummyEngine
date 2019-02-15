#include "Renderer.h"

#include <Ren/Camera.h>
#include <Ren/Context.h>
#include <Ren/GL.h>
#include <Sys/Log.h>

#define _AS_STR(x) #x
#define AS_STR(x) _AS_STR(x)

namespace RendererInternal {
#include "Renderer_GL_Shaders.inl"
#include "__skydome_mesh.inl"

    struct MatricesBlock {
        Ren::Mat4f uMVPMatrix;
        Ren::Mat4f uVMatrix;
        Ren::Mat4f uMMatrix;
        Ren::Mat4f uShadowMatrix[4];
    };
    static_assert(sizeof(MatricesBlock) == 448, "!");

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
    const int U_LIGHTS_COUNT = 15;

    const int U_EXPOSURE = 15;

    const int U_RESX = 16;
    const int U_RESY = 17;

    const int U_LIGHTS_BUFFER_TEXTURE = 16;

    const int DIFFUSEMAP_SLOT = 0;
    const int NORMALMAP_SLOT = 1;
    const int SPECULARMAP_SLOT = 2;
    const int SHADOWMAP_SLOT = 3;
    const int LM_DIRECT_SLOT = 4;
    const int LM_INDIR_SLOT = 5;
    const int LM_INDIR_SH_SLOT = 6;
    const int DECALSMAP_SLOT = 10;
    const int AOMAP_SLOT = 11;
    const int LIGHTS_BUFFER_SLOT = 12;
    const int DECALS_BUFFER_SLOT = 13;
    const int CELLS_BUFFER_SLOT = 14;
    const int ITEMS_BUFFER_SLOT = 15;

    const int LIGHTS_BUFFER_BINDING = 0;

    const int CLEAN_BUF_OPAQUE_ATTACHMENT = 0;
    const int CLEAN_BUF_NORMAL_ATTACHMENT = 1;
    const int CLEAN_BUF_SPECULAR_ATTACHMENT = 2;

    inline void BindTexture(int slot, uint32_t tex) {
        glActiveTexture((GLenum)(GL_TEXTURE0 + slot));
        glBindTexture(GL_TEXTURE_2D, (GLuint)tex);
    }

    inline void BindTextureMs(int slot, uint32_t tex) {
        glActiveTexture((GLenum)(GL_TEXTURE0 + slot));
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, (GLuint)tex);
    }

    const int TEMP_BUF_SIZE = 256;

    const bool DEPTH_PREPASS = true;
    const bool ENABLE_SSR = true;
    const bool ENABLE_SSAO = true;
}

void Renderer::InitRendererInternal() {
    using namespace RendererInternal;
    Ren::eProgLoadStatus status;
    LOGI("Compiling skydome");
    skydome_prog_ = ctx_.LoadProgramGLSL("skydome", skydome_vs, skydome_fs, &status);
    assert(status == Ren::ProgCreatedFromData);
    LOGI("Compiling fill_depth");
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
    LOGI("Compiling blit_debug");
    blit_debug_prog_ = ctx_.LoadProgramGLSL("blit_debug", blit_vs, blit_debug_fs, &status);
    assert(status == Ren::ProgCreatedFromData);
    LOGI("Compiling blit_debug_ms");
    blit_debug_ms_prog_ = ctx_.LoadProgramGLSL("blit_debug_ms", blit_vs, blit_debug_ms_fs, &status);
    assert(status == Ren::ProgCreatedFromData);
    LOGI("Compiling blit_ssr_ms");
    blit_ssr_ms_prog_ = ctx_.LoadProgramGLSL("blit_ssr_ms", blit_ms_vs, blit_ssr_ms_fs, &status);
    assert(status == Ren::ProgCreatedFromData);
    LOGI("Compiling blit_ao_ms");
    blit_ao_ms_prog_ = ctx_.LoadProgramGLSL("blit_ao_ms", blit_ms_vs, blit_ssao_ms_fs, &status);
    assert(status == Ren::ProgCreatedFromData);

    {
        GLuint matrices_ubo;

        glGenBuffers(1, &matrices_ubo);
        glBindBuffer(GL_UNIFORM_BUFFER, matrices_ubo);
        glBufferData(GL_UNIFORM_BUFFER, sizeof(MatricesBlock), NULL, GL_STATIC_DRAW);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);

        unif_matrices_block_ = (uint32_t)matrices_ubo;
    }

    {
        GLuint lights_ssbo;

        glGenBuffers(1, &lights_ssbo);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, lights_ssbo);
        glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(LightSourceItem) * MAX_LIGHTS_TOTAL, nullptr, GL_DYNAMIC_COPY);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        lights_ssbo_ = (uint32_t)lights_ssbo;

        GLuint lights_tbo;

        glGenTextures(1, &lights_tbo);
        glBindTexture(GL_TEXTURE_BUFFER, lights_tbo);

        glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, lights_ssbo);
        glBindTexture(GL_TEXTURE_BUFFER, 0);

        lights_tbo_ = (uint32_t)lights_tbo;
    }

    {
        GLuint decals_ssbo;

        glGenBuffers(1, &decals_ssbo);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, decals_ssbo);
        glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(DecalItem) * MAX_DECALS_TOTAL, nullptr, GL_DYNAMIC_COPY);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        decals_ssbo_ = (uint32_t)decals_ssbo;

        GLuint decals_tbo;

        glGenTextures(1, &decals_tbo);
        glBindTexture(GL_TEXTURE_BUFFER, decals_tbo);

        glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, decals_ssbo);
        glBindTexture(GL_TEXTURE_BUFFER, 0);

        decals_tbo_ = (uint32_t)decals_tbo;
    }

    {
        GLuint cells_ssbo;

        glGenBuffers(1, &cells_ssbo);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, cells_ssbo);
        glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(CellData) * CELLS_COUNT, nullptr, GL_DYNAMIC_COPY);

        // fill with zeros
        CellData dummy[GRID_RES_X * GRID_RES_Y] = {};
        for (int i = 0; i < GRID_RES_Z; i++) {
            glBufferSubData(GL_SHADER_STORAGE_BUFFER, i * sizeof(CellData) * GRID_RES_X * GRID_RES_Y, sizeof(CellData) * GRID_RES_X * GRID_RES_Y, &dummy[0]);
        }

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        cells_ssbo_ = (uint32_t)cells_ssbo;

        GLuint cells_tbo;

        glGenTextures(1, &cells_tbo);
        glBindTexture(GL_TEXTURE_BUFFER, cells_tbo);

        glTexBuffer(GL_TEXTURE_BUFFER, GL_RG32UI, cells_ssbo);
        glBindTexture(GL_TEXTURE_BUFFER, 0);

        cells_tbo_ = (uint32_t)cells_tbo;
    }

    {
        GLuint items_ssbo;

        glGenBuffers(1, &items_ssbo);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, items_ssbo);
        glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(ItemData) * MAX_ITEMS_TOTAL, nullptr, GL_DYNAMIC_COPY);

        // fill first entry with zeroes
        ItemData dummy = {};
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(ItemData), &dummy);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        items_ssbo_ = (uint32_t)items_ssbo;

        GLuint items_tbo;

        glGenTextures(1, &items_tbo);
        glBindTexture(GL_TEXTURE_BUFFER, items_tbo);

        glTexBuffer(GL_TEXTURE_BUFFER, GL_R32UI, items_ssbo);
        glBindTexture(GL_TEXTURE_BUFFER, 0);

        items_tbo_ = (uint32_t)items_tbo;
    }

    {
        GLuint temp_tex;
        glGenTextures(1, &temp_tex);
        glBindTexture(GL_TEXTURE_2D, temp_tex);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 128, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        temp_tex_ = (uint32_t)temp_tex;
        temp_tex_w_ = 256;
        temp_tex_h_ = 128;
        temp_tex_format_ = Ren::RawRGBA8888;
    }

    {   // Create timer queries
        for (int i = 0; i < 2; i++) {
            glGenQueries(TimersCount, queries_[i]);
            
            for (int j = 0; j < TimersCount; j++) {
                glQueryCounter(queries_[i][j], GL_TIMESTAMP);
            }
        }
    }
}

void Renderer::CheckInitVAOs() {
    using namespace RendererInternal;

    auto vtx_buf = ctx_.default_vertex_buf();
    auto ndx_buf = ctx_.default_indices_buf();

    GLuint gl_vertex_buf = (GLuint)ctx_.default_vertex_buf()->buf_id(),
           gl_indices_buf = (GLuint)ctx_.default_indices_buf()->buf_id();

    if (gl_vertex_buf != last_vertex_buffer_ || gl_indices_buf != last_index_buffer_) {
        GLuint shadow_pass_vao;
        glGenVertexArrays(1, &shadow_pass_vao);
        glBindVertexArray(shadow_pass_vao);

        glBindBuffer(GL_ARRAY_BUFFER, gl_vertex_buf);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gl_indices_buf);

        int stride = 13 * sizeof(float);
        glEnableVertexAttribArray(A_POS);
        glVertexAttribPointer(A_POS, 3, GL_FLOAT, GL_FALSE, stride, (void *)0);

        glBindVertexArray(0);

        shadow_pass_vao_ = (uint32_t)shadow_pass_vao;

        GLuint depth_pass_vao;
        glGenVertexArrays(1, &depth_pass_vao);
        glBindVertexArray(depth_pass_vao);

        glBindBuffer(GL_ARRAY_BUFFER, gl_vertex_buf);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gl_indices_buf);

        glEnableVertexAttribArray(A_POS);
        glVertexAttribPointer(A_POS, 3, GL_FLOAT, GL_FALSE, stride, (void *)0);

        glBindVertexArray(0);
        depth_pass_vao_ = (uint32_t)depth_pass_vao;

        GLuint draw_pass_vao;
        glGenVertexArrays(1, &draw_pass_vao);
        glBindVertexArray(draw_pass_vao);

        glBindBuffer(GL_ARRAY_BUFFER, gl_vertex_buf);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gl_indices_buf);

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

        {   // Allocate temporary buffer
            temp_buf_vtx_offset_ = vtx_buf->Alloc(TEMP_BUF_SIZE);
            temp_buf_ndx_offset_ = ndx_buf->Alloc(TEMP_BUF_SIZE);

            GLuint temp_vao;
            glGenVertexArrays(1, &temp_vao);

            temp_vao_ = (uint32_t)temp_vao;
        }

        {   // Allocate skydome vertices
            skydome_vtx_offset_ = vtx_buf->Alloc(sizeof(__skydome_positions), __skydome_positions);
            skydome_ndx_offset_ = ndx_buf->Alloc(sizeof(__skydome_indices), __skydome_indices);

            GLuint skydome_vao;
            glGenVertexArrays(1, &skydome_vao);
            glBindVertexArray(skydome_vao);

            glBindBuffer(GL_ARRAY_BUFFER, gl_vertex_buf);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gl_indices_buf);

            glEnableVertexAttribArray(A_POS);
            glVertexAttribPointer(A_POS, 3, GL_FLOAT, GL_FALSE, 0, (void *)uintptr_t(skydome_vtx_offset_));

            glBindVertexArray(0);
            skydome_vao_ = (uint32_t)skydome_vao;
        }

        last_vertex_buffer_ = (uint32_t)gl_vertex_buf;
        last_index_buffer_ = (uint32_t)gl_indices_buf;
    }
}

void Renderer::DestroyRendererInternal() {

    auto vtx_buf = ctx_.default_vertex_buf();
    auto ndx_buf = ctx_.default_indices_buf();

    {
        GLuint matrices_ubo = (GLuint)unif_matrices_block_;
        glDeleteBuffers(1, &matrices_ubo);
    }

    {
        GLuint lights_tbo = (GLuint)lights_tbo_;
        glDeleteTextures(1, &lights_tbo);

        GLuint lights_ssbo = (GLuint)lights_ssbo_;
        glDeleteBuffers(1, &lights_ssbo);
    }

    {
        GLuint decals_tbo = (GLuint)decals_tbo_;
        glDeleteTextures(1, &decals_tbo);

        GLuint lights_ssbo = (GLuint)lights_ssbo_;
        glDeleteBuffers(1, &lights_ssbo);
    }

    {
        GLuint cells_tbo = (GLuint)cells_tbo_;
        glDeleteTextures(1, &cells_tbo);

        GLuint cells_ssbo = (GLuint)cells_ssbo_;
        glDeleteBuffers(1, &cells_ssbo);
    }

    {
        GLuint items_tbo = (GLuint)items_tbo_;
        glDeleteTextures(1, &items_tbo);

        GLuint items_ssbo = (GLuint)items_ssbo_;
        glDeleteBuffers(1, &items_ssbo);
    }

    {
        vtx_buf->Free(skydome_vtx_offset_);
        ndx_buf->Free(skydome_ndx_offset_);

        vtx_buf->Free(temp_buf_vtx_offset_);
        ndx_buf->Free(temp_buf_ndx_offset_);

        GLuint skydome_vao = (GLuint)skydome_vao_;
        glDeleteVertexArrays(1, &skydome_vao);

        GLuint temp_vao = (GLuint)temp_vao_;
        glDeleteVertexArrays(1, &temp_vao);

        GLuint shadow_pass_vao = (GLuint)shadow_pass_vao_;
        glDeleteVertexArrays(1, &shadow_pass_vao);

        GLuint depth_pass_vao = (GLuint)depth_pass_vao_;
        glDeleteVertexArrays(1, &depth_pass_vao);
    }

    {
        for (int i = 0; i < 2; i++) {
            static_assert(sizeof(queries_[0][0]) == sizeof(GLuint), "!");
            glDeleteQueries(TimersCount, queries_[i]);
        }
    }
}

void Renderer::DrawObjectsInternal(const DrawableItem *drawables, size_t drawable_count, const LightSourceItem *lights, size_t lights_count,
                                   const DecalItem *decals, size_t decals_count,
                                   const CellData *cells, const ItemData *items, size_t items_count, const Ren::Mat4f shadow_transforms[4],
                                   const DrawableItem *shadow_drawables[4], size_t shadow_drawable_count[4], const Environment &env,
                                   const TextureAtlas *decals_atlas) {
    using namespace Ren;
    using namespace RendererInternal;

    CheckInitVAOs();

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);

    glDisable(GL_CULL_FACE);

    assert(lights_count < MAX_LIGHTS_TOTAL);
    assert(decals_count < MAX_DECALS_TOTAL);
    assert(items_count < MAX_ITEMS_TOTAL);

    {   // Update lights buffer
        size_t lights_mem_size = lights_count * sizeof(LightSourceItem);
        if (lights_mem_size) {
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, (GLuint)lights_ssbo_);
            void *pinned_mem = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, lights_mem_size, GL_MAP_WRITE_BIT);
            memcpy(pinned_mem, lights, lights_mem_size);
            glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
        }

        render_infos_[1].lights_count = (uint32_t)lights_count;
        render_infos_[1].lights_data_size = (uint32_t)lights_mem_size;

        // Update decals buffer
        size_t decals_mem_size = decals_count * sizeof(DecalItem);
        if (decals_mem_size) {
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, (GLuint)decals_ssbo_);
            void *pinned_mem = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, decals_mem_size, GL_MAP_WRITE_BIT);
            memcpy(pinned_mem, decals, decals_mem_size);
            glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
        }

        render_infos_[1].decals_count = (uint32_t)decals_count;
        render_infos_[1].decals_data_size = (uint32_t)decals_mem_size;

        // Update cells buffer
        size_t cells_mem_size = CELLS_COUNT * sizeof(CellData);
        if (cells_mem_size && cells) {
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, (GLuint)cells_ssbo_);
            void *pinned_mem = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, cells_mem_size, GL_MAP_WRITE_BIT);
            memcpy(pinned_mem, cells, cells_mem_size);
            glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
        }

        render_infos_[1].cells_data_size = (uint32_t)cells_mem_size;

        // Update items buffer
        size_t items_mem_size = items_count * sizeof(ItemData);
        if (items_mem_size) {
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, (GLuint)items_ssbo_);
            void *pinned_mem = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, items_mem_size, GL_MAP_WRITE_BIT);
            memcpy(pinned_mem, items, items_mem_size);
            glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
        }

        render_infos_[1].items_data_size = (uint32_t)items_mem_size;
    }

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

    glQueryCounter(queries_[1][TimeShadowMapStart], GL_TIMESTAMP);

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
                } else if (casc == 2) {
                    glViewport(0, shadow_buf_.h / 2, shadow_buf_.w / 2, shadow_buf_.h / 2);
                    glPolygonOffset(1.75f, 6.0f);
                } else {
                    glViewport(shadow_buf_.w / 2, shadow_buf_.h / 2, shadow_buf_.w / 2, shadow_buf_.h / 2);
                }

                const auto *shadow_dr_list = shadow_drawables[casc];

                for (size_t i = 0; i < shadow_drawable_count[casc]; i++) {
                    const auto &dr = shadow_dr_list[i];
                    
                    const Ren::Mat4f *clip_from_object = dr.clip_from_object;
                    const Ren::Mesh *mesh = dr.mesh;
                    const Ren::TriGroup *tris = dr.tris;

                    if (clip_from_object != cur_clip_from_object) {
                        glUniformMatrix4fv(cur_program->uniform(U_MVP_MATR).loc, 1, GL_FALSE, ValuePtr(clip_from_object));
                        cur_clip_from_object = clip_from_object;
                    }

                    glDrawElements(GL_TRIANGLES, tris->num_indices, GL_UNSIGNED_INT, (void *)uintptr_t(mesh->indices_offset() + tris->offset));
                }
            }
        }

        glPolygonOffset(0.0f, 0.0f);
        glDisable(GL_POLYGON_OFFSET_FILL);

        glBindVertexArray(0);
    }

    Ren::Mat4f view_from_world, clip_from_view, clip_from_world;

    if (!transforms_[0].empty()) {
        view_from_world = transforms_[0][0];
        clip_from_view = transforms_[0][1];
        clip_from_world = clip_from_view * view_from_world;
    }

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    // Bind main buffer for drawing
    glBindFramebuffer(GL_FRAMEBUFFER, clean_buf_.fb);
    //glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, clean_buf_.w, clean_buf_.h);
    //glClearColor(env.sky_col[0], env.sky_col[1], env.sky_col[2], 1.0f);
    //glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

    if (!wireframe_mode_) {   // Draw skydome (and clear depth with it)
        glDepthFunc(GL_ALWAYS);

        // Write to color and specular
        GLenum draw_buffers[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT2 };
        glDrawBuffers(2, draw_buffers);

        cur_program = skydome_prog_.get();
        glUseProgram(cur_program->prog_id());

        glBindVertexArray(skydome_vao_);

        Ren::Mat4f translate_matrix;
        translate_matrix = Ren::Translate(translate_matrix, draw_cam_.world_position());

        Ren::Mat4f scale_matrix;
        scale_matrix = Ren::Scale(scale_matrix, Ren::Vec3f{ 5000.0f, 5000.0f, 5000.0f });

        Ren::Mat4f _clip_from_world = clip_from_world * translate_matrix * scale_matrix;

        glUniformMatrix4fv(cur_program->uniform(U_MVP_MATR).loc, 1, GL_FALSE, ValuePtr(_clip_from_world));
        cur_clip_from_object = nullptr;

        glDrawElements(GL_TRIANGLES, (GLsizei)__skydome_indices_count, GL_UNSIGNED_BYTE, (void *)uintptr_t(skydome_ndx_offset_));

        glDepthFunc(GL_LESS);
    } else {
        glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
    }

    glQueryCounter(queries_[1][TimeDepthPassStart], GL_TIMESTAMP);

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
            const Ren::TriGroup *tris = dr.tris;

            if (clip_from_object != cur_clip_from_object) {
                glUniformMatrix4fv(cur_program->uniform(U_MVP_MATR).loc, 1, GL_FALSE, ValuePtr(clip_from_object));
                cur_clip_from_object = clip_from_object;
            }

            glDrawElements(GL_TRIANGLES, tris->num_indices, GL_UNSIGNED_INT, (void *)uintptr_t(mesh->indices_offset() + tris->offset));
        }

        glBindVertexArray(0);

        glDepthFunc(GL_EQUAL);
    }

    glQueryCounter(queries_[1][TimeAOPassStart], GL_TIMESTAMP);

    glBindVertexArray((GLuint)temp_vao_);

    if (ENABLE_SSAO) {   // prepare ao buffer
        glBindFramebuffer(GL_FRAMEBUFFER, ssao_buf_.fb);
        glViewport(0, 0, ssao_buf_.w, ssao_buf_.h);

        cur_program = blit_ao_ms_prog_.get();
        glUseProgram(cur_program->prog_id());

        const float positions[] = { -1.0f, -1.0f,                 -1.0f + 2.0f, -1.0f,
                                    -1.0f + 2.0f, -1.0f + 2.0f,   -1.0f, -1.0f + 2.0f };

        const float uvs[] = { 0.0f, 0.0f,                                   float(clean_buf_.w), 0.0f,
                              float(clean_buf_.w), float(clean_buf_.h),     0.0f, float(clean_buf_.h) };

        const uint8_t indices[] = { 0, 1, 2,    0, 2, 3 };

        glBindBuffer(GL_ARRAY_BUFFER, last_vertex_buffer_);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, last_index_buffer_);

        glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)temp_buf_vtx_offset_, sizeof(positions), positions);
        glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)(temp_buf_vtx_offset_ + sizeof(positions)), sizeof(uvs), uvs);
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, (GLintptr)temp_buf_ndx_offset_, sizeof(indices), indices);

        glEnableVertexAttribArray(A_POS);
        glVertexAttribPointer(A_POS, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)uintptr_t(temp_buf_vtx_offset_));

        glEnableVertexAttribArray(A_UVS1);
        glVertexAttribPointer(A_UVS1, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)uintptr_t(temp_buf_vtx_offset_ + sizeof(positions)));

        glUniform2f(0, float(clean_buf_.w), float(clean_buf_.h));

        if (true) {
            BindTextureMs(0, clean_buf_.depth_tex.GetValue());
        } else {
            BindTexture(0, clean_buf_.depth_tex.GetValue());
        }

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, (const GLvoid *)uintptr_t(temp_buf_ndx_offset_));

        glDisableVertexAttribArray(A_POS);
        glDisableVertexAttribArray(A_UVS1);

        glBindFramebuffer(GL_FRAMEBUFFER, clean_buf_.fb);
        glViewport(0, 0, clean_buf_.w, clean_buf_.h);
    }

    glBindVertexArray(0);

#if !defined(__ANDROID__)
    if (wireframe_mode_) {
        glDepthFunc(GL_LEQUAL);
        glDepthMask(GL_FALSE);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    } else {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
#endif

    glQueryCounter(queries_[1][TimeDrawStart], GL_TIMESTAMP);

    glBindVertexArray((GLuint)draw_pass_vao_);

    GLenum draw_buffers[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
    glDrawBuffers(3, draw_buffers);

    // actual drawing
    for (size_t i = 0; i < drawable_count; i++) {
        const auto &dr = drawables[i];

        const Ren::Mat4f *clip_from_object = dr.clip_from_object,
                         *world_from_object = dr.world_from_object,
                         *const *sh_clip_from_object = dr.sh_clip_from_object;
        const Ren::Material *mat = dr.mat;
        const Ren::Mesh *mesh = dr.mesh;
        const Ren::TriGroup *tris = dr.tris;

        const auto *p = mat->program().get();

        if (p != cur_program) {
            glUseProgram(p->prog_id());

            glBindBufferBase(GL_UNIFORM_BUFFER, p->uniform_block(U_MATRICES).loc, (GLuint)unif_matrices_block_);

            glUniform3fv(U_SUN_DIR, 1, Ren::ValuePtr(env.sun_dir));
            glUniform3fv(U_SUN_COL, 1, Ren::ValuePtr(env.sun_col));

            glUniform1i(U_RESX, w_);
            glUniform1i(U_RESY, h_);

            glUniform1f(U_GAMMA, 2.2f);
            glUniform1i(U_LIGHTS_COUNT, (GLint)lights_count);

            //glBindBufferBase(GL_SHADER_STORAGE_BUFFER, LIGHTS_BUFFER_BINDING, (GLuint)lights_ssbo_);

            if (clip_from_object == cur_clip_from_object) {
                //glUniformMatrix4fv(p->uniform(U_MVP_MATR).loc, 1, GL_FALSE, ValuePtr(clip_from_object));

                glBindBuffer(GL_UNIFORM_BUFFER, (GLuint)unif_matrices_block_);
                glBufferSubData(GL_UNIFORM_BUFFER, offsetof(MatricesBlock, uMVPMatrix), sizeof(Ren::Mat4f), ValuePtr(clip_from_object));
                glBindBuffer(GL_UNIFORM_BUFFER, 0);
            }

            {
                glBindBuffer(GL_UNIFORM_BUFFER, (GLuint)unif_matrices_block_);
                glBufferSubData(GL_UNIFORM_BUFFER, offsetof(MatricesBlock, uVMatrix), sizeof(Ren::Mat4f), ValuePtr(view_from_world));
                glBindBuffer(GL_UNIFORM_BUFFER, 0);
            }

            if (world_from_object == cur_world_from_object) {
                //glUniformMatrix4fv(p->uniform(U_MV_MATR).loc, 1, GL_FALSE, ValuePtr(world_from_object));

                glBindBuffer(GL_UNIFORM_BUFFER, (GLuint)unif_matrices_block_);
                glBufferSubData(GL_UNIFORM_BUFFER, offsetof(MatricesBlock, uMMatrix), sizeof(Ren::Mat4f), ValuePtr(world_from_object));
                glBindBuffer(GL_UNIFORM_BUFFER, 0);
            }

            BindTexture(SHADOWMAP_SLOT, shadow_buf_.depth_tex.GetValue());
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

            glActiveTexture((GLenum)(GL_TEXTURE0 + LIGHTS_BUFFER_SLOT));
            glBindTexture(GL_TEXTURE_BUFFER, (GLuint)lights_tbo_);

            glActiveTexture((GLenum)(GL_TEXTURE0 + DECALS_BUFFER_SLOT));
            glBindTexture(GL_TEXTURE_BUFFER, (GLuint)decals_tbo_);

            glActiveTexture((GLenum)(GL_TEXTURE0 + CELLS_BUFFER_SLOT));
            glBindTexture(GL_TEXTURE_BUFFER, (GLuint)cells_tbo_);
            
            glActiveTexture((GLenum)(GL_TEXTURE0 + ITEMS_BUFFER_SLOT));
            glBindTexture(GL_TEXTURE_BUFFER, (GLuint)items_tbo_);

            if (decals_atlas) {
                BindTexture(DECALSMAP_SLOT, decals_atlas->tex_id());
            }

            BindTexture(AOMAP_SLOT, ssao_buf_.attachments[0].tex);

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
            glBufferSubData(GL_UNIFORM_BUFFER, offsetof(MatricesBlock, uMMatrix), sizeof(Ren::Mat4f), ValuePtr(world_from_object));
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
            BindTexture(SPECULARMAP_SLOT, mat->texture(2)->tex_id());
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

        glDrawElements(GL_TRIANGLES, tris->num_indices, GL_UNSIGNED_INT, (void *)uintptr_t(mesh->indices_offset() + tris->offset));
    }

#if !defined(__ANDROID__)
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
#endif

    //glBindBufferBase(GL_SHADER_STORAGE_BUFFER, LIGHTS_BUFFER_BINDING, 0);
    glBindVertexArray((GLuint)temp_vao_);
    glDepthFunc(GL_LESS);

    auto view_from_clip = Ren::Inverse(clip_from_view);
    auto delta_matrix = prev_view_from_world_ * Ren::Inverse(view_from_world);

    glQueryCounter(queries_[1][TimeReflStart], GL_TIMESTAMP);

    if (ENABLE_SSR) {   // Compose reflecitons on top of clean buffer
        glBindFramebuffer(GL_FRAMEBUFFER, clean_buf_.fb);

        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);

        GLenum draw_buffers[] = { GL_COLOR_ATTACHMENT0 };
        glDrawBuffers(1, draw_buffers);

        cur_program = blit_ssr_ms_prog_.get();
        glUseProgram(cur_program->prog_id());

        float k = float(w_) / h_;

        const float positions[] = { -1.0f, -1.0f,                 -1.0f + 2.0f, -1.0f,
                                    -1.0f + 2.0f, -1.0f + 2.0f,   -1.0f, -1.0f + 2.0f };

        const float uvs[] = { 0.0f, 0.0f,               float(w_), 0.0f,
                              float(w_), float(h_),     0.0f, float(h_) };

        const uint8_t indices[] = { 0, 1, 2,    0, 2, 3 };

        glBindBuffer(GL_ARRAY_BUFFER, last_vertex_buffer_);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, last_index_buffer_);

        glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)temp_buf_vtx_offset_, sizeof(positions), positions);
        glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)(temp_buf_vtx_offset_ + sizeof(positions)), sizeof(uvs), uvs);
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, (GLintptr)temp_buf_ndx_offset_, sizeof(indices), indices);

        glEnableVertexAttribArray(A_POS);
        glVertexAttribPointer(A_POS, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)uintptr_t(temp_buf_vtx_offset_));

        glEnableVertexAttribArray(A_UVS1);
        glVertexAttribPointer(A_UVS1, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)uintptr_t(temp_buf_vtx_offset_ + sizeof(positions)));

        //glUniform1i(0, DIFFUSEMAP_SLOT);
        //glUniform1f(cur_program->uniform(4).loc, 1.0f);

        glUniformMatrix4fv(0, 1, GL_FALSE, Ren::ValuePtr(clip_from_view));
        glUniformMatrix4fv(1, 1, GL_FALSE, Ren::ValuePtr(view_from_clip));
        glUniformMatrix4fv(2, 1, GL_FALSE, Ren::ValuePtr(delta_matrix));
        glUniform2f(3, float(w_), float(h_));

        if (true) {
            BindTextureMs(0, clean_buf_.depth_tex.GetValue());
            BindTextureMs(1, clean_buf_.attachments[CLEAN_BUF_NORMAL_ATTACHMENT].tex);
            BindTextureMs(2, clean_buf_.attachments[CLEAN_BUF_SPECULAR_ATTACHMENT].tex);
        } else {
            BindTexture(0, clean_buf_.depth_tex.GetValue());
            BindTexture(1, clean_buf_.attachments[CLEAN_BUF_NORMAL_ATTACHMENT].tex);
            BindTexture(2, clean_buf_.attachments[CLEAN_BUF_SPECULAR_ATTACHMENT].tex);
        }

        BindTexture(3, down_buf_.attachments[0].tex);

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, (const GLvoid *)uintptr_t(temp_buf_ndx_offset_));

        glDisableVertexAttribArray(A_POS);
        glDisableVertexAttribArray(A_UVS1);

        glDisable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    glQueryCounter(queries_[1][TimeReflEnd], GL_TIMESTAMP);

    prev_view_from_world_ = view_from_world;
    
    if (debug_deffered_) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(viewport_before[0], viewport_before[1], viewport_before[2], viewport_before[3]);
        glClear(GL_DEPTH_BUFFER_BIT);

        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glDepthFunc(GL_ALWAYS);

        BlitBuffer(0.0f, -1.0f, 0.5f, 0.5f, down_buf_, 0, 1, 400.0f);
    } else {
        glDisable(GL_DEPTH_TEST);
    }

    //glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDepthFunc(GL_LESS);

    {   // prepare blured buffer
        glBindFramebuffer(GL_FRAMEBUFFER, down_buf_.fb);
        glViewport(0, 0, down_buf_.w, down_buf_.h);

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

        glBindBuffer(GL_ARRAY_BUFFER, last_vertex_buffer_);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, last_index_buffer_);

        glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)temp_buf_vtx_offset_, sizeof(fs_quad_pos), fs_quad_pos);
        glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)(temp_buf_vtx_offset_ + sizeof(fs_quad_pos)), sizeof(fs_quad_uvs), fs_quad_uvs);
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, (GLintptr)temp_buf_ndx_offset_, sizeof(fs_quad_indices), fs_quad_indices);

        glEnableVertexAttribArray(A_POS);
        glVertexAttribPointer(A_POS, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)uintptr_t(temp_buf_vtx_offset_));

        glEnableVertexAttribArray(A_UVS1);
        glVertexAttribPointer(A_UVS1, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)uintptr_t(temp_buf_vtx_offset_ + sizeof(fs_quad_pos)));

        if (clean_buf_.msaa > 1) {
            BindTextureMs(DIFFUSEMAP_SLOT, clean_buf_.attachments[0].tex);
        } else {
            BindTexture(DIFFUSEMAP_SLOT, clean_buf_.attachments[0].tex);
        }

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, (const GLvoid *)uintptr_t(temp_buf_ndx_offset_));

        {   // create mipmaps for small buffer
            BindTexture(DIFFUSEMAP_SLOT, down_buf_.attachments[0].tex);
            glGenerateMipmap(GL_TEXTURE_2D);
        }

        ////////////////

        glBindFramebuffer(GL_FRAMEBUFFER, blur_buf2_.fb);
        glViewport(0, 0, blur_buf2_.w, blur_buf2_.h);

        const float fs_quad_uvs1[] = { 0.0f, 0.0f,                                 float(down_buf_.w), 0.0f,
                                       float(down_buf_.w), float(down_buf_.h),     0.0f, float(down_buf_.h) };

        cur_program = blit_gauss_prog_.get();
        glUseProgram(cur_program->prog_id());

        glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)(temp_buf_vtx_offset_ + sizeof(fs_quad_pos)), sizeof(fs_quad_uvs1), fs_quad_uvs1);

        glUniform1i(cur_program->uniform(U_TEX).loc, DIFFUSEMAP_SLOT);
        glUniform1f(cur_program->uniform(4).loc, 0.5f);

        BindTexture(DIFFUSEMAP_SLOT, down_buf_.attachments[0].tex);

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, (const GLvoid *)uintptr_t(temp_buf_ndx_offset_));

        glUniform1f(cur_program->uniform(4).loc, 1.5f);

        glBindFramebuffer(GL_FRAMEBUFFER, blur_buf1_.fb);
        glViewport(0, 0, blur_buf1_.w, blur_buf1_.h);

        const float fs_quad_uvs2[] = { 0.0f, 0.0f,                                   float(blur_buf2_.w), 0.0f,
                                       float(blur_buf2_.w), float(blur_buf2_.h),     0.0f, float(blur_buf2_.h) };

        glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)(temp_buf_vtx_offset_ + sizeof(fs_quad_pos)), sizeof(fs_quad_uvs2), fs_quad_uvs2);

        BindTexture(DIFFUSEMAP_SLOT, blur_buf2_.attachments[0].tex);

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, (const GLvoid *)uintptr_t(temp_buf_ndx_offset_));

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

        glBindBuffer(GL_ARRAY_BUFFER, last_vertex_buffer_);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, last_index_buffer_);

        glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)temp_buf_vtx_offset_, sizeof(fs_quad_pos), fs_quad_pos);
        glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)(temp_buf_vtx_offset_ + sizeof(fs_quad_pos)), sizeof(fs_quad_uvs), fs_quad_uvs);
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, (GLintptr)temp_buf_ndx_offset_, sizeof(fs_quad_indices), fs_quad_indices);

        const Ren::Vec2f offset_step = { 1.0f / reduced_buf_.w, 1.0f / reduced_buf_.h };

        glEnableVertexAttribArray(A_POS);
        glVertexAttribPointer(A_POS, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)uintptr_t(temp_buf_vtx_offset_));

        glEnableVertexAttribArray(A_UVS1);
        glVertexAttribPointer(A_UVS1, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)uintptr_t(temp_buf_vtx_offset_ + sizeof(fs_quad_pos)));

        glUniform1i(cur_program->uniform(U_TEX).loc, DIFFUSEMAP_SLOT);

        static int cur_offset = 0;
        glUniform2f(cur_program->uniform(4).loc, 0.5f * poisson_disk[cur_offset][0] * offset_step[0],
                                                 0.5f * poisson_disk[cur_offset][1] * offset_step[1]);
        cur_offset = cur_offset >= 63 ? 0 : (cur_offset + 1);

        BindTexture(DIFFUSEMAP_SLOT, blur_buf1_.attachments[0].tex);

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, (const GLvoid *)uintptr_t(temp_buf_ndx_offset_));

        glDisableVertexAttribArray(A_POS);
        glDisableVertexAttribArray(A_UVS1);

        reduced_pixels_.resize(4 * reduced_buf_.w * reduced_buf_.h);
        glReadPixels(0, 0, reduced_buf_.w, reduced_buf_.h, GL_RGBA, GL_FLOAT, &reduced_pixels_[0]);

        float cur_average = 0.0f;
        for (size_t i = 0; i < reduced_pixels_.size(); i += 4) {
            if (!std::isnan(reduced_pixels_[i]))
            cur_average += reduced_pixels_[i];
        }

        float k = 1.0f / (reduced_pixels_.size() / 4);
        cur_average *= k;

        const float alpha = 1.0f / 64;
        reduced_average_ = alpha * cur_average + (1.0f - alpha) * reduced_average_;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(viewport_before[0], viewport_before[1], viewport_before[2], viewport_before[3]);

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

        glBindBuffer(GL_ARRAY_BUFFER, last_vertex_buffer_);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, last_index_buffer_);

        glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)temp_buf_vtx_offset_, sizeof(fs_quad_pos), fs_quad_pos);
        glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)(temp_buf_vtx_offset_ + sizeof(fs_quad_pos)), sizeof(fs_quad_uvs), fs_quad_uvs);
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, (GLintptr)temp_buf_ndx_offset_, sizeof(fs_quad_indices), fs_quad_indices);

        glEnableVertexAttribArray(A_POS);
        glVertexAttribPointer(A_POS, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)uintptr_t(temp_buf_vtx_offset_));

        glEnableVertexAttribArray(A_UVS1);
        glVertexAttribPointer(A_UVS1, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)uintptr_t(temp_buf_vtx_offset_ + sizeof(fs_quad_pos)));

        //glUniform1i(cur_program->uniform(U_TEX).loc, DIFFUSEMAP_SLOT);
        //glUniform1i(cur_program->uniform(U_TEX + 1).loc, DIFFUSEMAP_SLOT + 1);
        //glUniform1i(cur_program->uniform(U_TEX + 2).loc, DIFFUSEMAP_SLOT + 2);
        glUniform2f(13, float(w_), float(h_));

        glUniform1f(U_GAMMA, debug_lights_ ? 1.0f : 2.2f);

        float exposure = 0.7f / reduced_average_;
        exposure = std::min(exposure, 1000.0f);

        glUniform1f(U_EXPOSURE, exposure);

        if (clean_buf_.msaa > 1) {
            BindTextureMs(DIFFUSEMAP_SLOT, clean_buf_.attachments[CLEAN_BUF_OPAQUE_ATTACHMENT].tex);
        } else {
            BindTexture(DIFFUSEMAP_SLOT, clean_buf_.attachments[CLEAN_BUF_OPAQUE_ATTACHMENT].tex);
        }

        BindTexture(DIFFUSEMAP_SLOT + 1, blur_buf1_.attachments[0].tex);
        //BindTexture(DIFFUSEMAP_SLOT + 2, refl_buf_.attachments[0].tex);

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, (const GLvoid *)uintptr_t(temp_buf_ndx_offset_));

        glDisableVertexAttribArray(A_POS);
        glDisableVertexAttribArray(A_UVS1);
    }

    if (debug_lights_ || debug_decals_) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        if (clean_buf_.msaa > 1) {
            cur_program = blit_debug_ms_prog_.get();
        } else {
            cur_program = blit_debug_prog_.get();
        }
        glUseProgram(cur_program->prog_id());

        glUniform1i(U_RESX, w_);
        glUniform1i(U_RESY, h_);

        if (debug_lights_) {
            glUniform1i(18, 0);
        } else if (debug_decals_) {
            glUniform1i(18, 1);
        }

        const float fs_quad_pos[] = { -1.0f, -1.0f,       1.0f, -1.0f,
                                      1.0f, 1.0f,         -1.0f, 1.0f };

        const float fs_quad_uvs[] = { 0.0f, 0.0f,               float(w_), 0.0f,
                                      float(w_), float(h_),     0.0f, float(h_) };

        const uint8_t fs_quad_indices[] = { 0, 1, 2,    0, 2, 3 };

        glBindBuffer(GL_ARRAY_BUFFER, last_vertex_buffer_);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, last_index_buffer_);

        glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)temp_buf_vtx_offset_, sizeof(fs_quad_pos), fs_quad_pos);
        glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)(temp_buf_vtx_offset_ + sizeof(fs_quad_pos)), sizeof(fs_quad_uvs), fs_quad_uvs);
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, (GLintptr)temp_buf_ndx_offset_, sizeof(fs_quad_indices), fs_quad_indices);

        glEnableVertexAttribArray(A_POS);
        glVertexAttribPointer(A_POS, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)uintptr_t(temp_buf_vtx_offset_));

        glEnableVertexAttribArray(A_UVS1);
        glVertexAttribPointer(A_UVS1, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)uintptr_t(temp_buf_vtx_offset_ + sizeof(fs_quad_pos)));

        if (clean_buf_.msaa > 1) {
            BindTextureMs(DIFFUSEMAP_SLOT, clean_buf_.depth_tex.GetValue());
        } else {
            BindTexture(DIFFUSEMAP_SLOT, clean_buf_.depth_tex.GetValue());
        }

        glActiveTexture((GLenum)(GL_TEXTURE0 + CELLS_BUFFER_SLOT));
        glBindTexture(GL_TEXTURE_BUFFER, (GLuint)cells_tbo_);

        glActiveTexture((GLenum)(GL_TEXTURE0 + ITEMS_BUFFER_SLOT));
        glBindTexture(GL_TEXTURE_BUFFER, (GLuint)items_tbo_);

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, (const GLvoid *)uintptr_t(temp_buf_ndx_offset_));

        glDisableVertexAttribArray(A_POS);
        glDisableVertexAttribArray(A_UVS1);

        glDisable(GL_BLEND);
    }

    if (debug_cull_ && culling_enabled_ && !depth_pixels_[0].empty()) {
        cur_program = blit_prog_.get();
        glUseProgram(cur_program->prog_id());

        float sx = 2 * 256.0f / w_, sy = 2 * 128.0f / h_;

        const float positions[] = { -1.0f, -1.0f,               -1.0f + sx, -1.0f,
                                    -1.0f + sx, -1.0f + sy,     -1.0f, -1.0f + sy };

        const float uvs[] = { 0.0f, 0.0f,       256.0f, 0.0f,
                              256.0f, 128.0f,   0.0f, 128.0f };

        const uint8_t indices[] = { 0, 1, 2,    0, 2, 3 };

        glBindBuffer(GL_ARRAY_BUFFER, last_vertex_buffer_);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, last_index_buffer_);

        glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)temp_buf_vtx_offset_, sizeof(positions), positions);
        glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)(temp_buf_vtx_offset_ + sizeof(positions)), sizeof(uvs), uvs);
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, (GLintptr)temp_buf_ndx_offset_, sizeof(indices), indices);

        glEnableVertexAttribArray(A_POS);
        glVertexAttribPointer(A_POS, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)uintptr_t(temp_buf_vtx_offset_));

        glEnableVertexAttribArray(A_UVS1);
        glVertexAttribPointer(A_UVS1, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)uintptr_t(temp_buf_vtx_offset_ + sizeof(positions)));

        glUniform1i(cur_program->uniform(U_TEX).loc, DIFFUSEMAP_SLOT);
        glUniform1f(cur_program->uniform(4).loc, 1.0f);

        BindTexture(DIFFUSEMAP_SLOT, temp_tex_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 128, 0, GL_RGBA, GL_UNSIGNED_BYTE, &depth_pixels_[0][0]);

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, (const GLvoid *)uintptr_t(temp_buf_ndx_offset_));

        /////

        const float positions2[] = { -1.0f + sx, -1.0f,               -1.0f + sx + sx, -1.0f,
                                     -1.0f + sx + sx, -1.0f + sy,     -1.0f + sx, -1.0f + sy };

        glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)temp_buf_vtx_offset_, sizeof(positions2), positions2);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 128, 0, GL_RGBA, GL_UNSIGNED_BYTE, &depth_tiles_[0][0]);

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, (const GLvoid *)uintptr_t(temp_buf_ndx_offset_));

        glDisableVertexAttribArray(A_POS);
        glDisableVertexAttribArray(A_UVS1);
    }

    if (debug_shadow_) {
        cur_program = blit_prog_.get();
        glUseProgram(cur_program->prog_id());

        float k = float(w_) / h_;

        const float positions[] = { -1.0f, -1.0f,                       -1.0f + 0.25f, -1.0f,
                                    -1.0f + 0.25f, -1.0f + 0.25f * k,   -1.0f, -1.0f + 0.25f * k };

        const float uvs[] = { 0.0f, 0.0f,       1.0f, 0.0f,
                              1.0f, 1.0f,       0.0f, 1.0f };

        const uint8_t indices[] = { 0, 1, 2,    0, 2, 3 };

        glBindBuffer(GL_ARRAY_BUFFER, last_vertex_buffer_);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, last_index_buffer_);

        glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)temp_buf_vtx_offset_, sizeof(positions), positions);
        glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)(temp_buf_vtx_offset_ + sizeof(positions)), sizeof(uvs), uvs);
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, (GLintptr)temp_buf_ndx_offset_, sizeof(indices), indices);

        glEnableVertexAttribArray(A_POS);
        glVertexAttribPointer(A_POS, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)uintptr_t(temp_buf_vtx_offset_));

        glEnableVertexAttribArray(A_UVS1);
        glVertexAttribPointer(A_UVS1, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)uintptr_t(temp_buf_vtx_offset_ + sizeof(positions)));

        glUniform1i(cur_program->uniform(U_TEX).loc, DIFFUSEMAP_SLOT);
        glUniform1f(cur_program->uniform(4).loc, 1.0f);

        BindTexture(DIFFUSEMAP_SLOT, shadow_buf_.depth_tex.GetValue());

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, (const GLvoid *)uintptr_t(temp_buf_ndx_offset_));

        glDisableVertexAttribArray(A_POS);
        glDisableVertexAttribArray(A_UVS1);
    }

    if (debug_reduce_) {
        BlitBuffer(-1.0f, -1.0f, 0.5f, 0.5f, reduced_buf_, 0, 1, 400.0f);
    }

    if (debug_deffered_) {
        BlitBuffer(-1.0f, -1.0f, 0.5f, 0.5f, clean_buf_, 1, 2);
    }

    if (debug_blur_) {
        BlitBuffer(-1.0f, -1.0f, 1.0f, 1.0f, blur_buf1_, 0, 1, 400.0f);
    }

    if (debug_ssao_) {
        BlitBuffer(-1.0f, -1.0f, 1.0f, 1.0f, ssao_buf_, 0, 1);
    }

    if (debug_decals_ && decals_atlas) {
        int resx = decals_atlas->params().w,
            resy = decals_atlas->params().h;

        float k = float(w_) / h_;
        k *= float(resy) / resx;

        BlitTexture(-1.0f, -1.0f, 1.0f, 1.0f * k, decals_atlas->tex_id(), resx, resy);
    }

    glBindVertexArray(0);

    {   // Get timer queries result
        GLuint64 time1 = 0, time2 = 0;

        glGetQueryObjectui64v(queries_[0][TimeShadowMapStart], GL_QUERY_RESULT, &time1);

        glGetQueryObjectui64v(queries_[0][TimeDepthPassStart], GL_QUERY_RESULT, &time2);
        backend_info_.shadow_time_us = uint32_t((time2 - time1) / 1000);

        glGetQueryObjectui64v(queries_[0][TimeAOPassStart], GL_QUERY_RESULT, &time1);
        backend_info_.ao_pass_time_us = uint32_t((time1 - time2) / 1000);

        glGetQueryObjectui64v(queries_[0][TimeDrawStart], GL_QUERY_RESULT, &time2);
        backend_info_.depth_pass_time_us = uint32_t((time2 - time1) / 1000);

        glGetQueryObjectui64v(queries_[0][TimeReflStart], GL_QUERY_RESULT, &time1);
        backend_info_.opaque_pass_time_us = uint32_t((time1 - time2) / 1000);

        glGetQueryObjectui64v(queries_[0][TimeReflEnd], GL_QUERY_RESULT, &time2);
        backend_info_.refl_pass_time_us = uint32_t((time2 - time1) / 1000);
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

        float k = float(ctx_.h()) / ctx_.w();

        const float fs_quad_pos[] = { -1.0f, -1.0f,       1.0f * k, -1.0f,
                                      1.0f * k, 1.0f,     -1.0f, 1.0f };

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

void Renderer::BlitBuffer(float px, float py, float sx, float sy, const FrameBuf &buf, int first_att, int att_count, float multiplier) {
    using namespace RendererInternal;

    Ren::Program *cur_program = nullptr;

    if (buf.msaa > 1) {
        cur_program = blit_ms_prog_.get();
    } else {
        cur_program = blit_prog_.get();
    }
    glUseProgram(cur_program->prog_id());

    for (int i = first_att; i < first_att + att_count; i++) {
        const float positions[] = { px + (i - first_att) * sx, py,              px + (i - first_att + 1) * sx, py,
                                    px + (i - first_att + 1) * sx, py + sy,     px + (i - first_att) * sx, py + sy };

        if (i == first_att) {
            const float uvs[] = {
                0.0f, 0.0f,                 (float)buf.w, 0.0f,
                (float)buf.w, (float)buf.h,  0.0f, (float)buf.h
            };

            const uint8_t indices[] = { 0, 1, 2,    0, 2, 3 };

            glBindBuffer(GL_ARRAY_BUFFER, last_vertex_buffer_);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, last_index_buffer_);

            glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)(temp_buf_vtx_offset_ + sizeof(positions)), sizeof(uvs), uvs);
            glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, (GLintptr)temp_buf_ndx_offset_, sizeof(indices), indices);

            glEnableVertexAttribArray(A_POS);
            glVertexAttribPointer(A_POS, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)uintptr_t(temp_buf_vtx_offset_));

            glEnableVertexAttribArray(A_UVS1);
            glVertexAttribPointer(A_UVS1, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)uintptr_t(temp_buf_vtx_offset_ + sizeof(positions)));

            glUniform1i(cur_program->uniform(U_TEX).loc, DIFFUSEMAP_SLOT);
            glUniform1f(cur_program->uniform(4).loc, multiplier);
        }

        glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)temp_buf_vtx_offset_, sizeof(positions), positions);

        if (buf.msaa > 1) {
            BindTextureMs(DIFFUSEMAP_SLOT, buf.attachments[i].tex);
        } else {
            BindTexture(DIFFUSEMAP_SLOT, buf.attachments[i].tex);
        }

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, (const GLvoid *)uintptr_t(temp_buf_ndx_offset_));
    }

    glDisableVertexAttribArray(A_POS);
    glDisableVertexAttribArray(A_UVS1);
}

void Renderer::BlitTexture(float px, float py, float sx, float sy, uint32_t tex_id, int resx, int resy, bool is_ms) {
    using namespace RendererInternal;

    Ren::Program *cur_program = nullptr;

    if (is_ms) {
        cur_program = blit_ms_prog_.get();
    } else {
        cur_program = blit_prog_.get();
    }
    glUseProgram(cur_program->prog_id());

    {
        const float positions[] = { px, py,               px + sx, py,
                                    px + sx, py + sy,     px, py + sy };

        const float uvs[] = {
            0.0f, 0.0f,                 (float)resx, 0.0f,
            (float)resx, (float)resy,   0.0f, (float)resy
        };

        const uint8_t indices[] = { 0, 1, 2,    0, 2, 3 };

        glBindBuffer(GL_ARRAY_BUFFER, last_vertex_buffer_);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, last_index_buffer_);

        glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)(temp_buf_vtx_offset_ + sizeof(positions)), sizeof(uvs), uvs);
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, (GLintptr)temp_buf_ndx_offset_, sizeof(indices), indices);

        glEnableVertexAttribArray(A_POS);
        glVertexAttribPointer(A_POS, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)uintptr_t(temp_buf_vtx_offset_));

        glEnableVertexAttribArray(A_UVS1);
        glVertexAttribPointer(A_UVS1, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)uintptr_t(temp_buf_vtx_offset_ + sizeof(positions)));

        glUniform1i(cur_program->uniform(U_TEX).loc, DIFFUSEMAP_SLOT);
        glUniform1f(cur_program->uniform(4).loc, 1.0f);

        glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)temp_buf_vtx_offset_, sizeof(positions), positions);

        if (is_ms) {
            BindTextureMs(DIFFUSEMAP_SLOT, tex_id);
        } else {
            BindTexture(DIFFUSEMAP_SLOT, tex_id);
        }

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, (const GLvoid *)uintptr_t(temp_buf_ndx_offset_));
    }

    glDisableVertexAttribArray(A_POS);
    glDisableVertexAttribArray(A_UVS1);
}

#undef _AS_STR
#undef AS_STR