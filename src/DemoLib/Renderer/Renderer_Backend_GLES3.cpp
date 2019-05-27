#include "Renderer.h"

#include <chrono>

#include <Ren/Camera.h>
#include <Ren/Context.h>
#include <Ren/GL.h>
#include <Sys/Log.h>

namespace RendererInternal {
#include "Renderer_GL_Shaders.inl"
#include "__skydome_mesh.inl"
#include "__sphere_mesh.inl"

    struct SharedDataBlock {
        Ren::Mat4f uViewMatrix, uProjMatrix, uViewProjMatrix;
        Ren::Mat4f uInvViewMatrix, uInvProjMatrix, uInvViewProjMatrix, uDeltaMatrix;
        ShadowMapRegion uShadowMapRegions[REN_MAX_SHADOWMAPS_TOTAL];
        Ren::Vec4f uSunDir, uSunCol;
        Ren::Vec4f uClipInfo, uCamPosAndGamma;
        Ren::Vec4f uResAndFRes;
        ProbeItem uProbes[REN_MAX_PROBES_TOTAL];
    };
    static_assert(sizeof(SharedDataBlock) == 5648, "!");

    struct BatchDataBlock {
        Ren::Vec4i uInstanceIndices[REN_MAX_BATCH_SIZE / 4];
    };
    static_assert(sizeof(BatchDataBlock) == 32, "!");

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

    inline void BindTexBuffer(int slot, uint32_t tex) {
        glActiveTexture((GLenum)(GL_TEXTURE0 + slot));
        glBindTexture(GL_TEXTURE_BUFFER, (GLuint)tex);
    }

    const int TEMP_BUF_SIZE = 256;

    const float fs_quad_positions[] = { -1.0f, -1.0f,   1.0f, -1.0f,
                                         1.0f, 1.0f,   -1.0f, 1.0f };

    const float fs_quad_norm_uvs[] = { 0.0f, 0.0f,     1.0f, 0.0f,
                                       1.0f, 1.0f,     0.0f, 1.0f };

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
    blit_combine_ms_prog_ = ctx_.LoadProgramGLSL("blit_combine_ms", blit_vs, blit_combine_ms_fs, &status);
    assert(status == Ren::ProgCreatedFromData);
    LOGI("Compiling blit_ms");
    blit_ms_prog_ = ctx_.LoadProgramGLSL("blit_ms", blit_vs, blit_ms_fs, &status);
    assert(status == Ren::ProgCreatedFromData);
    LOGI("Compiling blit_red");
    blit_red_prog_ = ctx_.LoadProgramGLSL("blit_red", blit_vs, blit_reduced_fs, &status);
    assert(status == Ren::ProgCreatedFromData);
    LOGI("Compiling blit_down");
    blit_down_prog_ = ctx_.LoadProgramGLSL("blit_down", blit_vs, blit_down_fs, &status);
    assert(status == Ren::ProgCreatedFromData);
    LOGI("Compiling blit_down_ms");
    blit_down_ms_prog_ = ctx_.LoadProgramGLSL("blit_down_ms", blit_vs, blit_down_ms_fs, &status);
    assert(status == Ren::ProgCreatedFromData);
    LOGI("Compiling blit_gauss");
    blit_gauss_prog_ = ctx_.LoadProgramGLSL("blit_gauss", blit_vs, blit_gauss_fs, &status);
    assert(status == Ren::ProgCreatedFromData);
    LOGI("Compiling blit_gauss_sep");
    blit_gauss_sep_prog_ = ctx_.LoadProgramGLSL("blit_gauss_sep", blit_vs, blit_gauss_sep_fs, &status);
    assert(status == Ren::ProgCreatedFromData);
    LOGI("Compiling blit_debug");
    blit_debug_prog_ = ctx_.LoadProgramGLSL("blit_debug", blit_vs, blit_debug_fs, &status);
    assert(status == Ren::ProgCreatedFromData);
    LOGI("Compiling blit_debug_ms");
    blit_debug_ms_prog_ = ctx_.LoadProgramGLSL("blit_debug_ms", blit_vs, blit_debug_ms_fs, &status);
    assert(status == Ren::ProgCreatedFromData);
    LOGI("Compiling blit_ssr");
    blit_ssr_prog_ = ctx_.LoadProgramGLSL("blit_ssr", blit_vs, blit_ssr_fs, &status);
    assert(status == Ren::ProgCreatedFromData);
    LOGI("Compiling blit_ssr_ms");
    blit_ssr_ms_prog_ = ctx_.LoadProgramGLSL("blit_ssr_ms", blit_vs, blit_ssr_ms_fs, &status);
    assert(status == Ren::ProgCreatedFromData);
    LOGI("Compiling blit_ao");
    blit_ao_prog_ = ctx_.LoadProgramGLSL("blit_ao", blit_vs, blit_ssao_fs, &status);
    assert(status == Ren::ProgCreatedFromData);
    LOGI("Compiling blit_ao_ms");
    blit_ao_ms_prog_ = ctx_.LoadProgramGLSL("blit_ao_ms", blit_vs, blit_ssao_ms_fs, &status);
    assert(status == Ren::ProgCreatedFromData);
    LOGI("Compiling blit_multiply");
    blit_multiply_prog_ = ctx_.LoadProgramGLSL("blit_multiply", blit_vs, blit_multiply_fs, &status);
    assert(status == Ren::ProgCreatedFromData);
    LOGI("Compiling blit_multiply_ms");
    blit_multiply_ms_prog_ = ctx_.LoadProgramGLSL("blit_multiply_ms", blit_vs, blit_multiply_ms_fs, &status);
    assert(status == Ren::ProgCreatedFromData);
    LOGI("Compiling blit_debug_bvh");
    blit_debug_bvh_prog_ = ctx_.LoadProgramGLSL("blit_debug_bvh", blit_vs, blit_debug_bvh_fs, &status);
    assert(status == Ren::ProgCreatedFromData);
    LOGI("Compiling blit_debug_bvh_ms");
    blit_debug_bvh_ms_prog_ = ctx_.LoadProgramGLSL("blit_debug_bvh_ms", blit_vs, blit_debug_bvh_ms_fs, &status);
    assert(status == Ren::ProgCreatedFromData);
    LOGI("Compiling blit_depth");
    blit_depth_prog_ = ctx_.LoadProgramGLSL("blit_depth", blit_vs, blit_depth_fs, &status);
    assert(status == Ren::ProgCreatedFromData);
    LOGI("Compiling blit_rgbm");
    blit_rgbm_prog_ = ctx_.LoadProgramGLSL("blit_rgbm", blit_vs, blit_rgbm_fs, &status);
    assert(status == Ren::ProgCreatedFromData);
    LOGI("Compiling blit_mipmap");
    blit_mipmap_prog_ = ctx_.LoadProgramGLSL("blit_mipmap", blit_vs, blit_mipmap_fs, &status);
    assert(status == Ren::ProgCreatedFromData);
    LOGI("Compiling blit_project_sh_prog");
    blit_project_sh_prog_ = ctx_.LoadProgramGLSL("blit_project_sh_prog", blit_vs, blit_project_sh_fs, &status);
    assert(status == Ren::ProgCreatedFromData);
    LOGI("Compiling blit_fxaa_prog");
    blit_fxaa_prog_ = ctx_.LoadProgramGLSL("blit_fxaa_prog", blit_vs, blit_fxaa_fs, &status);
    assert(status == Ren::ProgCreatedFromData);
    LOGI("Compiling probe_prog");
    probe_prog_ = ctx_.LoadProgramGLSL("probe_prog", probe_vs, probe_fs, &status);
    assert(status == Ren::ProgCreatedFromData);
    LOGI("Compiling skinning_prog");
    //skinning_prog_ = ctx_.LoadProgramGLSL("skinning_prog", skinning_cs, &status);
    //assert(status == Ren::ProgCreatedFromData);
    
    {
        GLuint shared_data_ubo;

        glGenBuffers(1, &shared_data_ubo);
        glBindBuffer(GL_UNIFORM_BUFFER, shared_data_ubo);
        glBufferData(GL_UNIFORM_BUFFER, sizeof(SharedDataBlock), NULL, GL_STATIC_DRAW);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);

        unif_shared_data_block_ = (uint32_t)shared_data_ubo;

        GLuint batch_data_ubo;

        glGenBuffers(1, &batch_data_ubo);
        glBindBuffer(GL_UNIFORM_BUFFER, batch_data_ubo);
        glBufferData(GL_UNIFORM_BUFFER, sizeof(BatchDataBlock), NULL, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);

