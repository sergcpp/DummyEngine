#include "Renderer.h"

#include <Ren/Camera.h>
#include <Ren/Context.h>
#include <Ren/GL.h>

namespace RendererInternal {
#include "Renderer_GL_Shaders.inl"
#include "__skydome_mesh.inl"
#include "__sphere_mesh.inl"

struct SharedDataBlock {
    Ren::Mat4f uViewMatrix, uProjMatrix, uViewProjMatrix, uViewProjPrevMatrix;
    Ren::Mat4f uInvViewMatrix, uInvProjMatrix, uInvViewProjMatrix, uDeltaMatrix;
    ShadowMapRegion uShadowMapRegions[REN_MAX_SHADOWMAPS_TOTAL];
    Ren::Vec4f uSunDir, uSunCol, uTaaInfo;
    Ren::Vec4f uClipInfo, uCamPosAndGamma;
    Ren::Vec4f uResAndFRes, uTranspParamsAndTime;
    Ren::Vec4f uWindScroll, uWindScrollPrev;
    ProbeItem uProbes[REN_MAX_PROBES_TOTAL] = {};
    EllipsItem uEllipsoids[REN_MAX_ELLIPSES_TOTAL] = {};
};
static_assert(sizeof(SharedDataBlock) == 7824, "!");

const Ren::Vec2f poisson_disk[] = {
    Ren::Vec2f{-0.705374f, -0.668203f}, Ren::Vec2f{-0.780145f, 0.486251f},
    Ren::Vec2f{0.566637f, 0.605213f},   Ren::Vec2f{0.488876f, -0.783441f},
    Ren::Vec2f{-0.613392f, 0.617481f},  Ren::Vec2f{0.170019f, -0.040254f},
    Ren::Vec2f{-0.299417f, 0.791925f},  Ren::Vec2f{0.645680f, 0.493210f},
    Ren::Vec2f{-0.651784f, 0.717887f},  Ren::Vec2f{0.421003f, 0.027070f},
    Ren::Vec2f{-0.817194f, -0.271096f}, Ren::Vec2f{0.977050f, -0.108615f},
    Ren::Vec2f{0.063326f, 0.142369f},   Ren::Vec2f{0.203528f, 0.214331f},
    Ren::Vec2f{-0.667531f, 0.326090f},  Ren::Vec2f{-0.098422f, -0.295755f},
    Ren::Vec2f{-0.885922f, 0.215369f},  Ren::Vec2f{0.039766f, -0.396100f},
    Ren::Vec2f{0.751946f, 0.453352f},   Ren::Vec2f{0.078707f, -0.715323f},
    Ren::Vec2f{-0.075838f, -0.529344f}, Ren::Vec2f{0.724479f, -0.580798f},
    Ren::Vec2f{0.222999f, -0.215125f},  Ren::Vec2f{-0.467574f, -0.405438f},
    Ren::Vec2f{-0.248268f, -0.814753f}, Ren::Vec2f{0.354411f, -0.887570f},
    Ren::Vec2f{0.175817f, 0.382366f},   Ren::Vec2f{0.487472f, -0.063082f},
    Ren::Vec2f{-0.084078f, 0.898312f},  Ren::Vec2f{0.470016f, 0.217933f},
    Ren::Vec2f{-0.696890f, -0.549791f}, Ren::Vec2f{-0.149693f, 0.605762f},
    Ren::Vec2f{0.034211f, 0.979980f},   Ren::Vec2f{0.503098f, -0.308878f},
    Ren::Vec2f{-0.016205f, -0.872921f}, Ren::Vec2f{0.385784f, -0.393902f},
    Ren::Vec2f{-0.146886f, -0.859249f}, Ren::Vec2f{0.643361f, 0.164098f},
    Ren::Vec2f{0.634388f, -0.049471f},  Ren::Vec2f{-0.688894f, 0.007843f},
    Ren::Vec2f{0.464034f, -0.188818f},  Ren::Vec2f{-0.440840f, 0.137486f},
    Ren::Vec2f{0.364483f, 0.511704f},   Ren::Vec2f{0.034028f, 0.325968f},
    Ren::Vec2f{0.099094f, -0.308023f},  Ren::Vec2f{0.693960f, -0.366253f},
    Ren::Vec2f{0.678884f, -0.204688f},  Ren::Vec2f{0.001801f, 0.780328f},
    Ren::Vec2f{0.145177f, -0.898984f},  Ren::Vec2f{0.062655f, -0.611866f},
    Ren::Vec2f{0.315226f, -0.604297f},  Ren::Vec2f{-0.371868f, 0.882138f},
    Ren::Vec2f{0.200476f, 0.494430f},   Ren::Vec2f{-0.494552f, -0.711051f},
    Ren::Vec2f{0.612476f, 0.705252f},   Ren::Vec2f{-0.578845f, -0.768792f},
    Ren::Vec2f{-0.772454f, -0.090976f}, Ren::Vec2f{0.504440f, 0.372295f},
    Ren::Vec2f{0.155736f, 0.065157f},   Ren::Vec2f{0.391522f, 0.849605f},
    Ren::Vec2f{-0.620106f, -0.328104f}, Ren::Vec2f{0.789239f, -0.419965f},
    Ren::Vec2f{-0.545396f, 0.538133f},  Ren::Vec2f{-0.178564f, -0.596057f}};

extern const Ren::Vec2f HaltonSeq23[];

// const GLuint A_INDICES = 3;
// const GLuint A_WEIGHTS = 4;

const int U_MVP_MATR = 0;
// const int U_MV_MATR = 1;

// const int U_SH_MVP_MATR = 2;

// const int U_TEX = 3;

const int U_GAMMA = 14;

const int U_EXPOSURE = 15;

const int U_RES = 15;

// const int U_LM_TRANSFORM = 16;

// const int LIGHTS_BUFFER_BINDING = 0;

const int TEMP_BUF_SIZE = 256;

const float fs_quad_positions[] = {-1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f};

const float fs_quad_norm_uvs[] = {0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f};

const uint16_t fs_quad_indices[] = {0, 1, 2, 0, 2, 3};

const size_t SkinTransformsBufChunkSize =
    sizeof(SkinTransform) * REN_MAX_SKIN_XFORMS_TOTAL;
const size_t SkinRegionsBufChunkSize = sizeof(SkinRegion) * REN_MAX_SKIN_REGIONS_TOTAL;
const size_t InstanceDataBufChunkSize = sizeof(InstanceData) * REN_MAX_INSTANCES_TOTAL;
const size_t LightsBufChunkSize = sizeof(LightSourceItem) * REN_MAX_LIGHTS_TOTAL;
const size_t DecalsBufChunkSize = sizeof(DecalItem) * REN_MAX_DECALS_TOTAL;
const size_t CellsBufChunkSize = sizeof(CellData) * REN_CELLS_COUNT;
const size_t ItemsBufChunkSize = sizeof(ItemData) * REN_MAX_ITEMS_TOTAL;

const int TaaSampleCount = 8;

struct DebugMarker {
    explicit DebugMarker(const char *name) {
#ifndef DISABLE_MARKERS
        glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, name);
#endif
    }

    ~DebugMarker() {
#ifndef DISABLE_MARKERS
        glPopDebugGroup();
#endif
    }
};
} // namespace RendererInternal

