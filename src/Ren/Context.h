#pragma once

#include "Anim.h"
#include "Buffer.h"
#include "Common.h"
#include "Material.h"
#include "MemoryAllocator.h"
#include "Mesh.h"
#include "Pipeline.h"
#include "Program.h"
#include "Sampler.h"
#include "Shader.h"
#include "TaskExecutor.h"
#include "Texture.h"
#include "TextureAtlas.h"
#include "TextureRegion.h"

struct SWcontext;

namespace Ren {
const int TextureAtlasWidth = 1024, TextureAtlasHeight = 512, TextureAtlasLayers = 4;
const int StageBufferCount = 16;

const uint32_t DefaultSubAllocAlign = 768;

struct ApiContext;
class DescrMultiPoolAlloc;

struct StageBufRef {
    Context &ctx;
    BufferRef buf;
    SyncFence &fence;
    void *cmd_buf;
    bool &is_in_use;

    StageBufRef(Context &_ctx, BufferRef &_buf, SyncFence &_fence, void *_cmd_buf, bool &_is_in_use);
    ~StageBufRef();

    StageBufRef(const StageBufRef &rhs) = delete;
    StageBufRef(StageBufRef &&rhs) = default;
};

struct StageBufs {
    Context *ctx;
    BufferRef bufs[StageBufferCount];
    SyncFence fences[StageBufferCount];
    void *cmd_bufs[StageBufferCount];
    bool is_in_use[StageBufferCount];

  private:
    int cur = 0;

  public:
    StageBufRef GetNextBuffer() {
        int ret;
        do {
            ret = cur;
            cur = (cur + 1) % StageBufferCount;
        } while (is_in_use[ret]);
        return StageBufRef{*ctx, bufs[ret], fences[ret], cmd_bufs[ret], is_in_use[ret]};
    }
};

class Context : public TaskExecutor {
  protected:
    int w_ = 0, h_ = 0;
    ILog *log_ = nullptr;

    MeshStorage meshes_;
    MaterialStorage materials_;
    ProgramStorage programs_;
    PipelineStorage pipelines_;
    ShaderStorage shaders_;
    Texture2DStorage textures_;
    Texture1DStorage textures_1D_;
    TextureRegionStorage texture_regions_;
    SamplerStorage samplers_;
    AnimSeqStorage anims_;
    BufferStorage buffers_;

    BufferRef default_vertex_buf1_, default_vertex_buf2_, default_skin_vertex_buf_, default_delta_buf_,
        default_indices_buf_;
    StageBufs default_stage_bufs_;
    std::unique_ptr<MemoryAllocators> default_memory_allocs_;

#if defined(USE_VK_RENDER)
    std::unique_ptr<DescrMultiPoolAlloc> default_descr_alloc_[MaxFramesInFlight];
#endif

    TextureAtlasArray texture_atlas_;

#if defined(USE_VK_RENDER) || defined(USE_GL_RENDER)
    std::unique_ptr<ApiContext> api_ctx_;
#elif defined(USE_SW_RENDER)
    SWcontext *sw_ctx_;
#endif

    void CheckDeviceCapabilities();

  public:
    Context();
    ~Context();

    Context(const Context &rhs) = delete;

    bool Init(int w, int h, ILog *log, const char *preferred_device);

    int w() const { return w_; }
    int h() const { return h_; }

#if defined(USE_VK_RENDER) || defined(USE_GL_RENDER)
    ApiContext *api_ctx() { return api_ctx_.get(); }
#elif defined(USE_SW_RENDER)

#endif

    ILog *log() const { return log_; }

    Texture2DStorage &textures() { return textures_; }
    Texture1DStorage &textures1D() { return textures_1D_; }
    MaterialStorage &materials() { return materials_; }
    ProgramStorage &programs() { return programs_; }

    BufferRef default_vertex_buf1() const { return default_vertex_buf1_; }
    BufferRef default_vertex_buf2() const { return default_vertex_buf2_; }
    BufferRef default_skin_vertex_buf() const { return default_skin_vertex_buf_; }
    BufferRef default_delta_buf() const { return default_delta_buf_; }
    BufferRef default_indices_buf() const { return default_indices_buf_; }
    StageBufs &default_stage_bufs() { return default_stage_bufs_; }
    MemoryAllocators *default_mem_allocs() { return default_memory_allocs_.get(); }
    DescrMultiPoolAlloc *default_descr_alloc() const;

    void BegSingleTimeCommands(void *cmd_buf);
    void *BegTempSingleTimeCommands();
    SyncFence EndSingleTimeCommands(void *cmd_buf);
    void EndTempSingleTimeCommands(void *cmd_buf);

    void *current_cmd_buf();

    TextureAtlasArray &texture_atlas() { return texture_atlas_; }

    void Resize(int w, int h);

    /*** Mesh ***/
    MeshRef LoadMesh(const char *name, const float *positions, int vtx_count, const uint32_t *indices, int ndx_count,
                     eMeshLoadStatus *load_status);
    MeshRef LoadMesh(const char *name, const float *positions, int vtx_count, const uint32_t *indices, int ndx_count,
                     StageBufs &stage_bufs, BufferRef &vertex_buf1, BufferRef &vertex_buf2, BufferRef &index_buf,
                     eMeshLoadStatus *load_status);
    MeshRef LoadMesh(const char *name, std::istream *data, const material_load_callback &on_mat_load,
                     eMeshLoadStatus *load_status);
    MeshRef LoadMesh(const char *name, std::istream *data, const material_load_callback &on_mat_load,
                     StageBufs &stage_bufs, BufferRef &vertex_buf1, BufferRef &vertex_buf2, BufferRef &index_buf,
                     BufferRef &skin_vertex_buf, BufferRef &delta_buf, eMeshLoadStatus *load_status);