        unif_batch_data_block_ = (uint32_t)batch_data_ubo;
    }

    Ren::CheckError("[InitRendererInternal]: UBO creation");

    {
        GLuint instances_buf;

        glGenBuffers(1, &instances_buf);
        glBindBuffer(GL_TEXTURE_BUFFER, instances_buf);
        glBufferData(GL_TEXTURE_BUFFER, sizeof(InstanceData) * REN_MAX_INSTANCES_TOTAL, nullptr, GL_DYNAMIC_COPY);
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
        CellData dummy[REN_GRID_RES_X * REN_GRID_RES_Y] = {};
        for (int i = 0; i < REN_GRID_RES_Z; i++) {
            glBufferSubData(GL_TEXTURE_BUFFER, i * sizeof(CellData) * REN_GRID_RES_X * REN_GRID_RES_Y,
                            sizeof(CellData) * REN_GRID_RES_X * REN_GRID_RES_Y, &dummy[0]);
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
        GLuint probe_sample_pbo;
        glGenBuffers(1, &probe_sample_pbo);
        glBindBuffer(GL_PIXEL_PACK_BUFFER, probe_sample_pbo);
        glBufferData(GL_PIXEL_PACK_BUFFER, 4 * probe_sample_buf_.w * probe_sample_buf_.h * sizeof(float), 0, GL_DYNAMIC_READ);

        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        probe_sample_pbo_ = (uint32_t)probe_sample_pbo;
    }

    Ren::CheckError("[InitRendererInternal]: probe sample PBO");

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

    {
        GLuint temp_framebuf;
        glGenFramebuffers(1, &temp_framebuf);

        temp_framebuf_ = (uint32_t)temp_framebuf;
    }

    Ren::CheckError("[InitRendererInternal]: temp framebuffer");

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

bool Renderer::InitFramebuffersInternal() {
    if (!skydome_framebuf_) {
        GLuint new_framebuf;
        glGenFramebuffers(1, &new_framebuf);
        skydome_framebuf_ = (uint32_t)new_framebuf;
    }

    if (!depth_fill_framebuf_) {
        GLuint new_framebuf;
        glGenFramebuffers(1, &new_framebuf);
        depth_fill_framebuf_ = (uint32_t)new_framebuf;
    }

    if (!refl_comb_framebuf_) {
        GLuint new_framebuf;
        glGenFramebuffers(1, &new_framebuf);
        refl_comb_framebuf_ = (uint32_t)new_framebuf;
    }

    GLint framebuf_before;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuf_before);

    bool result = true;

    {   // Attach textures from clean framebuffer to skydome framebuffer
        glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)skydome_framebuf_);

        GLuint col_tex = (GLuint)clean_buf_.attachments[0].tex;
        if (clean_buf_.sample_count > 1) {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, col_tex, 0);
        } else {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, col_tex, 0);
        }

        GLuint spec_tex = (GLuint)clean_buf_.attachments[2].tex;
        if (clean_buf_.sample_count > 1) {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D_MULTISAMPLE, spec_tex, 0);
        } else {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, spec_tex, 0);
        }

        GLuint depth_tex = (GLuint)clean_buf_.depth_tex.GetValue();
        if (clean_buf_.sample_count > 1) {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D_MULTISAMPLE, depth_tex, 0);
        } else {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth_tex, 0);
        }

        GLenum bufs[] = { GL_COLOR_ATTACHMENT0, GL_NONE, GL_COLOR_ATTACHMENT2 };
        glDrawBuffers(3, bufs);

        auto s = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        result = (s == GL_FRAMEBUFFER_COMPLETE);
    }

    {   // Attach textures from clean framebuffer to depth-fill framebuffer
        glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)depth_fill_framebuf_);

        GLuint depth_tex = (GLuint)clean_buf_.depth_tex.GetValue();
        if (clean_buf_.sample_count > 1) {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D_MULTISAMPLE, depth_tex, 0);
        } else {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth_tex, 0);
        }

        GLenum bufs[] = { GL_NONE };
        glDrawBuffers(1, bufs);

        auto s = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        result = (s == GL_FRAMEBUFFER_COMPLETE);
    }

    {   // Attach textures from clean framebuffer to refl comb framebuffer
        glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)refl_comb_framebuf_);

        GLuint col_tex = (GLuint)clean_buf_.attachments[0].tex;
        if (clean_buf_.sample_count > 1) {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, col_tex, 0);
        } else {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, col_tex, 0);
        }

        GLenum bufs[] = { GL_COLOR_ATTACHMENT0 };
        glDrawBuffers(1, bufs);

        auto s = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        result = (s == GL_FRAMEBUFFER_COMPLETE);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)framebuf_before);
    return result;
}