void Renderer::InitRendererInternal() {
    using namespace RendererInternal;

    // Ren::ILog *log = ctx_.log();

    Ren::eProgLoadStatus status;
    skydome_prog_ = ctx_.LoadProgramGLSL("skydome", skydome_vs, skydome_fs, &status);
    assert(status == Ren::eProgLoadStatus::CreatedFromData);
    fillz_solid_prog_ =
        ctx_.LoadProgramGLSL("fillz_solid", fillz_solid_vs, fillz_solid_fs, &status);
    assert(status == Ren::eProgLoadStatus::CreatedFromData);
    fillz_vege_solid_prog_ = ctx_.LoadProgramGLSL("fillz_vege_solid", fillz_vege_solid_vs,
                                                  fillz_solid_fs, &status);
    assert(status == Ren::eProgLoadStatus::CreatedFromData);
    fillz_vege_solid_vel_prog_ = ctx_.LoadProgramGLSL(
        "fillz_vege_solid_vel", fillz_vege_solid_vel_vs, fillz_solid_vel_fs, &status);
    assert(status == Ren::eProgLoadStatus::CreatedFromData);
    fillz_transp_prog_ =
        ctx_.LoadProgramGLSL("fillz_transp", fillz_transp_vs, fillz_transp_fs, &status);
    assert(status == Ren::eProgLoadStatus::CreatedFromData);
    fillz_vege_transp_prog_ = ctx_.LoadProgramGLSL(
        "fillz_vege_transp", fillz_vege_transp_vs, fillz_transp_fs, &status);
    assert(status == Ren::eProgLoadStatus::CreatedFromData);
    fillz_vege_transp_vel_prog_ = ctx_.LoadProgramGLSL(
        "fillz_vege_transp_vel", fillz_vege_transp_vel_vs, fillz_transp_vel_fs, &status);
    assert(status == Ren::eProgLoadStatus::CreatedFromData);
    shadow_solid_prog_ =
        ctx_.LoadProgramGLSL("shadow_solid", shadow_solid_vs, shadow_solid_fs, &status);
    assert(status == Ren::eProgLoadStatus::CreatedFromData);
    shadow_vege_solid_prog_ = ctx_.LoadProgramGLSL(
        "shadow_vege_solid", shadow_vege_solid_vs, shadow_solid_fs, &status);
    assert(status == Ren::eProgLoadStatus::CreatedFromData);
    shadow_transp_prog_ = ctx_.LoadProgramGLSL("shadow_transp", shadow_transp_vs,
                                               shadow_transp_fs, &status);
    assert(status == Ren::eProgLoadStatus::CreatedFromData);
    shadow_vege_transp_prog_ = ctx_.LoadProgramGLSL(
        "shadow_vege_transp", shadow_vege_transp_vs, shadow_transp_fs, &status);
    assert(status == Ren::eProgLoadStatus::CreatedFromData);
    blit_prog_ = ctx_.LoadProgramGLSL("blit", blit_vs, blit_fs, &status);
    assert(status == Ren::eProgLoadStatus::CreatedFromData);
    blit_combine_prog_ =
        ctx_.LoadProgramGLSL("blit_combine", blit_vs, blit_combine_fs, &status);
    assert(status == Ren::eProgLoadStatus::CreatedFromData);
    blit_combine_ms_prog_ =
        ctx_.LoadProgramGLSL("blit_combine_ms", blit_vs, blit_combine_ms_fs, &status);
    assert(status == Ren::eProgLoadStatus::CreatedFromData);
    blit_ms_prog_ = ctx_.LoadProgramGLSL("blit_ms", blit_vs, blit_ms_fs, &status);
    assert(status == Ren::eProgLoadStatus::CreatedFromData);
    blit_red_prog_ = ctx_.LoadProgramGLSL("blit_red", blit_vs, blit_reduced_fs, &status);
    assert(status == Ren::eProgLoadStatus::CreatedFromData);
    blit_down_prog_ = ctx_.LoadProgramGLSL("blit_down", blit_vs, blit_down_fs, &status);
    assert(status == Ren::eProgLoadStatus::CreatedFromData);
    blit_down_ms_prog_ =
        ctx_.LoadProgramGLSL("blit_down_ms", blit_vs, blit_down_ms_fs, &status);
    assert(status == Ren::eProgLoadStatus::CreatedFromData);
    blit_down_depth_prog_ =
        ctx_.LoadProgramGLSL("blit_down_depth", blit_vs, blit_down_depth_fs, &status);
    assert(status == Ren::eProgLoadStatus::CreatedFromData);
    blit_down_depth_ms_prog_ = ctx_.LoadProgramGLSL("blit_down_depth_ms", blit_vs,
                                                    blit_down_depth_ms_fs, &status);
    assert(status == Ren::eProgLoadStatus::CreatedFromData);
    blit_gauss_prog_ =
        ctx_.LoadProgramGLSL("blit_gauss", blit_vs, blit_gauss_fs, &status);
    assert(status == Ren::eProgLoadStatus::CreatedFromData);
    blit_gauss_sep_prog_ =
        ctx_.LoadProgramGLSL("blit_gauss_sep", blit_vs, blit_gauss_sep_fs, &status);
    assert(status == Ren::eProgLoadStatus::CreatedFromData);
    blit_dof_init_coc_prog_ =
        ctx_.LoadProgramGLSL("blit_dof_init_coc", blit_vs, blit_dof_init_coc_fs, &status);
    assert(status == Ren::eProgLoadStatus::CreatedFromData);
    blit_dof_bilateral_prog_ = ctx_.LoadProgramGLSL("blit_dof_bilateral", blit_vs,
                                                    blit_dof_bilateral_fs, &status);
    assert(status == Ren::eProgLoadStatus::CreatedFromData);
    blit_dof_calc_near_prog_ = ctx_.LoadProgramGLSL("blit_dof_calc_near", blit_vs,
                                                    blit_dof_calc_near_fs, &status);
    assert(status == Ren::eProgLoadStatus::CreatedFromData);
    blit_dof_small_blur_prog_ = ctx_.LoadProgramGLSL("blit_dof_small_blur", blit_vs,
                                                     blit_dof_small_blur_fs, &status);
    assert(status == Ren::eProgLoadStatus::CreatedFromData);
    blit_dof_combine_prog_ =
        ctx_.LoadProgramGLSL("blit_dof_combine", blit_vs, blit_dof_combine_fs, &status);
    assert(status == Ren::eProgLoadStatus::CreatedFromData);
    blit_dof_combine_ms_prog_ = ctx_.LoadProgramGLSL("blit_dof_combine_ms", blit_vs,
                                                     blit_dof_combine_ms_fs, &status);
    assert(status == Ren::eProgLoadStatus::CreatedFromData);
    blit_bilateral_prog_ =
        ctx_.LoadProgramGLSL("blit_bilateral", blit_vs, blit_bilateral_fs, &status);
    assert(status == Ren::eProgLoadStatus::CreatedFromData);
    blit_upscale_prog_ =
        ctx_.LoadProgramGLSL("blit_upscale", blit_vs, blit_upscale_fs, &status);
    assert(status == Ren::eProgLoadStatus::CreatedFromData);
    blit_upscale_ms_prog_ =
        ctx_.LoadProgramGLSL("blit_upscale_ms", blit_vs, blit_upscale_ms_fs, &status);
    assert(status == Ren::eProgLoadStatus::CreatedFromData);
    blit_debug_prog_ =
        ctx_.LoadProgramGLSL("blit_debug", blit_vs, blit_debug_fs, &status);
    assert(status == Ren::eProgLoadStatus::CreatedFromData);
    blit_debug_ms_prog_ =
        ctx_.LoadProgramGLSL("blit_debug_ms", blit_vs, blit_debug_ms_fs, &status);
    assert(status == Ren::eProgLoadStatus::CreatedFromData);

    { // ssr related
        blit_ssr_prog_ = ctx_.LoadProgramGLSL("blit_ssr", blit_vs, blit_ssr_fs, &status);
        assert(status == Ren::eProgLoadStatus::CreatedFromData);
        blit_ssr_ms_prog_ =
            ctx_.LoadProgramGLSL("blit_ssr_ms", blit_vs, blit_ssr_ms_fs, &status);
        assert(status == Ren::eProgLoadStatus::CreatedFromData);
        blit_ssr_compose_prog_ = ctx_.LoadProgramGLSL("blit_ssr_compose", blit_vs,
                                                      blit_ssr_compose_fs, &status);
        assert(status == Ren::eProgLoadStatus::CreatedFromData);
        blit_ssr_compose_ms_prog_ = ctx_.LoadProgramGLSL("blit_ssr_compose_ms", blit_vs,
                                                         blit_ssr_compose_ms_fs, &status);
        assert(status == Ren::eProgLoadStatus::CreatedFromData);
        blit_ssr_dilate_prog_ =
            ctx_.LoadProgramGLSL("blit_ssr_dilate", blit_vs, blit_ssr_dilate_fs, &status);
        assert(status == Ren::eProgLoadStatus::CreatedFromData);
    }

    blit_ms_resolve_prog_ =
        ctx_.LoadProgramGLSL("blit_ms_resolve", blit_vs, blit_ms_resolve_fs, &status);
    assert(status == Ren::eProgLoadStatus::CreatedFromData);
    blit_ao_prog_ = ctx_.LoadProgramGLSL("blit_ao", blit_vs, blit_ssao_fs, &status);
    assert(status == Ren::eProgLoadStatus::CreatedFromData);
    blit_multiply_prog_ =
        ctx_.LoadProgramGLSL("blit_multiply", blit_vs, blit_multiply_fs, &status);
    assert(status == Ren::eProgLoadStatus::CreatedFromData);
    blit_multiply_ms_prog_ =
        ctx_.LoadProgramGLSL("blit_multiply_ms", blit_vs, blit_multiply_ms_fs, &status);
    assert(status == Ren::eProgLoadStatus::CreatedFromData);
    blit_debug_bvh_prog_ =
        ctx_.LoadProgramGLSL("blit_debug_bvh", blit_vs, blit_debug_bvh_fs, &status);
    assert(status == Ren::eProgLoadStatus::CreatedFromData);
    blit_debug_bvh_ms_prog_ =
        ctx_.LoadProgramGLSL("blit_debug_bvh_ms", blit_vs, blit_debug_bvh_ms_fs, &status);
    assert(status == Ren::eProgLoadStatus::CreatedFromData);
    blit_depth_prog_ =
        ctx_.LoadProgramGLSL("blit_depth", blit_vs, blit_depth_fs, &status);
    assert(status == Ren::eProgLoadStatus::CreatedFromData);
    blit_rgbm_prog_ = ctx_.LoadProgramGLSL("blit_rgbm", blit_vs, blit_rgbm_fs, &status);
    assert(status == Ren::eProgLoadStatus::CreatedFromData);
    blit_mipmap_prog_ =
        ctx_.LoadProgramGLSL("blit_mipmap", blit_vs, blit_mipmap_fs, &status);
    assert(status == Ren::eProgLoadStatus::CreatedFromData);
    blit_prefilter_prog_ =
        ctx_.LoadProgramGLSL("blit_prefilter", blit_vs, blit_prefilter_fs, &status);
    assert(status == Ren::eProgLoadStatus::CreatedFromData);
    blit_project_sh_prog_ = ctx_.LoadProgramGLSL("blit_project_sh_prog", blit_vs,
                                                 blit_project_sh_fs, &status);
    assert(status == Ren::eProgLoadStatus::CreatedFromData);
    blit_fxaa_prog_ =
        ctx_.LoadProgramGLSL("blit_fxaa_prog", blit_vs, blit_fxaa_fs, &status);
    assert(status == Ren::eProgLoadStatus::CreatedFromData);
    blit_taa_prog_ = ctx_.LoadProgramGLSL("blit_taa_prog", blit_vs, blit_taa_fs, &status);
    assert(status == Ren::eProgLoadStatus::CreatedFromData);
    blit_static_vel_prog_ = ctx_.LoadProgramGLSL("blit_static_vel_prog", blit_vs,
                                                 blit_static_vel_fs, &status);
    assert(status == Ren::eProgLoadStatus::CreatedFromData);
    blit_transparent_compose_prog_ = ctx_.LoadProgramGLSL(
        "blit_transparent_compose_prog", blit_vs, blit_transparent_compose_fs, &status);
    assert(status == Ren::eProgLoadStatus::CreatedFromData);
    blit_transparent_compose_ms_prog_ =
        ctx_.LoadProgramGLSL("blit_transparent_compose_ms_prog", blit_vs,
                             blit_transparent_compose_ms_fs, &status);
    assert(status == Ren::eProgLoadStatus::CreatedFromData);
    blit_transparent_init_prog_ = ctx_.LoadProgramGLSL(
        "blit_transparent_init_prog", blit_vs, blit_transparent_init_fs, &status);
    assert(status == Ren::eProgLoadStatus::CreatedFromData);
    probe_prog_ = ctx_.LoadProgramGLSL("probe_prog", probe_vs, probe_fs, &status);
    assert(status == Ren::eProgLoadStatus::CreatedFromData);
    ellipsoid_prog_ =
        ctx_.LoadProgramGLSL("ellipsoid_prog", ellipsoid_vs, ellipsoid_fs, &status);
    assert(status == Ren::eProgLoadStatus::CreatedFromData);
    skinning_prog_ = ctx_.LoadProgramGLSL("skinning_prog", skinning_cs, &status);
    assert(status == Ren::eProgLoadStatus::CreatedFromData);

    GLint tex_buf_offset_alignment;
    glGetIntegerv(GL_TEXTURE_BUFFER_OFFSET_ALIGNMENT, &tex_buf_offset_alignment);

    for (uint32_t &ubo : unif_shared_data_block_) {
        GLuint shared_data_ubo;

        glGenBuffers(1, &shared_data_ubo);
        glBindBuffer(GL_UNIFORM_BUFFER, shared_data_ubo);
        glBufferData(GL_UNIFORM_BUFFER, sizeof(SharedDataBlock), nullptr,
                     GL_DYNAMIC_COPY);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);

        ubo = (uint32_t)shared_data_ubo;
    }

    Ren::CheckError("[InitRendererInternal]: UBO creation", ctx_.log());

    { // Create buffer that holds per-instance transform matrices
        GLuint instances_buf;

        glGenBuffers(1, &instances_buf);
        glBindBuffer(GL_TEXTURE_BUFFER, instances_buf);
        glBufferData(GL_TEXTURE_BUFFER, FrameSyncWindow * InstanceDataBufChunkSize,
                     nullptr, GL_DYNAMIC_COPY);
        glBindBuffer(GL_TEXTURE_BUFFER, 0);

        instances_buf_ = (uint32_t)instances_buf;

        for (int i = 0; i < FrameSyncWindow; i++) {
            GLuint instances_tbo;

            glGenTextures(1, &instances_tbo);
            glBindTexture(GL_TEXTURE_BUFFER, instances_tbo);

            const GLuint offset = i * InstanceDataBufChunkSize;
            assert((offset % tex_buf_offset_alignment == 0) &&
                   "Offset is not properly aligned!");
            glTexBufferRange(GL_TEXTURE_BUFFER, GL_RGBA32F, instances_buf, offset,
                             InstanceDataBufChunkSize);
            glBindTexture(GL_TEXTURE_BUFFER, 0);

            instances_tbo_[i] = (uint32_t)instances_tbo;
        }
    }

    Ren::CheckError("[InitRendererInternal]: instances TBO", ctx_.log());

    { // Create buffer that holds offsets for skinning shader invocation
        GLuint skin_regions_buf;

        glGenBuffers(1, &skin_regions_buf);
        glBindBuffer(GL_TEXTURE_BUFFER, skin_regions_buf);
        glBufferData(GL_TEXTURE_BUFFER, FrameSyncWindow * SkinRegionsBufChunkSize,
                     nullptr, GL_DYNAMIC_COPY);
        glBindBuffer(GL_TEXTURE_BUFFER, 0);

        skin_regions_buf_ = (uint32_t)skin_regions_buf;

        GLuint skin_regions_tbo;

        glGenTextures(1, &skin_regions_tbo);
        glBindTexture(GL_TEXTURE_BUFFER, skin_regions_tbo);

        glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, skin_regions_buf);
        glBindTexture(GL_TEXTURE_BUFFER, 0);

        skin_regions_tbo_ = (uint32_t)skin_regions_tbo;
    }

    Ren::CheckError("[InitRendererInternal]: skin regions TBO", ctx_.log());

    { // Create buffer that holds bones transformation matrices
        GLuint skin_transforms_buf;

        glGenBuffers(1, &skin_transforms_buf);
        glBindBuffer(GL_TEXTURE_BUFFER, skin_transforms_buf);
        glBufferData(GL_TEXTURE_BUFFER, FrameSyncWindow * SkinTransformsBufChunkSize,
                     nullptr, GL_DYNAMIC_COPY);
        glBindBuffer(GL_TEXTURE_BUFFER, 0);

        skin_transforms_buf_ = (uint32_t)skin_transforms_buf;

        GLuint skin_transforms_tbo;

        glGenTextures(1, &skin_transforms_tbo);
        glBindTexture(GL_TEXTURE_BUFFER, skin_transforms_tbo);

        glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, skin_transforms_buf);
        glBindTexture(GL_TEXTURE_BUFFER, 0);

        skin_transforms_tbo_ = (uint32_t)skin_transforms_tbo;
    }

    Ren::CheckError("[InitRendererInternal]: skin transforms TBO", ctx_.log());

    { // Create buffer for lights information
        GLuint lights_buf;

        glGenBuffers(1, &lights_buf);
        glBindBuffer(GL_TEXTURE_BUFFER, lights_buf);
        glBufferData(GL_TEXTURE_BUFFER, FrameSyncWindow * LightsBufChunkSize, nullptr,
                     GL_DYNAMIC_COPY);
        glBindBuffer(GL_TEXTURE_BUFFER, 0);

        lights_buf_ = (uint32_t)lights_buf;

        for (int i = 0; i < FrameSyncWindow; i++) {
            GLuint lights_tbo;

            glGenTextures(1, &lights_tbo);
            glBindTexture(GL_TEXTURE_BUFFER, lights_tbo);

            GLuint offset = i * LightsBufChunkSize;
            assert((offset % tex_buf_offset_alignment == 0) &&
                   "Offset is not properly aligned!");
            glTexBufferRange(GL_TEXTURE_BUFFER, GL_RGBA32F, lights_buf, offset,
                             LightsBufChunkSize);
            glBindTexture(GL_TEXTURE_BUFFER, 0);

            lights_tbo_[i] = (uint32_t)lights_tbo;
        }
    }

    Ren::CheckError("[InitRendererInternal]: lights TBO", ctx_.log());

    { // Create buffer for decals
        GLuint decals_buf;

        glGenBuffers(1, &decals_buf);
        glBindBuffer(GL_TEXTURE_BUFFER, decals_buf);
        glBufferData(GL_TEXTURE_BUFFER, FrameSyncWindow * DecalsBufChunkSize, nullptr,
                     GL_DYNAMIC_COPY);
        glBindBuffer(GL_TEXTURE_BUFFER, 0);

        decals_buf_ = (uint32_t)decals_buf;

        for (int i = 0; i < FrameSyncWindow; i++) {
            GLuint decals_tbo;

            glGenTextures(1, &decals_tbo);
            glBindTexture(GL_TEXTURE_BUFFER, decals_tbo);

            const GLuint offset = i * DecalsBufChunkSize;
            assert((offset % tex_buf_offset_alignment == 0) &&
                   "Offset is not properly aligned!");
            glTexBufferRange(GL_TEXTURE_BUFFER, GL_RGBA32F, decals_buf, offset,
                             DecalsBufChunkSize);
            glBindTexture(GL_TEXTURE_BUFFER, 0);

            decals_tbo_[i] = (uint32_t)decals_tbo;
        }
    }

    Ren::CheckError("[InitRendererInternal]: decals TBO", ctx_.log());

    { // Create buffer for fructum cells
        GLuint cells_buf;

        glGenBuffers(1, &cells_buf);
        glBindBuffer(GL_TEXTURE_BUFFER, cells_buf);
        glBufferData(GL_TEXTURE_BUFFER, FrameSyncWindow * CellsBufChunkSize, nullptr,
                     GL_DYNAMIC_COPY);

        const GLuint CellsZSliceSize = sizeof(CellData) * REN_GRID_RES_X * REN_GRID_RES_Y;

        // fill with zeros
        CellData dummy[REN_GRID_RES_X * REN_GRID_RES_Y] = {};

        for (int i = 0; i < FrameSyncWindow; i++) {
            for (int j = 0; j < REN_GRID_RES_Z; j++) {
                glBufferSubData(GL_TEXTURE_BUFFER,
                                i * CellsBufChunkSize + j * CellsZSliceSize,
                                CellsZSliceSize, &dummy[0]);
            }
        }

        glBindBuffer(GL_TEXTURE_BUFFER, 0);

        cells_buf_ = (uint32_t)cells_buf;

        for (int i = 0; i < FrameSyncWindow; i++) {
            GLuint cells_tbo;

            glGenTextures(1, &cells_tbo);
            glBindTexture(GL_TEXTURE_BUFFER, cells_tbo);

            const GLuint offset = i * CellsBufChunkSize;
            assert((offset % tex_buf_offset_alignment == 0) &&
                   "Offset is not properly aligned!");
            glTexBufferRange(GL_TEXTURE_BUFFER, GL_RG32UI, cells_buf, offset,
                             CellsBufChunkSize);
            glBindTexture(GL_TEXTURE_BUFFER, 0);

            cells_tbo_[i] = (uint32_t)cells_tbo;
        }
    }

    Ren::CheckError("[InitRendererInternal]: cells TBO", ctx_.log());

    { // Create buffer for item offsets
        GLuint items_buf;

        glGenBuffers(1, &items_buf);
        glBindBuffer(GL_TEXTURE_BUFFER, items_buf);
        glBufferData(GL_TEXTURE_BUFFER, FrameSyncWindow * ItemsBufChunkSize, nullptr,
                     GL_DYNAMIC_COPY);

        // fill first entry with zeroes
        ItemData dummy = {};

        for (int i = 0; i < FrameSyncWindow; i++) {
            glBufferSubData(GL_TEXTURE_BUFFER, i * ItemsBufChunkSize, sizeof(ItemData),
                            &dummy);
        }

        glBindBuffer(GL_TEXTURE_BUFFER, 0);

        items_buf_ = (uint32_t)items_buf;

        for (int i = 0; i < FrameSyncWindow; i++) {
            GLuint items_tbo;

            glGenTextures(1, &items_tbo);
            glBindTexture(GL_TEXTURE_BUFFER, items_tbo);

            const GLuint offset = i * ItemsBufChunkSize;
            assert((offset % tex_buf_offset_alignment == 0) &&
                   "Offset is not properly aligned!");
            glTexBufferRange(GL_TEXTURE_BUFFER, GL_RG32UI, items_buf, offset,
                             ItemsBufChunkSize);
            glBindTexture(GL_TEXTURE_BUFFER, 0);

            items_tbo_[i] = (uint32_t)items_tbo;
        }
    }

    Ren::CheckError("[InitRendererInternal]: items TBO", ctx_.log());

    { // Create pbo for reading back frame brightness
        for (uint32_t &pbo : reduce_pbo_) {
            GLuint reduce_pbo;
            glGenBuffers(1, &reduce_pbo);
            glBindBuffer(GL_PIXEL_PACK_BUFFER, reduce_pbo);
            glBufferData(GL_PIXEL_PACK_BUFFER,
                         GLsizeiptr(4 * reduced_buf_.w * reduced_buf_.h * sizeof(float)),
                         nullptr, GL_DYNAMIC_READ);

            glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
            pbo = (uint32_t)reduce_pbo;
        }
    }

    Ren::CheckError("[InitRendererInternal]: reduce PBO", ctx_.log());

    {
        GLuint probe_sample_pbo;
        glGenBuffers(1, &probe_sample_pbo);
        glBindBuffer(GL_PIXEL_PACK_BUFFER, probe_sample_pbo);
        glBufferData(
            GL_PIXEL_PACK_BUFFER,
            GLsizeiptr(4 * probe_sample_buf_.w * probe_sample_buf_.h * sizeof(float)),
            nullptr, GL_DYNAMIC_READ);

        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        probe_sample_pbo_ = (uint32_t)probe_sample_pbo;
    }

    Ren::CheckError("[InitRendererInternal]: probe sample PBO", ctx_.log());

    {
        GLuint temp_tex;
        glGenTextures(1, &temp_tex);
        glBindTexture(GL_TEXTURE_2D, temp_tex);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 128, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     nullptr);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        temp_tex_ = (uint32_t)temp_tex;
        temp_tex_w_ = 256;
        temp_tex_h_ = 128;
        temp_tex_format_ = Ren::eTexFormat::RawRGBA8888;
    }

    Ren::CheckError("[InitRendererInternal]: temp texture", ctx_.log());

    {
        GLuint temp_framebuf;
        glGenFramebuffers(1, &temp_framebuf);

        temp_framebuf_ = (uint32_t)temp_framebuf;
    }

    Ren::CheckError("[InitRendererInternal]: temp framebuffer", ctx_.log());

    {                                               // Create timer queries
        for (int i = 0; i < FrameSyncWindow; i++) { // NOLINT
            glGenQueries(TimersCount, queries_[i]);

            for (int j = 0; j < TimersCount; j++) {
                glQueryCounter(queries_[i][j], GL_TIMESTAMP);
            }
        }
    }

    Ren::CheckError("[InitRendererInternal]: timer queries", ctx_.log());

    {
        Ren::BufferRef vtx_buf1 = ctx_.default_vertex_buf1(),
                       vtx_buf2 = ctx_.default_vertex_buf2(),
                       ndx_buf = ctx_.default_indices_buf();

        // Allocate temporary buffer
        temp_buf1_vtx_offset_ = vtx_buf1->Alloc(TEMP_BUF_SIZE);
        temp_buf2_vtx_offset_ = vtx_buf2->Alloc(TEMP_BUF_SIZE);
        assert(temp_buf1_vtx_offset_ == temp_buf2_vtx_offset_ && "Offsets do not match!");
        temp_buf_ndx_offset_ = ndx_buf->Alloc(TEMP_BUF_SIZE);

        // Allocate buffer for skinned vertices
        skinned_buf1_vtx_offset_ = vtx_buf1->Alloc(REN_MAX_SKIN_VERTICES_TOTAL * 16);
        skinned_buf2_vtx_offset_ = vtx_buf2->Alloc(REN_MAX_SKIN_VERTICES_TOTAL * 16);
        assert(skinned_buf1_vtx_offset_ == skinned_buf2_vtx_offset_ &&
               "Offsets do not match!");

        // Allocate skydome vertices
        skydome_vtx1_offset_ = vtx_buf1->Alloc(
            sizeof(__skydome_positions) + (16 - sizeof(__skydome_positions) % 16),
            __skydome_positions);
        skydome_vtx2_offset_ = vtx_buf2->Alloc(
            sizeof(__skydome_positions) + (16 - sizeof(__skydome_positions) % 16),
            nullptr);
        assert(skydome_vtx1_offset_ == skydome_vtx2_offset_ && "Offsets do not match!");
        skydome_ndx_offset_ =
            ndx_buf->Alloc(sizeof(__skydome_indices), __skydome_indices);

        // Allocate sphere vertices
        sphere_vtx1_offset_ = vtx_buf1->Alloc(sizeof(__sphere_positions) +
                                                  (16 - sizeof(__sphere_positions) % 16),
                                              __sphere_positions);
        sphere_vtx2_offset_ = vtx_buf2->Alloc(
            sizeof(__sphere_positions) + (16 - sizeof(__sphere_positions) % 16), nullptr);
        assert(sphere_vtx1_offset_ == sphere_vtx2_offset_ && "Offsets do not match!");
        sphere_ndx_offset_ = ndx_buf->Alloc(sizeof(__sphere_indices), __sphere_indices);

        // Allocate quad vertices
        const uint32_t mem_required =
            sizeof(fs_quad_positions) + sizeof(fs_quad_norm_uvs);
        quad_vtx1_offset_ =
            vtx_buf1->Alloc(mem_required + (16 - mem_required % 16), nullptr);
        quad_vtx2_offset_ =
            vtx_buf2->Alloc(mem_required + (16 - mem_required % 16), nullptr);
        assert(quad_vtx1_offset_ == quad_vtx2_offset_ && "Offsets do not match!");
        quad_ndx_offset_ = ndx_buf->Alloc(sizeof(fs_quad_indices), fs_quad_indices);
    }

    Ren::CheckError("[InitRendererInternal]: additional data allocation", ctx_.log());

    { // Set shadowmap compare mode
        const uint32_t shad_tex = shadow_buf_.depth_tex.GetValue();
        ren_glTextureParameteri_Comp(GL_TEXTURE_2D, shad_tex, GL_TEXTURE_COMPARE_MODE,
                                     GL_COMPARE_REF_TO_TEXTURE);
        ren_glTextureParameteri_Comp(GL_TEXTURE_2D, shad_tex, GL_TEXTURE_COMPARE_FUNC,
                                     GL_LEQUAL);
    }

    Ren::CheckError("[InitRendererInternal]: shadowmap compare mode setup", ctx_.log());
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

    if (!depth_fill_framebuf_vel_) {
        GLuint new_framebuf;
        glGenFramebuffers(1, &new_framebuf);
        depth_fill_framebuf_vel_ = (uint32_t)new_framebuf;
    }

    if (!refl_comb_framebuf_) {
        GLuint new_framebuf;
        glGenFramebuffers(1, &new_framebuf);
        refl_comb_framebuf_ = (uint32_t)new_framebuf;
    }

    if (!transparent_comb_framebuf_) {
        GLuint new_framebuf;
        glGenFramebuffers(1, &new_framebuf);
        transparent_comb_framebuf_ = (uint32_t)new_framebuf;
    }

    if (!clean_buf_color_only_) {
        GLuint new_framebuf;
        glGenFramebuffers(1, &new_framebuf);
        clean_buf_color_only_ = (uint32_t)new_framebuf;
    }

    if (!clean_buf_vel_only_) {
        GLuint new_framebuf;
        glGenFramebuffers(1, &new_framebuf);
        clean_buf_vel_only_ = (uint32_t)new_framebuf;
    }

    if (!clean_buf_transparent_) {
        GLuint new_framebuf;
        glGenFramebuffers(1, &new_framebuf);
        clean_buf_transparent_ = (uint32_t)new_framebuf;
    }

    if (!temporal_resolve_framebuf_) {
        GLuint new_framebuf;
        glGenFramebuffers(1, &new_framebuf);
        temporal_resolve_framebuf_ = (uint32_t)new_framebuf;
    }

    GLint framebuf_before;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuf_before);

    bool result = true;

    { // Attach textures from clean framebuffer to skydome framebuffer (only color,
      // specular and depth are drawn)
        glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)skydome_framebuf_);

        const auto col_tex = (GLuint)clean_buf_.attachments[0].tex;
        if (clean_buf_.sample_count > 1) {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                   GL_TEXTURE_2D_MULTISAMPLE, col_tex, 0);
        } else {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                                   col_tex, 0);
        }

        const auto spec_tex = (GLuint)clean_buf_.attachments[2].tex;
        if (clean_buf_.sample_count > 1) {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2,
                                   GL_TEXTURE_2D_MULTISAMPLE, spec_tex, 0);
        } else {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D,
                                   spec_tex, 0);
        }

        const auto depth_tex = (GLuint)clean_buf_.depth_tex.GetValue();
        if (clean_buf_.sample_count > 1) {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                                   GL_TEXTURE_2D_MULTISAMPLE, depth_tex, 0);
        } else {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                                   GL_TEXTURE_2D, depth_tex, 0);
        }

        const GLenum bufs[] = {GL_COLOR_ATTACHMENT0, GL_NONE, GL_COLOR_ATTACHMENT2};
        glDrawBuffers(3, bufs);

        const GLenum s = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        result &= (s == GL_FRAMEBUFFER_COMPLETE);
    }

    { // Attach textures from clean framebuffer to depth-fill framebuffer (only depth is
      // drawn)
        glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)depth_fill_framebuf_);

        const auto depth_tex = (GLuint)clean_buf_.depth_tex.GetValue();
        if (clean_buf_.sample_count > 1) {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                                   GL_TEXTURE_2D_MULTISAMPLE, depth_tex, 0);
        } else {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                                   GL_TEXTURE_2D, depth_tex, 0);
        }

        const GLenum bufs[] = {GL_NONE};
        glDrawBuffers(1, bufs);

        const GLenum s = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        result &= (s == GL_FRAMEBUFFER_COMPLETE);
    }

    if (clean_buf_.attachments_count > 3) {
        // Attach textures from clean framebuffer to depth-fill framebuffer (only depth
        // and velocity are drawn)
        glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)depth_fill_framebuf_vel_);

        const auto depth_tex = (GLuint)clean_buf_.depth_tex.GetValue();
        const auto vel_tex = (GLuint)clean_buf_.attachments[3].tex;
        if (clean_buf_.sample_count > 1) {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                   GL_TEXTURE_2D_MULTISAMPLE, vel_tex, 0);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                                   GL_TEXTURE_2D_MULTISAMPLE, depth_tex, 0);
        } else {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                                   vel_tex, 0);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                                   GL_TEXTURE_2D, depth_tex, 0);
        }

        const GLenum bufs[] = {GL_COLOR_ATTACHMENT0};
        glDrawBuffers(1, bufs);

        const GLenum s = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        result &= (s == GL_FRAMEBUFFER_COMPLETE);
    }

    { // Attach textures from clean framebuffer to refl comb framebuffer (only color is
      // drawn)
        glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)refl_comb_framebuf_);

        const auto col_tex = (GLuint)clean_buf_.attachments[0].tex;
        if (clean_buf_.sample_count > 1) {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                   GL_TEXTURE_2D_MULTISAMPLE, col_tex, 0);
        } else {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                                   col_tex, 0);
        }

        const GLenum bufs[] = {GL_COLOR_ATTACHMENT0};
        glDrawBuffers(1, bufs);

        const GLenum s = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        result &= (s == GL_FRAMEBUFFER_COMPLETE);
    }

#if (REN_OIT_MODE == REN_OIT_MOMENT_BASED)
    { // Attach depth from clean buffer to moments buffer
        glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)moments_buf_.fb);

        const auto depth_tex = (GLuint)clean_buf_.depth_tex.GetValue();
        if (clean_buf_.sample_count > 1) {
            assert(moments_buf_.sample_count == clean_buf_.sample_count);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                                   GL_TEXTURE_2D_MULTISAMPLE, depth_tex, 0);
        } else {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                                   GL_TEXTURE_2D, depth_tex, 0);
        }

        const GLenum s = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        result &= (s == GL_FRAMEBUFFER_COMPLETE);
    }
    if (clean_buf_.sample_count == 1) {
        // Attach depth from clean buffer to transparent buffer
        glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)resolved_or_transparent_buf_.fb);

        const auto depth_tex = (GLuint)clean_buf_.depth_tex.GetValue();
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D,
                               depth_tex, 0);

        const GLenum s = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        result &= (s == GL_FRAMEBUFFER_COMPLETE);
    }
#endif

    { // Attach color from clean buffer to transparent comb buffer
        glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)transparent_comb_framebuf_);

        const auto col_tex = (GLuint)clean_buf_.attachments[REN_OUT_COLOR_INDEX].tex;
        if (clean_buf_.sample_count > 1) {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                   GL_TEXTURE_2D_MULTISAMPLE, col_tex, 0);
        } else {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                                   col_tex, 0);
        }

        const GLenum bufs[] = {GL_COLOR_ATTACHMENT0};
        glDrawBuffers(1, bufs);

        const GLenum s = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        result &= (s == GL_FRAMEBUFFER_COMPLETE);
    }

    { // Attach color and depth from clean buffer
        glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)clean_buf_color_only_);

        const auto col_tex = (GLuint)clean_buf_.attachments[REN_OUT_COLOR_INDEX].tex,
                   depth_tex = (GLuint)clean_buf_.depth_tex.GetValue();
        if (clean_buf_.sample_count > 1) {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                   GL_TEXTURE_2D_MULTISAMPLE, col_tex, 0);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                                   GL_TEXTURE_2D_MULTISAMPLE, depth_tex, 0);
        } else {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                                   col_tex, 0);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                                   GL_TEXTURE_2D, depth_tex, 0);
        }

        const GLenum bufs[] = {GL_COLOR_ATTACHMENT0};
        glDrawBuffers(1, bufs);

        const GLenum s = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        result &= (s == GL_FRAMEBUFFER_COMPLETE);
    }

    if (clean_buf_.attachments_count > 3) {
        // Attach velocity and stencil from clean buffer
        glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)clean_buf_vel_only_);

        const auto vel_tex = (GLuint)clean_buf_.attachments[3].tex,
                   depth_tex = (GLuint)clean_buf_.depth_tex.GetValue();
        if (clean_buf_.sample_count > 1) {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                   GL_TEXTURE_2D_MULTISAMPLE, vel_tex, 0);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
                                   GL_TEXTURE_2D_MULTISAMPLE, depth_tex, 0);
        } else {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                                   vel_tex, 0);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D,
                                   depth_tex, 0);
        }

        const GLenum bufs[] = {GL_COLOR_ATTACHMENT0};
        glDrawBuffers(1, bufs);

        const GLenum s = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        result &= (s == GL_FRAMEBUFFER_COMPLETE);
    }

    { // Attach accum and revealage textures from clean buffer
        glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)clean_buf_transparent_);

        const auto col_tex = (GLuint)clean_buf_.attachments[REN_OUT_COLOR_INDEX].tex,
                   norm_tex = (GLuint)clean_buf_.attachments[REN_OUT_NORM_INDEX].tex,
                   depth_tex = (GLuint)clean_buf_.depth_tex.GetValue();

        if (clean_buf_.sample_count > 1) {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                   GL_TEXTURE_2D_MULTISAMPLE, col_tex, 0);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1,
                                   GL_TEXTURE_2D_MULTISAMPLE, norm_tex, 0);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                                   GL_TEXTURE_2D_MULTISAMPLE, depth_tex, 0);
        } else {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                                   col_tex, 0);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D,
                                   norm_tex, 0);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                                   GL_TEXTURE_2D, depth_tex, 0);
        }

        const GLenum bufs[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
        glDrawBuffers(2, bufs);

        const GLenum s = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        result &= (s == GL_FRAMEBUFFER_COMPLETE);
    }

    if (history_buf_.attachments_count) {
        // Attach color from resolved and history buffers
        glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)temporal_resolve_framebuf_);

        const auto col_tex1 = (GLuint)resolved_or_transparent_buf_.attachments[0].tex,
                   col_tex2 = (GLuint)history_buf_.attachments[0].tex;

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                               col_tex1, 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D,
                               col_tex2, 0);

        const GLenum bufs[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
        glDrawBuffers(2, bufs);

        const GLenum s = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        result &= (s == GL_FRAMEBUFFER_COMPLETE);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)framebuf_before);
    return result;
}

