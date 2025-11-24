#pragma once

#include "Anim.h"
#include "Buffer.h"
#include "Common.h"
#include "Image.h"
#include "ImageAtlas.h"
#include "ImageRegion.h"
#include "Material.h"
#include "MemoryAllocator.h"
#include "Mesh.h"
#include "Pipeline.h"
#include "Program.h"
#include "Sampler.h"
#include "Shader.h"

struct SWcontext;

namespace Ren {
const int ImageAtlasWidth = 1024, ImageAtlasHeight = 512, ImageAtlasLayers = 4;
const int StageBufferCount = 2;

struct ApiContext;
class DescrMultiPoolAlloc;

struct StageBufRef {
    Context &ctx;
    BufRef buf;
    SyncFence &fence;
    CommandBuffer cmd_buf;
    bool &is_in_use;

    StageBufRef(Context &_ctx, BufRef &_buf, SyncFence &_fence, CommandBuffer cmd_buf, bool &_is_in_use);
    ~StageBufRef();

    StageBufRef(const StageBufRef &rhs) = delete;
    StageBufRef(StageBufRef &&rhs) = default;
};

struct StageBufs {
    Context *ctx = nullptr;
    BufRef bufs[StageBufferCount];
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

class Context {
  protected:
    int w_ = 0, h_ = 0;
    int validation_level_ = 0;
    ILog *log_ = nullptr;

    MeshStorage meshes_;
    MaterialStorage materials_;
    ProgramStorage programs_;
    VertexInputStorage vtx_inputs_;
    RenderPassStorage render_passes_;
    PipelineStorage pipelines_;
    ShaderStorage shaders_;
    ImageStorage images_;
    ImageRegionStorage image_regions_;
    SamplerStorage samplers_;
    AnimSeqStorage anims_;
    BufferStorage buffers_;

    BufRef default_vertex_buf1_, default_vertex_buf2_, default_skin_vertex_buf_, default_delta_buf_,
        default_indices_buf_;
    std::unique_ptr<MemAllocators> default_mem_allocs_;

#if defined(REN_VK_BACKEND)
    std::unique_ptr<DescrMultiPoolAlloc> default_descr_alloc_[MaxFramesInFlight];
#endif

    ImageAtlasArray image_atlas_;

#if defined(REN_VK_BACKEND) || defined(REN_GL_BACKEND)
    std::unique_ptr<ApiContext> api_ctx_;
#elif defined(REN_SW_BACKEND)
    SWcontext *sw_ctx_;
#endif

    void CheckDeviceCapabilities();

  public:
    Context();
    ~Context();

    Context(const Context &rhs) = delete;

    bool Init(int w, int h, ILog *log, int validation_level, bool nohwrt, bool nosubgroup,
              std::string_view preferred_device);

    int w() const { return w_; }
    int h() const { return h_; }
    int validation_level() const { return validation_level_; }

#if defined(REN_VK_BACKEND) || defined(REN_GL_BACKEND)
    ApiContext *api_ctx() { return api_ctx_.get(); }
    uint64_t device_id() const;
#elif defined(REN_SW_BACKEND)

#endif

    ILog *log() const { return log_; }

    ImageStorage &images() { return images_; }
    MaterialStorage &materials() { return materials_; }
    ProgramStorage &programs() { return programs_; }

    BufRef default_vertex_buf1() const { return default_vertex_buf1_; }
    BufRef default_vertex_buf2() const { return default_vertex_buf2_; }
    BufRef default_skin_vertex_buf() const { return default_skin_vertex_buf_; }
    BufRef default_delta_buf() const { return default_delta_buf_; }
    BufRef default_indices_buf() const { return default_indices_buf_; }
    MemAllocators *default_mem_allocs() { return default_mem_allocs_.get(); }
    DescrMultiPoolAlloc &default_descr_alloc() const;

    void BegSingleTimeCommands(CommandBuffer cmd_buf);
    CommandBuffer BegTempSingleTimeCommands();
    SyncFence EndSingleTimeCommands(CommandBuffer cmd_buf);
    void EndTempSingleTimeCommands(CommandBuffer cmd_buf);

    void InsertReadbackMemoryBarrier(CommandBuffer cmd_buf);