void Renderer::CheckInitVAOs() {
    using namespace RendererInternal;

    auto vtx_buf = ctx_.default_vertex_buf();
    auto ndx_buf = ctx_.default_indices_buf();

    GLuint gl_vertex_buf = (GLuint)vtx_buf->buf_id(),
           gl_indices_buf = (GLuint)ndx_buf->buf_id();

    if (gl_vertex_buf != last_vertex_buffer_ || gl_indices_buf != last_index_buffer_) {
        GLuint shadow_pass_vao;
        glGenVertexArrays(1, &shadow_pass_vao);
        glBindVertexArray(shadow_pass_vao);

        glBindBuffer(GL_ARRAY_BUFFER, gl_vertex_buf);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gl_indices_buf);

        int stride = 32;// 13 * sizeof(float);
        glEnableVertexAttribArray(REN_VTX_POS_LOC);
        glVertexAttribPointer(REN_VTX_POS_LOC, 3, GL_FLOAT, GL_FALSE, stride, (void *)0);

        glEnableVertexAttribArray(REN_VTX_UV1_LOC);
        glVertexAttribPointer(REN_VTX_UV1_LOC, 2, GL_HALF_FLOAT, GL_FALSE, stride, (void *)(3 * sizeof(float) + 6 * sizeof(uint16_t) /*9 * sizeof(float)*/));

        glBindVertexArray(0);

        shadow_pass_vao_ = (uint32_t)shadow_pass_vao;

        GLuint depth_pass_vao;
        glGenVertexArrays(1, &depth_pass_vao);
        glBindVertexArray(depth_pass_vao);

        glBindBuffer(GL_ARRAY_BUFFER, gl_vertex_buf);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gl_indices_buf);

        glEnableVertexAttribArray(REN_VTX_POS_LOC);
        glVertexAttribPointer(REN_VTX_POS_LOC, 3, GL_FLOAT, GL_FALSE, stride, (void *)0);

        glEnableVertexAttribArray(REN_VTX_UV1_LOC);
        glVertexAttribPointer(REN_VTX_UV1_LOC, 2, GL_HALF_FLOAT, GL_FALSE, stride, (void *)(3 * sizeof(float) + 6 * sizeof(uint16_t) /*9 * sizeof(float)*/));

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
        glVertexAttribPointer(REN_VTX_NOR_LOC, 4, GL_SHORT, GL_TRUE, stride, (void *)(3 * sizeof(float)));

        glEnableVertexAttribArray(REN_VTX_TAN_LOC);
        glVertexAttribPointer(REN_VTX_TAN_LOC, 2, GL_SHORT, GL_TRUE, stride, (void *)(3 * sizeof(float) + 4 * sizeof(uint16_t) /*6 * sizeof(float)*/));

        glEnableVertexAttribArray(REN_VTX_UV1_LOC);
        glVertexAttribPointer(REN_VTX_UV1_LOC, 2, GL_HALF_FLOAT, GL_FALSE, stride, (void *)(3 * sizeof(float) + 6 * sizeof(uint16_t) /*9 * sizeof(float)*/));

        glEnableVertexAttribArray(REN_VTX_UV2_LOC);
        glVertexAttribPointer(REN_VTX_UV2_LOC, 2, GL_HALF_FLOAT, GL_FALSE, stride, (void *)(3 * sizeof(float) + 8 * sizeof(uint16_t) /*11 * sizeof(float)*/));

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

        {   // Allocate sphere vertices
            sphere_vtx_offset_ = vtx_buf->Alloc(sizeof(__sphere_positions), __sphere_positions);
            sphere_ndx_offset_ = ndx_buf->Alloc(sizeof(__sphere_indices), __sphere_indices);

            GLuint sphere_vao;
            glGenVertexArrays(1, &sphere_vao);
            glBindVertexArray(sphere_vao);

            glBindBuffer(GL_ARRAY_BUFFER, gl_vertex_buf);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gl_indices_buf);

            glEnableVertexAttribArray(REN_VTX_POS_LOC);
            glVertexAttribPointer(REN_VTX_POS_LOC, 3, GL_FLOAT, GL_FALSE, 0, (void *)uintptr_t(sphere_vtx_offset_));

            glBindVertexArray(0);
            sphere_vao_ = (uint32_t)sphere_vao;
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

        GLuint batch_data_ubo = (GLuint)unif_batch_data_block_;
        glDeleteBuffers(1, &batch_data_ubo);
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

    {
        GLuint temp_tex = (GLuint)temp_tex_;
        glDeleteTextures(1, &temp_tex);
    }

    {
        GLuint temp_framebuf = (GLuint)temp_framebuf_;
        glDeleteFramebuffers(1, &temp_framebuf);
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
        GLuint probe_sample_pbo = (GLuint)probe_sample_pbo_;
        glDeleteBuffers(1, &probe_sample_pbo);
    }

    if (skydome_framebuf_) {
        GLuint framebuf = (GLuint)skydome_framebuf_;
        glDeleteFramebuffers(1, &framebuf);
    }

    if (depth_fill_framebuf_) {
        GLuint framebuf = (GLuint)depth_fill_framebuf_;
        glDeleteFramebuffers(1, &framebuf);
    }

    if (refl_comb_framebuf_) {
        GLuint framebuf = (GLuint)refl_comb_framebuf_;
        glDeleteFramebuffers(1, &framebuf);
    }

    {
        vtx_buf->Free(sphere_vtx_offset_);
        ndx_buf->Free(sphere_ndx_offset_);

        vtx_buf->Free(skydome_vtx_offset_);
        ndx_buf->Free(skydome_ndx_offset_);

        vtx_buf->Free(temp_buf_vtx_offset_);
        ndx_buf->Free(temp_buf_ndx_offset_);

        GLuint sphere_vao = (GLuint)sphere_vao_;
        glDeleteVertexArrays(1, &sphere_vao);

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

void Renderer::DrawObjectsInternal(const DrawList &list, const FrameBuf *target) {
    using namespace Ren;
    using namespace RendererInternal;

    if (list.render_flags & EnableTimers) {
        glQueryCounter(queries_[1][TimeDrawStart], GL_TIMESTAMP);
    }

    CheckInitVAOs();

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);

    glDisable(GL_CULL_FACE);

    assert(list.light_sources.size() < MAX_LIGHTS_TOTAL);
    assert(list.decals.size() < MAX_DECALS_TOTAL);
    assert(list.items_count < MAX_ITEMS_TOTAL);

    backend_info_.shadow_draw_calls_count = 0;
    backend_info_.depth_fill_draw_calls_count = 0;
    backend_info_.opaque_draw_calls_count = 0;

    {   // Update buffers

        // TODO: try to use persistently mapped buffers

#ifndef DISABLE_MARKERS
        glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "UPDATE BUFFERS");
#endif

        // Update instance buffer
        size_t instance_mem_size = list.instances.size() * sizeof(InstanceData);
        if (instance_mem_size) {
            glBindBuffer(GL_TEXTURE_BUFFER, (GLuint)instances_buf_);
            glBufferSubData(GL_TEXTURE_BUFFER, 0, instance_mem_size, list.instances.data());
            glBindBuffer(GL_TEXTURE_BUFFER, 0);
        }

        // Update lights buffer
        size_t lights_mem_size = list.light_sources.size() * sizeof(LightSourceItem);
        if (lights_mem_size) {
            glBindBuffer(GL_TEXTURE_BUFFER, (GLuint)lights_buf_);
            glBufferSubData(GL_TEXTURE_BUFFER, 0, lights_mem_size, list.light_sources.data());
            glBindBuffer(GL_TEXTURE_BUFFER, 0);
        }

        // Update decals buffer
        size_t decals_mem_size = list.decals.size() * sizeof(DecalItem);
        if (decals_mem_size) {
            glBindBuffer(GL_TEXTURE_BUFFER, (GLuint)decals_buf_);
            glBufferSubData(GL_TEXTURE_BUFFER, 0, decals_mem_size, list.decals.data());
            glBindBuffer(GL_TEXTURE_BUFFER, 0);
        }

        // Update cells buffer
        size_t cells_mem_size = list.cells.size() * sizeof(CellData);
        if (cells_mem_size) {
            glBindBuffer(GL_TEXTURE_BUFFER, (GLuint)cells_buf_);
            glBufferSubData(GL_TEXTURE_BUFFER, 0, cells_mem_size, list.cells.data());
            glBindBuffer(GL_TEXTURE_BUFFER, 0);
        }

        // Update items buffer
        size_t items_mem_size = list.items_count * sizeof(ItemData);
        if (items_mem_size) {
            glBindBuffer(GL_TEXTURE_BUFFER, (GLuint)items_buf_);
            glBufferSubData(GL_TEXTURE_BUFFER, 0, items_mem_size, list.items.data());
            glBindBuffer(GL_TEXTURE_BUFFER, 0);
        }

#ifndef DISABLE_MARKERS
        glPopDebugGroup();
#endif
    }

    SharedDataBlock shrd_data;

    {   // Prepare data that is shared for all instances
        shrd_data.uViewMatrix = list.draw_cam.view_matrix();
        shrd_data.uProjMatrix = list.draw_cam.proj_matrix();
        shrd_data.uViewProjMatrix = shrd_data.uProjMatrix * shrd_data.uViewMatrix;
        shrd_data.uInvViewMatrix = Ren::Inverse(shrd_data.uViewMatrix);
        shrd_data.uInvProjMatrix = Ren::Inverse(shrd_data.uProjMatrix);
        shrd_data.uInvViewProjMatrix = Ren::Inverse(shrd_data.uViewProjMatrix);
        // delta matrix between current and previous frame
        shrd_data.uDeltaMatrix = down_buf_view_from_world_ * shrd_data.uInvViewMatrix;

        if (!list.shadow_regions.empty()) {
            assert(list.shadow_regions.size() <= REN_MAX_SHADOWMAPS_TOTAL);
            memcpy(&shrd_data.uShadowMapRegions[0], &list.shadow_regions[0], sizeof(ShadowMapRegion) * list.shadow_regions.size());
        }

        if (list.render_flags & EnableLights) {
            shrd_data.uSunDir = Ren::Vec4f{ list.env.sun_dir[0], list.env.sun_dir[1], list.env.sun_dir[2], 0.0f };
            shrd_data.uSunCol = Ren::Vec4f{ list.env.sun_col[0], list.env.sun_col[1], list.env.sun_col[2], 0.0f };
        }

        shrd_data.uResAndFRes = Ren::Vec4f{ float(act_w_), float(act_h_), float(clean_buf_.w), float(clean_buf_.h) };

        const float near = list.draw_cam.near(), far = list.draw_cam.far();
        shrd_data.uClipInfo = { near * far, near, far, std::log2(1.0f + far / near) };

        const auto &pos = list.draw_cam.world_position();
        shrd_data.uCamPosAndGamma = Ren::Vec4f{ pos[0], pos[1], pos[2], 2.2f };

        if (!list.probes.empty()) {
            assert(list.probes.size() <= REN_MAX_PROBES_TOTAL);
            memcpy(&shrd_data.uProbes[0], &list.probes[0], sizeof(ProbeItem) * list.probes.size());
        }

        glBindBuffer(GL_UNIFORM_BUFFER, (GLuint)unif_shared_data_block_);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(SharedDataBlock), &shrd_data);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }

    glBindBufferBase(GL_UNIFORM_BUFFER, REN_UB_SHARED_DATA_LOC, (GLuint)unif_shared_data_block_);
    glBindBufferBase(GL_UNIFORM_BUFFER, REN_UB_BATCH_DATA_LOC, (GLuint)unif_batch_data_block_);

    GLint framebuf_before;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuf_before);

    GLint viewport_before[4];
    glGetIntegerv(GL_VIEWPORT, viewport_before);

    /**************************************************************************************************/
    /*                                           SHADOW MAPS                                          */
    /**************************************************************************************************/

    if (list.render_flags & EnableTimers) {
        glQueryCounter(queries_[1][TimeShadowMapStart], GL_TIMESTAMP);
    }

    {   // draw shadow map
        glEnable(GL_POLYGON_OFFSET_FILL);

        glBindVertexArray(shadow_pass_vao_);

        BindTexBuffer(0, instances_tbo_);

        glBindFramebuffer(GL_FRAMEBUFFER, shadow_buf_.fb);

        glUseProgram(shadow_prog_->prog_id());

        glPolygonOffset(1.85f, 6.0f);
        glEnable(GL_SCISSOR_TEST);

#ifndef DISABLE_MARKERS
        glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "UPDATE SHADOW MAPS");
#endif
        uint32_t cur_alpha_test = 0;
        BindTexture(REN_DIFF_TEX_SLOT, dummy_white_->tex_id());

        for (int i = 0; i < (int)list.shadow_lists.size(); i++) {
            const auto &shadow_list = list.shadow_lists[i];
            if (!shadow_list.shadow_batch_count) continue;

            glViewport(shadow_list.shadow_map_pos[0], shadow_list.shadow_map_pos[1],
                       shadow_list.shadow_map_size[0], shadow_list.shadow_map_size[1]);

            {   // clear buffer region
                glScissor(shadow_list.shadow_map_pos[0], shadow_list.shadow_map_pos[1],
                          shadow_list.shadow_map_size[0], shadow_list.shadow_map_size[1]);
                glClear(GL_DEPTH_BUFFER_BIT);
            }

            glUniformMatrix4fv(REN_U_M_MATRIX_LOC, 1, GL_FALSE, Ren::ValuePtr(list.shadow_regions[i].clip_from_world));

            for (uint32_t j = shadow_list.shadow_batch_start; j < shadow_list.shadow_batch_start + shadow_list.shadow_batch_count; j++) {
                const auto &batch = list.shadow_batches[list.shadow_batch_indices[j]];
                if (!batch.instance_count) continue;

                if (cur_alpha_test != batch.alpha_test_bit) {
                    if (batch.alpha_test_bit) {
                        const Ren::Material *mat = ctx_.GetMaterial(batch.mat_id).get();
                        BindTexture(REN_DIFF_TEX_SLOT, mat->texture(0)->tex_id());
                    } else {
                        BindTexture(REN_DIFF_TEX_SLOT, dummy_white_->tex_id());
                    }

                    cur_alpha_test = batch.alpha_test_bit;
                }

                glUniform1iv(REN_U_INSTANCES_LOC, batch.instance_count, &batch.instance_indices[0]);

                glDrawElementsInstanced(GL_TRIANGLES, batch.indices_count, GL_UNSIGNED_INT, (const GLvoid *)uintptr_t(batch.indices_offset),
                                        (GLsizei)batch.instance_count);
                backend_info_.shadow_draw_calls_count++;
            }
        }

#ifndef DISABLE_MARKERS
        glPopDebugGroup();
#endif

        glDisable(GL_SCISSOR_TEST);
        glPolygonOffset(0.0f, 0.0f);
        glDisable(GL_POLYGON_OFFSET_FILL);

        glBindVertexArray(0);
    }

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glDisable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Setup viewport
    glViewport(0, 0, act_w_, act_h_);

    // Can draw skydome without multisampling (not sure if it helps)
    glDisable(GL_MULTISAMPLE);

    /**************************************************************************************************/
    /*                                   SKYDOME DRAW / DEPTH CLEAR                                   */
    /**************************************************************************************************/

    if ((list.render_flags & DebugWireframe) == 0 && list.env.env_map) {
        glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)skydome_framebuf_);

        // Draw skydome (and clear depth with it)
        glDepthFunc(GL_ALWAYS);

        glUseProgram(skydome_prog_->prog_id());

        glBindVertexArray(skydome_vao_);

        Ren::Mat4f translate_matrix;
        translate_matrix = Ren::Translate(translate_matrix, Ren::Vec3f{ shrd_data.uCamPosAndGamma });

        Ren::Mat4f scale_matrix;
        scale_matrix = Ren::Scale(scale_matrix, Ren::Vec3f{ 5000.0f, 5000.0f, 5000.0f });

        Ren::Mat4f world_from_object = translate_matrix * scale_matrix;
        glUniformMatrix4fv(REN_U_M_MATRIX_LOC, 1, GL_FALSE, Ren::ValuePtr(world_from_object));

        BindCubemap(REN_DIFF_TEX_SLOT, list.env.env_map->tex_id());

