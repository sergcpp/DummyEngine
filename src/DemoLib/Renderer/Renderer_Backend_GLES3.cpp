#include "Renderer.h"

#include <chrono>

#include <Ren/Camera.h>
#include <Ren/Context.h>
#include <Ren/GL.h>
#include <Sys/Log.h>

namespace RendererInternal {
#include "Renderer_GL_Shaders.inl"
#include "__skydome_mesh.inl"

    struct SharedDataBlock {
        Ren::Mat4f uViewMatrix, uProjMatrix, uViewProjMatrix;
        Ren::Mat4f uInvViewMatrix, uInvProjMatrix, uInvViewProjMatrix, uDeltaMatrix;
        Ren::Mat4f uSunShadowMatrix[4];
        Ren::Vec4f uSunDir, uSunCol;
        Ren::Vec4f uClipInfo, uCamPosAndGamma;
        Ren::Vec4f uResAndFRes;
    };
    static_assert(sizeof(SharedDataBlock) == 784, "!");

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

    const GLuint A_INDICES = 3;
    const GLuint A_WEIGHTS = 4;

    const int U_MVP_MATR = 0;
    const int U_MV_MATR = 1;

    const int U_SH_MVP_MATR = 2;

    const int U_TEX = 3;

    const int U_GAMMA = 14;

    const int U_EXPOSURE = 15;

    const int U_RES = 15;
    
    const int U_LM_TRANSFORM = 16;

    const int LIGHTS_BUFFER_BINDING = 0;

    inline void BindTexture(int slot, uint32_t tex) {
        glActiveTexture((GLenum)(GL_TEXTURE0 + slot));
        glBindTexture(GL_TEXTURE_2D, (GLuint)tex);
    }

    inline void BindTextureMs(int slot, uint32_t tex) {
        glActiveTexture((GLenum)(GL_TEXTURE0 + slot));
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, (GLuint)tex);
    }

    inline void BindCubemap(int slot, uint32_t tex) {
        glActiveTexture((GLenum)(GL_TEXTURE0 + slot));
        glBindTexture(GL_TEXTURE_CUBE_MAP, (GLuint)tex);
    }

    const int TEMP_BUF_SIZE = 256;

    const float fs_quad_positions[] = { -1.0f, -1.0f,   1.0f, -1.0f,
                                         1.0f, 1.0f,   -1.0f, 1.0f };

    const uint8_t fs_quad_indices[] = { 0, 1, 2,    0, 2, 3 };
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
    LOGI("Compiling blit_multiply");
    blit_multiply_prog_ = ctx_.LoadProgramGLSL("blit_multiply", blit_vs, blit_multiply_fs, &status);
    assert(status == Ren::ProgCreatedFromData);
    LOGI("Compiling blit_multiply_ms");
    blit_multiply_ms_prog_ = ctx_.LoadProgramGLSL("blit_multiply_ms", blit_ms_vs, blit_multiply_ms_fs, &status);
    assert(status == Ren::ProgCreatedFromData);
    LOGI("Compiling blit_debug_bvh");
    blit_debug_bvh_prog_ = ctx_.LoadProgramGLSL("blit_debug_bvh", blit_vs, blit_debug_bvh_fs, &status);
    assert(status == Ren::ProgCreatedFromData);
    LOGI("Compiling blit_debug_bvh_ms");
    blit_debug_bvh_ms_prog_ = ctx_.LoadProgramGLSL("blit_debug_bvh_ms", blit_ms_vs, blit_debug_bvh_ms_fs, &status);
    assert(status == Ren::ProgCreatedFromData);

    {
        GLuint shared_data_ubo;

        glGenBuffers(1, &shared_data_ubo);
        glBindBuffer(GL_UNIFORM_BUFFER, shared_data_ubo);
        glBufferData(GL_UNIFORM_BUFFER, sizeof(SharedDataBlock), NULL, GL_STATIC_DRAW);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);

        unif_shared_data_block_ = (uint32_t)shared_data_ubo;
    }

    Ren::CheckError("[InitRendererInternal]: UBO creation");

    {
        GLuint instances_buf;

        glGenBuffers(1, &instances_buf);
        glBindBuffer(GL_TEXTURE_BUFFER, instances_buf);
        glBufferData(GL_TEXTURE_BUFFER, sizeof(InstanceData) * MAX_INSTANCES_TOTAL, nullptr, GL_DYNAMIC_COPY);
        glBindBuffer(GL_TEXTURE_BUFFER, 0);

        instances_buf_ = (uint32_t)instances_buf;

        GLuint instances_tbo;

        glGenTextures(1, &instances_tbo);
        glBindTexture(GL_TEXTURE_BUFFER, instances_tbo);

        glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, instances_buf);
        glBindTexture(GL_TEXTURE_BUFFER, 0);

        instances_tbo_ = (uint32_t)instances_tbo;
    }

    Ren::CheckError("[InitRendererInternal]: instances TBO");

    {
        GLuint lights_buf;

        glGenBuffers(1, &lights_buf);
        glBindBuffer(GL_TEXTURE_BUFFER, lights_buf);
        glBufferData(GL_TEXTURE_BUFFER, sizeof(LightSourceItem) * MAX_LIGHTS_TOTAL, nullptr, GL_DYNAMIC_COPY);
        glBindBuffer(GL_TEXTURE_BUFFER, 0);

        lights_buf_ = (uint32_t)lights_buf;

        GLuint lights_tbo;

        glGenTextures(1, &lights_tbo);
        glBindTexture(GL_TEXTURE_BUFFER, lights_tbo);

        glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, lights_buf);
        glBindTexture(GL_TEXTURE_BUFFER, 0);

        lights_tbo_ = (uint32_t)lights_tbo;
    }

    Ren::CheckError("[InitRendererInternal]: lights TBO");

    {
        GLuint decals_buf;

        glGenBuffers(1, &decals_buf);
        glBindBuffer(GL_TEXTURE_BUFFER, decals_buf);
        glBufferData(GL_TEXTURE_BUFFER, sizeof(DecalItem) * MAX_DECALS_TOTAL, nullptr, GL_DYNAMIC_COPY);
        glBindBuffer(GL_TEXTURE_BUFFER, 0);

        decals_buf_ = (uint32_t)decals_buf;

        GLuint decals_tbo;

        glGenTextures(1, &decals_tbo);
        glBindTexture(GL_TEXTURE_BUFFER, decals_tbo);

        glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, decals_buf);
        glBindTexture(GL_TEXTURE_BUFFER, 0);

        decals_tbo_ = (uint32_t)decals_tbo;
    }

    Ren::CheckError("[InitRendererInternal]: decals TBO");

    {
        GLuint cells_buf;

        glGenBuffers(1, &cells_buf);
        glBindBuffer(GL_TEXTURE_BUFFER, cells_buf);
        glBufferData(GL_TEXTURE_BUFFER, sizeof(CellData) * CELLS_COUNT, nullptr, GL_DYNAMIC_COPY);

        // fill with zeros
        CellData dummy[GRID_RES_X * GRID_RES_Y] = {};
        for (int i = 0; i < GRID_RES_Z; i++) {
            glBufferSubData(GL_TEXTURE_BUFFER, i * sizeof(CellData) * GRID_RES_X * GRID_RES_Y, sizeof(CellData) * GRID_RES_X * GRID_RES_Y, &dummy[0]);
        }

        glBindBuffer(GL_TEXTURE_BUFFER, 0);

        cells_buf_ = (uint32_t)cells_buf;

        GLuint cells_tbo;

        glGenTextures(1, &cells_tbo);
        glBindTexture(GL_TEXTURE_BUFFER, cells_tbo);

        glTexBuffer(GL_TEXTURE_BUFFER, GL_RG32UI, cells_buf);
        glBindTexture(GL_TEXTURE_BUFFER, 0);

        cells_tbo_ = (uint32_t)cells_tbo;
    }

    Ren::CheckError("[InitRendererInternal]: cells TBO");

    {
        GLuint items_buf;

        glGenBuffers(1, &items_buf);
        glBindBuffer(GL_TEXTURE_BUFFER, items_buf);
        glBufferData(GL_TEXTURE_BUFFER, sizeof(ItemData) * MAX_ITEMS_TOTAL, nullptr, GL_DYNAMIC_COPY);

        // fill first entry with zeroes
        ItemData dummy = {};
        glBufferSubData(GL_TEXTURE_BUFFER, 0, sizeof(ItemData), &dummy);

        glBindBuffer(GL_TEXTURE_BUFFER, 0);

        items_buf_ = (uint32_t)items_buf;

        GLuint items_tbo;

        glGenTextures(1, &items_tbo);
        glBindTexture(GL_TEXTURE_BUFFER, items_tbo);

        glTexBuffer(GL_TEXTURE_BUFFER, GL_R32UI, items_buf);
        glBindTexture(GL_TEXTURE_BUFFER, 0);

        items_tbo_ = (uint32_t)items_tbo;
    }

    Ren::CheckError("[InitRendererInternal]: items TBO");

    {
        GLuint reduce_pbo;
        glGenBuffers(1, &reduce_pbo);
        glBindBuffer(GL_PIXEL_PACK_BUFFER, reduce_pbo);
        glBufferData(GL_PIXEL_PACK_BUFFER, 4 * reduced_buf_.w * reduced_buf_.h * sizeof(float), 0, GL_DYNAMIC_READ);

        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        reduce_pbo_ = (uint32_t)reduce_pbo;
    }

    Ren::CheckError("[InitRendererInternal]: reduce PBO");

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

    Ren::CheckError("[InitRendererInternal]: temp texture");

    {   // Create timer queries
        for (int i = 0; i < 2; i++) {
            glGenQueries(TimersCount, queries_[i]);
            
            for (int j = 0; j < TimersCount; j++) {
                glQueryCounter(queries_[i][j], GL_TIMESTAMP);
            }
        }
    }

    Ren::CheckError("[InitRendererInternal]: timer queries");
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
        glEnableVertexAttribArray(REN_VTX_POS_LOC);
        glVertexAttribPointer(REN_VTX_POS_LOC, 3, GL_FLOAT, GL_FALSE, stride, (void *)0);

        glBindVertexArray(0);

        shadow_pass_vao_ = (uint32_t)shadow_pass_vao;

        GLuint depth_pass_vao;
        glGenVertexArrays(1, &depth_pass_vao);
        glBindVertexArray(depth_pass_vao);

        glBindBuffer(GL_ARRAY_BUFFER, gl_vertex_buf);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gl_indices_buf);

        glEnableVertexAttribArray(REN_VTX_POS_LOC);
        glVertexAttribPointer(REN_VTX_POS_LOC, 3, GL_FLOAT, GL_FALSE, stride, (void *)0);

        glBindVertexArray(0);
        depth_pass_vao_ = (uint32_t)depth_pass_vao;

        GLuint draw_pass_vao;
        glGenVertexArrays(1, &draw_pass_vao);
        glBindVertexArray(draw_pass_vao);

        glBindBuffer(GL_ARRAY_BUFFER, gl_vertex_buf);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gl_indices_buf);

        glEnableVertexAttribArray(REN_VTX_POS_LOC);
        glVertexAttribPointer(REN_VTX_POS_LOC, 3, GL_FLOAT, GL_FALSE, stride, (void *)0);

        glEnableVertexAttribArray(REN_VTX_NOR_LOC);
        glVertexAttribPointer(REN_VTX_NOR_LOC, 3, GL_FLOAT, GL_FALSE, stride, (void *)(3 * sizeof(float)));

        glEnableVertexAttribArray(REN_VTX_TAN_LOC);
        glVertexAttribPointer(REN_VTX_TAN_LOC, 3, GL_FLOAT, GL_FALSE, stride, (void *)(6 * sizeof(float)));

        glEnableVertexAttribArray(REN_VTX_UV1_LOC);
        glVertexAttribPointer(REN_VTX_UV1_LOC, 2, GL_FLOAT, GL_FALSE, stride, (void *)(9 * sizeof(float)));

        glEnableVertexAttribArray(REN_VTX_UV2_LOC);
        glVertexAttribPointer(REN_VTX_UV2_LOC, 2, GL_FLOAT, GL_FALSE, stride, (void *)(11 * sizeof(float)));

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

            glEnableVertexAttribArray(REN_VTX_POS_LOC);
            glVertexAttribPointer(REN_VTX_POS_LOC, 3, GL_FLOAT, GL_FALSE, 0, (void *)uintptr_t(skydome_vtx_offset_));

            glBindVertexArray(0);
            skydome_vao_ = (uint32_t)skydome_vao;
        }

        last_vertex_buffer_ = (uint32_t)gl_vertex_buf;
        last_index_buffer_ = (uint32_t)gl_indices_buf;
    }
}