    CommandBuffer current_cmd_buf();

    ImageAtlasArray &image_atlas() { return image_atlas_; }

    void Resize(int w, int h);

    /*** Mesh ***/
    MeshRef LoadMesh(std::string_view name, const float *positions, int vtx_count, const uint32_t *indices,
                     int ndx_count, eMeshLoadStatus *load_status);
    MeshRef LoadMesh(std::string_view name, const float *positions, int vtx_count, const uint32_t *indices,
                     int ndx_count, BufRef &vertex_buf1, BufRef &vertex_buf2, BufRef &index_buf,
                     eMeshLoadStatus *load_status);
    MeshRef LoadMesh(std::string_view name, std::istream *data, const material_load_callback &on_mat_load,
                     eMeshLoadStatus *load_status);
    MeshRef LoadMesh(std::string_view name, std::istream *data, const material_load_callback &on_mat_load,
                     BufRef &vertex_buf1, BufRef &vertex_buf2, BufRef &index_buf, BufRef &skin_vertex_buf,
                     BufRef &delta_buf, eMeshLoadStatus *load_status);

    /*** Material ***/
    MaterialRef LoadMaterial(std::string_view name, std::string_view mat_src, eMatLoadStatus *status,
                             const pipelines_load_callback &on_pipes_load, const texture_load_callback &on_tex_load,
                             const sampler_load_callback &on_sampler_load);
    int NumMaterialsNotReady();
    void ReleaseMaterials();

    /*** Program ***/
#if defined(REN_GL_BACKEND)
    ShaderRef LoadShaderGLSL(std::string_view name, std::string_view shader_src, eShaderType type);
#endif
#if defined(REN_GL_BACKEND) || defined(REN_VK_BACKEND)
    ShaderRef LoadShaderSPIRV(std::string_view name, Span<const uint8_t> shader_data, eShaderType type);
#endif

#if defined(REN_GL_BACKEND) || defined(REN_VK_BACKEND)
    ProgramRef LoadProgram(ShaderRef vs_ref, ShaderRef fs_ref, ShaderRef tcs_ref, ShaderRef tes_ref, ShaderRef gs_ref);
    ProgramRef LoadProgram(ShaderRef cs_source);
#elif defined(REN_SW_BACKEND)
    ProgramRef LoadProgramSW(void *vs_shader, void *fs_shader, int num_fvars, const Attribute *attrs,
                             const Uniform *unifs);
#endif

#if defined(REN_VK_BACKEND)
    ProgramRef LoadProgram2(ShaderRef raygen_ref, ShaderRef closesthit_ref, ShaderRef anyhit_ref, ShaderRef miss_ref,
                            ShaderRef intersection_ref);
#endif

    ProgramRef GetProgram(uint32_t index);
    void ReleasePrograms();

    /*** VertexInput ***/
    VertexInputRef LoadVertexInput(Span<const VtxAttribDesc> attribs, const BufRef &elem_buf);

    /*** RenderPass ***/
    RenderPassRef LoadRenderPass(const RenderTarget &depth_rt, Span<const RenderTarget> color_rts) {
        SmallVector<RenderTargetInfo, 4> infos;
        for (int i = 0; i < color_rts.size(); ++i) {
            infos.emplace_back(color_rts[i]);
        }
        return LoadRenderPass(RenderTargetInfo{depth_rt}, infos);
    }
    RenderPassRef LoadRenderPass(const RenderTargetInfo &depth_rt, Span<const RenderTargetInfo> color_rts);

    /*** Pipeline ***/
    PipelineRef LoadPipeline(const ProgramRef &prog_ref, int subgroup_size = -1);
    PipelineRef LoadPipeline(const RastState &rast_state, const ProgramRef &prog, const VertexInputRef &vtx_input,
                             const RenderPassRef &render_pass, uint32_t subpass_index);