#ifndef DISABLE_MARKERS
        glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "DRAW SKYBOX");
#endif

        glDrawElements(GL_TRIANGLES, (GLsizei)__skydome_indices_count, GL_UNSIGNED_BYTE, (void *)uintptr_t(skydome_ndx_offset_));

#ifndef DISABLE_MARKERS
        glPopDebugGroup();
#endif

        glDepthFunc(GL_LESS);
    } else {
        glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
    }

    glEnable(GL_MULTISAMPLE);

    /**************************************************************************************************/
    /*                                         BIND RESOURCES                                         */
    /**************************************************************************************************/

    BindTexture(REN_SHAD_TEX_SLOT, shadow_buf_.depth_tex.GetValue());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

    if (list.decals_atlas) {
        BindTexture(REN_DECAL_TEX_SLOT, list.decals_atlas->tex_id(0));
    }

    if (list.render_flags & EnableSSAO) {
        BindTexture(REN_SSAO_TEX_SLOT, ssao_buf_.attachments[0].tex);
    } else {
        BindTexture(REN_SSAO_TEX_SLOT, dummy_white_->tex_id());
    }

    if ((list.render_flags & EnableLightmap) && list.env.lm_direct) {
        for (int sh_l = 0; sh_l < 4; sh_l++) {
            BindTexture(REN_LMAP_SH_SLOT + sh_l, list.env.lm_indir_sh[sh_l]->tex_id());
        }
    } else {
        for (int sh_l = 0; sh_l < 4; sh_l++) {
            BindTexture(REN_LMAP_SH_SLOT + sh_l, dummy_black_->tex_id());
        }
    }

    if (list.probe_storage) {
        glActiveTexture(GL_TEXTURE0 + REN_ENV_TEX_SLOT);
        glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, list.probe_storage->tex_id());
    }

    BindTexBuffer(REN_LIGHT_BUF_SLOT, lights_tbo_);
    BindTexBuffer(REN_DECAL_BUF_SLOT, decals_tbo_);
    BindTexBuffer(REN_CELLS_BUF_SLOT, cells_tbo_);
    BindTexBuffer(REN_ITEMS_BUF_SLOT, items_tbo_);

    BindTexBuffer(REN_INST_BUF_SLOT, instances_tbo_);

    /**************************************************************************************************/
    /*                                         DEPTH-FILL PASS                                        */
    /**************************************************************************************************/

    if (list.render_flags & EnableTimers) {
        glQueryCounter(queries_[1][TimeDepthOpaqueStart], GL_TIMESTAMP);
    }

    if ((list.render_flags & EnableZFill) && ((list.render_flags & DebugWireframe) == 0)) {
        // Write depth only
        glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)depth_fill_framebuf_);

        glDepthFunc(GL_LESS);

        glBindVertexArray(depth_pass_vao_);

        glUseProgram(fill_depth_prog_->prog_id());

        uint32_t cur_alpha_test = 0;
        BindTexture(REN_DIFF_TEX_SLOT, dummy_white_->tex_id());

#ifndef DISABLE_MARKERS
        glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "DEPTH-FILL");
#endif

        // fill depth
        for (uint32_t i : list.main_batch_indices) {
            const auto &batch = list.main_batches[i];
            if (!batch.instance_count) continue;
            if (batch.alpha_blend_bit) break;

            if (cur_alpha_test != batch.alpha_test_bit) {
                if (batch.alpha_test_bit) {
                    const Ren::Material *mat = ctx_.GetMaterial(batch.mat_id).get();
                    BindTexture(REN_DIFF_TEX_SLOT, mat->texture(0)->tex_id());
                } else {
                    BindTexture(REN_DIFF_TEX_SLOT, dummy_white_->tex_id());
                }

                cur_alpha_test = batch.alpha_test_bit;
            }

            glUniform1iv(REN_U_INSTANCES_LOC, batch.instance_count, &batch.instance_indices[0]);

            glDrawElementsInstanced(GL_TRIANGLES, batch.indices_count, GL_UNSIGNED_INT, (const GLvoid *)uintptr_t(batch.indices_offset),
                                    (GLsizei)batch.instance_count);
            backend_info_.depth_fill_draw_calls_count++;
        }

#ifndef DISABLE_MARKERS
        glPopDebugGroup();
#endif

        glBindVertexArray(0);

        glDepthFunc(GL_EQUAL);
    }

    /**************************************************************************************************/
    /*                                            SSAO PASS                                           */
    /**************************************************************************************************/

    if (list.render_flags & EnableTimers) {
        glQueryCounter(queries_[1][TimeAOPassStart], GL_TIMESTAMP);
    }

    glBindVertexArray((GLuint)temp_vao_);

    const uint32_t use_ssao = (EnableZFill | EnableSSAO);
    if ((list.render_flags & use_ssao) == use_ssao) {
        // prepare ao buffer
        glBindFramebuffer(GL_FRAMEBUFFER, ssao_buf_.fb);
        glViewport(0, 0, ssao_buf_.w, ssao_buf_.h);

        const Ren::Program *ssao_prog = nullptr;

        if (clean_buf_.sample_count > 1) {
            ssao_prog = blit_ao_ms_prog_.get();
        } else {
            ssao_prog = blit_ao_prog_.get();
        }

        glUseProgram(ssao_prog->prog_id());

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

        if (clean_buf_.sample_count > 1) {
            BindTextureMs(REN_DIFF_TEX_SLOT, clean_buf_.depth_tex.GetValue());
        } else {
            BindTexture(REN_DIFF_TEX_SLOT, clean_buf_.depth_tex.GetValue());
        }

#ifndef DISABLE_MARKERS
        glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "SSAO PASS");
#endif

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, (const GLvoid *)uintptr_t(temp_buf_ndx_offset_));

#ifndef DISABLE_MARKERS
        glPopDebugGroup();
#endif

        glDisableVertexAttribArray(REN_VTX_POS_LOC);
        glDisableVertexAttribArray(REN_VTX_UV1_LOC);

        glBindFramebuffer(GL_FRAMEBUFFER, clean_buf_.fb);
        glViewport(0, 0, act_w_, act_h_);
    }

#if !defined(__ANDROID__)
    if (list.render_flags & DebugWireframe) {
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

    // Bind main buffer for drawing
    glBindFramebuffer(GL_FRAMEBUFFER, clean_buf_.fb);

    if (list.render_flags & EnableTimers) {
        glQueryCounter(queries_[1][TimeOpaqueStart], GL_TIMESTAMP);
    }

    glBindVertexArray((GLuint)draw_pass_vao_);

    {   // actual drawing
#ifndef DISABLE_MARKERS
        glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "OPAQUE PASS");
#endif

        const Ren::Program *cur_program = nullptr;
        const Ren::Material *cur_mat = nullptr;

        for (uint32_t i : list.main_batch_indices) {
            const auto &batch = list.main_batches[i];
            if (!batch.instance_count) continue;
            if (batch.alpha_blend_bit) break;

            const Ren::Program *p = ctx_.GetProgram(batch.prog_id).get();
            const Ren::Material *mat = ctx_.GetMaterial(batch.mat_id).get();

            if (cur_program != p) {
                glUseProgram(p->prog_id());
                cur_program = p;
            }

            if (cur_mat != mat) {
                BindTexture(REN_DIFF_TEX_SLOT, mat->texture(0)->tex_id());
                BindTexture(REN_NORM_TEX_SLOT, mat->texture(1)->tex_id());
                BindTexture(REN_SPEC_TEX_SLOT, mat->texture(2)->tex_id());
                cur_mat = mat;
            }

            glBindBuffer(GL_UNIFORM_BUFFER, (GLuint)unif_batch_data_block_);
            glBufferSubData(GL_UNIFORM_BUFFER, offsetof(BatchDataBlock, uInstanceIndices[0]), batch.instance_count * sizeof(int), &batch.instance_indices[0]);

            glDrawElementsInstanced(GL_TRIANGLES, batch.indices_count, GL_UNSIGNED_INT, (const GLvoid *)uintptr_t(batch.indices_offset),
                                    (GLsizei)batch.instance_count);
            backend_info_.opaque_draw_calls_count++;
        }

#ifndef DISABLE_MARKERS
        glPopDebugGroup();
#endif
    }

    /**************************************************************************************************/
    /*                                      TRANSPARENT PASS                                          */
    /**************************************************************************************************/

    if (list.render_flags & EnableTimers) {
        glQueryCounter(queries_[1][TimeTranspStart], GL_TIMESTAMP);
    }

    glBindVertexArray((GLuint)draw_pass_vao_);

    glEnable(GL_BLEND);

    {   // actual drawing
#ifndef DISABLE_MARKERS
        glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "TRANSPARENT PASS");