void Renderer::CheckInitVAOs() {
    using namespace RendererInternal;

    Ren::BufferRef vtx_buf1 = ctx_.default_vertex_buf1(),
                   vtx_buf2 = ctx_.default_vertex_buf2(),
                   ndx_buf = ctx_.default_indices_buf();

    auto gl_vertex_buf1 = (GLuint)vtx_buf1->buf_id(),
         gl_vertex_buf2 = (GLuint)vtx_buf2->buf_id(),
         gl_indices_buf = (GLuint)ndx_buf->buf_id();

    if (gl_vertex_buf1 != last_vertex_buf1_ || gl_vertex_buf2 != last_vertex_buf2_ ||
        gl_indices_buf != last_index_buffer_) {
        // buffers were reallocated, recreate vertex arrays

        if (last_vertex_buf1_) {
            auto sphere_vao = (GLuint)sphere_vao_;
            glDeleteVertexArrays(1, &sphere_vao);

            auto skydome_vao = (GLuint)skydome_vao_;
            glDeleteVertexArrays(1, &skydome_vao);

            auto temp_vao = (GLuint)temp_vao_;
            glDeleteVertexArrays(1, &temp_vao);

            auto depth_pass_solid_vao = (GLuint)depth_pass_solid_vao_;
            glDeleteVertexArrays(1, &depth_pass_solid_vao);

            auto depth_pass_vege_solid_vao = (GLuint)depth_pass_vege_solid_vao_;
            glDeleteVertexArrays(1, &depth_pass_vege_solid_vao);

            auto depth_pass_transp_vao = (GLuint)depth_pass_transp_vao_;
            glDeleteVertexArrays(1, &depth_pass_transp_vao);

            auto depth_pass_vege_transp_vao = (GLuint)depth_pass_vege_transp_vao_;
            glDeleteVertexArrays(1, &depth_pass_vege_transp_vao);
        }

        const int buf1_stride = 16, buf2_stride = 16;

        { // VAO for shadow and depth-fill passes (solid, uses position attribute only)
            GLuint depth_pass_solid_vao;
            glGenVertexArrays(1, &depth_pass_solid_vao);
            glBindVertexArray(depth_pass_solid_vao);

            glBindBuffer(GL_ARRAY_BUFFER, gl_vertex_buf1);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gl_indices_buf);

            glEnableVertexAttribArray(REN_VTX_POS_LOC);
            glVertexAttribPointer(REN_VTX_POS_LOC, 3, GL_FLOAT, GL_FALSE, buf1_stride,
                                  (void *)0); // NOLINT

            glBindVertexArray(0);
            depth_pass_solid_vao_ = (uint32_t)depth_pass_solid_vao;
        }

        { // VAO for shadow and depth-fill passes of vegetation (solid, uses position and
          // color attributes only)
            GLuint depth_pass_vege_solid_vao;
            glGenVertexArrays(1, &depth_pass_vege_solid_vao);
            glBindVertexArray(depth_pass_vege_solid_vao);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gl_indices_buf);

            { // Setup attributes from buffer 1
                glBindBuffer(GL_ARRAY_BUFFER, gl_vertex_buf1);

                glEnableVertexAttribArray(REN_VTX_POS_LOC);
                glVertexAttribPointer(REN_VTX_POS_LOC, 3, GL_FLOAT, GL_FALSE, buf1_stride,
                                      (void *)0); // NOLINT
            }

            { // Setup attributes from buffer 2
                glBindBuffer(GL_ARRAY_BUFFER, gl_vertex_buf2);

                glEnableVertexAttribArray(REN_VTX_AUX_LOC);
                glVertexAttribIPointer(REN_VTX_AUX_LOC, 1, GL_UNSIGNED_INT, buf2_stride,
                                       (void *)(6 * sizeof(uint16_t)));
            }

            glBindVertexArray(0);
            depth_pass_vege_solid_vao_ = (uint32_t)depth_pass_vege_solid_vao;
        }

        { // VAO for shadow and depth-fill passes (alpha-tested, uses position and uv
          // attributes)
            GLuint depth_pass_transp_vao;
            glGenVertexArrays(1, &depth_pass_transp_vao);
            glBindVertexArray(depth_pass_transp_vao);

            glBindBuffer(GL_ARRAY_BUFFER, gl_vertex_buf1);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gl_indices_buf);

            glEnableVertexAttribArray(REN_VTX_POS_LOC);
            glVertexAttribPointer(REN_VTX_POS_LOC, 3, GL_FLOAT, GL_FALSE, buf1_stride,
                                  (void *)0); // NOLINT

            glEnableVertexAttribArray(REN_VTX_UV1_LOC);
            glVertexAttribPointer(REN_VTX_UV1_LOC, 2, GL_HALF_FLOAT, GL_FALSE,
                                  buf1_stride, (void *)(3 * sizeof(float)));

            glBindVertexArray(0);
            depth_pass_transp_vao_ = (uint32_t)depth_pass_transp_vao;
        }

        { // VAO for shadow and depth-fill passes of vegetation (solid, uses position and
          // color attributes only)
            GLuint depth_pass_vege_transp_vao;
            glGenVertexArrays(1, &depth_pass_vege_transp_vao);
            glBindVertexArray(depth_pass_vege_transp_vao);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gl_indices_buf);

            { // Setup attributes from buffer 1
                glBindBuffer(GL_ARRAY_BUFFER, gl_vertex_buf1);

                glEnableVertexAttribArray(REN_VTX_POS_LOC);
                glVertexAttribPointer(REN_VTX_POS_LOC, 3, GL_FLOAT, GL_FALSE, buf1_stride,
                                      (void *)0); // NOLINT

                glEnableVertexAttribArray(REN_VTX_UV1_LOC);
                glVertexAttribPointer(REN_VTX_UV1_LOC, 2, GL_HALF_FLOAT, GL_FALSE,
                                      buf1_stride, (void *)(3 * sizeof(float)));
            }

            { // Setup attributes from buffer 2
                glBindBuffer(GL_ARRAY_BUFFER, gl_vertex_buf2);

                glEnableVertexAttribArray(REN_VTX_AUX_LOC);
                glVertexAttribIPointer(REN_VTX_AUX_LOC, 1, GL_UNSIGNED_INT, buf2_stride,
                                       (void *)(6 * sizeof(uint16_t)));
            }

            glBindVertexArray(0);
            depth_pass_vege_transp_vao_ = (uint32_t)depth_pass_vege_transp_vao;
        }

        { // VAO for main drawing (uses all attributes)
            GLuint draw_pass_vao;
            glGenVertexArrays(1, &draw_pass_vao);
            glBindVertexArray(draw_pass_vao);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gl_indices_buf);

            { // Setup attributes from buffer 1
                glBindBuffer(GL_ARRAY_BUFFER, gl_vertex_buf1);

                glEnableVertexAttribArray(REN_VTX_POS_LOC);
                glVertexAttribPointer(REN_VTX_POS_LOC, 3, GL_FLOAT, GL_FALSE, buf1_stride,
                                      (void *)0); // NOLINT

                glEnableVertexAttribArray(REN_VTX_UV1_LOC);
                glVertexAttribPointer(REN_VTX_UV1_LOC, 2, GL_HALF_FLOAT, GL_FALSE,
                                      buf1_stride, (void *)(3 * sizeof(float)));
            }

            { // Setup attributes from buffer 2
                glBindBuffer(GL_ARRAY_BUFFER, gl_vertex_buf2);

                glEnableVertexAttribArray(REN_VTX_NOR_LOC);
                glVertexAttribPointer(REN_VTX_NOR_LOC, 4, GL_SHORT, GL_TRUE, buf2_stride,
                                      (void *)0); // NOLINT

                glEnableVertexAttribArray(REN_VTX_TAN_LOC);
                glVertexAttribPointer(REN_VTX_TAN_LOC, 2, GL_SHORT, GL_TRUE, buf2_stride,
                                      (void *)(4 * sizeof(uint16_t)));

                glEnableVertexAttribArray(REN_VTX_AUX_LOC);
                glVertexAttribIPointer(REN_VTX_AUX_LOC, 1, GL_UNSIGNED_INT, buf2_stride,
                                       (void *)(6 * sizeof(uint16_t)));
            }

            glBindVertexArray(0);
            draw_pass_vao_ = (uint32_t)draw_pass_vao;
        }

        { // Create vao for temporary buffer
            GLuint temp_vao;
            glGenVertexArrays(1, &temp_vao);

            temp_vao_ = (uint32_t)temp_vao;
        }

        { // Allocate vao for quad vertices
            GLuint fs_quad_vao;
            glGenVertexArrays(1, &fs_quad_vao);
            glBindVertexArray(fs_quad_vao);

            glBindBuffer(GL_ARRAY_BUFFER, gl_vertex_buf1);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gl_indices_buf);

            glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)quad_vtx1_offset_,
                            sizeof(fs_quad_positions), fs_quad_positions);
            glBufferSubData(GL_ARRAY_BUFFER,
                            (GLintptr)(quad_vtx1_offset_ + sizeof(fs_quad_positions)),
                            sizeof(fs_quad_norm_uvs), fs_quad_norm_uvs);
            glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, (GLintptr)quad_ndx_offset_,
                            sizeof(fs_quad_indices), fs_quad_indices);

            glEnableVertexAttribArray(REN_VTX_POS_LOC);
            glVertexAttribPointer(REN_VTX_POS_LOC, 2, GL_FLOAT, GL_FALSE, 0,
                                  (const GLvoid *)uintptr_t(quad_vtx1_offset_));

            glEnableVertexAttribArray(REN_VTX_UV1_LOC);
            glVertexAttribPointer(
                REN_VTX_UV1_LOC, 2, GL_FLOAT, GL_FALSE, 0,
                (const GLvoid *)uintptr_t(quad_vtx1_offset_ + sizeof(fs_quad_positions)));

            glBindVertexArray(0);
            fs_quad_vao_ = (uint32_t)fs_quad_vao;
        }

        { // Allocate vao for skydome vertices
            GLuint skydome_vao;
            glGenVertexArrays(1, &skydome_vao);
            glBindVertexArray(skydome_vao);

            glBindBuffer(GL_ARRAY_BUFFER, gl_vertex_buf1);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gl_indices_buf);

            glEnableVertexAttribArray(REN_VTX_POS_LOC);
            glVertexAttribPointer(REN_VTX_POS_LOC, 3, GL_FLOAT, GL_FALSE, 0,
                                  (void *)uintptr_t(skydome_vtx1_offset_));

            glBindVertexArray(0);
            skydome_vao_ = (uint32_t)skydome_vao;
        }

        { // Allocate vao for sphere vertices
            GLuint sphere_vao;
            glGenVertexArrays(1, &sphere_vao);
            glBindVertexArray(sphere_vao);

            glBindBuffer(GL_ARRAY_BUFFER, gl_vertex_buf1);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gl_indices_buf);

            glEnableVertexAttribArray(REN_VTX_POS_LOC);
            glVertexAttribPointer(REN_VTX_POS_LOC, 3, GL_FLOAT, GL_FALSE, 0,
                                  (void *)uintptr_t(sphere_vtx1_offset_));

            glBindVertexArray(0);
            sphere_vao_ = (uint32_t)sphere_vao;
        }

        last_vertex_buf1_ = (uint32_t)gl_vertex_buf1;
        last_vertex_buf2_ = (uint32_t)gl_vertex_buf2;
        last_index_buffer_ = (uint32_t)gl_indices_buf;
    }
}

void Renderer::DestroyRendererInternal() {
    Ren::ILog *log = ctx_.log();

    log->Info("DestroyRendererInternal");

    Ren::BufferRef vtx_buf1 = ctx_.default_vertex_buf1(),
                   vtx_buf2 = ctx_.default_vertex_buf2(),
                   ndx_buf = ctx_.default_indices_buf();

    for (uint32_t ubo : unif_shared_data_block_) {
        auto _ubo = (GLuint)ubo;
        glDeleteBuffers(1, &_ubo);
    }

    static_assert(sizeof(GLuint) == sizeof(uint32_t), "!");

    {
        glDeleteTextures(FrameSyncWindow, instances_tbo_);

        auto instances_buf = (GLuint)instances_buf_;
        glDeleteBuffers(1, &instances_buf);
    }

    {
        auto skin_transforms_tbo = (GLuint)skin_transforms_tbo_;
        glDeleteTextures(1, &skin_transforms_tbo);

        auto skin_transforms_buf = (GLuint)skin_transforms_buf_;
        glDeleteBuffers(1, &skin_transforms_buf);
    }

    {
        auto skin_regions_tbo = (GLuint)skin_regions_tbo_;
        glDeleteTextures(1, &skin_regions_tbo);

        auto skin_regions_buf = (GLuint)skin_regions_buf_;
        glDeleteBuffers(1, &skin_regions_buf);
    }

    {
        glDeleteTextures(FrameSyncWindow, lights_tbo_);

        auto lights_buf = (GLuint)lights_buf_;
        glDeleteBuffers(1, &lights_buf);
    }

    {
        glDeleteTextures(FrameSyncWindow, decals_tbo_);

        auto lights_buf = (GLuint)lights_buf_;
        glDeleteBuffers(1, &lights_buf);
    }

    {
        glDeleteTextures(FrameSyncWindow, cells_tbo_);

        auto cells_buf = (GLuint)cells_buf_;
        glDeleteBuffers(1, &cells_buf);
    }

    {
        glDeleteTextures(FrameSyncWindow, items_tbo_);

        auto items_buf = (GLuint)items_buf_;
        glDeleteBuffers(1, &items_buf);
    }

    {
        auto temp_tex = (GLuint)temp_tex_;
        glDeleteTextures(1, &temp_tex);
    }

    {
        auto temp_framebuf = (GLuint)temp_framebuf_;
        glDeleteFramebuffers(1, &temp_framebuf);
    }

    if (nodes_buf_) {
        auto nodes_tbo = (GLuint)nodes_tbo_;
        glDeleteTextures(1, &nodes_tbo);

        auto nodes_buf = (GLuint)nodes_buf_;
        glDeleteBuffers(1, &nodes_buf);
    }

    for (uint32_t pbo : reduce_pbo_) {
        auto _pbo = (GLuint)pbo;
        glDeleteBuffers(1, &_pbo);
    }

    {
        auto probe_sample_pbo = (GLuint)probe_sample_pbo_;
        glDeleteBuffers(1, &probe_sample_pbo);
    }

    if (skydome_framebuf_) {
        auto framebuf = (GLuint)skydome_framebuf_;
        glDeleteFramebuffers(1, &framebuf);
    }

    if (depth_fill_framebuf_) {
        auto framebuf = (GLuint)depth_fill_framebuf_;
        glDeleteFramebuffers(1, &framebuf);
    }

    if (depth_fill_framebuf_vel_) {
        auto framebuf = (GLuint)depth_fill_framebuf_vel_;
        glDeleteFramebuffers(1, &framebuf);
    }

    if (refl_comb_framebuf_) {
        auto framebuf = (GLuint)refl_comb_framebuf_;
        glDeleteFramebuffers(1, &framebuf);
    }

    if (transparent_comb_framebuf_) {
        auto framebuf = (GLuint)transparent_comb_framebuf_;
        glDeleteFramebuffers(1, &framebuf);
    }

    if (clean_buf_color_only_) {
        auto framebuf = (GLuint)clean_buf_color_only_;
        glDeleteFramebuffers(1, &framebuf);
    }

    if (clean_buf_transparent_) {
        auto framebuf = (GLuint)clean_buf_transparent_;
        glDeleteFramebuffers(1, &framebuf);
    }

    if (temporal_resolve_framebuf_) {
        auto framebuf = (GLuint)temporal_resolve_framebuf_;
        glDeleteFramebuffers(1, &framebuf);
    }

    {
        assert(vtx_buf1->Free(sphere_vtx1_offset_));
        assert(vtx_buf2->Free(sphere_vtx2_offset_));
        assert(ndx_buf->Free(sphere_ndx_offset_));

        assert(vtx_buf1->Free(skydome_vtx1_offset_));
        assert(vtx_buf2->Free(skydome_vtx2_offset_));
        assert(ndx_buf->Free(skydome_ndx_offset_));

        assert(vtx_buf1->Free(quad_vtx1_offset_));
        assert(vtx_buf2->Free(quad_vtx2_offset_));
        assert(ndx_buf->Free(quad_ndx_offset_));

        assert(vtx_buf1->Free(temp_buf1_vtx_offset_));
        assert(vtx_buf2->Free(temp_buf2_vtx_offset_));
        assert(ndx_buf->Free(temp_buf_ndx_offset_));

        auto sphere_vao = (GLuint)sphere_vao_;
        glDeleteVertexArrays(1, &sphere_vao);

        auto skydome_vao = (GLuint)skydome_vao_;
        glDeleteVertexArrays(1, &skydome_vao);

        auto temp_vao = (GLuint)temp_vao_;
        glDeleteVertexArrays(1, &temp_vao);

        auto fs_quad_vao = (GLuint)fs_quad_vao_;
        glDeleteVertexArrays(1, &fs_quad_vao);

        auto depth_pass_solid_vao = (GLuint)depth_pass_solid_vao_;
        glDeleteVertexArrays(1, &depth_pass_solid_vao);

        auto depth_pass_vege_solid_vao = (GLuint)depth_pass_vege_solid_vao_;
        glDeleteVertexArrays(1, &depth_pass_vege_solid_vao);

        auto depth_pass_transp_vao = (GLuint)depth_pass_transp_vao_;
        glDeleteVertexArrays(1, &depth_pass_transp_vao);

        auto depth_pass_vege_transp_vao = (GLuint)depth_pass_vege_transp_vao_;
        glDeleteVertexArrays(1, &depth_pass_vege_transp_vao);
    }

    for (int i = 0; i < FrameSyncWindow; i++) {
        static_assert(sizeof(queries_[0][0]) == sizeof(GLuint), "!");
        glDeleteQueries(TimersCount, queries_[i]);

        if (buf_range_fences_[i]) {
            auto sync = reinterpret_cast<GLsync>(buf_range_fences_[i]);
            glDeleteSync(sync);
            buf_range_fences_[i] = nullptr;
        }
    }
}