void Renderer::DestroyRendererInternal() {
    LOGI("DestroyRendererInternal");

    auto vtx_buf = ctx_.default_vertex_buf();
    auto ndx_buf = ctx_.default_indices_buf();

    {
        GLuint shared_data_ubo = (GLuint)unif_shared_data_block_;
        glDeleteBuffers(1, &shared_data_ubo);
    }

    {
        GLuint instances_tbo = (GLuint)instances_tbo_;
        glDeleteTextures(1, &instances_tbo);

        GLuint instances_buf = (GLuint)instances_buf_;
        glDeleteBuffers(1, &instances_buf);
    }

    {
        GLuint lights_tbo = (GLuint)lights_tbo_;
        glDeleteTextures(1, &lights_tbo);

        GLuint lights_buf = (GLuint)lights_buf_;
        glDeleteBuffers(1, &lights_buf);
    }

    {
        GLuint decals_tbo = (GLuint)decals_tbo_;
        glDeleteTextures(1, &decals_tbo);

        GLuint lights_buf = (GLuint)lights_buf_;
        glDeleteBuffers(1, &lights_buf);
    }

    {
        GLuint cells_tbo = (GLuint)cells_tbo_;
        glDeleteTextures(1, &cells_tbo);

        GLuint cells_buf = (GLuint)cells_buf_;
        glDeleteBuffers(1, &cells_buf);
    }

    {
        GLuint items_tbo = (GLuint)items_tbo_;
        glDeleteTextures(1, &items_tbo);

        GLuint items_buf = (GLuint)items_buf_;
        glDeleteBuffers(1, &items_buf);
    }

    if (nodes_buf_) {
        GLuint nodes_tbo = (GLuint)nodes_tbo_;
        glDeleteTextures(1, &nodes_tbo);

        GLuint nodes_buf = (GLuint)nodes_buf_;
        glDeleteBuffers(1, &nodes_buf);
    }

    {
        GLuint reduce_pbo = (GLuint)reduce_pbo_;
        glDeleteBuffers(1, &reduce_pbo);
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

void Renderer::DrawObjectsInternal(const DrawablesData &data) {
    using namespace Ren;
    using namespace RendererInternal;

    glQueryCounter(queries_[1][TimeDrawStart], GL_TIMESTAMP);

    CheckInitVAOs();

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);

    glDisable(GL_CULL_FACE);

    assert(data.light_sources.size() < MAX_LIGHTS_TOTAL);
    assert(data.decals.size() < MAX_DECALS_TOTAL);
    assert(data.items_count < MAX_ITEMS_TOTAL);

    backend_info_.shadow_draw_calls_count = 0;
    backend_info_.depth_fill_draw_calls_count = 0;
    backend_info_.opaque_draw_calls_count = 0;

    {   
        // Update instance buffer
        size_t instance_mem_size = data.instances.size() * sizeof(InstanceData);
        if (instance_mem_size) {
            glBindBuffer(GL_TEXTURE_BUFFER, (GLuint)instances_buf_);
            glBufferSubData(GL_TEXTURE_BUFFER, 0, instance_mem_size, data.instances.data());
            glBindBuffer(GL_TEXTURE_BUFFER, 0);
        }

        // Update lights buffer
        size_t lights_mem_size = data.light_sources.size() * sizeof(LightSourceItem);
        if (lights_mem_size) {
            glBindBuffer(GL_TEXTURE_BUFFER, (GLuint)lights_buf_);
            glBufferSubData(GL_TEXTURE_BUFFER, 0, lights_mem_size, data.light_sources.data());
            glBindBuffer(GL_TEXTURE_BUFFER, 0);
        }

        // Update decals buffer
        size_t decals_mem_size = data.decals.size() * sizeof(DecalItem);
        if (decals_mem_size) {
            glBindBuffer(GL_TEXTURE_BUFFER, (GLuint)decals_buf_);
            glBufferSubData(GL_TEXTURE_BUFFER, 0, decals_mem_size, data.decals.data());
            glBindBuffer(GL_TEXTURE_BUFFER, 0);
        }

        // Update cells buffer
        size_t cells_mem_size = data.cells.size() * sizeof(CellData);
        if (cells_mem_size) {
            glBindBuffer(GL_TEXTURE_BUFFER, (GLuint)cells_buf_);
            glBufferSubData(GL_TEXTURE_BUFFER, 0, cells_mem_size, data.cells.data());
            glBindBuffer(GL_TEXTURE_BUFFER, 0);
        }

        // Update items buffer
        size_t items_mem_size = data.items_count * sizeof(ItemData);
        if (items_mem_size) {
            glBindBuffer(GL_TEXTURE_BUFFER, (GLuint)items_buf_);
            glBufferSubData(GL_TEXTURE_BUFFER, 0, items_mem_size, data.items.data());
            glBindBuffer(GL_TEXTURE_BUFFER, 0);
        }
    }

    SharedDataBlock shrd_data;

    {   // Prepare data that is shared for all instaces
        shrd_data.uViewMatrix = data.draw_cam.view_matrix();
        shrd_data.uProjMatrix = data.draw_cam.proj_matrix();
        shrd_data.uViewProjMatrix = shrd_data.uProjMatrix * shrd_data.uViewMatrix;
        shrd_data.uInvViewMatrix = Ren::Inverse(shrd_data.uViewMatrix);
        shrd_data.uInvProjMatrix = Ren::Inverse(shrd_data.uProjMatrix);
        shrd_data.uInvViewProjMatrix = Ren::Inverse(shrd_data.uViewProjMatrix);
        // delta matrix between current and previous frame
        shrd_data.uDeltaMatrix = prev_view_from_world_ * shrd_data.uInvViewMatrix;
        shrd_data.uSunShadowMatrix[0] = data.shadow_cams[0].proj_matrix() * data.shadow_cams[0].view_matrix();
        shrd_data.uSunShadowMatrix[1] = data.shadow_cams[1].proj_matrix() * data.shadow_cams[1].view_matrix();
        shrd_data.uSunShadowMatrix[2] = data.shadow_cams[2].proj_matrix() * data.shadow_cams[2].view_matrix();
        shrd_data.uSunShadowMatrix[3] = data.shadow_cams[3].proj_matrix() * data.shadow_cams[3].view_matrix();
        shrd_data.uSunDir = Ren::Vec4f{ data.env.sun_dir[0], data.env.sun_dir[1], data.env.sun_dir[2], 0.0f };
        shrd_data.uSunCol = Ren::Vec4f{ data.env.sun_col[0], data.env.sun_col[1], data.env.sun_col[2], 0.0f };
        shrd_data.uResAndFRes = Ren::Vec4f{ float(act_w_), float(act_h_), float(clean_buf_.w), float(clean_buf_.h) };

        const float near = data.draw_cam.near(), far = data.draw_cam.far();
        shrd_data.uClipInfo = { near * far, near, far, std::log2(1.0f + far / near) };

        const auto &pos = data.draw_cam.world_position();
        shrd_data.uCamPosAndGamma = Ren::Vec4f{ pos[0], pos[1], pos[2], 2.2f };

        glBindBuffer(GL_UNIFORM_BUFFER, (GLuint)unif_shared_data_block_);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(SharedDataBlock), &shrd_data);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }

    int32_t viewport_before[4];
    glGetIntegerv(GL_VIEWPORT, viewport_before);

    /**************************************************************************************************/
    /*                                           SHADOW PASS                                          */
    /**************************************************************************************************/

    glQueryCounter(queries_[1][TimeShadowMapStart], GL_TIMESTAMP);

    {   // draw shadow map
        glEnable(GL_POLYGON_OFFSET_FILL);

        glBindVertexArray(shadow_pass_vao_);

        glActiveTexture((GLenum)(GL_TEXTURE0 + 0));
        glBindTexture(GL_TEXTURE_BUFFER, (GLuint)instances_tbo_);

        glBindFramebuffer(GL_FRAMEBUFFER, shadow_buf_.fb);

        glUseProgram(shadow_prog_->prog_id());

        for (int casc = 0; casc < 4; casc++) {
            const auto &shadow_list = data.shadow_lists[casc];

            if (shadow_list.shadow_batch_count) {
                const int OneCascadeRes = SUN_SHADOW_RES / 2;

                if (casc == 0) {
                    glViewport(0, 0, OneCascadeRes, OneCascadeRes);
                    glPolygonOffset(1.85f, 6.0f);
                } else if (casc == 1) {
                    glViewport(OneCascadeRes, 0, OneCascadeRes, OneCascadeRes);
                } else if (casc == 2) {
                    glViewport(0, OneCascadeRes, OneCascadeRes, OneCascadeRes);
                } else {
                    glViewport(OneCascadeRes, OneCascadeRes, OneCascadeRes, OneCascadeRes);
                }

                glUniformMatrix4fv(REN_U_M_MATRIX_LOC, 1, GL_FALSE, Ren::ValuePtr(shrd_data.uSunShadowMatrix[casc]));

                for (uint32_t i = shadow_list.shadow_batch_start; i < shadow_list.shadow_batch_start + shadow_list.shadow_batch_count; i++) {
                    const auto &batch = data.shadow_batches[i];
                    if (!batch.instance_count) continue;

                    glUniform1iv(REN_U_INSTANCES_LOC, batch.instance_count, &batch.instance_indices[0]);

                    glDrawElementsInstanced(GL_TRIANGLES, batch.indices_count, GL_UNSIGNED_INT, (const GLvoid *)uintptr_t(batch.indices_offset),
                                            (GLsizei)batch.instance_count);
                    backend_info_.shadow_draw_calls_count++;
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
    glViewport(0, 0, act_w_, act_h_);

    /**************************************************************************************************/
    /*                                          SKYDOME PASS                                          */
    /**************************************************************************************************/

    if ((data.render_flags & DebugWireframe) == 0 && data.env.env_map) {   // Draw skydome (and clear depth with it)
        glDepthFunc(GL_ALWAYS);

        // Write to color and specular
        GLenum draw_buffers[] = { GL_COLOR_ATTACHMENT0, GL_NONE, GL_COLOR_ATTACHMENT2 };
        glDrawBuffers(3, draw_buffers);

        glUseProgram(skydome_prog_->prog_id());

        glBindVertexArray(skydome_vao_);

        glBindBufferBase(GL_UNIFORM_BUFFER, skydome_prog_->uniform_block(REN_UB_SHARED_DATA_LOC).loc, (GLuint)unif_shared_data_block_);

        Ren::Mat4f translate_matrix;
        translate_matrix = Ren::Translate(translate_matrix, Ren::Vec3f{ shrd_data.uCamPosAndGamma });

        Ren::Mat4f scale_matrix;
        scale_matrix = Ren::Scale(scale_matrix, Ren::Vec3f{ 5000.0f, 5000.0f, 5000.0f });

        Ren::Mat4f world_from_object = translate_matrix * scale_matrix;
        glUniformMatrix4fv(REN_U_M_MATRIX_LOC, 1, GL_FALSE, Ren::ValuePtr(world_from_object));

        BindCubemap(REN_DIFF_TEX_SLOT, data.env.env_map->tex_id());

        glDrawElements(GL_TRIANGLES, (GLsizei)__skydome_indices_count, GL_UNSIGNED_BYTE, (void *)uintptr_t(skydome_ndx_offset_));

        glDepthFunc(GL_LESS);
    } else {
        glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
    }

    /**************************************************************************************************/
    /*                                        DEPTH-FILL PASS                                         */
    /**************************************************************************************************/

    glQueryCounter(queries_[1][TimeDepthPassStart], GL_TIMESTAMP);

    if ((data.render_flags & EnableZFill) && ((data.render_flags & DebugWireframe) == 0)) {
        glDepthFunc(GL_LESS);

        glBindVertexArray(depth_pass_vao_);

        glActiveTexture((GLenum)(GL_TEXTURE0 + 0));
        glBindTexture(GL_TEXTURE_BUFFER, (GLuint)instances_tbo_);

        glUseProgram(fill_depth_prog_->prog_id());

        glBindBufferBase(GL_UNIFORM_BUFFER, fill_depth_prog_->uniform_block(REN_UB_SHARED_DATA_LOC).loc, (GLuint)unif_shared_data_block_);

        // fill depth
        for (const auto &batch : data.main_batches) {
            if (!batch.instance_count) continue;

            glUniform1iv(REN_U_INSTANCES_LOC, batch.instance_count, &batch.instance_indices[0]);

            glDrawElementsInstanced(GL_TRIANGLES, batch.indices_count, GL_UNSIGNED_INT, (const GLvoid *)uintptr_t(batch.indices_offset),
                                    (GLsizei)batch.instance_count);
            backend_info_.depth_fill_draw_calls_count++;
        }

        glBindVertexArray(0);

        glDepthFunc(GL_EQUAL);
    }

    /**************************************************************************************************/
    /*                                            SSAO PASS                                           */
    /**************************************************************************************************/

    glQueryCounter(queries_[1][TimeAOPassStart], GL_TIMESTAMP);

    glBindVertexArray((GLuint)temp_vao_);

    if (data.render_flags & EnableSSAO) {
        // prepare ao buffer
        glBindFramebuffer(GL_FRAMEBUFFER, ssao_buf_.fb);
        glViewport(0, 0, ssao_buf_.w, ssao_buf_.h);

        const Ren::Program *ssao_prog = nullptr;

        if (clean_buf_.msaa > 1) {
            ssao_prog = blit_ao_ms_prog_.get();
        } else {

        }

        glUseProgram(ssao_prog->prog_id());

        glBindBufferBase(GL_UNIFORM_BUFFER, ssao_prog->uniform_block(REN_UB_SHARED_DATA_LOC).loc, (GLuint)unif_shared_data_block_);

        const float uvs[] = { 0.0f, 0.0f,                       float(act_w_), 0.0f,
                              float(act_w_), float(act_h_),     0.0f, float(act_h_) };

        glBindBuffer(GL_ARRAY_BUFFER, last_vertex_buffer_);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, last_index_buffer_);

        glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)temp_buf_vtx_offset_, sizeof(fs_quad_positions), fs_quad_positions);
        glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)(temp_buf_vtx_offset_ + sizeof(fs_quad_positions)), sizeof(uvs), uvs);
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, (GLintptr)temp_buf_ndx_offset_, sizeof(fs_quad_indices), fs_quad_indices);

        glEnableVertexAttribArray(REN_VTX_POS_LOC);
        glVertexAttribPointer(REN_VTX_POS_LOC, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)uintptr_t(temp_buf_vtx_offset_));

        glEnableVertexAttribArray(REN_VTX_UV1_LOC);
        glVertexAttribPointer(REN_VTX_UV1_LOC, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)uintptr_t(temp_buf_vtx_offset_ + sizeof(fs_quad_positions)));

        if (clean_buf_.msaa > 1) {
            BindTextureMs(0, clean_buf_.depth_tex.GetValue());
        } else {
            BindTexture(0, clean_buf_.depth_tex.GetValue());
        }

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, (const GLvoid *)uintptr_t(temp_buf_ndx_offset_));

        glDisableVertexAttribArray(REN_VTX_POS_LOC);
        glDisableVertexAttribArray(REN_VTX_UV1_LOC);

        glBindFramebuffer(GL_FRAMEBUFFER, clean_buf_.fb);
        glViewport(0, 0, act_w_, act_h_);
    }