    /*** Material ***/
    MaterialRef LoadMaterial(const char *name, const char *mat_src, eMatLoadStatus *status,
                             const pipelines_load_callback &on_pipes_load, const texture_load_callback &on_tex_load,
                             const sampler_load_callback &on_sampler_load);
    int NumMaterialsNotReady();
    void ReleaseMaterials();

    /*** Program ***/
#if defined(USE_GL_RENDER) || defined(USE_VK_RENDER)
    ShaderRef LoadShaderGLSL(const char *name, const char *shader_src, eShaderType type,
                             eShaderLoadStatus *load_status);
#ifndef __ANDROID__
    ShaderRef LoadShaderSPIRV(const char *name, const uint8_t *shader_data, int data_size, eShaderType type,
                              eShaderLoadStatus *load_status);
#endif

    ProgramRef LoadProgram(const char *name, ShaderRef vs_ref, ShaderRef fs_ref, ShaderRef tcs_ref, ShaderRef tes_ref,
                           eProgLoadStatus *load_status);
    ProgramRef LoadProgram(const char *name, ShaderRef cs_source, eProgLoadStatus *load_status);
#elif defined(USE_SW_RENDER)
    ProgramRef LoadProgramSW(const char *name, void *vs_shader, void *fs_shader, int num_fvars, const Attribute *attrs,
                             const Uniform *unifs, eProgLoadStatus *load_status);
#endif

#if defined(USE_VK_RENDER)
    ProgramRef LoadProgram(const char *name, ShaderRef raygen_ref, ShaderRef closesthit_ref, ShaderRef anyhit_ref,
                           ShaderRef miss_ref, ShaderRef intersection_ref, eProgLoadStatus *load_status);
#endif

    ProgramRef GetProgram(uint32_t index);
    int NumProgramsNotReady();
    void ReleasePrograms();

    /*** Texture ***/
    Tex2DRef LoadTexture2D(const char *name, const Tex2DParams &p, MemoryAllocators *mem_allocs,
                           eTexLoadStatus *load_status);
    Tex2DRef LoadTexture2D(const char *name, const void *data, int size, const Tex2DParams &p, StageBufs &stage_bufs,
                           MemoryAllocators *mem_allocs, eTexLoadStatus *load_status);
    Tex2DRef LoadTextureCube(const char *name, const void *data[6], const int size[6], const Tex2DParams &p,
                             StageBufs &stage_bufs, MemoryAllocators *mem_allocs, eTexLoadStatus *load_status);

    void VisitTextures(uint32_t mask, const std::function<void(Texture2D &tex)> &callback);
    int NumTexturesNotReady();
    void Release2DTextures();

    /*** Texture 1D ***/
    Tex1DRef CreateTexture1D(const char *name, BufferRef buf, eTexFormat format, uint32_t offset, uint32_t size);
    void Release1DTextures();

    /** Texture regions (placed on default atlas) **/
    TextureRegionRef LoadTextureRegion(const char *name, const void *data, int size, StageBufs &stage_bufs,
                                       const Tex2DParams &p, eTexLoadStatus *load_status);
    TextureRegionRef LoadTextureRegion(const char *name, const Buffer &sbuf, int data_off, int data_len, void *cmd_buf,
                                       const Tex2DParams &p, eTexLoadStatus *load_status);

    void ReleaseTextureRegions();

    /** Samplers **/
    SamplerRef LoadSampler(SamplingParams params, eSamplerLoadStatus *load_status);
    void ReleaseSamplers();

    /*** Anims ***/
    AnimSeqRef LoadAnimSequence(const char *name, std::istream &data);
    int NumAnimsNotReady();
    void ReleaseAnims();

    /*** Buffers ***/
    BufferRef LoadBuffer(const char *name, eBufType type, uint32_t initial_size,
                         uint32_t suballoc_align = DefaultSubAllocAlign);
    void ReleaseBuffers();

    void InitDefaultBuffers();
    void ReleaseDefaultBuffers();
    void ReleaseAll();

    int frontend_frame = 0;
    int backend_frame() const;

    Ren::Tex2DRef backbuffer_ref() const;

#if defined(USE_GL_RENDER)
    struct { // NOLINT
        float max_anisotropy = 0.0f;
        int max_vertex_input = 0, max_vertex_output = 0;
        bool spirv = false;
        bool persistent_buf_mapping = false;
        bool bindless_texture = false;
        bool raytracing = false;
        bool ray_query = false;
        int max_compute_work_group_size[3] = {};
        int tex_buf_offset_alignment = 1;
        int unif_buf_offset_alignment = 1;
        bool depth24_stencil8_format = true; // opengl handles this on amd somehow
    } capabilities;

    static bool IsExtensionSupported(const char *ext);
#elif defined(USE_VK_RENDER)
    struct { // NOLINT
        float max_anisotropy = 0.0f;
        int max_vertex_input = 0, max_vertex_output = 0;
        bool spirv = true;
        bool persistent_buf_mapping = true;
        bool bindless_texture = false;
        bool raytracing = false;
        bool ray_query = false;
        int max_compute_work_group_size[3] = {};
        int tex_buf_offset_alignment = 1;
        int unif_buf_offset_alignment = 1;
        bool depth24_stencil8_format = false;
        uint32_t max_combined_image_samplers = 16;
    } capabilities;
#elif defined(USE_SW_RENDER)
    int max_uniform_vec4 = 0;
#endif
};

#if defined(USE_GL_RENDER)
void ResetGLState();
void CheckError(const char *op, ILog *log);
#endif
} // namespace Ren