void Renderer::DrawObjectsInternal(const DrawList &list, const FrameBuf *target) {
    using namespace Ren;
    using namespace RendererInternal;

    Ren::ILog *log = ctx_.log();

    if (list.render_flags & EnableTimers) {
        glQueryCounter(queries_[cur_query_][TimeDrawStart], GL_TIMESTAMP);
    }

    CheckInitVAOs();

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);

    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);

    assert(list.instances.count < REN_MAX_INSTANCES_TOTAL);
    assert(list.skin_transforms.count < REN_MAX_SKIN_XFORMS_TOTAL);
    assert(list.skin_regions.count < REN_MAX_SKIN_REGIONS_TOTAL);
    assert(list.skin_vertices_count < REN_MAX_SKIN_VERTICES_TOTAL);
    assert(list.light_sources.count < REN_MAX_LIGHTS_TOTAL);
    assert(list.decals.count < REN_MAX_DECALS_TOTAL);
    assert(list.probes.count < REN_MAX_PROBES_TOTAL);
    assert(list.ellipsoids.count < REN_MAX_ELLIPSES_TOTAL);
    assert(list.items.count < REN_MAX_ITEMS_TOTAL);

    backend_info_.shadow_draw_calls_count = 0;
    backend_info_.depth_fill_draw_calls_count = 0;
    backend_info_.opaque_draw_calls_count = 0;

    backend_info_.triangles_rendered = 0;

    { // Update buffers
        DebugMarker _("UPDATE BUFFERS");

        // TODO: try to use persistently mapped buffers

        cur_buf_chunk_ = (cur_buf_chunk_ + 1) % FrameSyncWindow;
        if (buf_range_fences_[cur_buf_chunk_]) {
            auto sync = reinterpret_cast<GLsync>(buf_range_fences_[cur_buf_chunk_]);
            GLenum res = glClientWaitSync(sync, 0, 1000000000);
            if (res != GL_ALREADY_SIGNALED && res != GL_CONDITION_SATISFIED) {
                log->Error("[Renderer::DrawObjectsInternal]: Wait failed!");
            }
            glDeleteSync(sync);
            buf_range_fences_[cur_buf_chunk_] = nullptr;
        }

        const GLbitfield BufferRangeBindFlags =
            GLbitfield(GL_MAP_WRITE_BIT) | GLbitfield(GL_MAP_INVALIDATE_RANGE_BIT) |
            GLbitfield(GL_MAP_UNSYNCHRONIZED_BIT) | GLbitfield(GL_MAP_FLUSH_EXPLICIT_BIT);

        // Update skinning buffers
        if (list.skin_transforms.count) {
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, (GLuint)skin_transforms_buf_);

            void *pinned_mem = glMapBufferRange(
                GL_SHADER_STORAGE_BUFFER, cur_buf_chunk_ * SkinTransformsBufChunkSize,
                SkinTransformsBufChunkSize, BufferRangeBindFlags);
            if (pinned_mem) {
                const size_t skin_transforms_mem_size =
                    list.skin_transforms.count * sizeof(SkinTransform);
                memcpy(pinned_mem, list.skin_transforms.data, skin_transforms_mem_size);
                glFlushMappedBufferRange(GL_SHADER_STORAGE_BUFFER, 0,
                                         skin_transforms_mem_size);
                glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
            } else {
                log->Error("[Renderer::DrawObjectsInternal]: Failed to map skin "
                           "transforms buffer!");
            }

            glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
        }

        if (list.skin_regions.count) {
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, (GLuint)skin_regions_buf_);

            void *pinned_mem = glMapBufferRange(
                GL_SHADER_STORAGE_BUFFER, cur_buf_chunk_ * SkinRegionsBufChunkSize,
                SkinRegionsBufChunkSize, BufferRangeBindFlags);
            if (pinned_mem) {
                const size_t skin_regions_mem_size =
                    list.skin_regions.count * sizeof(SkinRegion);
                memcpy(pinned_mem, list.skin_regions.data, skin_regions_mem_size);
                glFlushMappedBufferRange(GL_SHADER_STORAGE_BUFFER, 0,
                                         skin_regions_mem_size);
                glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
            } else {
                log->Error("[Renderer::DrawObjectsInternal]: Failed to map skin regions "
                           "buffer!");
            }

            glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
        }

        // Update instance buffer
        if (list.instances.count) {
            glBindBuffer(GL_TEXTURE_BUFFER, (GLuint)instances_buf_);

            void *pinned_mem = glMapBufferRange(
                GL_TEXTURE_BUFFER, cur_buf_chunk_ * InstanceDataBufChunkSize,
                InstanceDataBufChunkSize, BufferRangeBindFlags);
            if (pinned_mem) {
                const size_t instance_mem_size =
                    list.instances.count * sizeof(InstanceData);
                memcpy(pinned_mem, list.instances.data, instance_mem_size);
                glFlushMappedBufferRange(GL_TEXTURE_BUFFER, 0, instance_mem_size);
                glUnmapBuffer(GL_TEXTURE_BUFFER);
            } else {
                log->Error(
                    "[Renderer::DrawObjectsInternal]: Failed to map instance buffer!");
            }

            glBindBuffer(GL_TEXTURE_BUFFER, 0);
        }

        // Update cells buffer
        if (list.cells.count) {
            glBindBuffer(GL_TEXTURE_BUFFER, (GLuint)cells_buf_);

            void *pinned_mem =
                glMapBufferRange(GL_TEXTURE_BUFFER, cur_buf_chunk_ * CellsBufChunkSize,
                                 CellsBufChunkSize, BufferRangeBindFlags);
            if (pinned_mem) {
                const size_t cells_mem_size = list.cells.count * sizeof(CellData);
                memcpy(pinned_mem, list.cells.data, cells_mem_size);
                glFlushMappedBufferRange(GL_TEXTURE_BUFFER, 0, cells_mem_size);
                glUnmapBuffer(GL_TEXTURE_BUFFER);
            } else {
                log->Error(
                    "[Renderer::DrawObjectsInternal]: Failed to map cells buffer!");
            }

            glBindBuffer(GL_TEXTURE_BUFFER, 0);
        }

        // Update lights buffer
        if (list.light_sources.count) {
            glBindBuffer(GL_TEXTURE_BUFFER, (GLuint)lights_buf_);

            void *pinned_mem =
                glMapBufferRange(GL_TEXTURE_BUFFER, cur_buf_chunk_ * LightsBufChunkSize,
                                 LightsBufChunkSize, BufferRangeBindFlags);
            if (pinned_mem) {
                const size_t lights_mem_size =
                    list.light_sources.count * sizeof(LightSourceItem);
                memcpy(pinned_mem, list.light_sources.data, lights_mem_size);
                glFlushMappedBufferRange(GL_TEXTURE_BUFFER, 0, lights_mem_size);
                glUnmapBuffer(GL_TEXTURE_BUFFER);
            } else {
                log->Error(
                    "[Renderer::DrawObjectsInternal]: Failed to map lights buffer!");
            }

            glBindBuffer(GL_TEXTURE_BUFFER, 0);
        }

        // Update decals buffer
        if (list.decals.count) {
            glBindBuffer(GL_TEXTURE_BUFFER, (GLuint)decals_buf_);

            void *pinned_mem =
                glMapBufferRange(GL_TEXTURE_BUFFER, cur_buf_chunk_ * DecalsBufChunkSize,
                                 DecalsBufChunkSize, BufferRangeBindFlags);
            if (pinned_mem) {
                const size_t decals_mem_size = list.decals.count * sizeof(DecalItem);
                memcpy(pinned_mem, list.decals.data, decals_mem_size);
                glFlushMappedBufferRange(GL_TEXTURE_BUFFER, 0, decals_mem_size);
                glUnmapBuffer(GL_TEXTURE_BUFFER);
            } else {
                log->Error(
                    "[Renderer::DrawObjectsInternal]: Failed to map decals buffer!");
            }

            glBindBuffer(GL_TEXTURE_BUFFER, 0);
        }

        // Update items buffer
        if (list.items.count) {
            glBindBuffer(GL_TEXTURE_BUFFER, (GLuint)items_buf_);

            void *pinned_mem =
                glMapBufferRange(GL_TEXTURE_BUFFER, cur_buf_chunk_ * ItemsBufChunkSize,
                                 ItemsBufChunkSize, BufferRangeBindFlags);
            if (pinned_mem) {
                const size_t items_mem_size = list.items.count * sizeof(ItemData);
                memcpy(pinned_mem, list.items.data, items_mem_size);
                glFlushMappedBufferRange(GL_TEXTURE_BUFFER, 0, items_mem_size);
                glUnmapBuffer(GL_TEXTURE_BUFFER);
            } else {
                log->Error(
                    "[Renderer::DrawObjectsInternal]: Failed to map items buffer!");
            }

            glBindBuffer(GL_TEXTURE_BUFFER, 0);
        }
    }

    //
    // Update UBO with data that is shared between passes
    //

    SharedDataBlock shrd_data;
    Ren::Mat4f clip_from_world_unjittered;

    { // Prepare data that is shared for all instances
        shrd_data.uViewMatrix = list.draw_cam.view_matrix();
        shrd_data.uProjMatrix = list.draw_cam.proj_matrix();

        clip_from_world_unjittered = shrd_data.uProjMatrix * shrd_data.uViewMatrix;

        if ((list.render_flags & EnableTaa) != 0) {
            // apply jitter to projection (assumed always perspective)
            assert(!list.draw_cam.is_orthographic());
            Ren::Vec2f jitter =
                RendererInternal::HaltonSeq23[frame_counter_ % TaaSampleCount];
            jitter = (jitter * 2.0f - Ren::Vec2f{1.0f}) /
                     Ren::Vec2f{float(act_w_), float(act_h_)};

            shrd_data.uTaaInfo[0] = jitter[0];
            shrd_data.uTaaInfo[1] = jitter[1];

            shrd_data.uProjMatrix[2][0] += jitter[0];
            shrd_data.uProjMatrix[2][1] += jitter[1];
        }

        shrd_data.uViewProjMatrix = shrd_data.uProjMatrix * shrd_data.uViewMatrix;
        shrd_data.uViewProjPrevMatrix = prev_clip_from_world_;
        shrd_data.uInvViewMatrix = Ren::Inverse(shrd_data.uViewMatrix);
        shrd_data.uInvProjMatrix = Ren::Inverse(shrd_data.uProjMatrix);
        shrd_data.uInvViewProjMatrix = Ren::Inverse(shrd_data.uViewProjMatrix);
        // delta matrix between current and previous frame
        shrd_data.uDeltaMatrix =
            prev_clip_from_view_ * (down_buf_view_from_world_ * shrd_data.uInvViewMatrix);

        if (list.shadow_regions.count) {
            assert(list.shadow_regions.count <= REN_MAX_SHADOWMAPS_TOTAL);
            memcpy(&shrd_data.uShadowMapRegions[0], &list.shadow_regions.data[0],
                   sizeof(ShadowMapRegion) * list.shadow_regions.count);
        }

        if (list.render_flags & EnableLights) {
            shrd_data.uSunDir = Ren::Vec4f{list.env.sun_dir[0], list.env.sun_dir[1],
                                           list.env.sun_dir[2], 0.0f};
            shrd_data.uSunCol = Ren::Vec4f{list.env.sun_col[0], list.env.sun_col[1],
                                           list.env.sun_col[2], 0.0f};
        } else {
            shrd_data.uSunDir = {};
            shrd_data.uSunCol = {};
        }

        // actual resolution and full resolution
        shrd_data.uResAndFRes = Ren::Vec4f{float(act_w_), float(act_h_),
                                           float(clean_buf_.w), float(clean_buf_.h)};

        const float near = list.draw_cam.near(), far = list.draw_cam.far();
        const float time_s = 0.001f * Sys::GetTimeMs();
        const float transparent_near = near;
        const float transparent_far = 16.0f;
        const int transparent_mode =
#if (REN_OIT_MODE == REN_OIT_MOMENT_BASED)
            (list.render_flags & EnableOIT) ? 2 : 0;
#elif (REN_OIT_MODE == REN_OIT_WEIGHTED_BLENDED)
            (list.render_flags & EnableOIT) ? 1 : 0;
#else
            0;
#endif

        shrd_data.uTranspParamsAndTime =
            Ren::Vec4f{std::log(transparent_near),
                       std::log(transparent_far) - std::log(transparent_near),
                       float(transparent_mode), time_s};
        shrd_data.uClipInfo =
            Ren::Vec4f{near * far, near, far, std::log2(1.0f + far / near)};

        const Ren::Vec3f &pos = list.draw_cam.world_position();
        shrd_data.uCamPosAndGamma = Ren::Vec4f{pos[0], pos[1], pos[2], 2.2f};
        shrd_data.uWindScroll =
            Ren::Vec4f{list.env.wind_scroll_lf[0], list.env.wind_scroll_lf[1],
                       list.env.wind_scroll_hf[0], list.env.wind_scroll_hf[1]};
        shrd_data.uWindScrollPrev = prev_wind_scroll_;
        prev_wind_scroll_ = shrd_data.uWindScroll;

        memcpy(&shrd_data.uProbes[0], list.probes.data,
               sizeof(ProbeItem) * list.probes.count);
        memcpy(&shrd_data.uEllipsoids[0], list.ellipsoids.data,
               sizeof(EllipsItem) * list.ellipsoids.count);

        glBindBuffer(GL_UNIFORM_BUFFER, (GLuint)unif_shared_data_block_[cur_buf_chunk_]);

        const GLbitfield BufferRangeBindFlags =
            GLbitfield(GL_MAP_WRITE_BIT) | GLbitfield(GL_MAP_INVALIDATE_RANGE_BIT) |
            GLbitfield(GL_MAP_UNSYNCHRONIZED_BIT) | GLbitfield(GL_MAP_FLUSH_EXPLICIT_BIT);

        void *pinned_mem = glMapBufferRange(GL_UNIFORM_BUFFER, 0, sizeof(SharedDataBlock),
                                            BufferRangeBindFlags);
        if (pinned_mem) {
            memcpy(pinned_mem, &shrd_data, sizeof(SharedDataBlock));
            glFlushMappedBufferRange(GL_UNIFORM_BUFFER, 0, sizeof(SharedDataBlock));
            glUnmapBuffer(GL_UNIFORM_BUFFER);
        }

        glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }

    glBindBufferBase(GL_UNIFORM_BUFFER, REN_UB_SHARED_DATA_LOC,
                     (GLuint)unif_shared_data_block_[cur_buf_chunk_]);

    //
    // Update vertex buffer for skinned meshes
    //

    if (list.render_flags & EnableTimers) {
        glQueryCounter(queries_[cur_query_][TimeSkinningStart], GL_TIMESTAMP);
    }

    if (list.skin_regions.count) {
        DebugMarker _("SKINNING");

        const Ren::Program *p = skinning_prog_.get();

        glUseProgram(p->prog_id());
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0,
                         (GLuint)ctx_.default_skin_vertex_buf()->buf_id());
        glBindBufferRange(GL_SHADER_STORAGE_BUFFER, 1, (GLuint)skin_transforms_buf_,
                          cur_buf_chunk_ * SkinTransformsBufChunkSize,
                          SkinTransformsBufChunkSize);
        glBindBufferRange(GL_SHADER_STORAGE_BUFFER, 2, (GLuint)skin_regions_buf_,
                          cur_buf_chunk_ * SkinRegionsBufChunkSize,
                          SkinRegionsBufChunkSize);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3,
                         (GLuint)ctx_.default_vertex_buf1()->buf_id());
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4,
                         (GLuint)ctx_.default_vertex_buf2()->buf_id());

        glDispatchCompute(list.skin_regions.count, 1, 1);
        glMemoryBarrier(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);
    }

    //
    // Update shadow maps
    //

    if (list.render_flags & EnableTimers) {
        glQueryCounter(queries_[cur_query_][TimeShadowMapStart], GL_TIMESTAMP);
    }

    ren_glBindTextureUnit_Comp(GL_TEXTURE_BUFFER, REN_INST_BUF_SLOT,
                               instances_tbo_[cur_buf_chunk_]);

    { // draw shadow map
        glBindFramebuffer(GL_FRAMEBUFFER, shadow_buf_.fb);

        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(4.0f, 8.0f);
        glEnable(GL_SCISSOR_TEST);

        DebugMarker _("UPDATE SHADOW MAPS");

        bool region_cleared[REN_MAX_SHADOWMAPS_TOTAL] = {};

        // draw opaque objects
        glBindVertexArray(depth_pass_solid_vao_);
        glUseProgram(shadow_solid_prog_->prog_id());

        for (int i = 0; i < (int)list.shadow_lists.count; i++) {
            const ShadowList &sh_list = list.shadow_lists.data[i];
            if (!sh_list.shadow_batch_count)
                continue;

            glViewport(sh_list.shadow_map_pos[0], sh_list.shadow_map_pos[1],
                       sh_list.shadow_map_size[0], sh_list.shadow_map_size[1]);
            glScissor(sh_list.scissor_test_pos[0], sh_list.scissor_test_pos[1],
                      sh_list.scissor_test_size[0], sh_list.scissor_test_size[1]);

            { // clear buffer region
                glClear(GL_DEPTH_BUFFER_BIT);
                region_cleared[i] = true;
            }

            glUniformMatrix4fv(
                REN_U_M_MATRIX_LOC, 1, GL_FALSE,
                Ren::ValuePtr(list.shadow_regions.data[i].clip_from_world));

            for (uint32_t j = sh_list.shadow_batch_start;
                 j < sh_list.shadow_batch_start + sh_list.shadow_batch_count; j++) {
                const DepthDrawBatch &batch =
                    list.shadow_batches.data[list.shadow_batch_indices.data[j]];
                if (!batch.instance_count || batch.alpha_test_bit || batch.vegetation_bit)
                    continue;

                glUniform4iv(REN_U_INSTANCES_LOC, (batch.instance_count + 3) / 4,
                             &batch.instance_indices[0]);

                glDrawElementsInstancedBaseVertex(
                    GL_TRIANGLES, batch.indices_count, GL_UNSIGNED_INT,
                    (const GLvoid *)uintptr_t(batch.indices_offset),
                    (GLsizei)batch.instance_count, (GLint)batch.base_vertex);
                backend_info_.shadow_draw_calls_count++;
            }
        }

        // draw opaque vegetation
        glBindVertexArray(depth_pass_vege_solid_vao_);
        glUseProgram(shadow_vege_solid_prog_->prog_id());

        for (int i = 0; i < (int)list.shadow_lists.count; i++) {
            const ShadowList &sh_list = list.shadow_lists.data[i];
            if (!sh_list.shadow_batch_count)
                continue;

            glViewport(sh_list.shadow_map_pos[0], sh_list.shadow_map_pos[1],
                       sh_list.shadow_map_size[0], sh_list.shadow_map_size[1]);
            glScissor(sh_list.scissor_test_pos[0], sh_list.scissor_test_pos[1],
                      sh_list.scissor_test_size[0], sh_list.scissor_test_size[1]);

            if (!region_cleared[i]) {
                glClear(GL_DEPTH_BUFFER_BIT);
                region_cleared[i] = true;
            }

            glUniformMatrix4fv(
                REN_U_M_MATRIX_LOC, 1, GL_FALSE,
                Ren::ValuePtr(list.shadow_regions.data[i].clip_from_world));

            for (uint32_t j = sh_list.shadow_batch_start;
                 j < sh_list.shadow_batch_start + sh_list.shadow_batch_count; j++) {
                const DepthDrawBatch &batch =
                    list.shadow_batches.data[list.shadow_batch_indices.data[j]];
                if (!batch.instance_count || batch.alpha_test_bit ||
                    !batch.vegetation_bit)
                    continue;

                glUniform4iv(REN_U_INSTANCES_LOC, (batch.instance_count + 3) / 4,
                             &batch.instance_indices[0]);

                glDrawElementsInstancedBaseVertex(
                    GL_TRIANGLES, batch.indices_count, GL_UNSIGNED_INT,
                    (const GLvoid *)uintptr_t(batch.indices_offset),
                    (GLsizei)batch.instance_count, (GLint)batch.base_vertex);
                backend_info_.shadow_draw_calls_count++;
            }
        }

        // draw transparent (alpha-tested) objects
        glBindVertexArray(depth_pass_transp_vao_);
        glUseProgram(shadow_transp_prog_->prog_id());

        for (int i = 0; i < (int)list.shadow_lists.count; i++) {
            const ShadowList &sh_list = list.shadow_lists.data[i];
            if (!sh_list.shadow_batch_count)
                continue;

            glViewport(sh_list.shadow_map_pos[0], sh_list.shadow_map_pos[1],
                       sh_list.shadow_map_size[0], sh_list.shadow_map_size[1]);
            glScissor(sh_list.scissor_test_pos[0], sh_list.scissor_test_pos[1],
                      sh_list.scissor_test_size[0], sh_list.scissor_test_size[1]);

            if (!region_cleared[i]) {
                glClear(GL_DEPTH_BUFFER_BIT);
                region_cleared[i] = true;
            }

            glUniformMatrix4fv(
                REN_U_M_MATRIX_LOC, 1, GL_FALSE,
                Ren::ValuePtr(list.shadow_regions.data[i].clip_from_world));

            uint32_t cur_mat_id = 0xffffffff;

            for (uint32_t j = sh_list.shadow_batch_start;
                 j < sh_list.shadow_batch_start + sh_list.shadow_batch_count; j++) {
                const DepthDrawBatch &batch =
                    list.shadow_batches.data[list.shadow_batch_indices.data[j]];
                if (!batch.instance_count || !batch.alpha_test_bit ||
                    batch.vegetation_bit)
                    continue;

                if (batch.mat_id != cur_mat_id) {
                    const Ren::Material *mat = ctx_.GetMaterial(batch.mat_id).get();
                    ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, 0,
                                               mat->textures[0]->tex_id());
                    cur_mat_id = batch.mat_id;
                }

                glUniform4iv(REN_U_INSTANCES_LOC, (batch.instance_count + 3) / 4,
                             &batch.instance_indices[0]);

                glDrawElementsInstancedBaseVertex(
                    GL_TRIANGLES, batch.indices_count, GL_UNSIGNED_INT,
                    (const GLvoid *)uintptr_t(batch.indices_offset),
                    (GLsizei)batch.instance_count, (GLint)batch.base_vertex);
                backend_info_.shadow_draw_calls_count++;
            }
        }

        // draw transparent (alpha-tested) vegetation
        glBindVertexArray(depth_pass_vege_transp_vao_);
        glUseProgram(shadow_vege_transp_prog_->prog_id());

        for (int i = 0; i < (int)list.shadow_lists.count; i++) {
            const ShadowList &sh_list = list.shadow_lists.data[i];
            if (!sh_list.shadow_batch_count)
                continue;

            glViewport(sh_list.shadow_map_pos[0], sh_list.shadow_map_pos[1],
                       sh_list.shadow_map_size[0], sh_list.shadow_map_size[1]);
            glScissor(sh_list.scissor_test_pos[0], sh_list.scissor_test_pos[1],
                      sh_list.scissor_test_size[0], sh_list.scissor_test_size[1]);

            if (!region_cleared[i]) {
                glClear(GL_DEPTH_BUFFER_BIT);
                region_cleared[i] = true;
            }

            glUniformMatrix4fv(
                REN_U_M_MATRIX_LOC, 1, GL_FALSE,
                Ren::ValuePtr(list.shadow_regions.data[i].clip_from_world));

            uint32_t cur_mat_id = 0xffffffff;

            for (uint32_t j = sh_list.shadow_batch_start;
                 j < sh_list.shadow_batch_start + sh_list.shadow_batch_count; j++) {
                const DepthDrawBatch &batch =
                    list.shadow_batches.data[list.shadow_batch_indices.data[j]];
                if (!batch.instance_count || !batch.alpha_test_bit ||
                    !batch.vegetation_bit)
                    continue;

                if (batch.mat_id != cur_mat_id) {
                    const Ren::Material *mat = ctx_.GetMaterial(batch.mat_id).get();
                    ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, 0,
                                               mat->textures[0]->tex_id());
                    cur_mat_id = batch.mat_id;
                }

                glUniform4iv(REN_U_INSTANCES_LOC, (batch.instance_count + 3) / 4,
                             &batch.instance_indices[0]);

                glDrawElementsInstancedBaseVertex(
                    GL_TRIANGLES, batch.indices_count, GL_UNSIGNED_INT,
                    (const GLvoid *)uintptr_t(batch.indices_offset),
                    (GLsizei)batch.instance_count, (GLint)batch.base_vertex);
                backend_info_.shadow_draw_calls_count++;
            }
        }

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

    //
    // Skydome drawing + implicit depth/specular clear
    //

    if ((list.render_flags & DebugWireframe) == 0 && list.env.env_map) {
#if defined(REN_DIRECT_DRAWING)
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
#else
        glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)skydome_framebuf_);