#endif

        const Ren::Program *cur_program = nullptr;
        const Ren::Material *cur_mat = nullptr;

        for (int j = (int)list.main_batch_indices.size() - 1; j >= 0; j--) {
            const auto &batch = list.main_batches[list.main_batch_indices[j]];
            if (!batch.instance_count) continue;
            if (!batch.alpha_blend_bit) break;

            const Ren::Program *p = ctx_.GetProgram(batch.prog_id).get();
            const Ren::Material *mat = ctx_.GetMaterial(batch.mat_id).get();

            if (cur_program != p) {
                glUseProgram(p->prog_id());
                cur_program = p;
            }

            if (cur_mat != mat) {
                BindTexture(REN_DIFF_TEX_SLOT, mat->texture(0)->tex_id());
                BindTexture(REN_NORM_TEX_SLOT, mat->texture(1)->tex_id());
                BindTexture(REN_SPEC_TEX_SLOT, mat->texture(2)->tex_id());
                cur_mat = mat;
            }

            glBindBuffer(GL_UNIFORM_BUFFER, (GLuint)unif_batch_data_block_);
            glBufferSubData(GL_UNIFORM_BUFFER, offsetof(BatchDataBlock, uInstanceIndices[0]), batch.instance_count * sizeof(int), &batch.instance_indices[0]);

            glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
            glDepthFunc(GL_LEQUAL);

            glDrawElementsInstanced(GL_TRIANGLES, batch.indices_count, GL_UNSIGNED_INT, (const GLvoid *)uintptr_t(batch.indices_offset),
                                    (GLsizei)batch.instance_count);

            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            glDepthFunc(GL_EQUAL);

            glDrawElementsInstanced(GL_TRIANGLES, batch.indices_count, GL_UNSIGNED_INT, (const GLvoid *)uintptr_t(batch.indices_offset),
                                    (GLsizei)batch.instance_count);

            backend_info_.opaque_draw_calls_count++;
        }

#ifndef DISABLE_MARKERS
        glPopDebugGroup();
#endif
    }

#if !defined(__ANDROID__)
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
#endif

    glBindVertexArray((GLuint)temp_vao_);
    glDisable(GL_BLEND);
    glDepthFunc(GL_LESS);

    /**************************************************************************************************/
    /*                                             SSR PASS                                           */
    /**************************************************************************************************/

    if (list.render_flags & EnableTimers) {
        glQueryCounter(queries_[1][TimeReflStart], GL_TIMESTAMP);
    }

    if (list.render_flags & EnableSSR) {
        glBindFramebuffer(GL_FRAMEBUFFER, refl_buf_.fb);
        glViewport(0, 0, act_w_ / 2, act_h_ / 2);

        const Ren::Program *ssr_program = nullptr;
        if (clean_buf_.sample_count > 1) {
            ssr_program = blit_ssr_ms_prog_.get();
        } else {
            ssr_program = blit_ssr_prog_.get();
        }
        glUseProgram(ssr_program->prog_id());

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

        if (clean_buf_.sample_count > 1) {
            BindTextureMs(REN_REFL_DEPTH_TEX_SLOT, clean_buf_.depth_tex.GetValue());
            BindTextureMs(REN_REFL_NORM_TEX_SLOT, clean_buf_.attachments[REN_OUT_NORM_INDEX].tex);
            BindTextureMs(REN_REFL_SPEC_TEX_SLOT, clean_buf_.attachments[REN_OUT_SPEC_INDEX].tex);
        } else {
            BindTexture(REN_REFL_DEPTH_TEX_SLOT, clean_buf_.depth_tex.GetValue());
            BindTexture(REN_REFL_NORM_TEX_SLOT, clean_buf_.attachments[REN_OUT_NORM_INDEX].tex);
            BindTexture(REN_REFL_SPEC_TEX_SLOT, clean_buf_.attachments[REN_OUT_SPEC_INDEX].tex);
        }

        BindTexture(REN_REFL_PREV_TEX_SLOT, down_buf_.attachments[0].tex);
        if (list.probe_storage) {
            glActiveTexture(GL_TEXTURE0 + REN_ENV_TEX_SLOT);
            glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, list.probe_storage->tex_id());
        }
        
        BindTexBuffer(REN_CELLS_BUF_SLOT, cells_tbo_);
        BindTexBuffer(REN_ITEMS_BUF_SLOT, items_tbo_);

#ifndef DISABLE_MARKERS
        glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "REFLECTIONS PASS");
#endif

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, (const GLvoid *)uintptr_t(temp_buf_ndx_offset_));

        // Compose reflections on top of clean buffer
        glBindFramebuffer(GL_FRAMEBUFFER, refl_comb_framebuf_);
        glViewport(0, 0, act_w_, act_h_);

        glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)(temp_buf_vtx_offset_ + sizeof(fs_quad_positions)), sizeof(uvs), uvs);

        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);

        const Ren::Program *blit_mul_prog = nullptr;

        if (clean_buf_.sample_count > 1) {
            blit_mul_prog = blit_multiply_ms_prog_.get();
        } else {
            blit_mul_prog = blit_multiply_prog_.get();
        }
        glUseProgram(blit_mul_prog->prog_id());

        BindTexture(REN_REFL_SSR_TEX_SLOT, refl_buf_.attachments[0].tex);

        if (clean_buf_.sample_count > 1) {
            BindTextureMs(REN_REFL_SPEC_TEX_SLOT, clean_buf_.attachments[REN_OUT_SPEC_INDEX].tex);
            BindTextureMs(REN_REFL_DEPTH_TEX_SLOT, clean_buf_.depth_tex.GetValue());
            BindTextureMs(REN_REFL_NORM_TEX_SLOT, clean_buf_.attachments[REN_OUT_NORM_INDEX].tex);
        } else {
            BindTexture(REN_REFL_SPEC_TEX_SLOT, clean_buf_.attachments[REN_OUT_SPEC_INDEX].tex);
            BindTexture(REN_REFL_DEPTH_TEX_SLOT, clean_buf_.depth_tex.GetValue());
            BindTexture(REN_REFL_NORM_TEX_SLOT, clean_buf_.attachments[REN_OUT_NORM_INDEX].tex);
        }
        BindTexture(REN_REFL_PREV_TEX_SLOT, down_buf_.attachments[0].tex);

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, (const GLvoid *)uintptr_t(temp_buf_ndx_offset_));

#ifndef DISABLE_MARKERS
        glPopDebugGroup();
#endif

        glDisableVertexAttribArray(REN_VTX_POS_LOC);
        glDisableVertexAttribArray(REN_VTX_UV1_LOC);

        glDisable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
    
    if (list.render_flags & DebugProbes) {
        glDisable(GL_DEPTH_TEST);

        // Write to color
        glBindFramebuffer(GL_FRAMEBUFFER, refl_comb_framebuf_);

        glUseProgram(probe_prog_->prog_id());

        glBindVertexArray(sphere_vao_);

        glUniform1f(1, debug_roughness_);
        debug_roughness_ += 0.1f;
        if (debug_roughness_ > 8.0f) {
            debug_roughness_ = 0.0f;
        }

        if (list.probe_storage) {
            glActiveTexture(GL_TEXTURE0 + REN_DIFF_TEX_SLOT);
            glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, list.probe_storage->tex_id());
        }

        for (int i = 0; i < (int)list.probes.size(); i++) {
            const auto &pr = list.probes[i];

            glUniform1i(2, i);

            Ren::Mat4f world_from_object;
            world_from_object = Ren::Translate(world_from_object, Ren::Vec3f{ pr.position[0], pr.position[1], pr.position[2] });
            glUniformMatrix4fv(REN_U_M_MATRIX_LOC, 1, GL_FALSE, Ren::ValuePtr(world_from_object));

            glDrawElements(GL_TRIANGLES, (GLsizei)__sphere_indices_count, GL_UNSIGNED_SHORT, (void *)uintptr_t(sphere_ndx_offset_));
        }

        glBindVertexArray((GLuint)temp_vao_);
    }

    if (list.render_flags & DebugDeferred) {
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

    /**************************************************************************************************/
    /*                                            BLUR PASS                                           */
    /**************************************************************************************************/

    if (list.render_flags & EnableTimers) {
        glQueryCounter(queries_[1][TimeBlurStart], GL_TIMESTAMP);
    }

    //glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDepthFunc(GL_LESS);

    if (list.render_flags & (EnableSSR | EnableBloom | EnableTonemap)) {
        // store matrix to use it in next frame
        down_buf_view_from_world_ = shrd_data.uViewMatrix;

        // prepare blured buffer
        glBindFramebuffer(GL_FRAMEBUFFER, down_buf_.fb);
        glViewport(0, 0, down_buf_.w, down_buf_.h);

        const float fs_quad_uvs[] = { 0.0f, 0.0f,                       float(act_w_), 0.0f,
                                      float(act_w_), float(act_h_),     0.0f, float(act_h_) };

        const Ren::Program *cur_program = nullptr;

        if (clean_buf_.sample_count > 1) {
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

        if (clean_buf_.sample_count > 1) {
            BindTextureMs(REN_DIFF_TEX_SLOT, clean_buf_.attachments[0].tex);
        } else {
            BindTexture(REN_DIFF_TEX_SLOT, clean_buf_.attachments[0].tex);
        }

#ifndef DISABLE_MARKERS
        glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "BLUR PASS");
#endif

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, (const GLvoid *)uintptr_t(temp_buf_ndx_offset_));

        if (list.render_flags & EnableBloom) {
            glBindFramebuffer(GL_FRAMEBUFFER, blur_buf2_.fb);
            glViewport(0, 0, blur_buf2_.w, blur_buf2_.h);

            const float fs_quad_uvs1[] = { 0.0f, 0.0f,                                 float(down_buf_.w), 0.0f,
                                           float(down_buf_.w), float(down_buf_.h),     0.0f, float(down_buf_.h) };

            cur_program = blit_gauss_prog_.get();
            glUseProgram(cur_program->prog_id());

            glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)(temp_buf_vtx_offset_ + sizeof(fs_quad_positions)), sizeof(fs_quad_uvs1), fs_quad_uvs1);

            glUniform1i(cur_program->uniform(U_TEX).loc, REN_DIFF_TEX_SLOT);
            glUniform1f(cur_program->uniform(4).loc, 0.0f);

            BindTexture(REN_DIFF_TEX_SLOT, down_buf_.attachments[0].tex);

            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, (const GLvoid *)uintptr_t(temp_buf_ndx_offset_));

            glUniform1f(cur_program->uniform(4).loc, 1.0f);

            glBindFramebuffer(GL_FRAMEBUFFER, blur_buf1_.fb);
            glViewport(0, 0, blur_buf1_.w, blur_buf1_.h);

            const float fs_quad_uvs2[] = { 0.0f, 0.0f,                                   float(blur_buf2_.w), 0.0f,
                                           float(blur_buf2_.w), float(blur_buf2_.h),     0.0f, float(blur_buf2_.h) };

            glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)(temp_buf_vtx_offset_ + sizeof(fs_quad_positions)), sizeof(fs_quad_uvs2), fs_quad_uvs2);

            BindTexture(REN_DIFF_TEX_SLOT, blur_buf2_.attachments[0].tex);

            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, (const GLvoid *)uintptr_t(temp_buf_ndx_offset_));
        }

