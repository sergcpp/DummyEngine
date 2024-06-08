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
#include "Texture.h"
#include "TextureAtlas.h"
#include "TextureRegion.h"

struct SWcontext;

namespace Ren {
const int TextureAtlasWidth = 1024, TextureAtlasHeight = 512, TextureAtlasLayers = 4;
const int StageBufferCount = 2;

struct ApiContext;
class DescrMultiPoolAlloc;

struct StageBufRef {
    Context &ctx;
    BufferRef buf;
    SyncFence &fence;
    CommandBuffer cmd_buf;
    bool &is_in_use;

    StageBufRef(Context &_ctx, BufferRef &_buf, SyncFence &_fence, CommandBuffer cmd_buf, bool &_is_in_use);
    ~StageBufRef();

    StageBufRef(const StageBufRef &rhs) = delete;
    StageBufRef(StageBufRef &&rhs) = default;
};

struct StageBufs {
    Context *ctx = nullptr;
    BufferRef bufs[StageBufferCount];
    SyncFence fences[StageBufferCount];
    CommandBuffer cmd_bufs[StageBufferCount] = {};
    bool is_in_use[StageBufferCount] = {};

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

const char *Version();

class Context {
  protected:
    int w_ = 0, h_ = 0;
    ILog *log_ = nullptr;

    MeshStorage meshes_;
    MaterialStorage materials_;
    ProgramStorage programs_;
    PipelineStorage pipelines_;
    ShaderStorage shaders_;
    Texture3DStorage textures_3D_;
    Texture2DStorage textures_2D_;
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

    bool Init(int w, int h, ILog *log, int validation_level, bool nohwrt, std::string_view preferred_device);

    int w() const { return w_; }
    int h() const { return h_; }

#if defined(USE_VK_RENDER) || defined(USE_GL_RENDER)
    ApiContext *api_ctx() { return api_ctx_.get(); }
#elif defined(USE_SW_RENDER)

#endif

    ILog *log() const { return log_; }

    Texture3DStorage &textures3D() { return textures_3D_; }
    Texture2DStorage &textures2D() { return textures_2D_; }
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

    void BegSingleTimeCommands(CommandBuffer cmd_buf);
    CommandBuffer BegTempSingleTimeCommands();
    SyncFence EndSingleTimeCommands(CommandBuffer cmd_buf);
    void EndTempSingleTimeCommands(CommandBuffer cmd_buf);

    void InsertReadbackMemoryBarrier(CommandBuffer cmd_buf);

    CommandBuffer current_cmd_buf();

    TextureAtlasArray &texture_atlas() { return texture_atlas_; }

    void Resize(int w, int h);

    /*** Mesh ***/
    MeshRef LoadMesh(std::string_view name, const float *positions, int vtx_count, const uint32_t *indices,
                     int ndx_count, eMeshLoadStatus *load_status);
    MeshRef LoadMesh(std::string_view name, const float *positions, int vtx_count, const uint32_t *indices,
                     int ndx_count, StageBufs &stage_bufs, BufferRef &vertex_buf1, BufferRef &vertex_buf2,
                     BufferRef &index_buf, eMeshLoadStatus *load_status);
    MeshRef LoadMesh(std::string_view name, std::istream *data, const material_load_callback &on_mat_load,
                     eMeshLoadStatus *load_status);
    MeshRef LoadMesh(std::string_view name, std::istream *data, const material_load_callback &on_mat_load,
                     StageBufs &stage_bufs, BufferRef &vertex_buf1, BufferRef &vertex_buf2, BufferRef &index_buf,
                     BufferRef &skin_vertex_buf, BufferRef &delta_buf, eMeshLoadStatus *load_status);

    /*** Material ***/
    MaterialRef LoadMaterial(std::string_view name, std::string_view mat_src, eMatLoadStatus *status,
                             const pipelines_load_callback &on_pipes_load, const texture_load_callback &on_tex_load,
                             const sampler_load_callback &on_sampler_load);
    int NumMaterialsNotReady();
    void ReleaseMaterials();

    /*** Program ***/
#if defined(USE_GL_RENDER) || defined(USE_VK_RENDER)
    ShaderRef LoadShaderGLSL(std::string_view name, std::string_view shader_src, eShaderType type,
                             eShaderLoadStatus *load_status);
#ifndef __ANDROID__
    ShaderRef LoadShaderSPIRV(std::string_view name, Span<const uint8_t> shader_data, eShaderType type,
                              eShaderLoadStatus *load_status);
#endif