#endif
        glUseProgram(skydome_prog_->prog_id());

        glEnable(GL_STENCIL_TEST);
        glStencilMask(0xff);
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
        glStencilFunc(GL_ALWAYS, 0, 0xFF);

        // Draw skydome (and clear depth with it)
        glDepthFunc(GL_ALWAYS);

        glBindVertexArray(skydome_vao_);

        Ren::Mat4f translate_matrix;
        translate_matrix =
            Ren::Translate(translate_matrix, Ren::Vec3f{shrd_data.uCamPosAndGamma});

        Ren::Mat4f scale_matrix;
        scale_matrix = Ren::Scale(scale_matrix, Ren::Vec3f{5000.0f, 5000.0f, 5000.0f});

        Ren::Mat4f world_from_object = translate_matrix * scale_matrix;
        glUniformMatrix4fv(REN_U_M_MATRIX_LOC, 1, GL_FALSE,
                           Ren::ValuePtr(world_from_object));

        ren_glBindTextureUnit_Comp(GL_TEXTURE_CUBE_MAP, REN_BASE0_TEX_SLOT,
                                   list.env.env_map->tex_id());

        DebugMarker _("DRAW SKYDOME");

        glDrawElements(GL_TRIANGLES, (GLsizei)__skydome_indices_count, GL_UNSIGNED_SHORT,
                       (void *)uintptr_t(skydome_ndx_offset_));

        glDepthFunc(GL_LESS);

        glDisable(GL_STENCIL_TEST);
    } else {
        glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)skydome_framebuf_);
        glClear(GLbitfield(GL_DEPTH_BUFFER_BIT) | GLbitfield(GL_COLOR_BUFFER_BIT));
    }

    glEnable(GL_MULTISAMPLE);

    //
    // Bind persistent resources (shadow atlas, lightmap, cells item data)
    //

    ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_SHAD_TEX_SLOT,
                               shadow_buf_.depth_tex.GetValue());

    if (list.decals_atlas) {
        ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_DECAL_TEX_SLOT,
                                   list.decals_atlas->tex_id(0));
    }

    if (list.render_flags & EnableSSAO) {
        ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_SSAO_TEX_SLOT,
                                   combined_buf_.attachments[0].tex);
    } else {
        ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_SSAO_TEX_SLOT,
                                   dummy_white_->tex_id());
    }

    ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_BRDF_TEX_SLOT, brdf_lut_->tex_id());

    if ((list.render_flags & EnableLightmap) && list.env.lm_direct) {
        for (int sh_l = 0; sh_l < 4; sh_l++) {
            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_LMAP_SH_SLOT + sh_l,
                                       list.env.lm_indir_sh[sh_l]->tex_id());
        }
    } else {
        for (int sh_l = 0; sh_l < 4; sh_l++) {
            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_LMAP_SH_SLOT + sh_l,
                                       dummy_black_->tex_id());
        }
    }

    if (list.probe_storage) {
        ren_glBindTextureUnit_Comp(GL_TEXTURE_CUBE_MAP_ARRAY, REN_ENV_TEX_SLOT,
                                   list.probe_storage->tex_id());
    }

    ren_glBindTextureUnit_Comp(GL_TEXTURE_BUFFER, REN_LIGHT_BUF_SLOT,
                               lights_tbo_[cur_buf_chunk_]);
    ren_glBindTextureUnit_Comp(GL_TEXTURE_BUFFER, REN_DECAL_BUF_SLOT,
                               decals_tbo_[cur_buf_chunk_]);
    ren_glBindTextureUnit_Comp(GL_TEXTURE_BUFFER, REN_CELLS_BUF_SLOT,
                               cells_tbo_[cur_buf_chunk_]);
    ren_glBindTextureUnit_Comp(GL_TEXTURE_BUFFER, REN_ITEMS_BUF_SLOT,
                               items_tbo_[cur_buf_chunk_]);

    ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_NOISE_TEX_SLOT, noise_tex_->tex_id());
    ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_CONE_RT_LUT_SLOT,
                               cone_rt_lut_->tex_id());

    //
    // Depth-fill pass (draw opaque surfaces -> draw alpha-tested surfaces)
    //

    if (list.render_flags & EnableTimers) {
        glQueryCounter(queries_[cur_query_][TimeDepthOpaqueStart], GL_TIMESTAMP);
    }

    if ((list.render_flags & EnableZFill) &&
        ((list.render_flags & DebugWireframe) == 0)) {
        // Write depth only
        glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)depth_fill_framebuf_);

        glDepthFunc(GL_LESS);

        DebugMarker _("DEPTH-FILL");

        // draw solid objects
        glBindVertexArray(depth_pass_solid_vao_);
        glUseProgram(fillz_solid_prog_->prog_id());

        for (uint32_t i = 0; i < list.zfill_batch_indices.count; i++) {
            const DepthDrawBatch &batch =
                list.zfill_batches.data[list.zfill_batch_indices.data[i]];
            if (!batch.instance_count || batch.alpha_test_bit || batch.vegetation_bit)
                continue;

            glUniform4iv(REN_U_INSTANCES_LOC, (batch.instance_count + 3) / 4,
                         &batch.instance_indices[0]);

            glDrawElementsInstancedBaseVertex(
                GL_TRIANGLES, batch.indices_count, GL_UNSIGNED_INT,
                (const GLvoid *)uintptr_t(batch.indices_offset),
                (GLsizei)batch.instance_count, (GLint)batch.base_vertex);
            backend_info_.depth_fill_draw_calls_count++;
        }

        // draw alpha-tested objects
        glBindVertexArray(depth_pass_transp_vao_);
        glUseProgram(fillz_transp_prog_->prog_id());

        ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_ALPHATEST_TEX_SLOT,
                                   dummy_white_->tex_id());

        uint32_t cur_mat_id = 0xffffffff;

        for (uint32_t i = 0; i < list.zfill_batch_indices.count; i++) {
            const DepthDrawBatch &batch =
                list.zfill_batches.data[list.zfill_batch_indices.data[i]];
            if (!batch.instance_count || !batch.alpha_test_bit || batch.vegetation_bit)
                continue;

            if (batch.mat_id != cur_mat_id) {
                const Ren::Material *mat = ctx_.GetMaterial(batch.mat_id).get();
                ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_ALPHATEST_TEX_SLOT,
                                           mat->textures[0]->tex_id());
                cur_mat_id = batch.mat_id;
            }

            glUniform4iv(REN_U_INSTANCES_LOC, (batch.instance_count + 3) / 4,
                         &batch.instance_indices[0]);

            glDrawElementsInstancedBaseVertex(
                GL_TRIANGLES, batch.indices_count, GL_UNSIGNED_INT,
                (const GLvoid *)uintptr_t(batch.indices_offset),
                (GLsizei)batch.instance_count, (GLint)batch.base_vertex);
            backend_info_.depth_fill_draw_calls_count++;
        }

        if ((list.render_flags & EnableTaa) != 0) {
            // Write depth and velocity
            glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)depth_fill_framebuf_vel_);

            glClear(GL_STENCIL_BUFFER_BIT);

            glEnable(GL_STENCIL_TEST);
            glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
            glStencilFunc(GL_ALWAYS, 1, 0xFF);
        } else {
            // Write depth only
            glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)depth_fill_framebuf_);
        }

        // draw solid vegetation
        glBindVertexArray(depth_pass_vege_solid_vao_);
        if ((list.render_flags & EnableTaa) != 0) {
            glUseProgram(fillz_vege_solid_vel_prog_->prog_id());
        } else {
            glUseProgram(fillz_vege_solid_prog_->prog_id());
        }

        for (uint32_t i = 0; i < list.zfill_batch_indices.count; i++) {
            const DepthDrawBatch &batch =
                list.zfill_batches.data[list.zfill_batch_indices.data[i]];
            if (!batch.instance_count || batch.alpha_test_bit || !batch.vegetation_bit)
                continue;

            glUniform4iv(REN_U_INSTANCES_LOC, (batch.instance_count + 3) / 4,
                         &batch.instance_indices[0]);

            glDrawElementsInstancedBaseVertex(
                GL_TRIANGLES, batch.indices_count, GL_UNSIGNED_INT,
                (const GLvoid *)uintptr_t(batch.indices_offset),
                (GLsizei)batch.instance_count, (GLint)batch.base_vertex);
            backend_info_.depth_fill_draw_calls_count++;
        }

        // draw alpha-tested vegetation
        glBindVertexArray(depth_pass_vege_transp_vao_);
        if ((list.render_flags & EnableTaa) != 0) {
            glUseProgram(fillz_vege_transp_vel_prog_->prog_id());
        } else {
            glUseProgram(fillz_vege_transp_prog_->prog_id());
        }

        ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_ALPHATEST_TEX_SLOT,
                                   dummy_white_->tex_id());

        cur_mat_id = 0xffffffff;

        for (uint32_t i = 0; i < list.zfill_batch_indices.count; i++) {
            const DepthDrawBatch &batch =
                list.zfill_batches.data[list.zfill_batch_indices.data[i]];
            if (!batch.instance_count || !batch.alpha_test_bit || !batch.vegetation_bit)
                continue;

            if (batch.mat_id != cur_mat_id) {
                const Ren::Material *mat = ctx_.GetMaterial(batch.mat_id).get();
                ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_ALPHATEST_TEX_SLOT,
                                           mat->textures[0]->tex_id());
                cur_mat_id = batch.mat_id;
            }

            glUniform4iv(REN_U_INSTANCES_LOC, (batch.instance_count + 3) / 4,
                         &batch.instance_indices[0]);

            glDrawElementsInstancedBaseVertex(
                GL_TRIANGLES, batch.indices_count, GL_UNSIGNED_INT,
                (const GLvoid *)uintptr_t(batch.indices_offset),
                (GLsizei)batch.instance_count, (GLint)batch.base_vertex);
            backend_info_.depth_fill_draw_calls_count++;
        }

        glDisable(GL_STENCIL_TEST);

        glBindVertexArray(0);

        glDepthFunc(GL_EQUAL);
    }

    //
    // SSAO pass (downsample depth -> calc line integrals ao -> upscale)
    //

    if (list.render_flags & EnableTimers) {
        glQueryCounter(queries_[cur_query_][TimeAOPassStart], GL_TIMESTAMP);
    }

    glBindVertexArray((GLuint)fs_quad_vao_);

    // const uint32_t use_down_depth_mask = (EnableZFill | EnableSSAO | EnableSSR);
    if ((list.render_flags & EnableZFill) &&
        (list.render_flags & (EnableSSAO | EnableSSR)) &&
        ((list.render_flags & DebugWireframe) == 0)) {
        DebugMarker _("DOWNSAMPLE DEPTH");

        // Setup viewport once for all ssao passes
        glViewport(0, 0, act_w_ / 2, act_h_ / 2);

        // downsample depth buffer
        glBindFramebuffer(GL_FRAMEBUFFER, down_depth_2x_.fb);

        const Ren::Program *down_depth_prog = nullptr;

        if (clean_buf_.sample_count > 1) {
            down_depth_prog = blit_down_depth_ms_prog_.get();
        } else {
            down_depth_prog = blit_down_depth_prog_.get();
        }

        glUseProgram(down_depth_prog->prog_id());

        glUniform4f(0, 0.0f, 0.0f, float(act_w_), float(act_h_));
        glUniform1f(1, 1.0f); // linearize

        if (clean_buf_.sample_count > 1) {
            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D_MULTISAMPLE, REN_BASE0_TEX_SLOT,
                                       clean_buf_.depth_tex.GetValue());
        } else {
            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_BASE0_TEX_SLOT,
                                       clean_buf_.depth_tex.GetValue());
        }

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT,
                       (const GLvoid *)uintptr_t(quad_ndx_offset_));
    }

    const uint32_t use_ssao_mask = (EnableZFill | EnableSSAO | DebugWireframe);
    const uint32_t use_ssao = (EnableZFill | EnableSSAO);
    if ((list.render_flags & use_ssao_mask) == use_ssao) {
        DebugMarker _("SSAO PASS");

        assert(down_depth_2x_.w == ssao_buf1_.w && down_depth_2x_.w == ssao_buf2_.w &&
               down_depth_2x_.h == ssao_buf1_.h && down_depth_2x_.h == ssao_buf2_.h);

        const int ssao_res[] = {act_w_ / 2, act_h_ / 2};

        // Setup viewport once for all ssao passes
        glViewport(0, 0, ssao_res[0], ssao_res[1]);

        { // prepare ao buffer
            glBindFramebuffer(GL_FRAMEBUFFER, ssao_buf1_.fb);

            glUseProgram(blit_ao_prog_->prog_id());

            glUniform4f(0, 0.0f, 0.0f, float(ssao_res[0]), float(ssao_res[1]));

            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_BASE0_TEX_SLOT,
                                       down_depth_2x_.attachments[0].tex);
            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_BASE1_TEX_SLOT,
                                       rand2d_dirs_4x4_->tex_id());
            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_BASE2_TEX_SLOT,
                                       cone_rt_lut_->tex_id());

            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT,
                           (const GLvoid *)uintptr_t(quad_ndx_offset_));
        }

        { // blur ao buffer
            glBindFramebuffer(GL_FRAMEBUFFER, ssao_buf2_.fb);

            glUseProgram(blit_bilateral_prog_->prog_id());

            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, 0,
                                       down_depth_2x_.attachments[0].tex);
            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, 1, ssao_buf1_.attachments[0].tex);

            glUniform4f(0, 0.0f, 0.0f, float(ssao_res[0]), float(ssao_res[1]));
            glUniform1f(3, 0.0f);

            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT,
                           (const GLvoid *)uintptr_t(quad_ndx_offset_));

            glBindFramebuffer(GL_FRAMEBUFFER, ssao_buf1_.fb);

            glUseProgram(blit_bilateral_prog_->prog_id());

            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, 1, ssao_buf2_.attachments[0].tex);

            glUniform4f(0, 0.0f, 0.0f, float(ssao_res[0]), float(ssao_res[1]));
            glUniform1f(3, 1.0f);

            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT,
                           (const GLvoid *)uintptr_t(quad_ndx_offset_));
        }

        { // upsample ao
            glBindFramebuffer(GL_FRAMEBUFFER, combined_buf_.fb);
            glViewport(0, 0, act_w_, act_h_);

            const Ren::Program *blit_upscale_prog = nullptr;

            if (clean_buf_.sample_count > 1) {
                blit_upscale_prog = blit_upscale_ms_prog_.get();
                ren_glBindTextureUnit_Comp(GL_TEXTURE_2D_MULTISAMPLE, 0,
                                           clean_buf_.depth_tex.GetValue());
            } else {
                blit_upscale_prog = blit_upscale_prog_.get();
                ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, 0,
                                           clean_buf_.depth_tex.GetValue());
            }

            glUseProgram(blit_upscale_prog->prog_id());

            glUniform4f(0, 0.0f, 0.0f, 1.0f, 1.0f);

            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, 1,
                                       down_depth_2x_.attachments[0].tex);
            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, 2, ssao_buf1_.attachments[0].tex);

            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT,
                           (const GLvoid *)uintptr_t(quad_ndx_offset_));
        }
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

    //
    // Opaque pass (draw opaque surfaces -> resolve multisampled color buffer if enabled)
    //

    // Bind main buffer for drawing
#if defined(REN_DIRECT_DRAWING)
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, scr_w_, scr_h_);
#else
    glBindFramebuffer(GL_FRAMEBUFFER, clean_buf_.fb);
    glViewport(0, 0, act_w_, act_h_);
#endif

    if (list.render_flags & EnableTimers) {
        glQueryCounter(queries_[cur_query_][TimeOpaqueStart], GL_TIMESTAMP);
    }

    glBindVertexArray((GLuint)draw_pass_vao_);

    { // actual drawing
        DebugMarker _("OPAQUE PASS");

        const Ren::Program *cur_program = nullptr;
        const Ren::Material *cur_mat = nullptr;

        for (uint32_t i = 0; i < list.main_batch_indices.count; i++) {
            const MainDrawBatch &batch =
                list.main_batches.data[list.main_batch_indices.data[i]];
            if (!batch.instance_count)
                continue;
            if (batch.alpha_blend_bit)
                break;

            const Ren::Program *p = ctx_.GetProgram(batch.prog_id).get();
            const Ren::Material *mat = ctx_.GetMaterial(batch.mat_id).get();

            if (cur_program != p) {
                glUseProgram(p->prog_id());
                cur_program = p;
            }

            if (cur_mat != mat) {
                ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_MAT_TEX0_SLOT,
                                           mat->textures[0]->tex_id());
                ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_MAT_TEX1_SLOT,
                                           mat->textures[1]->tex_id());
                ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_MAT_TEX2_SLOT,
                                           mat->textures[2]->tex_id());
                if (mat->textures[3]) {
                    ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_MAT_TEX3_SLOT,
                                               mat->textures[3]->tex_id());
                }
                cur_mat = mat;
            }

            glUniform4iv(REN_U_INSTANCES_LOC, (batch.instance_count + 3) / 4,
                         &batch.instance_indices[0]);

            glDrawElementsInstancedBaseVertex(
                GL_TRIANGLES, batch.indices_count, GL_UNSIGNED_INT,
                (const GLvoid *)uintptr_t(batch.indices_offset),
                (GLsizei)batch.instance_count, (GLint)batch.base_vertex);
            backend_info_.opaque_draw_calls_count++;
            backend_info_.triangles_rendered +=
                (batch.indices_count / 3) * batch.instance_count;
        }
    }

#if !defined(REN_DIRECT_DRAWING)
#if !defined(__ANDROID__)
    if (list.render_flags & DebugWireframe) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
#endif

    if ((list.render_flags & EnableOIT) && clean_buf_.sample_count > 1) {
        DebugMarker _("RESOLVE MS BUFFER");

        glBindVertexArray((GLuint)fs_quad_vao_);
        glBindFramebuffer(GL_FRAMEBUFFER, resolved_or_transparent_buf_.fb);

        glUseProgram(blit_ms_resolve_prog_->prog_id());
        glUniform4f(0, 0.0f, 0.0f, float(act_w_), float(act_h_));

        ren_glBindTextureUnit_Comp(GL_TEXTURE_2D_MULTISAMPLE, REN_BASE0_TEX_SLOT,
                                   clean_buf_.attachments[0].tex);

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT,
                       (const GLvoid *)uintptr_t(quad_ndx_offset_));
    }
#endif

    //
    // Transparent pass
    //

    if (list.render_flags & EnableTimers) {
        glQueryCounter(queries_[cur_query_][TimeTranspStart], GL_TIMESTAMP);
    }

    glBindVertexArray((GLuint)draw_pass_vao_);

    if (list.render_flags & EnableOIT) {
#if (REN_OIT_MODE == REN_OIT_MOMENT_BASED)
        glBindFramebuffer(GL_FRAMEBUFFER, moments_buf_.fb);
        glClear(GL_COLOR_BUFFER_BIT);

        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);

        glDepthFunc(GL_LEQUAL);
        glDepthMask(GL_FALSE);

        { // Draw alpha-blended surfaces
            DebugMarker _("MOMENTS GENERATION PASS");

            const Ren::Program *cur_program = nullptr;
            const Ren::Material *cur_mat = nullptr;

            for (int j = (int)list.main_batch_indices.count - 1; j >= 0; j--) {
                const MainDrawBatch &batch =
                    list.main_batches.data[list.main_batch_indices.data[j]];
                if (!batch.instance_count)
                    continue;
                if (!batch.alpha_blend_bit)
                    break;

                const Ren::Program *p = ctx_.GetProgram(batch.prog_id).get();
                const Ren::Material *mat = ctx_.GetMaterial(batch.mat_id).get();

                if (cur_program != p) {
                    glUseProgram(p->prog_id());
                    cur_program = p;
                }

                if (cur_mat != mat) {
                    ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_MAT_TEX0_SLOT,
                                               mat->texture(0)->tex_id());
                    cur_mat = mat;
                }

                glUniform4iv(REN_U_INSTANCES_LOC, (batch.instance_count + 3) / 4,
                             &batch.instance_indices[0]);

                glDrawElementsInstancedBaseVertex(
                    GL_TRIANGLES, batch.indices_count, GL_UNSIGNED_INT,
                    (const GLvoid *)uintptr_t(batch.indices_offset),
                    (GLsizei)batch.instance_count, (GLint)batch.base_vertex);

                backend_info_.opaque_draw_calls_count += 2;
                backend_info_.triangles_rendered +=
                    (batch.indices_count / 3) * batch.instance_count;
            }
        }

        { // Change transparency draw mode
            glBindBuffer(GL_UNIFORM_BUFFER,
                         (GLuint)unif_shared_data_block_[cur_buf_chunk_]);
            const float transp_mode = clean_buf_.sample_count > 1 ? 4.0f : 3.0f;
            glBufferSubData(GL_UNIFORM_BUFFER,
                            offsetof(SharedDataBlock, uTranspParamsAndTime) +
                                2 * sizeof(float),
                            sizeof(float), &transp_mode);
        }

        uint32_t target_framebuf = (clean_buf_.sample_count > 1)
                                       ? clean_buf_color_only_
                                       : resolved_or_transparent_buf_.fb;

        glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)target_framebuf);

        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        if (moments_buf_.sample_count > 1) {
            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D_MULTISAMPLE,
                                       REN_MOMENTS0_MS_TEX_SLOT,
                                       moments_buf_.attachments[0].tex);
            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D_MULTISAMPLE,
                                       REN_MOMENTS1_MS_TEX_SLOT,
                                       moments_buf_.attachments[1].tex);
            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D_MULTISAMPLE,
                                       REN_MOMENTS2_MS_TEX_SLOT,
                                       moments_buf_.attachments[2].tex);
        } else {
            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_MOMENTS0_TEX_SLOT,
                                       moments_buf_.attachments[0].tex);
            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_MOMENTS1_TEX_SLOT,
                                       moments_buf_.attachments[1].tex);
            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_MOMENTS2_TEX_SLOT,
                                       moments_buf_.attachments[2].tex);
        }

        { // Draw alpha-blended surfaces
            DebugMarker _("TRANSPARENT PASS");

            const Ren::Program *cur_program = nullptr;
            const Ren::Material *cur_mat = nullptr;

            for (int j = (int)list.main_batch_indices.count - 1; j >= 0; j--) {
                const MainDrawBatch &batch =
                    list.main_batches.data[list.main_batch_indices.data[j]];
                if (!batch.instance_count)
                    continue;
                if (!batch.alpha_blend_bit)
                    break;

                const Ren::Program *p = ctx_.GetProgram(batch.prog_id).get();
                const Ren::Material *mat = ctx_.GetMaterial(batch.mat_id).get();

                if (cur_program != p) {
                    glUseProgram(p->prog_id());
                    cur_program = p;
                }

                if (cur_mat != mat) {
                    ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_MAT_TEX0_SLOT,
                                               mat->texture(0)->tex_id());
                    ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_MAT_TEX1_SLOT,
                                               mat->texture(1)->tex_id());
                    if (mat->texture(3)) {
                        ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_MAT_TEX3_SLOT,
                                                   mat->texture(3)->tex_id());
                    }
                    cur_mat = mat;
                }

                glUniform4iv(REN_U_INSTANCES_LOC, (batch.instance_count + 3) / 4,
                             &batch.instance_indices[0]);

                glDrawElementsInstancedBaseVertex(
                    GL_TRIANGLES, batch.indices_count, GL_UNSIGNED_INT,
                    (const GLvoid *)uintptr_t(batch.indices_offset),
                    (GLsizei)batch.instance_count, (GLint)batch.base_vertex);

                backend_info_.opaque_draw_calls_count += 2;
                backend_info_.triangles_rendered +=
                    (batch.indices_count / 3) * batch.instance_count;
            }
        }

        glBindFramebuffer(GL_FRAMEBUFFER, clean_buf_.fb);
#elif (REN_OIT_MODE == REN_OIT_WEIGHTED_BLENDED)
        glBindFramebuffer(GL_FRAMEBUFFER, clean_buf_transparent_);

        {
            DebugMarker _("INIT TRANSPARENT BUF");

            glBindVertexArray((GLuint)fs_quad_vao_);

            glDisable(GL_DEPTH_TEST);

            glUseProgram(blit_transparent_init_prog_->prog_id());
            glUniform4f(0, 0.0f, 0.0f, float(act_w_), float(act_h_));

            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT,
                           (const GLvoid *)uintptr_t(quad_ndx_offset_));
        }

        {
            DebugMarker _("TRANSPARENT PASS");

            glBindVertexArray((GLuint)draw_pass_vao_);
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(GL_LEQUAL);

            glDepthMask(GL_FALSE);
            glEnable(GL_BLEND);

            glBlendFunci(REN_OUT_COLOR_INDEX, GL_ONE, GL_ONE);
            glBlendFunci(REN_OUT_NORM_INDEX, GL_ZERO, GL_ONE_MINUS_SRC_COLOR);

            const Ren::Program *cur_program = nullptr;
            const Ren::Material *cur_mat = nullptr;

            for (int j = (int)list.main_batch_indices.count - 1; j >= 0; j--) {
                const MainDrawBatch &batch =
                    list.main_batches.data[list.main_batch_indices.data[j]];
                if (!batch.instance_count)
                    continue;
                if (!batch.alpha_blend_bit)
                    break;

                const Ren::Program *p = ctx_.GetProgram(batch.prog_id).get();
                const Ren::Material *mat = ctx_.GetMaterial(batch.mat_id).get();

                if (cur_program != p) {
                    glUseProgram(p->prog_id());
                    cur_program = p;
                }

                if (cur_mat != mat) {
                    ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_MAT_TEX0_SLOT,
                                               mat->texture(0)->tex_id());
                    ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_MAT_TEX1_SLOT,
                                               mat->texture(1)->tex_id());
                    ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_MAT_TEX2_SLOT,
                                               mat->texture(2)->tex_id());
                    if (mat->texture(3)) {
                        ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_MAT_TEX3_SLOT,
                                                   mat->texture(3)->tex_id());
                    }
                    cur_mat = mat;
                }

                glUniform4iv(REN_U_INSTANCES_LOC, (batch.instance_count + 3) / 4,
                             &batch.instance_indices[0]);

                glDrawElementsInstancedBaseVertex(
                    GL_TRIANGLES, batch.indices_count, GL_UNSIGNED_INT,
                    (const GLvoid *)uintptr_t(batch.indices_offset),
                    (GLsizei)batch.instance_count, (GLint)batch.base_vertex);

                backend_info_.opaque_draw_calls_count++;
                backend_info_.triangles_rendered +=
                    (batch.indices_count / 3) * batch.instance_count;
            }
        }

        glBindFramebuffer(GL_FRAMEBUFFER, clean_buf_.fb);
#endif
    } else {
#if defined(REN_DIRECT_DRAWING)
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
#else
        glBindFramebuffer(GL_FRAMEBUFFER, clean_buf_.fb);
#endif

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        DebugMarker _("TRANSPARENT PASS");

        const Ren::Program *cur_program = nullptr;
        const Ren::Material *cur_mat = nullptr;

        for (int j = (int)list.main_batch_indices.count - 1; j >= 0; j--) {
            const MainDrawBatch &batch =
                list.main_batches.data[list.main_batch_indices.data[j]];
            if (!batch.instance_count)
                continue;
            if (!batch.alpha_blend_bit)
                break;

            const Ren::Program *p = ctx_.GetProgram(batch.prog_id).get();
            const Ren::Material *mat = ctx_.GetMaterial(batch.mat_id).get();

            if (cur_program != p) {
                glUseProgram(p->prog_id());
                cur_program = p;
            }

            if (cur_mat != mat) {
                ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_MAT_TEX0_SLOT,
                                           mat->textures[0]->tex_id());
                ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_MAT_TEX1_SLOT,
                                           mat->textures[1]->tex_id());
                ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_MAT_TEX2_SLOT,
                                           mat->textures[2]->tex_id());
                if (mat->textures[3]) {
                    ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_MAT_TEX3_SLOT,
                                               mat->textures[3]->tex_id());
                }
                cur_mat = mat;
            }

            glUniform4iv(REN_U_INSTANCES_LOC, (batch.instance_count + 3) / 4,
                         &batch.instance_indices[0]);

            glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
            glDepthFunc(GL_LEQUAL);

            glDrawElementsInstancedBaseVertex(
                GL_TRIANGLES, batch.indices_count, GL_UNSIGNED_INT,
                (const GLvoid *)uintptr_t(batch.indices_offset),
                (GLsizei)batch.instance_count, (GLint)batch.base_vertex);

            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            glDepthFunc(GL_EQUAL);

            glDrawElementsInstancedBaseVertex(
                GL_TRIANGLES, batch.indices_count, GL_UNSIGNED_INT,
                (const GLvoid *)uintptr_t(batch.indices_offset),
                (GLsizei)batch.instance_count, (GLint)batch.base_vertex);

            backend_info_.opaque_draw_calls_count += 2;
            backend_info_.triangles_rendered +=
                (batch.indices_count / 3) * batch.instance_count;
        }

#if !defined(REN_DIRECT_DRAWING)
        if (clean_buf_.sample_count > 1) {
            DebugMarker _resolve_ms("RESOLVE MS BUFFER");

            glBindVertexArray((GLuint)fs_quad_vao_);
            glBindFramebuffer(GL_FRAMEBUFFER, resolved_or_transparent_buf_.fb);

            glUseProgram(blit_ms_resolve_prog_->prog_id());
            glUniform4f(0, 0.0f, 0.0f, float(act_w_), float(act_h_));

            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D_MULTISAMPLE, REN_BASE0_TEX_SLOT,
                                       clean_buf_.attachments[0].tex);

            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT,
                           (const GLvoid *)uintptr_t(quad_ndx_offset_));
        }
#endif

        glBindFramebuffer(GL_FRAMEBUFFER, clean_buf_.fb);
    }