#if !defined(__ANDROID__)
    if (data.render_flags & DebugWireframe) {
        glDepthFunc(GL_LEQUAL);
        glDepthMask(GL_FALSE);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    } else {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
#endif

    /**************************************************************************************************/
    /*                                           OPAQUE PASS                                          */
    /**************************************************************************************************/

    glQueryCounter(queries_[1][TimeOpaqueStart], GL_TIMESTAMP);

    glBindVertexArray((GLuint)draw_pass_vao_);

    glActiveTexture((GLenum)(GL_TEXTURE0 + REN_INSTANCE_BUF_SLOT));
    glBindTexture(GL_TEXTURE_BUFFER, (GLuint)instances_tbo_);

    BindTexture(REN_SHAD_TEX_SLOT, shadow_buf_.depth_tex.GetValue());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

    glActiveTexture((GLenum)(GL_TEXTURE0 + REN_LIGHT_BUF_SLOT));
    glBindTexture(GL_TEXTURE_BUFFER, (GLuint)lights_tbo_);

    glActiveTexture((GLenum)(GL_TEXTURE0 + REN_DECAL_BUF_SLOT));
    glBindTexture(GL_TEXTURE_BUFFER, (GLuint)decals_tbo_);

    glActiveTexture((GLenum)(GL_TEXTURE0 + REN_CELLS_BUF_SLOT));
    glBindTexture(GL_TEXTURE_BUFFER, (GLuint)cells_tbo_);

    glActiveTexture((GLenum)(GL_TEXTURE0 + REN_ITEMS_BUF_SLOT));
    glBindTexture(GL_TEXTURE_BUFFER, (GLuint)items_tbo_);

    if (data.decals_atlas) {
        BindTexture(REN_DECAL_TEX_SLOT, data.decals_atlas->tex_id(0));
    }

    if (data.render_flags & EnableSSAO) {
        BindTexture(REN_SSAO_TEX_SLOT, ssao_buf_.attachments[0].tex);
    } else {
        BindTexture(REN_SSAO_TEX_SLOT, default_ao_->tex_id());
    }

    if ((data.render_flags & EnableLightmap) && data.env.lm_direct) {
        BindTexture(REN_LMAP_DIR_SLOT, data.env.lm_direct->tex_id());
        BindTexture(REN_LMAP_INDIR_SLOT, data.env.lm_indir->tex_id());
        for (int sh_l = 0; sh_l < 4; sh_l++) {
            BindTexture(REN_LMAP_SH_SLOT + sh_l, data.env.lm_indir_sh[sh_l]->tex_id());
        }
    } else {
        BindTexture(REN_LMAP_DIR_SLOT, default_lightmap_->tex_id());
        BindTexture(REN_LMAP_INDIR_SLOT, default_lightmap_->tex_id());
        for (int sh_l = 0; sh_l < 4; sh_l++) {
            BindTexture(REN_LMAP_SH_SLOT + sh_l, default_lightmap_->tex_id());
        }
    }

    GLenum draw_buffers[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
    glDrawBuffers(3, draw_buffers);
    
    {   // actual drawing
        const Ren::Program *cur_program = nullptr;
        const Ren::Material *cur_mat = nullptr;

        for (const auto &batch : data.main_batches) {
            if (!batch.instance_count) continue;

            const Ren::Program *p = ctx_.GetProgram(batch.prog_id).get();
            const Ren::Material *mat = ctx_.GetMaterial(batch.mat_id).get();

            if (cur_program != p) {
                glUseProgram(p->prog_id());

                glBindBufferBase(GL_UNIFORM_BUFFER, p->uniform_block(REN_UB_SHARED_DATA_LOC).loc, (GLuint)unif_shared_data_block_);

                cur_program = p;
            }

            if (cur_mat != mat) {
                BindTexture(REN_DIFF_TEX_SLOT, mat->texture(0)->tex_id());
                BindTexture(REN_NORM_TEX_SLOT, mat->texture(1)->tex_id());
                BindTexture(REN_SPEC_TEX_SLOT, mat->texture(2)->tex_id());
                cur_mat = mat;
            }

            glUniform1iv(REN_U_INSTANCES_LOC, batch.instance_count, &batch.instance_indices[0]);

            glDrawElementsInstanced(GL_TRIANGLES, batch.indices_count, GL_UNSIGNED_INT, (const GLvoid *)uintptr_t(batch.indices_offset),
                                    (GLsizei)batch.instance_count);
            backend_info_.opaque_draw_calls_count++;
        }
    }

#if !defined(__ANDROID__)
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
#endif

    glBindVertexArray((GLuint)temp_vao_);
    glDepthFunc(GL_LESS);

    /**************************************************************************************************/
    /*                                             SSR PASS                                           */
    /**************************************************************************************************/

    glQueryCounter(queries_[1][TimeReflStart], GL_TIMESTAMP);

    if (data.render_flags & EnableSSR) {
        glBindFramebuffer(GL_FRAMEBUFFER, refl_buf_.fb);
        glViewport(0, 0, refl_buf_.w, refl_buf_.h);

        const Ren::Program *cur_program = blit_ssr_ms_prog_.get();
        glUseProgram(cur_program->prog_id());

        glBindBufferBase(GL_UNIFORM_BUFFER, cur_program->uniform_block(REN_UB_SHARED_DATA_LOC).loc, (GLuint)unif_shared_data_block_);

        const float uvs[] = { 0.0f, 0.0f,                       float(act_w_), 0.0f,
                              float(act_w_), float(act_h_),     0.0f, float(act_h_) };

        glBindBuffer(GL_ARRAY_BUFFER, last_vertex_buffer_);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, last_index_buffer_);

        glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)temp_buf_vtx_offset_, sizeof(fs_quad_positions), fs_quad_positions);
        glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)(temp_buf_vtx_offset_ + sizeof(fs_quad_positions)), sizeof(uvs), uvs);
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, (GLintptr)temp_buf_ndx_offset_, sizeof(fs_quad_indices), fs_quad_indices);

        glEnableVertexAttribArray(REN_VTX_POS_LOC);
        glVertexAttribPointer(REN_VTX_POS_LOC, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)uintptr_t(temp_buf_vtx_offset_));

        glEnableVertexAttribArray(REN_VTX_UV1_LOC);
        glVertexAttribPointer(REN_VTX_UV1_LOC, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)uintptr_t(temp_buf_vtx_offset_ + sizeof(fs_quad_positions)));

        if (true) {
            BindTextureMs(REN_SSR_DEPTH_TEX_SLOT, clean_buf_.depth_tex.GetValue());
            BindTextureMs(REN_SSR_NORM_TEX_SLOT, clean_buf_.attachments[REN_OUT_NORM_INDEX].tex);
            BindTextureMs(REN_SSR_SPEC_TEX_SLOT, clean_buf_.attachments[REN_OUT_SPEC_INDEX].tex);
        } else {
            BindTexture(REN_SSR_DEPTH_TEX_SLOT, clean_buf_.depth_tex.GetValue());
            BindTexture(REN_SSR_NORM_TEX_SLOT, clean_buf_.attachments[REN_OUT_NORM_INDEX].tex);
            BindTexture(REN_SSR_SPEC_TEX_SLOT, clean_buf_.attachments[REN_OUT_SPEC_INDEX].tex);
        }

        BindTexture(REN_SSR_PREV_TEX_SLOT, down_buf_.attachments[0].tex);
        if (data.env.env_map) {
            BindCubemap(REN_SSR_ENV_TEX_SLOT, data.env.env_map->tex_id());
        }

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, (const GLvoid *)uintptr_t(temp_buf_ndx_offset_));

        // Compose reflections on top of clean buffer
        glBindFramebuffer(GL_FRAMEBUFFER, clean_buf_.fb);
        glViewport(0, 0, act_w_, act_h_);

        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);

        GLenum draw_buffers[] = { GL_COLOR_ATTACHMENT0 };
        glDrawBuffers(1, draw_buffers);

        cur_program = blit_multiply_ms_prog_.get();
        glUseProgram(cur_program->prog_id());

        glUniform2f(13, float(act_w_), float(act_h_));

        BindTexture(0, refl_buf_.attachments[0].tex);

        if (true) {
            BindTextureMs(1, clean_buf_.attachments[REN_OUT_SPEC_INDEX].tex);
        } else {
            BindTexture(1, clean_buf_.attachments[REN_OUT_SPEC_INDEX].tex);
        }

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, (const GLvoid *)uintptr_t(temp_buf_ndx_offset_));

        glDisableVertexAttribArray(REN_VTX_POS_LOC);
        glDisableVertexAttribArray(REN_VTX_UV1_LOC);

        glDisable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    prev_view_from_world_ = shrd_data.uViewMatrix;
    
    if (data.render_flags & DebugDeferred) {
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

    glQueryCounter(queries_[1][TimeBlurStart], GL_TIMESTAMP);

    //glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDepthFunc(GL_LESS);

    {   // prepare blured buffer
        glBindFramebuffer(GL_FRAMEBUFFER, down_buf_.fb);
        glViewport(0, 0, down_buf_.w, down_buf_.h);

        const float fs_quad_uvs[] = { 0.0f, 0.0f,                       float(act_w_), 0.0f,
                                      float(act_w_), float(act_h_),     0.0f, float(act_h_) };

        const Ren::Program *cur_program = nullptr;

        if (clean_buf_.msaa > 1) {
            cur_program = blit_down_ms_prog_.get();
        } else {
            cur_program = blit_down_prog_.get();
        }
        glUseProgram(cur_program->prog_id());

        glBindBuffer(GL_ARRAY_BUFFER, last_vertex_buffer_);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, last_index_buffer_);

        glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)temp_buf_vtx_offset_, sizeof(fs_quad_positions), fs_quad_positions);
        glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)(temp_buf_vtx_offset_ + sizeof(fs_quad_positions)), sizeof(fs_quad_uvs), fs_quad_uvs);
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, (GLintptr)temp_buf_ndx_offset_, sizeof(fs_quad_indices), fs_quad_indices);

        glEnableVertexAttribArray(REN_VTX_POS_LOC);
        glVertexAttribPointer(REN_VTX_POS_LOC, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)uintptr_t(temp_buf_vtx_offset_));

        glEnableVertexAttribArray(REN_VTX_UV1_LOC);
        glVertexAttribPointer(REN_VTX_UV1_LOC, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)uintptr_t(temp_buf_vtx_offset_ + sizeof(fs_quad_positions)));

        if (clean_buf_.msaa > 1) {
            BindTextureMs(REN_DIFF_TEX_SLOT, clean_buf_.attachments[0].tex);
        } else {
            BindTexture(REN_DIFF_TEX_SLOT, clean_buf_.attachments[0].tex);
        }

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, (const GLvoid *)uintptr_t(temp_buf_ndx_offset_));

        ////////////////

        glBindFramebuffer(GL_FRAMEBUFFER, blur_buf2_.fb);
        glViewport(0, 0, blur_buf2_.w, blur_buf2_.h);

        const float fs_quad_uvs1[] = { 0.0f, 0.0f,                                 float(down_buf_.w), 0.0f,
                                       float(down_buf_.w), float(down_buf_.h),     0.0f, float(down_buf_.h) };

        cur_program = blit_gauss_prog_.get();
        glUseProgram(cur_program->prog_id());

        glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)(temp_buf_vtx_offset_ + sizeof(fs_quad_positions)), sizeof(fs_quad_uvs1), fs_quad_uvs1);

        glUniform1i(cur_program->uniform(U_TEX).loc, REN_DIFF_TEX_SLOT);
        glUniform1f(cur_program->uniform(4).loc, 0.5f);

        BindTexture(REN_DIFF_TEX_SLOT, down_buf_.attachments[0].tex);

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, (const GLvoid *)uintptr_t(temp_buf_ndx_offset_));

        glUniform1f(cur_program->uniform(4).loc, 1.5f);

        glBindFramebuffer(GL_FRAMEBUFFER, blur_buf1_.fb);
        glViewport(0, 0, blur_buf1_.w, blur_buf1_.h);

        const float fs_quad_uvs2[] = { 0.0f, 0.0f,                                   float(blur_buf2_.w), 0.0f,
                                       float(blur_buf2_.w), float(blur_buf2_.h),     0.0f, float(blur_buf2_.h) };

        glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)(temp_buf_vtx_offset_ + sizeof(fs_quad_positions)), sizeof(fs_quad_uvs2), fs_quad_uvs2);

        BindTexture(REN_DIFF_TEX_SLOT, blur_buf2_.attachments[0].tex);

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, (const GLvoid *)uintptr_t(temp_buf_ndx_offset_));

        glDisableVertexAttribArray(REN_VTX_POS_LOC);
        glDisableVertexAttribArray(REN_VTX_UV1_LOC);
    }

    {   // draw to small framebuffer
        glBindFramebuffer(GL_FRAMEBUFFER, reduced_buf_.fb);
        glViewport(0, 0, reduced_buf_.w, reduced_buf_.h);

        glUseProgram(blit_red_prog_->prog_id());

        const float fs_quad_uvs[] = { 0.0f, 0.0f,     1.0f, 0.0f,
                                      1.0f, 1.0f,     0.0f, 1.0f };

        glBindBuffer(GL_ARRAY_BUFFER, last_vertex_buffer_);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, last_index_buffer_);

        glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)temp_buf_vtx_offset_, sizeof(fs_quad_positions), fs_quad_positions);
        glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)(temp_buf_vtx_offset_ + sizeof(fs_quad_positions)), sizeof(fs_quad_uvs), fs_quad_uvs);
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, (GLintptr)temp_buf_ndx_offset_, sizeof(fs_quad_indices), fs_quad_indices);

        const Ren::Vec2f offset_step = { 1.0f / reduced_buf_.w, 1.0f / reduced_buf_.h };

        glEnableVertexAttribArray(REN_VTX_POS_LOC);
        glVertexAttribPointer(REN_VTX_POS_LOC, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)uintptr_t(temp_buf_vtx_offset_));

        glEnableVertexAttribArray(REN_VTX_UV1_LOC);
        glVertexAttribPointer(REN_VTX_UV1_LOC, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)uintptr_t(temp_buf_vtx_offset_ + sizeof(fs_quad_positions)));

        glUniform1i(blit_red_prog_->uniform(U_TEX).loc, REN_DIFF_TEX_SLOT);

        static int cur_offset = 0;
        glUniform2f(blit_red_prog_->uniform(4).loc, 0.5f * poisson_disk[cur_offset][0] * offset_step[0],
                                                    0.5f * poisson_disk[cur_offset][1] * offset_step[1]);
        cur_offset = cur_offset >= 63 ? 0 : (cur_offset + 1);

        BindTexture(REN_DIFF_TEX_SLOT, blur_buf1_.attachments[0].tex);

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, (const GLvoid *)uintptr_t(temp_buf_ndx_offset_));

        glDisableVertexAttribArray(REN_VTX_POS_LOC);
        glDisableVertexAttribArray(REN_VTX_UV1_LOC);

        const float max_value = 64.0f;
        float cur_average = 0.0f;

        {   // Retrieve result of glReadPixels call from previous frame
            glBindBuffer(GL_PIXEL_PACK_BUFFER, (GLuint)reduce_pbo_);
            float *reduced_pixels = (float *)glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, reduced_buf_.w * reduced_buf_.h, GL_MAP_READ_BIT);
            if (reduced_pixels) {
                for (int i = 0; i < 4 * reduced_buf_.w * reduced_buf_.h; i += 4) {
                    if (!std::isnan(reduced_pixels[i])) {
                        cur_average += std::min(reduced_pixels[i], max_value);
                    }
                }
                glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
            }
            glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        }

        float k = 1.0f / (reduced_buf_.w * reduced_buf_.h);
        cur_average *= k;

        const float alpha = 1.0f / 64;
        reduced_average_ = alpha * cur_average + (1.0f - alpha) * reduced_average_;
    }

    glQueryCounter(queries_[1][TimeBlitStart], GL_TIMESTAMP);
    

    {   // Clear shadowmap buffer
        glBindFramebuffer(GL_FRAMEBUFFER, shadow_buf_.fb);
        glViewport(0, 0, shadow_buf_.w, shadow_buf_.h);
        glDepthMask(GL_TRUE);
        glClear(GL_DEPTH_BUFFER_BIT);
        glDepthMask(GL_FALSE);
    }
    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(viewport_before[0], viewport_before[1], viewport_before[2], viewport_before[3]);

    {   // Blit main framebuffer
        const Ren::Program *blit_prog = nullptr;

        if (clean_buf_.msaa > 1) {
            blit_prog = blit_combine_ms_prog_.get();
        } else {
            blit_prog = blit_combine_prog_.get();
        }
        glUseProgram(blit_prog->prog_id());

        const float fs_quad_uvs[] = { 0.0f, 0.0f,                       float(act_w_), 0.0f,
                                      float(act_w_), float(act_h_),     0.0f, float(act_h_) };

        glBindBuffer(GL_ARRAY_BUFFER, last_vertex_buffer_);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, last_index_buffer_);

        glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)temp_buf_vtx_offset_, sizeof(fs_quad_positions), fs_quad_positions);
        glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)(temp_buf_vtx_offset_ + sizeof(fs_quad_positions)), sizeof(fs_quad_uvs), fs_quad_uvs);
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, (GLintptr)temp_buf_ndx_offset_, sizeof(fs_quad_indices), fs_quad_indices);

        glEnableVertexAttribArray(REN_VTX_POS_LOC);
        glVertexAttribPointer(REN_VTX_POS_LOC, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)uintptr_t(temp_buf_vtx_offset_));

        glEnableVertexAttribArray(REN_VTX_UV1_LOC);
        glVertexAttribPointer(REN_VTX_UV1_LOC, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)uintptr_t(temp_buf_vtx_offset_ + sizeof(fs_quad_positions)));

        glUniform2f(13, float(act_w_), float(act_h_));

        glUniform1f(U_GAMMA, (data.render_flags & DebugLights) ? 1.0f : 2.2f);

        float exposure = reduced_average_ > FLT_EPSILON ? (0.85f / reduced_average_) : 1.0f;
        exposure = std::min(exposure, 1000.0f);

        glUniform1f(U_EXPOSURE, exposure);

        if (clean_buf_.msaa > 1) {
            BindTextureMs(REN_DIFF_TEX_SLOT, clean_buf_.attachments[REN_OUT_COLOR_INDEX].tex);
        } else {
            BindTexture(REN_DIFF_TEX_SLOT, clean_buf_.attachments[REN_OUT_COLOR_INDEX].tex);
        }

        BindTexture(REN_DIFF_TEX_SLOT + 1, blur_buf1_.attachments[0].tex);

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, (const GLvoid *)uintptr_t(temp_buf_ndx_offset_));

        glDisableVertexAttribArray(REN_VTX_POS_LOC);
        glDisableVertexAttribArray(REN_VTX_UV1_LOC);
    }
    
    {   
        if (data.render_flags & EnableSSR) {
            BindTexture(REN_DIFF_TEX_SLOT, down_buf_.attachments[0].tex);
            glGenerateMipmap(GL_TEXTURE_2D);
        }

        // Start asynchronous memory read from framebuffer

        glBindFramebuffer(GL_FRAMEBUFFER, reduced_buf_.fb);
        glReadBuffer(GL_COLOR_ATTACHMENT0);

        glBindBuffer(GL_PIXEL_PACK_BUFFER, (GLuint)reduce_pbo_);

        glReadPixels(0, 0, reduced_buf_.w, reduced_buf_.h, GL_RGBA, GL_FLOAT, nullptr);

        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    if (data.render_flags & (DebugLights | DebugDecals)) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        const Ren::Program *blit_prog = nullptr;

        if (clean_buf_.msaa > 1) {
            blit_prog = blit_debug_ms_prog_.get();
        } else {
            blit_prog = blit_debug_prog_.get();
        }
        glUseProgram(blit_prog->prog_id());

        glUniform2i(U_RES, scr_w_, scr_h_);

        if (data.render_flags & DebugLights) {
            glUniform1i(16, 0);
        } else if (data.render_flags & DebugDecals) {
            glUniform1i(16, 1);
        }

        glUniform4fv(17, 1, Ren::ValuePtr(shrd_data.uClipInfo));

        const float fs_quad_uvs[] = { 0.0f, 0.0f,                       float(act_w_), 0.0f,
                                      float(act_w_), float(act_h_),     0.0f, float(act_h_) };

        glBindBuffer(GL_ARRAY_BUFFER, last_vertex_buffer_);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, last_index_buffer_);

        glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)temp_buf_vtx_offset_, sizeof(fs_quad_positions), fs_quad_positions);
        glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)(temp_buf_vtx_offset_ + sizeof(fs_quad_positions)), sizeof(fs_quad_uvs), fs_quad_uvs);
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, (GLintptr)temp_buf_ndx_offset_, sizeof(fs_quad_indices), fs_quad_indices);

        glEnableVertexAttribArray(REN_VTX_POS_LOC);
        glVertexAttribPointer(REN_VTX_POS_LOC, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)uintptr_t(temp_buf_vtx_offset_));

        glEnableVertexAttribArray(REN_VTX_UV1_LOC);
        glVertexAttribPointer(REN_VTX_UV1_LOC, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)uintptr_t(temp_buf_vtx_offset_ + sizeof(fs_quad_positions)));

        if (clean_buf_.msaa > 1) {
            BindTextureMs(REN_DIFF_TEX_SLOT, clean_buf_.depth_tex.GetValue());
        } else {
            BindTexture(REN_DIFF_TEX_SLOT, clean_buf_.depth_tex.GetValue());
        }

        glActiveTexture((GLenum)(GL_TEXTURE0 + REN_CELLS_BUF_SLOT));
        glBindTexture(GL_TEXTURE_BUFFER, (GLuint)cells_tbo_);

        glActiveTexture((GLenum)(GL_TEXTURE0 + REN_ITEMS_BUF_SLOT));
        glBindTexture(GL_TEXTURE_BUFFER, (GLuint)items_tbo_);

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, (const GLvoid *)uintptr_t(temp_buf_ndx_offset_));

        glDisableVertexAttribArray(REN_VTX_POS_LOC);
        glDisableVertexAttribArray(REN_VTX_UV1_LOC);

        glDisable(GL_BLEND);
    }

    if (((data.render_flags & (EnableCulling | DebugCulling)) == (EnableCulling | DebugCulling)) && !depth_pixels_[0].empty()) {
        glUseProgram(blit_prog_->prog_id());

        float sx = 2 * 256.0f / scr_w_, sy = 2 * 128.0f / scr_h_;

        const float positions[] = { -1.0f, -1.0f,               -1.0f + sx, -1.0f,
                                    -1.0f + sx, -1.0f + sy,     -1.0f, -1.0f + sy };

        const float uvs[] = { 0.0f, 0.0f,       256.0f, 0.0f,
                              256.0f, 128.0f,   0.0f, 128.0f };

        glBindBuffer(GL_ARRAY_BUFFER, last_vertex_buffer_);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, last_index_buffer_);

        glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)temp_buf_vtx_offset_, sizeof(positions), positions);
        glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)(temp_buf_vtx_offset_ + sizeof(positions)), sizeof(uvs), uvs);
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, (GLintptr)temp_buf_ndx_offset_, sizeof(fs_quad_indices), fs_quad_indices);

        glEnableVertexAttribArray(REN_VTX_POS_LOC);
        glVertexAttribPointer(REN_VTX_POS_LOC, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)uintptr_t(temp_buf_vtx_offset_));

        glEnableVertexAttribArray(REN_VTX_UV1_LOC);
        glVertexAttribPointer(REN_VTX_UV1_LOC, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)uintptr_t(temp_buf_vtx_offset_ + sizeof(positions)));

        glUniform1i(blit_prog_->uniform(U_TEX).loc, REN_DIFF_TEX_SLOT);
        glUniform1f(blit_prog_->uniform(4).loc, 1.0f);

        BindTexture(REN_DIFF_TEX_SLOT, temp_tex_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 128, 0, GL_RGBA, GL_UNSIGNED_BYTE, &depth_pixels_[0][0]);

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, (const GLvoid *)uintptr_t(temp_buf_ndx_offset_));

        /////

        const float positions2[] = { -1.0f + sx, -1.0f,               -1.0f + sx + sx, -1.0f,
                                     -1.0f + sx + sx, -1.0f + sy,     -1.0f + sx, -1.0f + sy };

        glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)temp_buf_vtx_offset_, sizeof(positions2), positions2);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 128, 0, GL_RGBA, GL_UNSIGNED_BYTE, &depth_tiles_[0][0]);

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, (const GLvoid *)uintptr_t(temp_buf_ndx_offset_));

        glDisableVertexAttribArray(REN_VTX_POS_LOC);
        glDisableVertexAttribArray(REN_VTX_UV1_LOC);
    }

    if (data.render_flags & DebugShadow) {
        glUseProgram(blit_prog_->prog_id());

        float k = (float(shadow_buf_.h) / shadow_buf_.w) * (float(scr_w_) / scr_h_);

        const float positions[] = { -1.0f, -1.0f,                   -1.0f + 1.0f, -1.0f,
                                    -1.0f + 1.0f, -1.0f + 1.0f * k, -1.0f, -1.0f + 1.0f * k };

        const float uvs[] = { 0.0f, 0.0f,       1.0f, 0.0f,
                              1.0f, 1.0f,       0.0f, 1.0f };

        glBindBuffer(GL_ARRAY_BUFFER, last_vertex_buffer_);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, last_index_buffer_);

        glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)temp_buf_vtx_offset_, sizeof(positions), positions);
        glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)(temp_buf_vtx_offset_ + sizeof(positions)), sizeof(uvs), uvs);
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, (GLintptr)temp_buf_ndx_offset_, sizeof(fs_quad_indices), fs_quad_indices);

        glEnableVertexAttribArray(REN_VTX_POS_LOC);
        glVertexAttribPointer(REN_VTX_POS_LOC, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)uintptr_t(temp_buf_vtx_offset_));

        glEnableVertexAttribArray(REN_VTX_UV1_LOC);
        glVertexAttribPointer(REN_VTX_UV1_LOC, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)uintptr_t(temp_buf_vtx_offset_ + sizeof(positions)));

        glUniform1i(blit_prog_->uniform(U_TEX).loc, REN_DIFF_TEX_SLOT);
        glUniform1f(blit_prog_->uniform(4).loc, 1.0f);

        BindTexture(REN_DIFF_TEX_SLOT, shadow_buf_.depth_tex.GetValue());

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, (const GLvoid *)uintptr_t(temp_buf_ndx_offset_));

        glDisableVertexAttribArray(REN_VTX_POS_LOC);
        glDisableVertexAttribArray(REN_VTX_UV1_LOC);
    }

    if (data.render_flags & DebugReduce) {
        BlitBuffer(-1.0f, -1.0f, 0.5f, 0.5f, reduced_buf_, 0, 1, 10.0f);
    }

    if (data.render_flags & DebugDeferred) {
        BlitBuffer(-1.0f, -1.0f, 0.5f, 0.5f, clean_buf_, 1, 2);
    }

    if (data.render_flags & DebugBlur) {
        BlitBuffer(-1.0f, -1.0f, 1.0f, 1.0f, blur_buf1_, 0, 1, 400.0f);
    }

    if (data.render_flags & DebugSSAO) {
        BlitBuffer(-1.0f, -1.0f, 1.0f, 1.0f, ssao_buf_, 0, 1);
    }

    if ((data.render_flags & DebugDecals) && data.decals_atlas) {
        int resx = data.decals_atlas->resx(),
            resy = data.decals_atlas->resy();

        float k = float(scr_w_) / scr_h_;
        k *= float(resy) / resx;

        BlitTexture(-1.0f, -1.0f, 1.0f, 1.0f * k, data.decals_atlas->tex_id(0), resx, resy);
    }

    if (data.render_flags & DebugBVH) {
        if (!nodes_buf_) {
            GLuint nodes_buf;
            glGenBuffers(1, &nodes_buf);

            glBindBuffer(GL_TEXTURE_BUFFER, (GLuint)nodes_buf);
            glBufferData(GL_TEXTURE_BUFFER, data.temp_nodes.size() * sizeof(bvh_node_t), data.temp_nodes.data(), GL_DYNAMIC_DRAW);

            nodes_buf_ = (uint32_t)nodes_buf;

            GLuint nodes_tbo;

            glGenTextures(1, &nodes_tbo);
            glBindTexture(GL_TEXTURE_BUFFER, nodes_tbo);

            glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, nodes_buf);
            glBindTexture(GL_TEXTURE_BUFFER, 0);

            nodes_tbo_ = (uint32_t)nodes_tbo;
        } else {
            glBindBuffer(GL_TEXTURE_BUFFER, (GLuint)nodes_buf_);
            glBufferData(GL_TEXTURE_BUFFER, data.temp_nodes.size() * sizeof(bvh_node_t), data.temp_nodes.data(), GL_DYNAMIC_DRAW);
        }

        {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            const Ren::Program *debug_bvh_prog = nullptr;

            debug_bvh_prog = blit_debug_bvh_ms_prog_.get();
            glUseProgram(debug_bvh_prog->prog_id());

            const float uvs[] = { 0.0f, 0.0f,                       float(scr_w_), 0.0f,
                                  float(scr_w_), float(scr_h_),     0.0f, float(scr_h_) };

            glBindBuffer(GL_ARRAY_BUFFER, last_vertex_buffer_);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, last_index_buffer_);

            glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)temp_buf_vtx_offset_, sizeof(fs_quad_positions), fs_quad_positions);
            glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)(temp_buf_vtx_offset_ + sizeof(fs_quad_positions)), sizeof(uvs), uvs);
            glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, (GLintptr)temp_buf_ndx_offset_, sizeof(fs_quad_indices), fs_quad_indices);

            glEnableVertexAttribArray(REN_VTX_POS_LOC);
            glVertexAttribPointer(REN_VTX_POS_LOC, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)uintptr_t(temp_buf_vtx_offset_));

            glEnableVertexAttribArray(REN_VTX_UV1_LOC);
            glVertexAttribPointer(REN_VTX_UV1_LOC, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)uintptr_t(temp_buf_vtx_offset_ + sizeof(fs_quad_positions)));

            BindTextureMs(0, clean_buf_.depth_tex.GetValue());

            glActiveTexture((GLenum)(GL_TEXTURE0 + 1));
            glBindTexture(GL_TEXTURE_BUFFER, (GLuint)nodes_tbo_);

            glUniform1i(debug_bvh_prog->uniform("uRootIndex").loc, data.root_index);

            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, (const GLvoid *)uintptr_t(temp_buf_ndx_offset_));

            glDisableVertexAttribArray(REN_VTX_POS_LOC);
            glDisableVertexAttribArray(REN_VTX_UV1_LOC);

            glDisable(GL_BLEND);
        }

        glBindBuffer(GL_TEXTURE_BUFFER, 0);
    }

    glBindVertexArray(0);

    glQueryCounter(queries_[1][TimeDrawEnd], GL_TIMESTAMP);

    {   // Get timer queries result (for previous frame)
        GLuint64 time_draw_start,
                 time_shadow_start,
                 time_depth_start,
                 time_ao_start,
                 time_opaque_start,
                 time_refl_start,
                 time_blur_start,
                 time_blit_start,
                 time_draw_end;

        glGetQueryObjectui64v(queries_[0][TimeDrawStart], GL_QUERY_RESULT, &time_draw_start);
        glGetQueryObjectui64v(queries_[0][TimeShadowMapStart], GL_QUERY_RESULT, &time_shadow_start);
        glGetQueryObjectui64v(queries_[0][TimeDepthPassStart], GL_QUERY_RESULT, &time_depth_start);
        glGetQueryObjectui64v(queries_[0][TimeAOPassStart], GL_QUERY_RESULT, &time_ao_start);
        glGetQueryObjectui64v(queries_[0][TimeOpaqueStart], GL_QUERY_RESULT, &time_opaque_start);
        glGetQueryObjectui64v(queries_[0][TimeReflStart], GL_QUERY_RESULT, &time_refl_start);
        glGetQueryObjectui64v(queries_[0][TimeBlurStart], GL_QUERY_RESULT, &time_blur_start);
        glGetQueryObjectui64v(queries_[0][TimeBlitStart], GL_QUERY_RESULT, &time_blit_start);
        glGetQueryObjectui64v(queries_[0][TimeDrawEnd], GL_QUERY_RESULT, &time_draw_end);

        // assign values from previous frame
        backend_info_.cpu_start_timepoint_us = backend_cpu_start_;
        backend_info_.cpu_end_timepoint_us = backend_cpu_end_;
        backend_info_.gpu_cpu_time_diff_us = backend_time_diff_;

        backend_info_.gpu_start_timepoint_us = uint64_t(time_draw_start / 1000);
        backend_info_.gpu_end_timepoint_us = uint64_t(time_draw_end / 1000);

        backend_info_.shadow_time_us = uint32_t((time_depth_start - time_shadow_start) / 1000);
        backend_info_.depth_pass_time_us = uint32_t((time_ao_start - time_depth_start) / 1000);
        backend_info_.ao_pass_time_us = uint32_t((time_opaque_start - time_ao_start) / 1000);
        backend_info_.opaque_pass_time_us = uint32_t((time_refl_start - time_opaque_start) / 1000);
        backend_info_.refl_pass_time_us = uint32_t((time_blur_start - time_refl_start) / 1000);
        backend_info_.blur_pass_time_us = uint32_t((time_blit_start - time_blur_start) / 1000);
        backend_info_.blit_pass_time_us = uint32_t((time_draw_end - time_blit_start) / 1000);

        for (int i = 0; i < TimersCount; i++) {
            std::swap(queries_[0][i], queries_[1][i]);
        }
    }