    /*** Image ***/
    ImgRef LoadImage(std::string_view name, const ImgParams &p, MemAllocators *mem_allocs, eImgLoadStatus *load_status);
    ImgRef LoadImage(std::string_view name, const ImgHandle &handle, const ImgParams &p, MemAllocation &&alloc,
                     eImgLoadStatus *load_status);
    ImgRef LoadImage(std::string_view name, Span<const uint8_t> data, const ImgParams &p, MemAllocators *mem_allocs,
                     eImgLoadStatus *load_status);
    ImgRef LoadImageCube(std::string_view name, Span<const uint8_t> data[6], const ImgParams &p,
                         MemAllocators *mem_allocs, eImgLoadStatus *load_status);

    void VisitImages(eImgFlags mask, const std::function<void(Image &img)> &callback);
    int NumImagesNotReady();
    void ReleaseImages();

    /** Image regions (placed on default atlas) **/
    ImageRegionRef LoadImageRegion(std::string_view name, Span<const uint8_t> data, const ImgParams &p,
                                     CommandBuffer cmd_buf, eImgLoadStatus *load_status);
    ImageRegionRef LoadImageRegion(std::string_view name, const Buffer &sbuf, int data_off, int data_len,
                                     const ImgParams &p, CommandBuffer cmd_buf, eImgLoadStatus *load_status);

    void ReleaseTextureRegions();

    /** Samplers **/
    SamplerRef LoadSampler(SamplingParams params, eSamplerLoadStatus *load_status);
    void ReleaseSamplers();

    /*** Anims ***/
    AnimSeqRef LoadAnimSequence(std::string_view name, std::istream &data);
    int NumAnimsNotReady();
    void ReleaseAnims();

    /*** Buffers ***/
    BufRef LoadBuffer(std::string_view name, eBufType type, uint32_t initial_size, uint32_t size_alignment = 1,
                      MemAllocators *mem_allocs = nullptr);
    BufRef LoadBuffer(std::string_view name, eBufType type, const BufHandle &handle, MemAllocation &&alloc,
                      uint32_t initial_size, uint32_t size_alignment = 1);
    void ReleaseBuffers();

    void InitDefaultBuffers();
    void ReleaseDefaultBuffers();
    void ReleaseAll();

    int next_frontend_frame = 0;
    int in_flight_frontend_frame[MaxFramesInFlight];
    int backend_frame() const;
    int active_present_image() const;

    ImgRef backbuffer_ref() const;

    int WriteTimestamp(bool start);
    uint64_t GetTimestampIntervalDurationUs(int query_start, int query_end) const;

    void WaitIdle();

    void ResetAllocators();

#if defined(REN_GL_BACKEND)
    struct { // NOLINT
        float max_anisotropy = 0;
        int max_vertex_input = 0, max_vertex_output = 0;
        bool spirv = false;
        bool persistent_buf_mapping = false;
        bool memory_heaps = false;
        bool bindless_texture = false;
        bool hwrt = false;
        bool swrt = false;
        int max_compute_work_group_size[3] = {};
        int tex_buf_offset_alignment = 1;
        int unif_buf_offset_alignment = 1;
        bool depth24_stencil8_format = true; // opengl handles this on amd somehow
        bool rgb565_render_target = false;
        bool subgroup = false;
        bool bc4_3d_texture_format = false;
    } capabilities;

    static bool IsExtensionSupported(const char *ext);
#elif defined(REN_VK_BACKEND)
    struct { // NOLINT
        float max_anisotropy = 0;
        int max_vertex_input = 0, max_vertex_output = 0;
        bool spirv = true;
        bool persistent_buf_mapping = true;
        bool memory_heaps = true;
        bool bindless_texture = false;
        bool hwrt = false;
        bool swrt = false;
        int max_compute_work_group_size[3] = {};
        int tex_buf_offset_alignment = 1;
        int unif_buf_offset_alignment = 1;
        bool depth24_stencil8_format = false;
        bool rgb565_render_target = false;
        uint32_t max_combined_image_samplers = 16;
        bool subgroup = false;
        bool bc4_3d_texture_format = false;
    } capabilities;

    bool InitPipelineCache(Span<const uint8_t> in_data);
    void DestroyPipelineCache();
    size_t WritePipelineCache(Span<uint8_t> out_data);
#elif defined(REN_SW_BACKEND)
    int max_uniform_vec4 = 0;
#endif
};

#if defined(REN_GL_BACKEND)
void ResetGLState();
void CheckError(const char *op, ILog *log);
#endif
} // namespace Ren