#if !defined(__ANDROID__)
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
#endif

    glDisable(GL_BLEND);
    glDepthFunc(GL_LESS);

    //
    // Reflections pass (calc ssr buffer -> dilate -> combine with cubemap reflections ->
    // blend on top of color buffer)
    //

    if (list.render_flags & EnableTimers) {
        glQueryCounter(queries_[cur_query_][TimeReflStart], GL_TIMESTAMP);
    }

    glBindVertexArray((GLuint)fs_quad_vao_);

    const uint32_t use_ssr_mask = (EnableSSR | DebugWireframe);
    const uint32_t use_ssr = EnableSSR;
    if ((list.render_flags & use_ssr_mask) == use_ssr) {
        DebugMarker _("REFLECTIONS PASS");

        glBindFramebuffer(GL_FRAMEBUFFER, ssr_buf1_.fb);
        glViewport(0, 0, act_w_ / 2, act_h_ / 2);

        const Ren::Program *ssr_program = nullptr;
        if (clean_buf_.sample_count > 1) {
            ssr_program = blit_ssr_ms_prog_.get();
        } else {
            ssr_program = blit_ssr_prog_.get();
        }
        glUseProgram(ssr_program->prog_id());

        glUniform4f(0, 0.0f, 0.0f, float(act_w_), float(act_h_));

        ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_REFL_DEPTH_TEX_SLOT,
                                   down_depth_2x_.attachments[0].tex);

        const GLenum clean_buf_bind_target =
            (clean_buf_.sample_count > 1) ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
        ren_glBindTextureUnit_Comp(clean_buf_bind_target, REN_REFL_NORM_TEX_SLOT,
                                   clean_buf_.attachments[REN_OUT_NORM_INDEX].tex);
        ren_glBindTextureUnit_Comp(clean_buf_bind_target, REN_REFL_SPEC_TEX_SLOT,
                                   clean_buf_.attachments[REN_OUT_SPEC_INDEX].tex);

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT,
                       (const GLvoid *)uintptr_t(quad_ndx_offset_));

        { // dilate ssr buffer
            const Ren::Program *dilate_prog = blit_ssr_dilate_prog_.get();
            glUseProgram(dilate_prog->prog_id());

            glUniform4f(0, 0.0f, 0.0f, float(act_w_) / 2.0f, float(act_h_) / 2.0f);

            glBindFramebuffer(GL_FRAMEBUFFER, ssr_buf2_.fb);

            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_BASE0_TEX_SLOT,
                                       ssr_buf1_.attachments[0].tex);

            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT,
                           (const GLvoid *)uintptr_t(quad_ndx_offset_));
        }

        // Compose reflections on top of clean buffer
        const Ren::Program *blit_ssr_compose_prog = nullptr;

        if (clean_buf_.sample_count > 1) {
            glBindFramebuffer(GL_FRAMEBUFFER, resolved_or_transparent_buf_.fb);
            blit_ssr_compose_prog = blit_ssr_compose_ms_prog_.get();
        } else {
            glBindFramebuffer(GL_FRAMEBUFFER, refl_comb_framebuf_);
            blit_ssr_compose_prog = blit_ssr_compose_prog_.get();
        }
        glViewport(0, 0, act_w_, act_h_);

        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);

        glUseProgram(blit_ssr_compose_prog->prog_id());
        glUniform4f(0, 0.0f, 0.0f, 1.0f, 1.0f);

        ren_glBindTextureUnit_Comp(clean_buf_bind_target, REN_REFL_SPEC_TEX_SLOT,
                                   clean_buf_.attachments[REN_OUT_SPEC_INDEX].tex);
        ren_glBindTextureUnit_Comp(clean_buf_bind_target, REN_REFL_DEPTH_TEX_SLOT,
                                   clean_buf_.depth_tex.GetValue());
        ren_glBindTextureUnit_Comp(clean_buf_bind_target, REN_REFL_NORM_TEX_SLOT,
                                   clean_buf_.attachments[REN_OUT_NORM_INDEX].tex);

        ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_REFL_DEPTH_LOW_TEX_SLOT,
                                   down_depth_2x_.attachments[0].tex);
        ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_REFL_SSR_TEX_SLOT,
                                   ssr_buf2_.attachments[0].tex);

        ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_REFL_PREV_TEX_SLOT,
                                   down_buf_4x_.attachments[0].tex);
        ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_REFL_BRDF_TEX_SLOT,
                                   brdf_lut_->tex_id());

        ren_glBindTextureUnit_Comp(GL_TEXTURE_BUFFER, REN_CELLS_BUF_SLOT,
                                   cells_tbo_[cur_buf_chunk_]);
        ren_glBindTextureUnit_Comp(GL_TEXTURE_BUFFER, REN_ITEMS_BUF_SLOT,
                                   items_tbo_[cur_buf_chunk_]);

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT,
                       (const GLvoid *)uintptr_t(quad_ndx_offset_));

        glDisable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

#if (REN_OIT_MODE != REN_OIT_DISABLED)
    if (list.render_flags & EnableOIT) {
        DebugMarker _("COMPOSE TRANSPARENT");

        glEnable(GL_BLEND);
#if (REN_OIT_MODE == REN_OIT_WEIGHTED_BLENDED) ||                                        \
    (REN_OIT_MODE == REN_OIT_MOMENT_BASED && REN_OIT_MOMENT_RENORMALIZE)
        glBlendFunc(GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA);
#else
        glBlendFunc(GL_ONE, GL_SRC_ALPHA);
#endif

        const Ren::Program *blit_transparent_compose =
            (clean_buf_.sample_count > 1) ? blit_transparent_compose_ms_prog_.get()
                                          : blit_transparent_compose_prog_.get();

        const uint32_t target_framebuffer = (clean_buf_.sample_count > 1)
                                                ? resolved_or_transparent_buf_.fb
                                                : transparent_comb_framebuf_;

        glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)target_framebuffer);
        glViewport(0, 0, act_w_, act_h_);

        glUseProgram(blit_transparent_compose->prog_id());
        glUniform4f(0, 0.0f, 0.0f, float(act_w_), float(act_h_));

        if (clean_buf_.sample_count > 1) {
            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D_MULTISAMPLE, REN_BASE0_TEX_SLOT,
                                       clean_buf_.attachments[REN_OUT_COLOR_INDEX].tex);
#if REN_OIT_MODE == REN_OIT_MOMENT_BASED
            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D_MULTISAMPLE, REN_BASE1_TEX_SLOT,
                                       moments_buf_.attachments[0].tex);
#elif REN_OIT_MODE == REN_OIT_WEIGHTED_BLENDED
            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D_MULTISAMPLE, REN_BASE1_TEX_SLOT,
                                       clean_buf_.attachments[REN_OUT_NORM_INDEX].tex);
#endif
        } else {
            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_BASE0_TEX_SLOT,
                                       resolved_or_transparent_buf_.attachments[0].tex);
#if REN_OIT_MODE == REN_OIT_MOMENT_BASED
            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_BASE1_TEX_SLOT,
                                       moments_buf_.attachments[0].tex);
#elif REN_OIT_MODE == REN_OIT_WEIGHTED_BLENDED
            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_BASE1_TEX_SLOT,
                                       clean_buf_.attachments[REN_OUT_NORM_INDEX].tex);
#endif
        }

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT,
                       (const GLvoid *)uintptr_t(quad_ndx_offset_));

        glDisable(GL_BLEND);
    }
#endif

    if (list.render_flags & EnableTimers) {
        glQueryCounter(queries_[cur_query_][TimeTaaStart], GL_TIMESTAMP);
    }

    if (list.render_flags & EnableTaa) {
        DebugMarker _("TEMPORAL AA");

        glBindFramebuffer(GL_FRAMEBUFFER, clean_buf_vel_only_);
        glViewport(0, 0, act_w_, act_h_);

        { // Init static objects velocities
            glEnable(GL_STENCIL_TEST);
            glStencilMask(0x00);
            glStencilFunc(GL_EQUAL, 0, 0xFF);

            const Ren::Program *blit_prog = blit_static_vel_prog_.get();
            glUseProgram(blit_prog->prog_id());

            glUniform4f(0, 0.0f, 0.0f, float(act_w_), float(act_h_));

            assert(clean_buf_.sample_count == 1);
            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, 0, clean_buf_.depth_tex.GetValue());

            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT,
                           (const GLvoid *)uintptr_t(quad_ndx_offset_));

            glDisable(GL_STENCIL_TEST);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)temporal_resolve_framebuf_);
        glViewport(0, 0, act_w_, act_h_);

        { // Blit taa
            const Ren::Program *blit_prog = blit_taa_prog_.get();
            glUseProgram(blit_prog->prog_id());

            glUniform4f(0, 0.0f, 0.0f, float(act_w_), float(act_h_));

            assert(clean_buf_.sample_count == 1);
            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, 0, clean_buf_.attachments[0].tex);
            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, 1, history_buf_.attachments[0].tex);
            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, 2, clean_buf_.depth_tex.GetValue());
            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, 3, clean_buf_.attachments[3].tex);

            glUniform2f(13, float(act_w_), float(act_h_));

            // exposure from previous frame
            float exposure = reduced_average_ > std::numeric_limits<float>::epsilon()
                                 ? (1.0f / reduced_average_)
                                 : 1.0f;
            exposure = std::min(exposure, list.draw_cam.max_exposure);

            glUniform1f(14, exposure);

            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT,
                           (const GLvoid *)uintptr_t(quad_ndx_offset_));
        }
    }

    if (list.render_flags & DebugProbes) {
        glBindVertexArray(sphere_vao_);

        glDisable(GL_DEPTH_TEST);

        // Write to color
        if (clean_buf_.sample_count > 1) {
            glBindFramebuffer(GL_FRAMEBUFFER, resolved_or_transparent_buf_.fb);
        } else {
            glBindFramebuffer(GL_FRAMEBUFFER, refl_comb_framebuf_);
        }

        glUseProgram(probe_prog_->prog_id());

        glUniform1f(1, debug_roughness_);
        debug_roughness_ += 0.1f;
        if (debug_roughness_ > 8.0f) {
            debug_roughness_ = 0.0f;
        }

        if (list.probe_storage) {
            ren_glBindTextureUnit_Comp(GL_TEXTURE_CUBE_MAP_ARRAY, REN_BASE0_TEX_SLOT,
                                       list.probe_storage->tex_id());
        }

        for (int i = 0; i < (int)list.probes.count; i++) {
            const ProbeItem &pr = list.probes.data[i];

            glUniform1i(2, i);

            Ren::Mat4f world_from_object;
            world_from_object = Ren::Translate(
                world_from_object,
                Ren::Vec3f{pr.position[0], pr.position[1], pr.position[2]});
            glUniformMatrix4fv(REN_U_M_MATRIX_LOC, 1, GL_FALSE,
                               Ren::ValuePtr(world_from_object));

            glDrawElements(GL_TRIANGLES, (GLsizei)__sphere_indices_count,
                           GL_UNSIGNED_SHORT, (void *)uintptr_t(sphere_ndx_offset_));
        }
    }

    if (list.render_flags & DebugEllipsoids) {
        glBindVertexArray(sphere_vao_);

        glDisable(GL_DEPTH_TEST);
#if !defined(__ANDROID__)
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
#endif

        // Write to color
        if (clean_buf_.sample_count > 1) {
            glBindFramebuffer(GL_FRAMEBUFFER, resolved_or_transparent_buf_.fb);
        } else {
            glBindFramebuffer(GL_FRAMEBUFFER, refl_comb_framebuf_);
        }

        glUseProgram(ellipsoid_prog_->prog_id());

        for (int i = 0; i < (int)list.ellipsoids.count; i++) {
            const EllipsItem &e = list.ellipsoids.data[i];

            Ren::Mat4f world_from_object;
            world_from_object =
                Ren::Translate(world_from_object,
                               Ren::Vec3f{e.position[0], e.position[1], e.position[2]});

            auto sph_ls = Ren::Mat3f{Ren::Uninitialize};
            sph_ls[0] = Ren::Vec3f{0.0f};
            sph_ls[0][e.perp] = 1.0f;
            sph_ls[1] = Ren::MakeVec3(e.axis);
            sph_ls[2] = Ren::Normalize(Ren::Cross(sph_ls[0], sph_ls[1]));
            sph_ls[0] = Ren::Normalize(Ren::Cross(sph_ls[1], sph_ls[2]));
            // sph_ls = Ren::Transpose(sph_ls);

            // const float l = Ren::Length(sph_ls[1]);
            // sph_ls[1] = Ren::Normalize(sph_ls[1]) / l;

            sph_ls *= e.radius;

            world_from_object[0] = Ren::Vec4f{sph_ls[0]};
            world_from_object[1] = Ren::Vec4f{sph_ls[1]};
            world_from_object[2] = Ren::Vec4f{sph_ls[2]};

            glUniformMatrix4fv(REN_U_M_MATRIX_LOC, 1, GL_FALSE,
                               Ren::ValuePtr(world_from_object));

            glDrawElements(GL_TRIANGLES, (GLsizei)__sphere_indices_count,
                           GL_UNSIGNED_SHORT, (void *)uintptr_t(sphere_ndx_offset_));
        }

#if !defined(__ANDROID__)
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
#endif
    }

    ren_glBindTextureUnit_Comp(GL_TEXTURE_CUBE_MAP_ARRAY, REN_ENV_TEX_SLOT, 0);

    glDisable(GL_DEPTH_TEST);

    //
    // Blur pass (apply gauss blur to color buffer -> blend on top)
    //

    if (list.render_flags & EnableTimers) {
        glQueryCounter(queries_[cur_query_][TimeBlurStart], GL_TIMESTAMP);
    }

    glBindVertexArray(fs_quad_vao_);
    glDepthMask(GL_FALSE);
    glDepthFunc(GL_LESS);

    // store matrix to use it in next frame
    down_buf_view_from_world_ = shrd_data.uViewMatrix;
    prev_clip_from_world_ = clip_from_world_unjittered;
    prev_clip_from_view_ = shrd_data.uProjMatrix;

    if ((list.render_flags & (EnableSSR | EnableBloom | EnableTonemap | EnableDOF)) &&
        ((list.render_flags & DebugWireframe) == 0)) {
        DebugMarker _("DOWNSAMPLE COLOR");

        glBindFramebuffer(GL_FRAMEBUFFER, down_buf_4x_.fb);
        glViewport(0, 0, down_buf_4x_.w, down_buf_4x_.h);

        const Ren::Program *cur_program = blit_down_prog_.get();
        glUseProgram(cur_program->prog_id());

        glUniform4f(0, 0.0f, 0.0f, float(act_w_) / float(scr_w_),
                    float(act_h_) / float(scr_h_));

        if (clean_buf_.sample_count > 1 || ((list.render_flags & EnableTaa) != 0)) {
            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_BASE0_TEX_SLOT,
                                       resolved_or_transparent_buf_.attachments[0].tex);
        } else {
            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_BASE0_TEX_SLOT,
                                       clean_buf_.attachments[0].tex);
        }

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT,
                       (const GLvoid *)uintptr_t(quad_ndx_offset_));
    }

    const bool apply_dof =
        (list.render_flags & EnableDOF) && list.draw_cam.focus_near_mul > 0.0f &&
        list.draw_cam.focus_far_mul > 0.0f && ((list.render_flags & DebugWireframe) == 0);

    if (apply_dof) {
        DebugMarker _("DOF");

        const int qres_w = clean_buf_.w / 4, qres_h = clean_buf_.h / 4;
        glViewport(0, 0, qres_w, qres_h);

        { // prepare coc buffer
            glBindFramebuffer(GL_FRAMEBUFFER, down_buf_coc_[0].fb);
            assert(down_buf_coc_[0].w == qres_w && down_buf_coc_[0].h == qres_h);

            glUseProgram(blit_dof_init_coc_prog_->prog_id());

            glUniform4f(0, 0.0f, 0.0f, 1.0f, 1.0f);
            glUniform4f(1, -list.draw_cam.focus_near_mul,
                        list.draw_cam.focus_distance - 0.5f * list.draw_cam.focus_depth,
                        0.0f, 0.0f);

            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_BASE0_TEX_SLOT,
                                       down_depth_2x_.attachments[0].tex);

            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT,
                           (const GLvoid *)uintptr_t(quad_ndx_offset_));
        }

        { // blur coc buffer
            glBindFramebuffer(GL_FRAMEBUFFER, blur_buf2_.fb);
            assert(blur_buf2_.w == qres_w && blur_buf2_.h == qres_h);

            glUseProgram(blit_gauss_prog_->prog_id());

            glUniform4f(0, 0.0f, 0.0f, float(down_buf_coc_[0].w),
                        float(down_buf_coc_[0].h));
            glUniform1f(1, 0.0f); // horizontal

            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_BASE0_TEX_SLOT,
                                       down_buf_coc_[0].attachments[0].tex);

            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT,
                           (const GLvoid *)uintptr_t(quad_ndx_offset_));

            glBindFramebuffer(GL_FRAMEBUFFER, down_buf_coc_[1].fb);
            assert(down_buf_coc_[1].w == qres_w && down_buf_coc_[1].h == qres_h);

            glUniform4f(0, 0.0f, 0.0f, float(blur_buf2_.w), float(blur_buf2_.h));
            glUniform1f(1, 1.0f); // vertical

            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_BASE0_TEX_SLOT,
                                       blur_buf2_.attachments[0].tex);

            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT,
                           (const GLvoid *)uintptr_t(quad_ndx_offset_));
        }

        { // downsample depth (once more)
            glBindFramebuffer(GL_FRAMEBUFFER, down_depth_4x_.fb);
            assert(down_depth_4x_.w == qres_w && down_depth_4x_.h == qres_h);

            glUseProgram(blit_down_depth_prog_->prog_id());

            glUniform4f(0, 0.0f, 0.0f, float(down_depth_2x_.w), float(down_depth_2x_.h));
            glUniform1f(1, 0.0f); // already linearized

            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_BASE0_TEX_SLOT,
                                       down_depth_2x_.attachments[0].tex);

            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT,
                           (const GLvoid *)uintptr_t(quad_ndx_offset_));
        }

        { // calc near coc
            // TODO: hdr buf is unnecessary here
            glBindFramebuffer(GL_FRAMEBUFFER, blur_buf2_.fb);
            assert(blur_buf2_.w == qres_w && blur_buf2_.h == qres_h);

            glUseProgram(blit_dof_calc_near_prog_->prog_id());

            glUniform4f(0, 0.0f, 0.0f, float(qres_w), float(qres_h));

            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_BASE0_TEX_SLOT,
                                       down_buf_coc_[0].attachments[0].tex);
            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_BASE1_TEX_SLOT,
                                       down_buf_coc_[1].attachments[0].tex);

            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT,
                           (const GLvoid *)uintptr_t(quad_ndx_offset_));
        }

        { // apply 3x3 blur to coc
            glBindFramebuffer(GL_FRAMEBUFFER, down_buf_coc_[0].fb);
            assert(down_buf_coc_[0].w == qres_w && down_buf_coc_[0].h == qres_h);

            glUseProgram(blit_dof_small_blur_prog_->prog_id());

            glUniform4f(0, 0.0f, 0.0f, float(qres_w), float(qres_h));

            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_BASE0_TEX_SLOT,
                                       blur_buf2_.attachments[0].tex);

            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT,
                           (const GLvoid *)uintptr_t(quad_ndx_offset_));
        }

        { // blur color buffer
            glBindFramebuffer(GL_FRAMEBUFFER, blur_buf1_.fb);
            assert(blur_buf1_.w == qres_w && blur_buf1_.h == qres_h);

            glUseProgram(blit_dof_bilateral_prog_->prog_id());

            glUniform4f(0, 0.0f, 0.0f, float(down_buf_4x_.w), float(down_buf_4x_.h));
            glUniform1f(1, 0.0f); // horizontal
            glUniform1f(2, list.draw_cam.focus_distance);

            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_BASE0_TEX_SLOT,
                                       down_depth_4x_.attachments[0].tex);
            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_BASE1_TEX_SLOT,
                                       down_buf_4x_.attachments[0].tex);

            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT,
                           (const GLvoid *)uintptr_t(quad_ndx_offset_));

            glBindFramebuffer(GL_FRAMEBUFFER, blur_buf2_.fb);
            assert(blur_buf2_.w == qres_w && blur_buf2_.h == qres_h);

            glUniform4f(0, 0.0f, 0.0f, float(blur_buf2_.w), float(blur_buf2_.h));
            glUniform1f(1, 1.0f); // vertical

            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_BASE1_TEX_SLOT,
                                       blur_buf1_.attachments[0].tex);

            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT,
                           (const GLvoid *)uintptr_t(quad_ndx_offset_));
        }

        { // apply 3x3 blur to color
            glBindFramebuffer(GL_FRAMEBUFFER, blur_buf1_.fb);
            assert(blur_buf1_.w == qres_w && blur_buf1_.h == qres_h);

            glUseProgram(blit_dof_small_blur_prog_->prog_id());

            glUniform4f(0, 0.0f, 0.0f, float(qres_w), float(qres_h));

            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_BASE0_TEX_SLOT,
                                       down_buf_4x_.attachments[0].tex);

            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT,
                           (const GLvoid *)uintptr_t(quad_ndx_offset_));
        }

        { // combine dof buffers, apply blur
            if (clean_buf_.sample_count > 1) {
                glBindFramebuffer(GL_FRAMEBUFFER, dof_buf_.fb);
                glUseProgram(blit_dof_combine_ms_prog_->prog_id());
            } else {
                glBindFramebuffer(GL_FRAMEBUFFER, resolved_or_transparent_buf_.fb);
                glUseProgram(blit_dof_combine_prog_->prog_id());
            }

            glViewport(0, 0, clean_buf_.w, clean_buf_.h);

            glUniform4f(0, 0.0f, 0.0f, float(clean_buf_.w), float(clean_buf_.h));
            glUniform3f(
                1, list.draw_cam.focus_far_mul,
                -(list.draw_cam.focus_distance + 0.5f * list.draw_cam.focus_depth), 1.0f);

            {                            // calc dof lerp parameters
                const float d0 = 0.333f; // unblurred to small blur distance
                const float d1 = 0.333f; // small to medium blur distance
                const float d2 = 0.333f; // medium to large blur distance

                const auto dof_lerp_scale =
                    Ren::Vec4f{-1.0f / d0, -1.0f / d1, -1.0f / d2, 1.0f / d2};
                const auto dof_lerp_bias =
                    Ren::Vec4f{1.0f, (1.0f - d2) / d1, 1.0f / d2, (d2 - 1.0f) / d2};

                glUniform4fv(2, 1, ValuePtr(dof_lerp_scale));
                glUniform4fv(3, 1, ValuePtr(dof_lerp_bias));
            }

            if (clean_buf_.sample_count > 1) {
                ren_glBindTextureUnit_Comp(
                    GL_TEXTURE_2D, REN_BASE0_TEX_SLOT,
                    resolved_or_transparent_buf_.attachments[0].tex);
                ren_glBindTextureUnit_Comp(GL_TEXTURE_2D_MULTISAMPLE, 3,
                                           clean_buf_.depth_tex.GetValue());
            } else {
                ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_BASE0_TEX_SLOT,
                                           clean_buf_.attachments[0].tex);
                ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, 3,
                                           clean_buf_.depth_tex.GetValue());
            }
            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_BASE1_TEX_SLOT,
                                       blur_buf1_.attachments[0].tex);
            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_BASE2_TEX_SLOT,
                                       blur_buf2_.attachments[0].tex);
            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, 4,
                                       down_buf_coc_[0].attachments[0].tex);

            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT,
                           (const GLvoid *)uintptr_t(quad_ndx_offset_));
        }
    }

    if ((list.render_flags & (EnableSSR | EnableBloom | EnableTonemap)) &&
        ((list.render_flags & DebugWireframe) == 0)) {
        DebugMarker _("BLUR PASS");

        if (list.render_flags & EnableBloom) {
            glBindFramebuffer(GL_FRAMEBUFFER, blur_buf2_.fb);
            glViewport(0, 0, blur_buf2_.w, blur_buf2_.h);

            glUseProgram(blit_gauss_prog_->prog_id());

            glUniform4f(0, 0.0f, 0.0f, float(down_buf_4x_.w), float(down_buf_4x_.h));
            glUniform1f(1, 0.0f); // horizontal

            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_BASE0_TEX_SLOT,
                                       down_buf_4x_.attachments[0].tex);

            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT,
                           (const GLvoid *)uintptr_t(quad_ndx_offset_));

            glBindFramebuffer(GL_FRAMEBUFFER, blur_buf1_.fb);
            glViewport(0, 0, blur_buf1_.w, blur_buf1_.h);

            glUniform4f(0, 0.0f, 0.0f, float(blur_buf2_.w), float(blur_buf2_.h));
            glUniform1f(1, 1.0f); // vertical

            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_BASE0_TEX_SLOT,
                                       blur_buf2_.attachments[0].tex);

            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT,
                           (const GLvoid *)uintptr_t(quad_ndx_offset_));
        }
    }

    assert(!buf_range_fences_[cur_buf_chunk_]);
    buf_range_fences_[cur_buf_chunk_] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

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
            buf_to_sample = &down_buf_4x_;
        }

        glUseProgram(blit_red_prog_->prog_id());

        glUniform4f(0, 0.0f, 0.0f, 1.0f, 1.0f);

        const auto offset_step =
            Ren::Vec2f{1.0f / float(reduced_buf_.w), 1.0f / float(reduced_buf_.h)};

        static int cur_offset = 0;
        glUniform2f(4, 0.5f * poisson_disk[cur_offset][0] * offset_step[0],
                    0.5f * poisson_disk[cur_offset][1] * offset_step[1]);
        cur_offset = (cur_offset + 1) % 64;

        ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_BASE0_TEX_SLOT,
                                   buf_to_sample->attachments[0].tex);

        DebugMarker _("SAMPLE FRAME BRIGHTNESS");

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT,
                       (const GLvoid *)uintptr_t(quad_ndx_offset_));

        const float max_value = 64.0f;
        float lum = 0.0f;

        { // Retrieve result of glReadPixels call from previous frame
            glBindBuffer(GL_PIXEL_PACK_BUFFER, (GLuint)reduce_pbo_[cur_reduce_pbo_]);
            auto *reduced_pixels = (float *)glMapBufferRange(
                GL_PIXEL_PACK_BUFFER, 0,
                4 * reduced_buf_.w * reduced_buf_.h * sizeof(float), GL_MAP_READ_BIT);
            if (reduced_pixels) {
                for (int i = 0; i < 4 * reduced_buf_.w * reduced_buf_.h; i += 4) {
                    if (!std::isnan(reduced_pixels[i])) {
                        lum += std::min(reduced_pixels[i], max_value);
                    }
                }
                glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
            }
            glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        }

        lum /= float(reduced_buf_.w * reduced_buf_.h);

        const float alpha = 1.0f / 64;
        reduced_average_ = alpha * lum + (1.0f - alpha) * reduced_average_;
    }

    float exposure = reduced_average_ > std::numeric_limits<float>::epsilon()
                         ? (1.0f / reduced_average_)
                         : 1.0f;
    exposure = std::min(exposure, list.draw_cam.max_exposure);

    //
    // Blit pass (tonemap buffer / apply fxaa / blit to backbuffer)
    //

    if (list.render_flags & EnableTimers) {
        glQueryCounter(queries_[cur_query_][TimeBlitStart], GL_TIMESTAMP);
    }

    if ((list.render_flags & (EnableFxaa /*| EnableTaa*/)) &&
        !(list.render_flags & DebugWireframe)) {
        glBindFramebuffer(GL_FRAMEBUFFER, combined_buf_.fb);
        glViewport(0, 0, act_w_, act_h_);
    } else {
        if (!target) {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, scr_w_, scr_h_);
        } else {
            glBindFramebuffer(GL_FRAMEBUFFER, target->fb);
            glViewport(0, 0, target->w, target->h);
        }
    }