#if 0
    glFinish();
#endif
}

uint64_t Renderer::GetGpuTimeBlockingUs() {
    GLint64 time = 0;
    glGetInteger64v(GL_TIMESTAMP, &time);
    return (uint64_t)(time / 1000);
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

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glBindVertexArray((GLuint)temp_vao_);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    BlitTexture(-1.0f, 1.0f, 2.0f, -2.0f, temp_tex_, w, h);

    glBindVertexArray(0);
}

void Renderer::BlitPixelsTonemap(const void *data, int w, int h, const Ren::eTexColorFormat format) {
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

    assert(format == Ren::RawRGBA32F);

    float avarage_color[3] = {};
    int sample_count = 0;
    const float *_data = (const float *)data;
    
    for (int y = 0; y < h; y += 100) {
        for (int x = 0; x < w; x += 100) {
            int i = y * w + x;
            avarage_color[0] += _data[i * 4 + 0];
            avarage_color[1] += _data[i * 4 + 1];
            avarage_color[2] += _data[i * 4 + 2];
            sample_count++;
        }
    }

    avarage_color[0] /= sample_count;
    avarage_color[1] /= sample_count;
    avarage_color[2] /= sample_count;

    float lum = 0.299f * avarage_color[0] + 0.587f * avarage_color[1] + 0.114f * avarage_color[2];

    const float alpha = 0.25f;
    reduced_average_ = alpha * lum + (1.0f - alpha) * reduced_average_;

    glBindVertexArray((GLuint)temp_vao_);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    {
        glBindFramebuffer(GL_FRAMEBUFFER, blur_buf2_.fb);
        glViewport(0, 0, blur_buf2_.w, blur_buf2_.h);

        glClear(GL_COLOR_BUFFER_BIT);

        const float fs_quad_pos[] = { -1.0f, -1.0f,       1.0f, -1.0f,
                                       1.0f, 1.0f,         -1.0f, 1.0f };

        const float fs_quad_uvs1[] = { 0.0f, 0.0f,             float(w), 0.0f,
                                       float(w), float(h),     0.0f, float(h) };

        const Ren::Program *cur_program = blit_gauss_prog_.get();
        glUseProgram(cur_program->prog_id());

        glBindBuffer(GL_ARRAY_BUFFER, last_vertex_buffer_);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, last_index_buffer_);

        glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)temp_buf_vtx_offset_, sizeof(fs_quad_pos), fs_quad_pos);
        glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)(temp_buf_vtx_offset_ + sizeof(fs_quad_pos)), sizeof(fs_quad_uvs1), fs_quad_uvs1);
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, (GLintptr)temp_buf_ndx_offset_, sizeof(fs_quad_indices), fs_quad_indices);

        glEnableVertexAttribArray(REN_VTX_POS_LOC);
        glVertexAttribPointer(REN_VTX_POS_LOC, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)uintptr_t(temp_buf_vtx_offset_));

        glEnableVertexAttribArray(REN_VTX_UV1_LOC);
        glVertexAttribPointer(REN_VTX_UV1_LOC, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)uintptr_t(temp_buf_vtx_offset_ + sizeof(fs_quad_pos)));

        glUniform1i(cur_program->uniform(U_TEX).loc, REN_DIFF_TEX_SLOT);
        glUniform1f(cur_program->uniform(4).loc, 0.5f);

        BindTexture(REN_DIFF_TEX_SLOT, temp_tex_);

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, (const GLvoid *)uintptr_t(temp_buf_ndx_offset_));

        glUniform1f(cur_program->uniform(4).loc, 1.5f);

        glBindFramebuffer(GL_FRAMEBUFFER, blur_buf1_.fb);
        glViewport(0, 0, blur_buf1_.w, blur_buf1_.h);

        const float fs_quad_uvs2[] = { 0.0f, float(blur_buf2_.h),     float(blur_buf2_.w), float(blur_buf2_.h),
                                       float(blur_buf2_.w), 0.0f,     0.0f, 0.0f };

        glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)(temp_buf_vtx_offset_ + sizeof(fs_quad_pos)), sizeof(fs_quad_uvs2), fs_quad_uvs2);

        BindTexture(REN_DIFF_TEX_SLOT, blur_buf2_.attachments[0].tex);

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, (const GLvoid *)uintptr_t(temp_buf_ndx_offset_));

        glDisableVertexAttribArray(REN_VTX_POS_LOC);
        glDisableVertexAttribArray(REN_VTX_UV1_LOC);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, scr_w_, scr_h_);

    {   
        const Ren::Program *cur_program = blit_combine_prog_.get();
        glUseProgram(cur_program->prog_id());

        const float fs_quad_uvs[] = { 0.0f, float(h),       float(w), float(h),
                                      float(w), 0.0f,       0.0f, 0.0f };

        glBindBuffer(GL_ARRAY_BUFFER, last_vertex_buffer_);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, last_index_buffer_);

        glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)temp_buf_vtx_offset_, sizeof(fs_quad_positions), fs_quad_positions);
        glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)(temp_buf_vtx_offset_ + sizeof(fs_quad_positions)), sizeof(fs_quad_uvs), fs_quad_uvs);
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, (GLintptr)temp_buf_ndx_offset_, sizeof(fs_quad_indices), fs_quad_indices);

        glEnableVertexAttribArray(REN_VTX_POS_LOC);
        glVertexAttribPointer(REN_VTX_POS_LOC, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)uintptr_t(temp_buf_vtx_offset_));

        glEnableVertexAttribArray(REN_VTX_UV1_LOC);
        glVertexAttribPointer(REN_VTX_UV1_LOC, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)uintptr_t(temp_buf_vtx_offset_ + sizeof(fs_quad_positions)));

        glUniform2f(13, float(w), float(h));
        glUniform1f(U_GAMMA, 2.2f);

        float exposure = 0.85f / reduced_average_;
        exposure = std::min(exposure, 1000.0f);

        glUniform1f(U_EXPOSURE, exposure);

        BindTexture(REN_DIFF_TEX_SLOT, temp_tex_);
        BindTexture(REN_DIFF_TEX_SLOT + 1, blur_buf2_.attachments[0].tex);

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, (const GLvoid *)uintptr_t(temp_buf_ndx_offset_));

        glDisableVertexAttribArray(REN_VTX_POS_LOC);
        glDisableVertexAttribArray(REN_VTX_UV1_LOC);
    }

    glBindVertexArray(0);
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

            glBindBuffer(GL_ARRAY_BUFFER, last_vertex_buffer_);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, last_index_buffer_);

            glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)(temp_buf_vtx_offset_ + sizeof(positions)), sizeof(uvs), uvs);
            glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, (GLintptr)temp_buf_ndx_offset_, sizeof(fs_quad_indices), fs_quad_indices);

            glEnableVertexAttribArray(REN_VTX_POS_LOC);
            glVertexAttribPointer(REN_VTX_POS_LOC, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)uintptr_t(temp_buf_vtx_offset_));

            glEnableVertexAttribArray(REN_VTX_UV1_LOC);
            glVertexAttribPointer(REN_VTX_UV1_LOC, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)uintptr_t(temp_buf_vtx_offset_ + sizeof(positions)));

            glUniform1i(cur_program->uniform(U_TEX).loc, REN_DIFF_TEX_SLOT);
            glUniform1f(cur_program->uniform(4).loc, multiplier);
        }

        glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)temp_buf_vtx_offset_, sizeof(positions), positions);

        if (buf.msaa > 1) {
            BindTextureMs(REN_DIFF_TEX_SLOT, buf.attachments[i].tex);
        } else {
            BindTexture(REN_DIFF_TEX_SLOT, buf.attachments[i].tex);
        }

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, (const GLvoid *)uintptr_t(temp_buf_ndx_offset_));
    }

    glDisableVertexAttribArray(REN_VTX_POS_LOC);
    glDisableVertexAttribArray(REN_VTX_UV1_LOC);
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

        uint8_t indices[] = { 0, 1, 2,    0, 2, 3 };

        if (sy < 0.0f) {
            // keep counter-clockwise winding order
            std::swap(indices[0], indices[2]);
            std::swap(indices[3], indices[5]);
        }

        glBindBuffer(GL_ARRAY_BUFFER, last_vertex_buffer_);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, last_index_buffer_);

        glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)(temp_buf_vtx_offset_ + sizeof(positions)), sizeof(uvs), uvs);
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, (GLintptr)temp_buf_ndx_offset_, sizeof(indices), indices);

        glEnableVertexAttribArray(REN_VTX_POS_LOC);
        glVertexAttribPointer(REN_VTX_POS_LOC, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)uintptr_t(temp_buf_vtx_offset_));

        glEnableVertexAttribArray(REN_VTX_UV1_LOC);
        glVertexAttribPointer(REN_VTX_UV1_LOC, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)uintptr_t(temp_buf_vtx_offset_ + sizeof(positions)));

        glUniform1i(cur_program->uniform(U_TEX).loc, REN_DIFF_TEX_SLOT);
        glUniform1f(cur_program->uniform(4).loc, 1.0f);

        glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)temp_buf_vtx_offset_, sizeof(positions), positions);

        if (is_ms) {
            BindTextureMs(REN_DIFF_TEX_SLOT, tex_id);
        } else {
            BindTexture(REN_DIFF_TEX_SLOT, tex_id);
        }

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, (const GLvoid *)uintptr_t(temp_buf_ndx_offset_));
    }

    glDisableVertexAttribArray(REN_VTX_POS_LOC);
    glDisableVertexAttribArray(REN_VTX_UV1_LOC);
}

#undef _AS_STR
#undef AS_STR