#ifndef DISABLE_MARKERS
        glPopDebugGroup();
#endif

        glDisableVertexAttribArray(REN_VTX_POS_LOC);
        glDisableVertexAttribArray(REN_VTX_UV1_LOC);
    }

    if (list.render_flags & EnableTonemap) {
        // draw to small framebuffer
        glBindFramebuffer(GL_FRAMEBUFFER, reduced_buf_.fb);
        glViewport(0, 0, reduced_buf_.w, reduced_buf_.h);

        FrameBuf *buf_to_sample = nullptr;

        if (list.render_flags & EnableBloom) {
            // sample blured buffer
            buf_to_sample = &blur_buf1_;
        } else {
            // sample small buffer
            buf_to_sample = &down_buf_;
        }

        glUseProgram(blit_red_prog_->prog_id());

        glBindBuffer(GL_ARRAY_BUFFER, last_vertex_buffer_);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, last_index_buffer_);

        glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)temp_buf_vtx_offset_, sizeof(fs_quad_positions), fs_quad_positions);
        glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)(temp_buf_vtx_offset_ + sizeof(fs_quad_positions)), sizeof(fs_quad_norm_uvs), fs_quad_norm_uvs);
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

        BindTexture(REN_DIFF_TEX_SLOT, buf_to_sample->attachments[0].tex);

#ifndef DISABLE_MARKERS
        glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "SAMPLE FRAME BRIGHTNESS");
#endif

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, (const GLvoid *)uintptr_t(temp_buf_ndx_offset_));

#ifndef DISABLE_MARKERS
        glPopDebugGroup();
#endif

        glDisableVertexAttribArray(REN_VTX_POS_LOC);
        glDisableVertexAttribArray(REN_VTX_UV1_LOC);

        const float max_value = 64.0f;
        float cur_average = 0.0f;

        {   // Retrieve result of glReadPixels call from previous frame
            glBindBuffer(GL_PIXEL_PACK_BUFFER, (GLuint)reduce_pbo_);
            float *reduced_pixels = (float *)glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, 4 * reduced_buf_.w * reduced_buf_.h * sizeof(float), GL_MAP_READ_BIT);
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

    /**************************************************************************************************/
    /*                                            BLIT PASS                                           */
    /**************************************************************************************************/

    if (list.render_flags & EnableTimers) {
        glQueryCounter(queries_[1][TimeBlitStart], GL_TIMESTAMP);
    }
    
    if (list.render_flags & EnableFxaa) {
        glBindFramebuffer(GL_FRAMEBUFFER, combined_buf_.fb);
        glViewport(0, 0, act_w_, act_h_);
    } else {
        if (!target) {
            glBindFramebuffer(GL_FRAMEBUFFER, framebuf_before);
            glViewport(viewport_before[0], viewport_before[1], viewport_before[2], viewport_before[3]);
        } else {
            glBindFramebuffer(GL_FRAMEBUFFER, target->fb);
            glViewport(0, 0, target->w, target->h);
        }
    }

    {   // Blit main framebuffer
        const Ren::Program *blit_prog = nullptr;

        if (clean_buf_.sample_count > 1) {
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

        glUniform1f(12, (list.render_flags & EnableTonemap) ? 1.0f : 0.0f);
        glUniform2f(13, float(act_w_), float(act_h_));

        glUniform1f(U_GAMMA, (list.render_flags & DebugLights) ? 1.0f : 2.2f);

        float exposure = reduced_average_ > std::numeric_limits<float>::epsilon() ? (0.95f / reduced_average_) : 1.0f;
        exposure = std::min(exposure, 1000.0f);

        glUniform1f(U_EXPOSURE, exposure);

        if (clean_buf_.sample_count > 1) {
            BindTextureMs(REN_DIFF_TEX_SLOT, clean_buf_.attachments[REN_OUT_COLOR_INDEX].tex);
        } else {
            BindTexture(REN_DIFF_TEX_SLOT, clean_buf_.attachments[REN_OUT_COLOR_INDEX].tex);
        }

        if (list.render_flags & EnableBloom) {
            BindTexture(REN_DIFF_TEX_SLOT + 1, blur_buf1_.attachments[0].tex);
        } else {
            BindTexture(REN_DIFF_TEX_SLOT + 1, dummy_black_->tex_id());
        }

#ifndef DISABLE_MARKERS
        glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "FINAL BLIT");
#endif

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, (const GLvoid *)uintptr_t(temp_buf_ndx_offset_));

        glDisableVertexAttribArray(REN_VTX_POS_LOC);
        glDisableVertexAttribArray(REN_VTX_UV1_LOC);
    }

    if (list.render_flags & EnableFxaa) {
        if (!target) {
            glBindFramebuffer(GL_FRAMEBUFFER, framebuf_before);
            glViewport(viewport_before[0], viewport_before[1], viewport_before[2], viewport_before[3]);
        } else {
            glBindFramebuffer(GL_FRAMEBUFFER, target->fb);
            glViewport(0, 0, target->w, target->h);
        }

        {   // Blit fxaa
            const Ren::Program *blit_prog = blit_fxaa_prog_.get();
            glUseProgram(blit_prog->prog_id());

            glBindBuffer(GL_ARRAY_BUFFER, last_vertex_buffer_);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, last_index_buffer_);

            glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)temp_buf_vtx_offset_, sizeof(fs_quad_positions), fs_quad_positions);
            glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)(temp_buf_vtx_offset_ + sizeof(fs_quad_positions)), sizeof(fs_quad_norm_uvs), fs_quad_norm_uvs);
            glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, (GLintptr)temp_buf_ndx_offset_, sizeof(fs_quad_indices), fs_quad_indices);

            glEnableVertexAttribArray(REN_VTX_POS_LOC);
            glVertexAttribPointer(REN_VTX_POS_LOC, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)uintptr_t(temp_buf_vtx_offset_));

            glEnableVertexAttribArray(REN_VTX_UV1_LOC);
            glVertexAttribPointer(REN_VTX_UV1_LOC, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)uintptr_t(temp_buf_vtx_offset_ + sizeof(fs_quad_positions)));

            BindTexture(REN_DIFF_TEX_SLOT, combined_buf_.attachments[0].tex);

            glUniform2f(12, 1.0f / act_w_, 1.0f / act_h_);

            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, (const GLvoid *)uintptr_t(temp_buf_ndx_offset_));

            glDisableVertexAttribArray(REN_VTX_POS_LOC);
            glDisableVertexAttribArray(REN_VTX_UV1_LOC);
        }
    }

#ifndef DISABLE_MARKERS
    glPopDebugGroup();