    ProgramRef LoadProgram(std::string_view name, ShaderRef vs_ref, ShaderRef fs_ref, ShaderRef tcs_ref,
                           ShaderRef tes_ref, eProgLoadStatus *load_status);
    ProgramRef LoadProgram(std::string_view name, ShaderRef cs_source, eProgLoadStatus *load_status);
#elif defined(USE_SW_RENDER)
    ProgramRef LoadProgramSW(std::string_view name, void *vs_shader, void *fs_shader, int num_fvars,
                             const Attribute *attrs, const Uniform *unifs, eProgLoadStatus *load_status);
#endif

#if defined(USE_VK_RENDER)
    ProgramRef LoadProgram(std::string_view name, ShaderRef raygen_ref, ShaderRef closesthit_ref, ShaderRef anyhit_ref,
                           ShaderRef miss_ref, ShaderRef intersection_ref, eProgLoadStatus *load_status);
#endif

    ProgramRef GetProgram(uint32_t index);
    int NumProgramsNotReady();
    void ReleasePrograms();

    /*** Texture 3D ***/
    Tex3DRef LoadTexture3D(std::string_view name, const Tex3DParams &p, MemoryAllocators *mem_allocs,
                           eTexLoadStatus *load_status);

    /*** Texture 2D ***/
    Tex2DRef LoadTexture2D(std::string_view name, const Tex2DParams &p, MemoryAllocators *mem_allocs,
                           eTexLoadStatus *load_status);
    Tex2DRef LoadTexture2D(std::string_view name, Span<const uint8_t> data, const Tex2DParams &p, StageBufs &stage_bufs,
                           MemoryAllocators *mem_allocs, eTexLoadStatus *load_status);
    Tex2DRef LoadTextureCube(std::string_view name, Span<const uint8_t> data[6], const Tex2DParams &p,
                             StageBufs &stage_bufs, MemoryAllocators *mem_allocs, eTexLoadStatus *load_status);

    void VisitTextures(eTexFlags mask, const std::function<void(Texture2D &tex)> &callback);
    int NumTexturesNotReady();
    void Release2DTextures();

    /*** Texture 1D ***/
    Tex1DRef CreateTexture1D(std::string_view name, BufferRef buf, eTexFormat format, uint32_t offset, uint32_t size);
    void Release1DTextures();

    /** Texture regions (placed on default atlas) **/
    TextureRegionRef LoadTextureRegion(std::string_view name, Span<const uint8_t> data, StageBufs &stage_bufs,
                                       const Tex2DParams &p, eTexLoadStatus *load_status);
    TextureRegionRef LoadTextureRegion(std::string_view name, const Buffer &sbuf, int data_off, int data_len,
                                       CommandBuffer cmd_buf, const Tex2DParams &p, eTexLoadStatus *load_status);

    void ReleaseTextureRegions();

    /** Samplers **/
    SamplerRef LoadSampler(SamplingParams params, eSamplerLoadStatus *load_status);
    void ReleaseSamplers();

    /*** Anims ***/
    AnimSeqRef LoadAnimSequence(std::string_view name, std::istream &data);
    int NumAnimsNotReady();
    void ReleaseAnims();

    /*** Buffers ***/
    BufferRef LoadBuffer(std::string_view name, eBufType type, uint32_t initial_size, uint32_t suballoc_align = 1);
    void ReleaseBuffers();

    void InitDefaultBuffers();
    void ReleaseDefaultBuffers();
    void ReleaseAll();

    int frontend_frame = 0;
    int backend_frame() const;
    int active_present_image() const;

    Ren::Tex2DRef backbuffer_ref() const;

    int WriteTimestamp(const bool start);
    uint64_t GetTimestampIntervalDurationUs(int query_start, int query_end) const;

#if defined(USE_GL_RENDER)
    struct { // NOLINT
        float max_anisotropy = 0.0f;
        int max_vertex_input = 0, max_vertex_output = 0;
        bool spirv = false;
        bool persistent_buf_mapping = false;
        bool bindless_texture = false;
        bool raytracing = false;
        bool ray_query = false;
        bool swrt = false;
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
        bool swrt = false;
        int max_compute_work_group_size[3] = {};
        int tex_buf_offset_alignment = 1;
        int unif_buf_offset_alignment = 1;
        bool depth24_stencil8_format = false;
        uint32_t max_combined_image_samplers = 16;
        bool dynamic_rendering = false;
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