#if !defined(REN_DIRECT_DRAWING)
    { // Blit main framebuffer
        const Ren::Program *blit_prog = blit_combine_prog_.get();
        glUseProgram(blit_prog->prog_id());

        glUniform4f(0, 0.0f, 0.0f, float(act_w_), float(act_h_));

        glUniform1f(12, (list.render_flags & EnableTonemap) ? 1.0f : 0.0f);
        glUniform2f(13, float(act_w_), float(act_h_));

        glUniform1f(U_GAMMA, (list.render_flags & DebugLights) ? 1.0f : 2.2f);
        glUniform1f(U_EXPOSURE, exposure);

        if (clean_buf_.sample_count > 1 || ((list.render_flags & EnableTaa) != 0) ||
            apply_dof) {
            if (apply_dof) {
                ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_BASE0_TEX_SLOT,
                                           dof_buf_.attachments[0].tex);
            } else {
                ren_glBindTextureUnit_Comp(
                    GL_TEXTURE_2D, REN_BASE0_TEX_SLOT,
                    resolved_or_transparent_buf_.attachments[0].tex);
            }
        } else {
            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_BASE0_TEX_SLOT,
                                       clean_buf_.attachments[REN_OUT_COLOR_INDEX].tex);
        }

        if ((list.render_flags & EnableBloom) && !(list.render_flags & DebugWireframe)) {
            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_BASE1_TEX_SLOT,
                                       blur_buf1_.attachments[0].tex);
        } else {
            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_BASE1_TEX_SLOT,
                                       dummy_black_->tex_id());
        }

#ifndef DISABLE_MARKERS
        glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "FINAL BLIT");
#endif

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT,
                       (const GLvoid *)uintptr_t(quad_ndx_offset_));
    }

    if ((list.render_flags & EnableFxaa) && !(list.render_flags & DebugWireframe)) {
        if (!target) {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, scr_w_, scr_h_);
        } else {
            glBindFramebuffer(GL_FRAMEBUFFER, target->fb);
            glViewport(0, 0, target->w, target->h);
        }

        { // Blit fxaa
            const Ren::Program *blit_prog = blit_fxaa_prog_.get();
            glUseProgram(blit_prog->prog_id());

            glUniform4f(0, 0.0f, 0.0f, 1.0f, 1.0f);

            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_BASE0_TEX_SLOT,
                                       combined_buf_.attachments[0].tex);

            glUniform2f(12, 1.0f / float(scr_w_), 1.0f / float(scr_h_));

            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT,
                           (const GLvoid *)uintptr_t(quad_ndx_offset_));
        }
    }

#ifndef DISABLE_MARKERS
    glPopDebugGroup();
#endif
#endif // !defined(REN_DIRECT_DRAWING)

    if (list.render_flags & EnableTonemap) {
        // Start asynchronous memory read from framebuffer
        glBindFramebuffer(GL_FRAMEBUFFER, reduced_buf_.fb);
        glReadBuffer(GL_COLOR_ATTACHMENT0);

        glBindBuffer(GL_PIXEL_PACK_BUFFER, (GLuint)reduce_pbo_[cur_reduce_pbo_]);

        glReadPixels(0, 0, reduced_buf_.w, reduced_buf_.h, GL_RGBA, GL_FLOAT, nullptr);

        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        cur_reduce_pbo_ = (cur_reduce_pbo_ + 1) % FrameSyncWindow;
    }

    //
    // Debugging (draw auxiliary surfaces)
    //

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

        glUniform4f(0, 0.0f, 0.0f, float(act_w_), float(act_h_));

        glUniform2i(U_RES, scr_w_, scr_h_);

        if (list.render_flags & DebugLights) {
            glUniform1i(16, 0);
        } else if (list.render_flags & DebugDecals) {
            glUniform1i(16, 1);
        }

        glUniform4fv(17, 1, Ren::ValuePtr(shrd_data.uClipInfo));

        if (clean_buf_.sample_count > 1) {
            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D_MULTISAMPLE, REN_BASE0_TEX_SLOT,
                                       clean_buf_.depth_tex.GetValue());
        } else {
            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_BASE0_TEX_SLOT,
                                       clean_buf_.depth_tex.GetValue());
        }

        ren_glBindTextureUnit_Comp(GL_TEXTURE_BUFFER, REN_CELLS_BUF_SLOT,
                                   cells_tbo_[cur_buf_chunk_]);
        ren_glBindTextureUnit_Comp(GL_TEXTURE_BUFFER, REN_ITEMS_BUF_SLOT,
                                   items_tbo_[cur_buf_chunk_]);

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT,
                       (const GLvoid *)uintptr_t(quad_ndx_offset_));

        glDisable(GL_BLEND);
    }

    if (((list.render_flags & (EnableCulling | DebugCulling)) ==
         (EnableCulling | DebugCulling)) &&
        !depth_pixels_[0].empty()) {
        glUseProgram(blit_prog_->prog_id());

        glUniform4f(0, 0.0f, 0.0f, 256.0f, 128.0f);

        const float sx = 2 * 256.0f / float(scr_w_), sy = 2 * 128.0f / float(scr_h_);

        // const float positions[] = {
        //    -1.0f, -1.0f,               -1.0f + sx, -1.0f,
        //    -1.0f + sx, -1.0f + sy,     -1.0f, -1.0f + sy
        //};

        glUniform1f(4, 1.0f);

        ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_BASE0_TEX_SLOT, temp_tex_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 128, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     &depth_pixels_[0][0]);

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT,
                       (const GLvoid *)uintptr_t(quad_ndx_offset_));

        /////

        const float positions2[] = {-1.0f + sx, -1.0f,           -1.0f + sx + sx,
                                    -1.0f,      -1.0f + sx + sx, -1.0f + sy,
                                    -1.0f + sx, -1.0f + sy};

        glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)temp_buf1_vtx_offset_,
                        sizeof(positions2), positions2);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 128, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     &depth_tiles_[0][0]);

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT,
                       (const GLvoid *)uintptr_t(quad_ndx_offset_));

        glDisableVertexAttribArray(REN_VTX_POS_LOC);
        glDisableVertexAttribArray(REN_VTX_UV1_LOC);
    }

    if (list.render_flags & DebugShadow) {
        glBindVertexArray(temp_vao_);

        glUseProgram(blit_depth_prog_->prog_id());

        glUniform4f(0, 0.0f, 0.0f, 1.0f, 1.0f);

        glBindBuffer(GL_ARRAY_BUFFER, last_vertex_buf1_);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, last_index_buffer_);

        glEnableVertexAttribArray(REN_VTX_POS_LOC);
        glVertexAttribPointer(REN_VTX_POS_LOC, 2, GL_FLOAT, GL_FALSE, 0,
                              (const GLvoid *)uintptr_t(temp_buf1_vtx_offset_));

        glEnableVertexAttribArray(REN_VTX_UV1_LOC);
        glVertexAttribPointer(
            REN_VTX_UV1_LOC, 2, GL_FLOAT, GL_FALSE, 0,
            (const GLvoid *)uintptr_t(temp_buf1_vtx_offset_ + 8 * sizeof(float)));

        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, (GLintptr)temp_buf_ndx_offset_,
                        sizeof(fs_quad_indices), fs_quad_indices);

        ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_BASE0_TEX_SLOT,
                                   shadow_buf_.depth_tex.GetValue());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);

        const float k = (float(shadow_buf_.h) / float(shadow_buf_.w)) *
                        (float(scr_w_) / float(scr_h_));

        { // Clear region
            glEnable(GL_SCISSOR_TEST);

            glScissor(0, 0, scr_w_ / 2, int(k * float(scr_h_) / 2));
            glClear(GL_COLOR_BUFFER_BIT);

            glDisable(GL_SCISSOR_TEST);
        }

        // Draw visible shadow regions
        for (int i = 0; i < (int)list.shadow_lists.count; i++) {
            const ShadowList &sh_list = list.shadow_lists.data[i];
            const ShadowMapRegion &reg = list.shadow_regions.data[i];

            const float positions[] = {-1.0f + reg.transform[0],
                                       -1.0f + reg.transform[1] * k,
                                       -1.0f + reg.transform[0] + reg.transform[2],
                                       -1.0f + reg.transform[1] * k,
                                       -1.0f + reg.transform[0] + reg.transform[2],
                                       -1.0f + (reg.transform[1] + reg.transform[3]) * k,
                                       -1.0f + reg.transform[0],
                                       -1.0f + (reg.transform[1] + reg.transform[3]) * k};

            const float uvs[] = {
                float(sh_list.shadow_map_pos[0]),
                float(sh_list.shadow_map_pos[1]),
                float(sh_list.shadow_map_pos[0] + sh_list.shadow_map_size[0]),
                float(sh_list.shadow_map_pos[1]),
                float(sh_list.shadow_map_pos[0] + sh_list.shadow_map_size[0]),
                float(sh_list.shadow_map_pos[1] + sh_list.shadow_map_size[1]),
                float(sh_list.shadow_map_pos[0]),
                float(sh_list.shadow_map_pos[1] + sh_list.shadow_map_size[1])};

            glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)temp_buf1_vtx_offset_,
                            sizeof(positions), positions);
            glBufferSubData(GL_ARRAY_BUFFER,
                            (GLintptr)(temp_buf1_vtx_offset_ + sizeof(positions)),
                            sizeof(uvs), uvs);

            glUniform1f(1, sh_list.cam_near);
            glUniform1f(2, sh_list.cam_far);

            if (sh_list.shadow_batch_count) {
                // mark updated region with red
                glUniform3f(3, 1.0f, 0.5f, 0.5f);
            } else {
                // mark cached region with green
                glUniform3f(3, 0.5f, 1.0f, 0.5f);
            }

            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT,
                           (const GLvoid *)uintptr_t(temp_buf_ndx_offset_));
        }

        // Draw invisible cached shadow regions
        for (int i = 0; i < (int)list.cached_shadow_regions.count; i++) {
            const ShadReg &r = list.cached_shadow_regions.data[i];

            const float positions[] = {
                -1.0f + float(r.pos[0]) / SHADOWMAP_WIDTH,
                -1.0f + k * float(r.pos[1]) / SHADOWMAP_HEIGHT,
                -1.0f + float(r.pos[0] + r.size[0]) / SHADOWMAP_WIDTH,
                -1.0f + k * float(r.pos[1]) / SHADOWMAP_HEIGHT,
                -1.0f + float(r.pos[0] + r.size[0]) / SHADOWMAP_WIDTH,
                -1.0f + k * float(r.pos[1] + r.size[1]) / SHADOWMAP_HEIGHT,
                -1.0f + float(r.pos[0]) / SHADOWMAP_WIDTH,
                -1.0f + k * float(r.pos[1] + r.size[1]) / SHADOWMAP_HEIGHT};

            const float uvs[] = {float(r.pos[0]),
                                 float(r.pos[1]),
                                 float(r.pos[0] + r.size[0]),
                                 float(r.pos[1]),
                                 float(r.pos[0] + r.size[0]),
                                 float(r.pos[1] + r.size[1]),
                                 float(r.pos[0]),
                                 float(r.pos[1] + r.size[1])};

            glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)temp_buf1_vtx_offset_,
                            sizeof(positions), positions);
            glBufferSubData(GL_ARRAY_BUFFER,
                            (GLintptr)(temp_buf1_vtx_offset_ + sizeof(positions)),
                            sizeof(uvs), uvs);

            glUniform1f(1, r.cam_near);
            glUniform1f(2, r.cam_far);

            // mark cached region with blue
            glUniform3f(3, 0.5f, 0.5f, 1.0f);

            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT,
                           (const GLvoid *)uintptr_t(temp_buf_ndx_offset_));
        }

        // Draw view frustum edges
        for (int i = 0; i < (int)list.shadow_lists.count; i++) {
            const ShadowList &sh_list = list.shadow_lists.data[i];
            const ShadowMapRegion &reg = list.shadow_regions.data[i];

            if (!sh_list.view_frustum_outline_count)
                continue;

            for (int j = 0; j < sh_list.view_frustum_outline_count; j += 2) {
                const Ren::Vec2f &p1 = sh_list.view_frustum_outline[j],
                                 &p2 = sh_list.view_frustum_outline[j + 1];

                const float positions[] = {
                    -1.0f + reg.transform[0] + (p1[0] * 0.5f + 0.5f) * reg.transform[2],
                    -1.0f +
                        (reg.transform[1] + (p1[1] * 0.5f + 0.5f) * reg.transform[3]) * k,
                    -1.0f + reg.transform[0] + (p2[0] * 0.5f + 0.5f) * reg.transform[2],
                    -1.0f +
                        (reg.transform[1] + (p2[1] * 0.5f + 0.5f) * reg.transform[3]) * k,
                };

                glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)temp_buf1_vtx_offset_,
                                sizeof(positions), positions);

                // draw line with black color
                glUniform3f(3, 0.0f, 0.0f, 0.0f);

                glDrawElements(GL_LINES, 2, GL_UNSIGNED_SHORT,
                               (const GLvoid *)uintptr_t(temp_buf_ndx_offset_));
            }
        }

        // Restore compare mode
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE,
                        GL_COMPARE_REF_TO_TEXTURE);

        glDisableVertexAttribArray(REN_VTX_POS_LOC);
        glDisableVertexAttribArray(REN_VTX_UV1_LOC);
    }

    glBindVertexArray(temp_vao_);

    if (list.render_flags & DebugReduce) {
        BlitBuffer(-1.0f, -1.0f, 0.5f, 0.5f, reduced_buf_, 0, 1, 10.0f);
    }

    if (list.render_flags & DebugDeferred) {
        BlitBuffer(-1.0f, -1.0f, 0.5f, 0.5f, clean_buf_, 1, 2);

        float exposure = reduced_average_ > std::numeric_limits<float>::epsilon()
                             ? (1.0f / reduced_average_)
                             : 1.0f;
        exposure = std::min(exposure, list.draw_cam.max_exposure);
        BlitBuffer(0.0f, -1.0f, 0.5f, 0.5f, down_buf_4x_, 0, 1, exposure);
    }

    if (list.render_flags & DebugBlur) {
        BlitBuffer(-1.0f, -1.0f, 1.0f, 1.0f, blur_buf1_, 0, 1, 400.0f);
    }

    if (list.render_flags & DebugSSAO) {
        BlitBuffer(-1.0f, -1.0f, 1.0f, 1.0f, ssao_buf1_, 0, 1);
    }

    if ((list.render_flags & DebugDecals) && list.decals_atlas) {
        int resx = list.decals_atlas->resx(), resy = list.decals_atlas->resy();

        float k = float(scr_w_) / float(scr_h_);
        k *= float(resy) / float(resx);

        BlitTexture(-1.0f, -1.0f, 1.0f, 1.0f * k, list.decals_atlas->tex_id(0), resx,
                    resy);
    }

    if (list.render_flags & DebugBVH) {
        if (!nodes_buf_) {
            GLuint nodes_buf;
            glGenBuffers(1, &nodes_buf);

            glBindBuffer(GL_TEXTURE_BUFFER, (GLuint)nodes_buf);
            glBufferData(GL_TEXTURE_BUFFER, list.temp_nodes.size() * sizeof(bvh_node_t),
                         list.temp_nodes.data(), GL_DYNAMIC_DRAW);

            nodes_buf_ = (uint32_t)nodes_buf;

            GLuint nodes_tbo;

            glGenTextures(1, &nodes_tbo);
            glBindTexture(GL_TEXTURE_BUFFER, nodes_tbo);

            glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, nodes_buf);
            glBindTexture(GL_TEXTURE_BUFFER, 0);

            nodes_tbo_ = (uint32_t)nodes_tbo;
        } else {
            glBindBuffer(GL_TEXTURE_BUFFER, (GLuint)nodes_buf_);
            glBufferData(GL_TEXTURE_BUFFER, list.temp_nodes.size() * sizeof(bvh_node_t),
                         list.temp_nodes.data(), GL_DYNAMIC_DRAW);
        }

        {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            const Ren::Program *debug_bvh_prog = nullptr;

            debug_bvh_prog = blit_debug_bvh_ms_prog_.get();
            glUseProgram(debug_bvh_prog->prog_id());

            glUniform4f(0, 0.0f, 0.0f, 1.0f, 1.0f);

            const float uvs[] = {0.0f,          0.0f, float(scr_w_), 0.0f, float(scr_w_),
                                 float(scr_h_), 0.0f, float(scr_h_)};

            glBindBuffer(GL_ARRAY_BUFFER, last_vertex_buf1_);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, last_index_buffer_);

            glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)temp_buf1_vtx_offset_,
                            sizeof(fs_quad_positions), fs_quad_positions);
            glBufferSubData(GL_ARRAY_BUFFER,
                            (GLintptr)(temp_buf1_vtx_offset_ + sizeof(fs_quad_positions)),
                            sizeof(uvs), uvs);
            glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, (GLintptr)temp_buf_ndx_offset_,
                            sizeof(fs_quad_indices), fs_quad_indices);

            glEnableVertexAttribArray(REN_VTX_POS_LOC);
            glVertexAttribPointer(REN_VTX_POS_LOC, 2, GL_FLOAT, GL_FALSE, 0,
                                  (const GLvoid *)uintptr_t(temp_buf1_vtx_offset_));

            glEnableVertexAttribArray(REN_VTX_UV1_LOC);
            glVertexAttribPointer(REN_VTX_UV1_LOC, 2, GL_FLOAT, GL_FALSE, 0,
                                  (const GLvoid *)uintptr_t(temp_buf1_vtx_offset_ +
                                                            sizeof(fs_quad_positions)));

            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D_MULTISAMPLE, 0,
                                       clean_buf_.depth_tex.GetValue());

            ren_glBindTextureUnit_Comp(GL_TEXTURE_BUFFER, 1, nodes_tbo_);

            glUniform1i(debug_bvh_prog->uniform("uRootIndex").loc, list.root_index);

            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT,
                           (const GLvoid *)uintptr_t(temp_buf_ndx_offset_));

            glDisableVertexAttribArray(REN_VTX_POS_LOC);
            glDisableVertexAttribArray(REN_VTX_UV1_LOC);

            glDisable(GL_BLEND);
        }

        glBindBuffer(GL_TEXTURE_BUFFER, 0);
    }

    glBindVertexArray(0);

    if (list.render_flags & EnableTimers) {
        glQueryCounter(queries_[cur_query_][TimeDrawEnd], GL_TIMESTAMP);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, scr_w_, scr_h_);

    //
    // Retrieve debug timers result
    //

    if (list.render_flags & EnableTimers) {
        // Get timer queries result (for previous frame)

        GLuint64 time_draw_start, time_skinning_start, time_shadow_start,
            time_depth_opaque_start, time_ao_start, time_opaque_start, time_transp_start,
            time_refl_start, time_taa_start, time_blur_start, time_blit_start,
            time_draw_end;

        cur_query_ = (cur_query_ + 1) % FrameSyncWindow;

        glGetQueryObjectui64v(queries_[cur_query_][TimeDrawStart], GL_QUERY_RESULT,
                              &time_draw_start);
        glGetQueryObjectui64v(queries_[cur_query_][TimeSkinningStart], GL_QUERY_RESULT,
                              &time_skinning_start);
        glGetQueryObjectui64v(queries_[cur_query_][TimeShadowMapStart], GL_QUERY_RESULT,
                              &time_shadow_start);
        glGetQueryObjectui64v(queries_[cur_query_][TimeDepthOpaqueStart], GL_QUERY_RESULT,
                              &time_depth_opaque_start);
        glGetQueryObjectui64v(queries_[cur_query_][TimeAOPassStart], GL_QUERY_RESULT,
                              &time_ao_start);
        glGetQueryObjectui64v(queries_[cur_query_][TimeOpaqueStart], GL_QUERY_RESULT,
                              &time_opaque_start);
        glGetQueryObjectui64v(queries_[cur_query_][TimeTranspStart], GL_QUERY_RESULT,
                              &time_transp_start);
        glGetQueryObjectui64v(queries_[cur_query_][TimeReflStart], GL_QUERY_RESULT,
                              &time_refl_start);
        glGetQueryObjectui64v(queries_[cur_query_][TimeTaaStart], GL_QUERY_RESULT,
                              &time_taa_start);
        glGetQueryObjectui64v(queries_[cur_query_][TimeBlurStart], GL_QUERY_RESULT,
                              &time_blur_start);
        glGetQueryObjectui64v(queries_[cur_query_][TimeBlitStart], GL_QUERY_RESULT,
                              &time_blit_start);
        glGetQueryObjectui64v(queries_[cur_query_][TimeDrawEnd], GL_QUERY_RESULT,
                              &time_draw_end);

        // assign values from previous frame
        backend_info_.cpu_start_timepoint_us = backend_cpu_start_;
        backend_info_.cpu_end_timepoint_us = backend_cpu_end_;
        backend_info_.gpu_cpu_time_diff_us = backend_time_diff_;

        backend_info_.gpu_start_timepoint_us = uint64_t(time_draw_start / 1000);
        backend_info_.gpu_end_timepoint_us = uint64_t(time_draw_end / 1000);

        backend_info_.skinning_time_us =
            uint32_t((time_shadow_start - time_skinning_start) / 1000);
        backend_info_.shadow_time_us =
            uint32_t((time_depth_opaque_start - time_shadow_start) / 1000);
        backend_info_.depth_opaque_pass_time_us =
            uint32_t((time_ao_start - time_depth_opaque_start) / 1000);
        backend_info_.ao_pass_time_us =
            uint32_t((time_opaque_start - time_ao_start) / 1000);
        backend_info_.opaque_pass_time_us =
            uint32_t((time_transp_start - time_opaque_start) / 1000);
        backend_info_.transp_pass_time_us =
            uint32_t((time_refl_start - time_transp_start) / 1000);
        backend_info_.refl_pass_time_us =
            uint32_t((time_taa_start - time_refl_start) / 1000);
        backend_info_.taa_pass_time_us =
            uint32_t((time_blur_start - time_taa_start) / 1000);
        backend_info_.blur_pass_time_us =
            uint32_t((time_blit_start - time_blur_start) / 1000);
        backend_info_.blit_pass_time_us =
            uint32_t((time_draw_end - time_blit_start) / 1000);
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

void Renderer::BlitPixels(const void *data, int w, int h, const Ren::eTexFormat format) {
    using namespace RendererInternal;

    if (temp_tex_w_ != w || temp_tex_h_ != h || temp_tex_format_ != format) {
        if (temp_tex_w_ != 0 && temp_tex_h_ != 0) {
            auto gl_tex = (GLuint)temp_tex_;
            glDeleteTextures(1, &gl_tex);
        }

        GLuint new_tex;
        glGenTextures(1, &new_tex);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, new_tex);

        if (format == Ren::eTexFormat::RawRGBA32F) {
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

        if (format == Ren::eTexFormat::RawRGBA32F) {
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_FLOAT, data);
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, scr_w_, scr_h_);

    glBindVertexArray((GLuint)temp_vao_);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    BlitTexture(-1.0f, 1.0f, 2.0f, -2.0f, temp_tex_, w, h);

    glBindVertexArray(0);
}

void Renderer::BlitPixelsTonemap(const void *data, int w, int h,
                                 const Ren::eTexFormat format) {
    using namespace RendererInternal;

    if (temp_tex_w_ != w || temp_tex_h_ != h || temp_tex_format_ != format) {
        if (temp_tex_w_ != 0 && temp_tex_h_ != 0) {
            auto gl_tex = (GLuint)temp_tex_;
            glDeleteTextures(1, &gl_tex);
        }

        GLuint new_tex;
        glGenTextures(1, &new_tex);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, new_tex);

        if (format == Ren::eTexFormat::RawRGBA32F) {
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

        if (format == Ren::eTexFormat::RawRGBA32F) {
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_FLOAT, data);
        }
    }

    assert(format == Ren::eTexFormat::RawRGBA32F);

    Ren::Vec3f avarage_color;
    int sample_count = 0;
    const auto *_data = (const float *)data;

    for (int y = 0; y < h; y += 100) {
        for (int x = 0; x < w; x += 100) {
            int i = y * w + x;
            avarage_color += Ren::MakeVec3(&_data[i * 4 + 0]);
            sample_count++;
        }
    }

    avarage_color /= float(sample_count);

    const float lum = Dot(avarage_color, Ren::Vec3f{0.299f, 0.587f, 0.114f});

    const float alpha = 0.25f;
    reduced_average_ = alpha * lum + (1.0f - alpha) * reduced_average_;

    glBindVertexArray(fs_quad_vao_);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    { // prepare downsampled buffer
        glBindFramebuffer(GL_FRAMEBUFFER, down_buf_4x_.fb);
        glViewport(0, 0, down_buf_4x_.w, down_buf_4x_.h);

        const Ren::Program *cur_program = blit_down_prog_.get();
        glUseProgram(cur_program->prog_id());

        glUniform4f(0, 0.0f, 0.0f, float(w), float(h));

        ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_BASE0_TEX_SLOT, temp_tex_);

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT,
                       (const GLvoid *)uintptr_t(quad_ndx_offset_));
    }

    { // prepare blurred buffer
        glBindFramebuffer(GL_FRAMEBUFFER, blur_buf2_.fb);
        glViewport(0, 0, blur_buf2_.w, blur_buf2_.h);

        glClear(GL_COLOR_BUFFER_BIT);

        const Ren::Program *cur_program = blit_gauss_prog_.get();
        glUseProgram(cur_program->prog_id());

        glUniform4f(0, 0.0f, 0.0f, float(blur_buf2_.w), float(blur_buf2_.h));
        glUniform1f(1, 0.0f); // horizontal

        ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_BASE0_TEX_SLOT,
                                   down_buf_4x_.attachments[0].tex);

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT,
                       (const GLvoid *)uintptr_t(quad_ndx_offset_));

        glUniform4f(0, 0.0f, 0.0f, float(blur_buf2_.w), float(blur_buf2_.h));
        glUniform1f(1, 1.0f); // vertical

        glBindFramebuffer(GL_FRAMEBUFFER, blur_buf1_.fb);
        glViewport(0, 0, blur_buf1_.w, blur_buf1_.h);

        ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_BASE0_TEX_SLOT,
                                   blur_buf2_.attachments[0].tex);

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT,
                       (const GLvoid *)uintptr_t(quad_ndx_offset_));
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, scr_w_, scr_h_);

    { // combine buffers
        const Ren::Program *cur_program = blit_combine_prog_.get();
        glUseProgram(cur_program->prog_id());

        // vertically flipped
        glUniform4f(0, 0.0f, float(h), float(w), -float(h));
        glUniform1f(12, 1.0f);
        glUniform2f(13, float(w), float(h));
        glUniform1f(U_GAMMA, 2.2f);

        float exposure = 1.0f / reduced_average_;
        exposure = std::min(exposure, 1000.0f);

        glUniform1f(U_EXPOSURE, exposure);

        ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_BASE0_TEX_SLOT, temp_tex_);
        ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_BASE1_TEX_SLOT,
                                   blur_buf1_.attachments[0].tex);

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT,
                       (const GLvoid *)uintptr_t(quad_ndx_offset_));
    }

    glBindVertexArray(0);
}