#endif
    
    {   
        if (list.render_flags & EnableSSR) {
            BindTexture(REN_DIFF_TEX_SLOT, down_buf_.attachments[0].tex);
            glGenerateMipmap(GL_TEXTURE_2D);
        }

        if (list.render_flags & EnableTonemap) {
            // Start asynchronous memory read from framebuffer
            glBindFramebuffer(GL_FRAMEBUFFER, reduced_buf_.fb);
            glReadBuffer(GL_COLOR_ATTACHMENT0);

            glBindBuffer(GL_PIXEL_PACK_BUFFER, (GLuint)reduce_pbo_);

            glReadPixels(0, 0, reduced_buf_.w, reduced_buf_.h, GL_RGBA, GL_FLOAT, nullptr);

            glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }
    }

    /**************************************************************************************************/
    /*                                            DEBUGGING                                           */
    /**************************************************************************************************/

    if (list.render_flags & (DebugLights | DebugDecals)) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        const Ren::Program *blit_prog = nullptr;

        if (clean_buf_.sample_count > 1) {
            blit_prog = blit_debug_ms_prog_.get();
        } else {
            blit_prog = blit_debug_prog_.get();
        }
        glUseProgram(blit_prog->prog_id());

        glUniform2i(U_RES, scr_w_, scr_h_);

        if (list.render_flags & DebugLights) {
            glUniform1i(16, 0);
        } else if (list.render_flags & DebugDecals) {
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

        if (clean_buf_.sample_count > 1) {
            BindTextureMs(REN_DIFF_TEX_SLOT, clean_buf_.depth_tex.GetValue());
        } else {
            BindTexture(REN_DIFF_TEX_SLOT, clean_buf_.depth_tex.GetValue());
        }

        BindTexBuffer(REN_CELLS_BUF_SLOT, cells_tbo_);
        BindTexBuffer(REN_ITEMS_BUF_SLOT, items_tbo_);

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, (const GLvoid *)uintptr_t(temp_buf_ndx_offset_));

        glDisableVertexAttribArray(REN_VTX_POS_LOC);
        glDisableVertexAttribArray(REN_VTX_UV1_LOC);

        glDisable(GL_BLEND);
    }

    if (((list.render_flags & (EnableCulling | DebugCulling)) == (EnableCulling | DebugCulling)) && !depth_pixels_[0].empty()) {
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

    if (list.render_flags & DebugShadow) {
        glUseProgram(blit_depth_prog_->prog_id());

        glEnableVertexAttribArray(REN_VTX_POS_LOC);
        glVertexAttribPointer(REN_VTX_POS_LOC, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)uintptr_t(temp_buf_vtx_offset_));

        glEnableVertexAttribArray(REN_VTX_UV1_LOC);
        glVertexAttribPointer(REN_VTX_UV1_LOC, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)uintptr_t(temp_buf_vtx_offset_ + 8 * sizeof(float)));

        glBindBuffer(GL_ARRAY_BUFFER, last_vertex_buffer_);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, last_index_buffer_);

        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, (GLintptr)temp_buf_ndx_offset_, sizeof(fs_quad_indices), fs_quad_indices);

        BindTexture(REN_DIFF_TEX_SLOT, shadow_buf_.depth_tex.GetValue());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);

        float k = (float(shadow_buf_.h) / shadow_buf_.w) * (float(scr_w_) / scr_h_);

        const int near_loc = blit_depth_prog_->uniform("near").loc;
        const int far_loc = blit_depth_prog_->uniform("far").loc;
        const int col_loc = blit_depth_prog_->uniform("color").loc;

        {   // Clear region
            glEnable(GL_SCISSOR_TEST);

            glScissor(0, 0, scr_w_ / 2, int(k * scr_h_ / 2));
            glClear(GL_COLOR_BUFFER_BIT);

            glDisable(GL_SCISSOR_TEST);
        }

        // Draw visible shadow regions
        for (int i = 0; i < (int)list.shadow_lists.size(); i++) {
            const auto &sh_list = list.shadow_lists[i];
            const auto &reg = list.shadow_regions[i];

            const float positions[] = { -1.0f + reg.transform[0],                       -1.0f + reg.transform[1] * k,
                                        -1.0f + reg.transform[0] + reg.transform[2],    -1.0f + reg.transform[1] * k,
                                        -1.0f + reg.transform[0] + reg.transform[2],    -1.0f + (reg.transform[1] + reg.transform[3]) * k,
                                        -1.0f + reg.transform[0],                       -1.0f + (reg.transform[1] + reg.transform[3]) * k };

            const float uvs[] = { float(sh_list.shadow_map_pos[0]),                                 float(sh_list.shadow_map_pos[1]),
                                  float(sh_list.shadow_map_pos[0] + sh_list.shadow_map_size[0]),    float(sh_list.shadow_map_pos[1]),
                                  float(sh_list.shadow_map_pos[0] + sh_list.shadow_map_size[0]),    float(sh_list.shadow_map_pos[1] + sh_list.shadow_map_size[1]),
                                  float(sh_list.shadow_map_pos[0]),                                 float(sh_list.shadow_map_pos[1] + sh_list.shadow_map_size[1]) };

            glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)temp_buf_vtx_offset_, sizeof(positions), positions);
            glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)(temp_buf_vtx_offset_ + sizeof(positions)), sizeof(uvs), uvs);
            
            glUniform1f(near_loc, sh_list.cam_near);
            glUniform1f(far_loc, sh_list.cam_far);

            if (sh_list.shadow_batch_count) {
                // mark updated region with red
                glUniform3f(col_loc, 1.0f, 0.5f, 0.5f);
            } else {
                // mark cached region with green
                glUniform3f(col_loc, 0.5f, 1.0f, 0.5f);
            }

            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, (const GLvoid *)uintptr_t(temp_buf_ndx_offset_));
        }

        // Draw invisible cached shadow regions
        for (int i = 0; i < (int)list.cached_shadow_regions.size(); i++) {
            const auto &r = list.cached_shadow_regions[i];

            const float positions[] = { -1.0f + float(r.pos[0]) / SHADOWMAP_WIDTH,                       -1.0f + k * float(r.pos[1]) / SHADOWMAP_HEIGHT,
                                        -1.0f + float(r.pos[0] + r.size[0]) / SHADOWMAP_WIDTH,           -1.0f + k * float(r.pos[1]) / SHADOWMAP_HEIGHT,
                                        -1.0f + float(r.pos[0] + r.size[0]) / SHADOWMAP_WIDTH,           -1.0f + k * float(r.pos[1] + r.size[1]) / SHADOWMAP_HEIGHT,
                                        -1.0f + float(r.pos[0]) / SHADOWMAP_WIDTH,                       -1.0f + k * float(r.pos[1] + r.size[1]) / SHADOWMAP_HEIGHT };

            const float uvs[] = { float(r.pos[0]),                  float(r.pos[1]),
                                  float(r.pos[0] + r.size[0]),      float(r.pos[1]),
                                  float(r.pos[0] + r.size[0]),      float(r.pos[1] + r.size[1]),
                                  float(r.pos[0]),                  float(r.pos[1] + r.size[1]) };

            glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)temp_buf_vtx_offset_, sizeof(positions), positions);
            glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)(temp_buf_vtx_offset_ + sizeof(positions)), sizeof(uvs), uvs);

            glUniform1f(near_loc, r.cam_near);
            glUniform1f(far_loc, r.cam_far);

            // mark cached region with blue
            glUniform3f(col_loc, 0.5f, 0.5f, 1.0f);

            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, (const GLvoid *)uintptr_t(temp_buf_ndx_offset_));
        }

        glDisableVertexAttribArray(REN_VTX_POS_LOC);
        glDisableVertexAttribArray(REN_VTX_UV1_LOC);
    }

    if (list.render_flags & DebugReduce) {
        BlitBuffer(-1.0f, -1.0f, 0.5f, 0.5f, reduced_buf_, 0, 1, 10.0f);
    }

    if (list.render_flags & DebugDeferred) {
        BlitBuffer(-1.0f, -1.0f, 0.5f, 0.5f, clean_buf_, 1, 2);
    }

    if (list.render_flags & DebugBlur) {
        BlitBuffer(-1.0f, -1.0f, 1.0f, 1.0f, blur_buf1_, 0, 1, 400.0f);
    }

    if (list.render_flags & DebugSSAO) {
        BlitBuffer(-1.0f, -1.0f, 1.0f, 1.0f, ssao_buf_, 0, 1);
    }

    if ((list.render_flags & DebugDecals) && list.decals_atlas) {
        int resx = list.decals_atlas->resx(),
            resy = list.decals_atlas->resy();

        float k = float(scr_w_) / scr_h_;
        k *= float(resy) / resx;

        BlitTexture(-1.0f, -1.0f, 1.0f, 1.0f * k, list.decals_atlas->tex_id(0), resx, resy);
    }

    if (list.render_flags & DebugBVH) {
        if (!nodes_buf_) {
            GLuint nodes_buf;
            glGenBuffers(1, &nodes_buf);

            glBindBuffer(GL_TEXTURE_BUFFER, (GLuint)nodes_buf);
            glBufferData(GL_TEXTURE_BUFFER, list.temp_nodes.size() * sizeof(bvh_node_t), list.temp_nodes.data(), GL_DYNAMIC_DRAW);

            nodes_buf_ = (uint32_t)nodes_buf;

            GLuint nodes_tbo;

            glGenTextures(1, &nodes_tbo);
            glBindTexture(GL_TEXTURE_BUFFER, nodes_tbo);

            glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, nodes_buf);
            glBindTexture(GL_TEXTURE_BUFFER, 0);

            nodes_tbo_ = (uint32_t)nodes_tbo;
        } else {
            glBindBuffer(GL_TEXTURE_BUFFER, (GLuint)nodes_buf_);
            glBufferData(GL_TEXTURE_BUFFER, list.temp_nodes.size() * sizeof(bvh_node_t), list.temp_nodes.data(), GL_DYNAMIC_DRAW);
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

            BindTexBuffer(1, nodes_tbo_);

            glUniform1i(debug_bvh_prog->uniform("uRootIndex").loc, list.root_index);

            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, (const GLvoid *)uintptr_t(temp_buf_ndx_offset_));

            glDisableVertexAttribArray(REN_VTX_POS_LOC);
            glDisableVertexAttribArray(REN_VTX_UV1_LOC);

            glDisable(GL_BLEND);
        }

        glBindBuffer(GL_TEXTURE_BUFFER, 0);
    }

    glBindVertexArray(0);

    if (list.render_flags & EnableTimers) {
        glQueryCounter(queries_[1][TimeDrawEnd], GL_TIMESTAMP);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, framebuf_before);
    glViewport(viewport_before[0], viewport_before[1], viewport_before[2], viewport_before[3]);

    /**************************************************************************************************/
    /*                                              TIMERS                                            */
    /**************************************************************************************************/

    if (list.render_flags & EnableTimers) {
        // Get timer queries result (for previous frame)

        GLuint64 time_draw_start,
                 time_shadow_start,
                 time_depth_opaque_start,
                 time_ao_start,
                 time_opaque_start,
                 time_transp_start,
                 time_refl_start,
                 time_blur_start,
                 time_blit_start,
                 time_draw_end;

        glGetQueryObjectui64v(queries_[0][TimeDrawStart], GL_QUERY_RESULT, &time_draw_start);
        glGetQueryObjectui64v(queries_[0][TimeShadowMapStart], GL_QUERY_RESULT, &time_shadow_start);
        glGetQueryObjectui64v(queries_[0][TimeDepthOpaqueStart], GL_QUERY_RESULT, &time_depth_opaque_start);
        glGetQueryObjectui64v(queries_[0][TimeAOPassStart], GL_QUERY_RESULT, &time_ao_start);
        glGetQueryObjectui64v(queries_[0][TimeOpaqueStart], GL_QUERY_RESULT, &time_opaque_start);
        glGetQueryObjectui64v(queries_[0][TimeTranspStart], GL_QUERY_RESULT, &time_transp_start);
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

        backend_info_.shadow_time_us = uint32_t((time_depth_opaque_start - time_shadow_start) / 1000);
        backend_info_.depth_opaque_pass_time_us = uint32_t((time_ao_start - time_depth_opaque_start) / 1000);
        backend_info_.ao_pass_time_us = uint32_t((time_opaque_start - time_ao_start) / 1000);
        backend_info_.opaque_pass_time_us = uint32_t((time_transp_start - time_opaque_start) / 1000);
        backend_info_.transp_pass_time_us = uint32_t((time_refl_start - time_transp_start) / 1000);
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
        glUniform1f(cur_program->uniform(4).loc, 0.0f);

        BindTexture(REN_DIFF_TEX_SLOT, temp_tex_);

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, (const GLvoid *)uintptr_t(temp_buf_ndx_offset_));

        glUniform1f(cur_program->uniform(4).loc, 1.0f);

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

        glUniform1f(12, 1.0f);
        glUniform2f(13, float(w), float(h));
        glUniform1f(U_GAMMA, 2.2f);

        float exposure = 0.95f / reduced_average_;
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

    if (buf.sample_count > 1) {
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

        if (buf.sample_count > 1) {
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

void Renderer::BlitToLightProbeFace(const FrameBuf &src_buf, const ProbeStorage &dst_store, int probe_index, int face) {
    using namespace RendererInternal;

    GLint framebuf_before;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuf_before);

    GLint viewport_before[4];
    glGetIntegerv(GL_VIEWPORT, viewport_before);

    GLuint framebuf = (GLuint)temp_framebuf_;
    glBindFramebuffer(GL_FRAMEBUFFER, framebuf);

    GLuint cube_array = (GLuint)dst_store.tex_id();
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, cube_array, 0, (GLint)(probe_index * 6 + face));
    assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

    glViewport(0, 0, (GLint)dst_store.res(), (GLint)dst_store.res());

    glBindVertexArray((GLuint)temp_vao_);

    glBindBuffer(GL_ARRAY_BUFFER, last_vertex_buffer_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, last_index_buffer_);

    glEnableVertexAttribArray(REN_VTX_POS_LOC);
    glVertexAttribPointer(REN_VTX_POS_LOC, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)uintptr_t(temp_buf_vtx_offset_));

    glEnableVertexAttribArray(REN_VTX_UV1_LOC);
    glVertexAttribPointer(REN_VTX_UV1_LOC, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)uintptr_t(temp_buf_vtx_offset_ + sizeof(fs_quad_positions)));

    glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)temp_buf_vtx_offset_, sizeof(fs_quad_positions), fs_quad_positions);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, (GLintptr)temp_buf_ndx_offset_, sizeof(fs_quad_indices), fs_quad_indices);

    {   // Update first mip level of a cubemap
        Ren::Program *prog = blit_rgbm_prog_.get();
        glUseProgram(prog->prog_id());

        const float uvs[] = {
            0.0f, 0.0f,                             (float)src_buf.w, 0.0f,
            (float)src_buf.w, (float)src_buf.h,     0.0f, (float)src_buf.h
        };

        glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)(temp_buf_vtx_offset_ + sizeof(fs_quad_positions)), sizeof(uvs), uvs);

        BindTexture(0, src_buf.attachments[0].tex);

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, (const GLvoid *)uintptr_t(temp_buf_ndx_offset_));
    }

    {   // Update rest of mipmaps
        Ren::Program *prog = blit_mipmap_prog_.get();
        glUseProgram(prog->prog_id());

        glUniform1f(1, float(probe_index));
        glUniform1i(2, face);

        const float uvs[] = {
            -1.0f, 1.0f,   -1.0, -1.0f,
            1.0f, -1.0f,   1.0f, 1.0f
        };

        glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)(temp_buf_vtx_offset_ + sizeof(fs_quad_positions)), sizeof(uvs), uvs);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, cube_array);

        int res = dst_store.res() / 2;
        int level = 1;

        while (level <= dst_store.max_level()) {
            glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, cube_array, level, (GLint)(probe_index * 6 + face));
            assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

            glViewport(0, 0, res, res);

            glUniform1f(3, float(level - 1));

            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, (const GLvoid *)uintptr_t(temp_buf_ndx_offset_));

            res /= 2;
            level++;
        }
    }

    glDisableVertexAttribArray(REN_VTX_POS_LOC);
    glDisableVertexAttribArray(REN_VTX_UV1_LOC);

    glBindVertexArray(0);

    glBindFramebuffer(GL_FRAMEBUFFER, framebuf_before);
    glViewport(viewport_before[0], viewport_before[1], viewport_before[2], viewport_before[3]);
}

bool Renderer::BlitProjectSH(const ProbeStorage &store, int probe_index, int iteration, LightProbe &probe) {
    using namespace RendererInternal;

    GLint framebuf_before;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuf_before);

    GLint viewport_before[4];
    glGetIntegerv(GL_VIEWPORT, viewport_before);

    glBindFramebuffer(GL_FRAMEBUFFER, probe_sample_buf_.fb);

    glViewport(0, 0, probe_sample_buf_.w, probe_sample_buf_.h);

    glBindVertexArray((GLuint)temp_vao_);

    glBindBuffer(GL_ARRAY_BUFFER, last_vertex_buffer_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, last_index_buffer_);

    glEnableVertexAttribArray(REN_VTX_POS_LOC);
    glVertexAttribPointer(REN_VTX_POS_LOC, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)uintptr_t(temp_buf_vtx_offset_));

    glEnableVertexAttribArray(REN_VTX_UV1_LOC);
    glVertexAttribPointer(REN_VTX_UV1_LOC, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid *)uintptr_t(temp_buf_vtx_offset_ + sizeof(fs_quad_positions)));

    const float uvs[] = {
            0.0f, 0.0f,                                                 (float)probe_sample_buf_.w, 0.0f,
            (float)probe_sample_buf_.w, (float)probe_sample_buf_.h,     0.0f, (float)probe_sample_buf_.h
    };

    glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)temp_buf_vtx_offset_, sizeof(fs_quad_positions), fs_quad_positions);
    glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)(temp_buf_vtx_offset_ + sizeof(fs_quad_positions)), sizeof(uvs), uvs);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, (GLintptr)temp_buf_ndx_offset_, sizeof(fs_quad_indices), fs_quad_indices);

    if (iteration != 0) {
        // Retrieve result of previous read
        glBindBuffer(GL_PIXEL_PACK_BUFFER, (GLuint)probe_sample_pbo_);
        
        Ren::Vec3f sh_coeffs[4];
        
        float *pixels = (float *)glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, 4 * probe_sample_buf_.w * probe_sample_buf_.h * sizeof(float), GL_MAP_READ_BIT);
        if (pixels) {
            for (int y = 0; y < probe_sample_buf_.h; y++) {
                for (int x = 0; x < probe_sample_buf_.w; x++) {
                    int i = (x >= 8) ? ((x >= 16) ? 2 : 1) : 0;

                    sh_coeffs[0][i] += pixels[4 * (y * probe_sample_buf_.w + x) + 0];
                    sh_coeffs[1][i] += pixels[4 * (y * probe_sample_buf_.w + x) + 1];
                    sh_coeffs[2][i] += pixels[4 * (y * probe_sample_buf_.w + x) + 2];
                    sh_coeffs[3][i] += pixels[4 * (y * probe_sample_buf_.w + x) + 3];
                }
            }
            glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
        }
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

        const float inv_weight = 4.0f * Ren::Pi<float>() / (probe_sample_buf_.w * probe_sample_buf_.w);
        for (int i = 0; i < 4; i++) {
            sh_coeffs[i][0] *= inv_weight;
            sh_coeffs[i][1] *= inv_weight;
            sh_coeffs[i][2] *= inv_weight;
        }

        const float k = 1.0f / iteration;
        for (int i = 0; i < 4; i++) {
            Ren::Vec3f diff = sh_coeffs[i] - probe.sh_coeffs[i];
            probe.sh_coeffs[i] += diff * k;
        }
    }

    if (iteration < 64) {
        {   // Sample cubemap and project on sh basis
            Ren::Program *prog = blit_project_sh_prog_.get();
            glUseProgram(prog->prog_id());

            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, store.tex_id());

            BindTexture(1, rand2d_8x8_->tex_id());

            glUniform1f(1, float(probe_index));
            glUniform1i(2, iteration);

            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, (const GLvoid *)uintptr_t(temp_buf_ndx_offset_));
        }

        {   // Start readback from buffer (result will be retrieved at the start of next iteration)
            glReadBuffer(GL_COLOR_ATTACHMENT0);
            glBindBuffer(GL_PIXEL_PACK_BUFFER, (GLuint)probe_sample_pbo_);

            glReadPixels(0, 0, probe_sample_buf_.w, probe_sample_buf_.h, GL_RGBA, GL_FLOAT, nullptr);

            glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        }
    }

    glDisableVertexAttribArray(REN_VTX_POS_LOC);
    glDisableVertexAttribArray(REN_VTX_UV1_LOC);

    glBindVertexArray(0);

    glBindFramebuffer(GL_FRAMEBUFFER, framebuf_before);
    glViewport(viewport_before[0], viewport_before[1], viewport_before[2], viewport_before[3]);

    return iteration == 64;
}

#undef _AS_STR
#undef AS_STR