void Renderer::BlitBuffer(float px, float py, float sx, float sy, const FrameBuf &buf,
                          int first_att, int att_count, float multiplier) {
    using namespace RendererInternal;

    glBindVertexArray(temp_vao_);

    Ren::Program *cur_program = nullptr;

    if (buf.sample_count > 1) {
        cur_program = blit_ms_prog_.get();
    } else {
        cur_program = blit_prog_.get();
    }
    glUseProgram(cur_program->prog_id());

    glUniform4f(0, 0.0f, 0.0f, 1.0f, 1.0f);

    for (int i = first_att; i < first_att + att_count; i++) {
        const float positions[] = {px + float(i - first_att) * sx,     py,
                                   px + float(i - first_att + 1) * sx, py,
                                   px + float(i - first_att + 1) * sx, py + sy,
                                   px + float(i - first_att) * sx,     py + sy};

        if (i == first_att) {
            const float uvs[] = {0.0f,         0.0f,         (float)buf.w, 0.0f,
                                 (float)buf.w, (float)buf.h, 0.0f,         (float)buf.h};

            glBindBuffer(GL_ARRAY_BUFFER, last_vertex_buf1_);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, last_index_buffer_);

            glBufferSubData(GL_ARRAY_BUFFER,
                            (GLintptr)(temp_buf1_vtx_offset_ + sizeof(positions)),
                            sizeof(uvs), uvs);
            glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, (GLintptr)temp_buf_ndx_offset_,
                            sizeof(fs_quad_indices), fs_quad_indices);

            glEnableVertexAttribArray(REN_VTX_POS_LOC);
            glVertexAttribPointer(REN_VTX_POS_LOC, 2, GL_FLOAT, GL_FALSE, 0,
                                  (const GLvoid *)uintptr_t(temp_buf1_vtx_offset_));

            glEnableVertexAttribArray(REN_VTX_UV1_LOC);
            glVertexAttribPointer(
                REN_VTX_UV1_LOC, 2, GL_FLOAT, GL_FALSE, 0,
                (const GLvoid *)uintptr_t(temp_buf1_vtx_offset_ + sizeof(positions)));

            glUniform1f(4, multiplier);
        }

        glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)temp_buf1_vtx_offset_,
                        sizeof(positions), positions);

        if (buf.sample_count > 1) {
            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D_MULTISAMPLE, REN_BASE0_TEX_SLOT,
                                       buf.attachments[i].tex);
        } else {
            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_BASE0_TEX_SLOT,
                                       buf.attachments[i].tex);
        }

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT,
                       (const GLvoid *)uintptr_t(temp_buf_ndx_offset_));
    }

    glDisableVertexAttribArray(REN_VTX_POS_LOC);
    glDisableVertexAttribArray(REN_VTX_UV1_LOC);
}

void Renderer::BlitTexture(float px, float py, float sx, float sy, uint32_t tex_id,
                           int resx, int resy, bool is_ms) {
    using namespace RendererInternal;

    Ren::Program *cur_program = nullptr;

    if (is_ms) {
        cur_program = blit_ms_prog_.get();
    } else {
        cur_program = blit_prog_.get();
    }
    glUseProgram(cur_program->prog_id());

    glUniform4f(0, 0.0f, 0.0f, 1.0f, 1.0f);

    {
        const float positions[] = {px, py, px + sx, py, px + sx, py + sy, px, py + sy};

        const float uvs[] = {0.0f,        0.0f,        (float)resx, 0.0f,
                             (float)resx, (float)resy, 0.0f,        (float)resy};

        uint16_t indices[] = {0, 1, 2, 0, 2, 3};

        if (sy < 0.0f) {
            // keep counter-clockwise winding order
            std::swap(indices[0], indices[2]);
            std::swap(indices[3], indices[5]);
        }

        glBindBuffer(GL_ARRAY_BUFFER, last_vertex_buf1_);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, last_index_buffer_);

        glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)temp_buf1_vtx_offset_,
                        sizeof(positions), positions);
        glBufferSubData(GL_ARRAY_BUFFER,
                        (GLintptr)(temp_buf1_vtx_offset_ + sizeof(positions)),
                        sizeof(uvs), uvs);
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, (GLintptr)temp_buf_ndx_offset_,
                        sizeof(indices), indices);

        glEnableVertexAttribArray(REN_VTX_POS_LOC);
        glVertexAttribPointer(REN_VTX_POS_LOC, 2, GL_FLOAT, GL_FALSE, 0,
                              (const GLvoid *)uintptr_t(temp_buf1_vtx_offset_));

        glEnableVertexAttribArray(REN_VTX_UV1_LOC);
        glVertexAttribPointer(
            REN_VTX_UV1_LOC, 2, GL_FLOAT, GL_FALSE, 0,
            (const GLvoid *)uintptr_t(temp_buf1_vtx_offset_ + sizeof(positions)));

        glUniform1f(4, 1.0f);

        if (is_ms) {
            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D_MULTISAMPLE, REN_BASE0_TEX_SLOT,
                                       tex_id);
        } else {
            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, REN_BASE0_TEX_SLOT, tex_id);
        }

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT,
                       (const GLvoid *)uintptr_t(temp_buf_ndx_offset_));
    }

    glDisableVertexAttribArray(REN_VTX_POS_LOC);
    glDisableVertexAttribArray(REN_VTX_UV1_LOC);
}

void Renderer::BlitToTempProbeFace(const FrameBuf &src_buf, const ProbeStorage &dst_store,
                                   int face) {
    using namespace RendererInternal;

    GLint framebuf_before;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuf_before);

    GLint viewport_before[4];
    glGetIntegerv(GL_VIEWPORT, viewport_before);

    auto framebuf = (GLuint)temp_framebuf_;
    glBindFramebuffer(GL_FRAMEBUFFER, framebuf);

    int temp_probe_index = dst_store.reserved_temp_layer();

    auto cube_array = (GLuint)dst_store.tex_id();
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, cube_array, 0,
                              (GLint)(temp_probe_index * 6 + face));
    assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

    glViewport(0, 0, (GLint)dst_store.res(), (GLint)dst_store.res());

    glDisable(GL_BLEND);

    glBindVertexArray((GLuint)temp_vao_);

    glBindBuffer(GL_ARRAY_BUFFER, last_vertex_buf1_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, last_index_buffer_);

    glEnableVertexAttribArray(REN_VTX_POS_LOC);
    glVertexAttribPointer(REN_VTX_POS_LOC, 2, GL_FLOAT, GL_FALSE, 0,
                          (const GLvoid *)uintptr_t(temp_buf1_vtx_offset_));

    glEnableVertexAttribArray(REN_VTX_UV1_LOC);
    glVertexAttribPointer(
        REN_VTX_UV1_LOC, 2, GL_FLOAT, GL_FALSE, 0,
        (const GLvoid *)uintptr_t(temp_buf1_vtx_offset_ + sizeof(fs_quad_positions)));

    glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)temp_buf1_vtx_offset_,
                    sizeof(fs_quad_positions), fs_quad_positions);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, (GLintptr)temp_buf_ndx_offset_,
                    sizeof(fs_quad_indices), fs_quad_indices);

    { // Update first mip level of a cubemap
        Ren::Program *prog = blit_rgbm_prog_.get();
        glUseProgram(prog->prog_id());

        glUniform4f(0, 0.0f, 0.0f, 1.0f, 1.0f);

        const float uvs[] = {0.0f,
                             0.0f,
                             (float)src_buf.w,
                             0.0f,
                             (float)src_buf.w,
                             (float)src_buf.h,
                             0.0f,
                             (float)src_buf.h};

        glBufferSubData(GL_ARRAY_BUFFER,
                        (GLintptr)(temp_buf1_vtx_offset_ + sizeof(fs_quad_positions)),
                        sizeof(uvs), uvs);

        ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, 0, src_buf.attachments[0].tex);

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT,
                       (const GLvoid *)uintptr_t(temp_buf_ndx_offset_));
    }

    { // Update rest of mipmaps
        Ren::Program *prog = blit_mipmap_prog_.get();
        glUseProgram(prog->prog_id());

        glUniform4f(0, 0.0f, 0.0f, 1.0f, 1.0f);

        glUniform1f(1, float(temp_probe_index));
        glUniform1i(2, face);

        const float uvs[] = {-1.0f, 1.0f, -1.0, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f};

        glBufferSubData(GL_ARRAY_BUFFER,
                        (GLintptr)(temp_buf1_vtx_offset_ + sizeof(fs_quad_positions)),
                        sizeof(uvs), uvs);

        ren_glBindTextureUnit_Comp(GL_TEXTURE_CUBE_MAP_ARRAY, 0, cube_array);

        int res = dst_store.res() / 2;
        int level = 1;

        while (level <= dst_store.max_level()) {
            glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, cube_array,
                                      level, (GLint)(temp_probe_index * 6 + face));
            assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

            glViewport(0, 0, res, res);

            glUniform1f(3, float(level - 1));

            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT,
                           (const GLvoid *)uintptr_t(temp_buf_ndx_offset_));

            res /= 2;
            level++;
        }
    }

    glDisableVertexAttribArray(REN_VTX_POS_LOC);
    glDisableVertexAttribArray(REN_VTX_UV1_LOC);

    glBindVertexArray(0);

    glBindFramebuffer(GL_FRAMEBUFFER, framebuf_before);
    glViewport(viewport_before[0], viewport_before[1], viewport_before[2],
               viewport_before[3]);
}

void Renderer::BlitPrefilterFromTemp(const ProbeStorage &dst_store, int probe_index) {
    using namespace RendererInternal;

    GLint framebuf_before;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuf_before);

    GLint viewport_before[4];
    glGetIntegerv(GL_VIEWPORT, viewport_before);

    auto framebuf = (GLuint)temp_framebuf_;
    glBindFramebuffer(GL_FRAMEBUFFER, framebuf);

    int temp_probe_index = dst_store.reserved_temp_layer();

    auto cube_array = (GLuint)dst_store.tex_id();

    glDisable(GL_BLEND);

    glBindVertexArray((GLuint)temp_vao_);

    glBindBuffer(GL_ARRAY_BUFFER, last_vertex_buf1_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, last_index_buffer_);

    glEnableVertexAttribArray(REN_VTX_POS_LOC);
    glVertexAttribPointer(REN_VTX_POS_LOC, 2, GL_FLOAT, GL_FALSE, 0,
                          (const GLvoid *)uintptr_t(temp_buf1_vtx_offset_));

    glEnableVertexAttribArray(REN_VTX_UV1_LOC);
    glVertexAttribPointer(
        REN_VTX_UV1_LOC, 2, GL_FLOAT, GL_FALSE, 0,
        (const GLvoid *)uintptr_t(temp_buf1_vtx_offset_ + sizeof(fs_quad_positions)));

    glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)temp_buf1_vtx_offset_,
                    sizeof(fs_quad_positions), fs_quad_positions);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, (GLintptr)temp_buf_ndx_offset_,
                    sizeof(fs_quad_indices), fs_quad_indices);

    const float uvs[] = {-1.0f, 1.0f, -1.0, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f};

    glBufferSubData(GL_ARRAY_BUFFER,
                    (GLintptr)(temp_buf1_vtx_offset_ + sizeof(fs_quad_positions)),
                    sizeof(uvs), uvs);

    Ren::Program *prog = blit_prefilter_prog_.get();
    glUseProgram(prog->prog_id());

    glUniform4f(0, 0.0f, 0.0f, 1.0f, 1.0f);
    glUniform1f(1, float(temp_probe_index));

    ren_glBindTextureUnit_Comp(GL_TEXTURE_CUBE_MAP_ARRAY, 0, cube_array);

    int res = dst_store.res();
    int level = 0;

    while (level <= dst_store.max_level()) {
        glViewport(0, 0, res, res);

        const float roughness = (1.0f / 6.0f) * float(level);
        glUniform1f(3, roughness);

        for (int face = 0; face < 6; face++) {
            glUniform1i(2, face);

            glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, cube_array,
                                      level, (GLint)(probe_index * 6 + face));
            assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT,
                           (const GLvoid *)uintptr_t(temp_buf_ndx_offset_));
        }

        res /= 2;
        level++;
    }

    glDisableVertexAttribArray(REN_VTX_POS_LOC);
    glDisableVertexAttribArray(REN_VTX_UV1_LOC);

    glBindVertexArray(0);

    glBindFramebuffer(GL_FRAMEBUFFER, framebuf_before);
    glViewport(viewport_before[0], viewport_before[1], viewport_before[2],
               viewport_before[3]);
}

bool Renderer::BlitProjectSH(const ProbeStorage &store, int probe_index, int iteration,
                             LightProbe &probe) {
    using namespace RendererInternal;

    GLint framebuf_before;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuf_before);

    GLint viewport_before[4];
    glGetIntegerv(GL_VIEWPORT, viewport_before);

    glBindFramebuffer(GL_FRAMEBUFFER, probe_sample_buf_.fb);

    glViewport(0, 0, probe_sample_buf_.w, probe_sample_buf_.h);

    glDisable(GL_BLEND);

    glBindVertexArray((GLuint)temp_vao_);

    glBindBuffer(GL_ARRAY_BUFFER, last_vertex_buf1_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, last_index_buffer_);

    glEnableVertexAttribArray(REN_VTX_POS_LOC);
    glVertexAttribPointer(REN_VTX_POS_LOC, 2, GL_FLOAT, GL_FALSE, 0,
                          (const GLvoid *)uintptr_t(temp_buf1_vtx_offset_));

    glEnableVertexAttribArray(REN_VTX_UV1_LOC);
    glVertexAttribPointer(
        REN_VTX_UV1_LOC, 2, GL_FLOAT, GL_FALSE, 0,
        (const GLvoid *)uintptr_t(temp_buf1_vtx_offset_ + sizeof(fs_quad_positions)));

    const float uvs[] = {0.0f,
                         0.0f,
                         (float)probe_sample_buf_.w,
                         0.0f,
                         (float)probe_sample_buf_.w,
                         (float)probe_sample_buf_.h,
                         0.0f,
                         (float)probe_sample_buf_.h};

    glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)temp_buf1_vtx_offset_,
                    sizeof(fs_quad_positions), fs_quad_positions);
    glBufferSubData(GL_ARRAY_BUFFER,
                    (GLintptr)(temp_buf1_vtx_offset_ + sizeof(fs_quad_positions)),
                    sizeof(uvs), uvs);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, (GLintptr)temp_buf_ndx_offset_,
                    sizeof(fs_quad_indices), fs_quad_indices);

    if (iteration != 0) {
        // Retrieve result of previous read
        glBindBuffer(GL_PIXEL_PACK_BUFFER, (GLuint)probe_sample_pbo_);

        Ren::Vec3f sh_coeffs[4];

        auto *pixels = (float *)glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0,
                                                 4 * probe_sample_buf_.w *
                                                     probe_sample_buf_.h * sizeof(float),
                                                 GL_MAP_READ_BIT);
        if (pixels) {
            for (int y = 0; y < probe_sample_buf_.h; y++) {
                for (int x = 0; x < probe_sample_buf_.w; x++) {
                    const int i = (x >= 8) ? ((x >= 16) ? 2 : 1) : 0;

                    sh_coeffs[0][i] += pixels[4 * (y * probe_sample_buf_.w + x) + 0];
                    sh_coeffs[1][i] += pixels[4 * (y * probe_sample_buf_.w + x) + 1];
                    sh_coeffs[2][i] += pixels[4 * (y * probe_sample_buf_.w + x) + 2];
                    sh_coeffs[3][i] += pixels[4 * (y * probe_sample_buf_.w + x) + 3];
                }
            }
            glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
        }
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

        const float inv_weight = 1.0f / float(probe_sample_buf_.h * probe_sample_buf_.h);
        for (Ren::Vec3f &sh_coeff : sh_coeffs) {
            sh_coeff *= inv_weight;
        }

        const float k = 1.0f / float(iteration);
        for (int i = 0; i < 4; i++) {
            const Ren::Vec3f diff = sh_coeffs[i] - probe.sh_coeffs[i];
            probe.sh_coeffs[i] += diff * k;
        }
    }

    if (iteration < 64) {
        { // Sample cubemap and project on sh basis
            Ren::Program *prog = blit_project_sh_prog_.get();
            glUseProgram(prog->prog_id());

            glUniform4f(0, 0.0f, 0.0f, 1.0f, 1.0f);

            ren_glBindTextureUnit_Comp(GL_TEXTURE_CUBE_MAP_ARRAY, 1, store.tex_id());

            ren_glBindTextureUnit_Comp(GL_TEXTURE_2D, 1, rand2d_8x8_->tex_id());

            glUniform1f(1, float(probe_index));
            glUniform1i(2, iteration);

            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT,
                           (const GLvoid *)uintptr_t(temp_buf_ndx_offset_));
        }

        { // Start readback from buffer (result will be retrieved at the start of next
          // iteration)
            glReadBuffer(GL_COLOR_ATTACHMENT0);
            glBindBuffer(GL_PIXEL_PACK_BUFFER, (GLuint)probe_sample_pbo_);

            glReadPixels(0, 0, probe_sample_buf_.w, probe_sample_buf_.h, GL_RGBA,
                         GL_FLOAT, nullptr);

            glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        }
    }

    glDisableVertexAttribArray(REN_VTX_POS_LOC);
    glDisableVertexAttribArray(REN_VTX_UV1_LOC);

    glBindVertexArray(0);

    glBindFramebuffer(GL_FRAMEBUFFER, framebuf_before);
    glViewport(viewport_before[0], viewport_before[1], viewport_before[2],
               viewport_before[3]);

    return iteration == 64;
}

#undef _AS_STR
#undef AS_STR
