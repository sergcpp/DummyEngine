#pragma once

#include "AccStructure.h"
#include "Anim.h"
#include "Buffer.h"
#include "Common.h"
#include "Framebuffer.h"
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

class DescrMultiPoolAlloc;

struct StageBufRef {
    Context &ctx;
    BufferHandle buf;
    SyncFence &fence;
    CommandBuffer cmd_buf;
    bool &is_in_use;

    StageBufRef(Context &_ctx, BufferHandle _buf, SyncFence &_fence, CommandBuffer cmd_buf, bool &_is_in_use);
    ~StageBufRef();

    StageBufRef(const StageBufRef &rhs) = delete;
    StageBufRef(StageBufRef &&rhs) = default;
};

struct StageBufs {
    Context *ctx = nullptr;
    BufferHandle bufs[StageBufferCount];
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
    bool novsync_ = false;
    int validation_level_ = 0;
    ILog *log_ = nullptr;

    SparseStorage<VertexInput> vtx_inputs_;
    SparseDualStorage<ShaderMain, ShaderCold> shaders_;
    SparseDualStorage<ProgramMain, ProgramCold> programs_;
    SparseDualStorage<PipelineMain, PipelineCold> pipelines_;
    SparseStorage<RenderPass> render_passes_;
    SparseDualStorage<BufferMain, BufferCold> buffers_;
    SparseDualStorage<ImageMain, ImageCold> images_;
    SparseStorage<Sampler> samplers_;
    SparseDualStorage<FramebufferMain, FramebufferCold> framebuffers_;
    SparseDualStorage<AccStructMain, AccStructCold> acc_structs_;
    SparseDualStorage<MaterialMain, MaterialCold> materials_;
    SparseDualStorage<MeshMain, MeshCold> meshes_;
    SparseDualStorage<AnimSeqMain, AnimSeqCold> anims_;

    // TODO: Move this to GUI
    SparseDualStorage<ImageRegionMain, ImageRegionCold> image_regions_;

    StoragesRef storages_ = {vtx_inputs_, shaders_,      programs_,    pipelines_, render_passes_, buffers_, images_,
                             samplers_,   framebuffers_, acc_structs_, materials_, meshes_,        anims_};

    std::unique_ptr<ResizableBuffer> default_vertex_buf1_, default_vertex_buf2_, default_skin_vertex_buf_,
        default_delta_vertex_buf_, default_indices_buf_;
    std::unique_ptr<MemAllocators> default_mem_allocs_;

    std::unique_ptr<DescrMultiPoolAlloc> default_descr_alloc_[MaxFramesInFlight];

    ImageAtlasArray image_atlas_;

#if defined(REN_VK_BACKEND) || defined(REN_GL_BACKEND)
    std::unique_ptr<ApiContext> api_;
#endif

    void CheckDeviceCapabilities();

  public:
    Context();
    ~Context();

    Context(const Context &rhs) = delete;

    bool Init(int w, int h, ILog *log, int validation_level, bool novsync, bool nohwrt, bool nosubgroup,
              std::string_view preferred_device);

    int w() const { return w_; }
    int h() const { return h_; }
    int validation_level() const { return validation_level_; }

#if defined(REN_VK_BACKEND) || defined(REN_GL_BACKEND)
    const ApiContext &api() const { return *api_; }
    ApiContext &api() { return *api_; }
    uint64_t device_id() const;
#endif

    ILog *log() const { return log_; }

    const SparseDualStorage<ImageRegionMain, ImageRegionCold> &image_regions() const { return image_regions_; }
    SparseDualStorage<ImageRegionMain, ImageRegionCold> &image_regions() { return image_regions_; }

    const StoragesRef &storages() const { return storages_; }

    ResizableBuffer &default_vertex_buf1() const { return *default_vertex_buf1_; }
    ResizableBuffer &default_vertex_buf2() const { return *default_vertex_buf2_; }
    ResizableBuffer &default_skin_vertex_buf() const { return *default_skin_vertex_buf_; }
    ResizableBuffer &default_delta_vertex_buf() const { return *default_delta_vertex_buf_; }
    ResizableBuffer &default_indices_buf() const { return *default_indices_buf_; }

    MemAllocators *default_mem_allocs() { return default_mem_allocs_.get(); }
    DescrMultiPoolAlloc &default_descr_alloc() const;

    void BegSingleTimeCommands(CommandBuffer cmd_buf);
    CommandBuffer BegTempSingleTimeCommands();
    SyncFence EndSingleTimeCommands(CommandBuffer cmd_buf);
    void EndTempSingleTimeCommands(CommandBuffer cmd_buf);

    void InsertReadbackMemoryBarrier(CommandBuffer cmd_buf);

    CommandBuffer current_cmd_buf();

    ImageAtlasArray &image_atlas() { return image_atlas_; }

    void Resize(int w, int h, bool novsync);

    // Mesh
    MeshHandle CreateMesh(Ren::String name, std::istream &data, const material_load_callback &on_mat_load,
                          ResizableBuffer &vertex_buf1, ResizableBuffer &vertex_buf2, ResizableBuffer &index_buf,
                          ResizableBuffer &skin_vertex_buf, ResizableBuffer &delta_buf);
    MeshHandle CreateMesh(Ren::String name, std::istream &data, const material_load_callback &on_mat_load);
    void ReleaseMesh(MeshHandle handle);
    void ReleaseMeshes();

    // Material
    MaterialHandle CreateMaterial(Ren::String name, Bitmask<eMatFlags> flags, Span<const PipelineHandle> pipelines,
                                  Span<const ImageHandle> textures, Span<const SamplerHandle> samplers,
                                  Span<const Vec4f> params);
    MaterialHandle CreateMaterial(Ren::String name, std::string_view mat_src,
                                  const pipelines_load_callback &on_pipes_load,
                                  const texture_load_callback &on_tex_load,
                                  const sampler_load_callback &on_sampler_load);
    void ReleaseMaterial(MaterialHandle handle);
    void ReleaseMaterials();

    // Program
#if defined(REN_GL_BACKEND)
    ShaderHandle CreateShader(const Ren::String &name, std::string_view shader_src, eShaderType type);
#endif
    ShaderHandle CreateShader(const Ren::String &name, Span<const uint8_t> spirv_data, eShaderType type);
    void ReleaseShader(ShaderHandle handle);
    void ReleaseShaders();

    ProgramHandle CreateProgram(ShaderROHandle vs, ShaderROHandle fs, ShaderROHandle tcs, ShaderROHandle tes,
                                ShaderROHandle gs);
    ProgramHandle CreateProgram(ShaderROHandle cs);
    void ReleaseProgram(ProgramHandle handle);

#if defined(REN_VK_BACKEND)
    ProgramHandle CreateProgram2(ShaderROHandle rgs, ShaderROHandle chs, ShaderROHandle ahs, ShaderROHandle ms,
                                 ShaderROHandle is);
#endif
    void ReleasePrograms();

    // VertexInput
    VertexInputHandle CreateVertexInput(Span<const VtxAttribDesc> attribs);
    void ReleaseVertexInput(VertexInputHandle handle);
    void ReleaseVertexInputs();

    // RenderPass
    RenderPassHandle CreateRenderPass(const RenderTargetInfo &depth_rt, Span<const RenderTargetInfo> color_rts);
    RenderPassHandle CreateRenderPass(const RenderTarget &depth_rt, Span<const RenderTarget> color_rts);
    void ReleaseRenderPass(RenderPassHandle handle, bool immediately = false);
    void ReleaseRenderPasses();

    // Pipeline
    PipelineHandle CreatePipeline(const ProgramROHandle prog, int subgroup_size = -1);
    PipelineHandle CreatePipeline(const RastState &rast_state, ProgramROHandle prog, VertexInputROHandle vtx_input,
                                  RenderPassROHandle render_pass, uint32_t subpass_index);
    PipelineHandle CreatePipeline(PipelineMain &&pi_main, PipelineCold &&pi_cold);
    void ReleasePipeline(PipelineHandle handle, bool immediately = false);
    void ReleasePipelines();

    // Image
    ImageHandle CreateImage(const String &name, Span<const uint8_t> data, const ImgParams &p,
                            MemAllocators *mem_allocs);
    ImageHandle CreateImage(const String &name, Span<const uint8_t> data[6], const ImgParams &p,
                            MemAllocators *mem_allocs);
    ImageHandle CreateImage(const String &name, const ImgParams &p, const ImageMain &img_main, MemAllocation &&alloc);
    ImageHandle CreateImage(ImageHandle src, const ImgParams &p, MemAllocators *mem_allocs, CommandBuffer cmd_buf);
    void ReleaseImage(ImageHandle handle, bool immediately = false);
    int CreateImageView(ImageHandle handle, eFormat format, int mip_level, int mip_count, int base_layer,
                        int layer_count);

    void CmdClearImage(ImageHandle handle, const ClearColor &col, CommandBuffer cmd_buf);
    void CmdCopyImageToBuffer(ImageROHandle img, BufferRWHandle buf, CommandBuffer cmd_buf, uint32_t data_off);
    void CmdCopyImageToImage(CommandBuffer cmd_buf, ImageROHandle src, uint32_t src_level, const Vec3i &src_offset,
                             ImageRWHandle dst, uint32_t dst_level, const Vec3i &dst_offset, uint32_t dst_face,
                             const Vec3i &size);

    void ReleaseImages();

    // Framebuffer
    FramebufferHandle CreateFramebuffer(RenderPassROHandle render_pass, const FramebufferAttachment &depth,
                                        const FramebufferAttachment &stencil,
                                        Span<const FramebufferAttachment> color_attachments);
    void ReleaseFramebuffer(FramebufferHandle handle, bool immediately = false);
    void ReleaseFramebuffers();

    AccStructHandle CreateAccStruct();
    void ReleaseAccStruct(AccStructHandle handle, bool immediately = false);
    void ReleaseAccStructs();

    // Image regions (placed on default atlas)
    ImageRegionHandle CreateImageRegion(String name, Span<const uint8_t> data, const ImgParams &p,
                                        CommandBuffer cmd_buf);
    ImageRegionHandle CreateImageRegion(String name, const BufferMain &sbuf, int data_off, int data_len,
                                        const ImgParams &p, CommandBuffer cmd_buf);
    void ReleaseImageRegion(ImageRegionHandle handle);
    void ReleaseImageRegions();

    // Samplers
    SamplerHandle CreateSampler(SamplingParams params);
    void ReleaseSampler(SamplerHandle handle, bool immediately = false);
    void ReleaseSamplers();

    // Anims
    AnimSeqHandle CreateAnimSequence(const String &name, std::istream &data);
    void ReleaseAnimSequence(AnimSeqHandle handle);
    void ReleaseAnimSequences();

    // Buffers
    BufferHandle CreateBuffer(const String &name, eBufType type, uint32_t initial_size, uint32_t size_alignment = 1,
                              MemAllocators *mem_allocs = nullptr);
    BufferHandle CreateBuffer(const String &name, eBufType type, const BufferMain &buf_main, MemAllocation &&alloc,
                              uint32_t initial_size, uint32_t size_alignment = 1);
    bool ResizeBuffer(BufferHandle handle, uint32_t new_size, bool keep_content = true,
                      bool release_immediately = false);
    int FindOrCreateBufferView(BufferHandle handle, eFormat format);
    uint8_t *MapBufferRange(BufferHandle handle, uint32_t offset, uint32_t size, bool persistent = false);
    uint8_t *MapBuffer(BufferHandle handle, bool persistent = false);
    void UnmapBuffer(BufferHandle handle);
    SubAllocation AllocBufferSubRegion(BufferHandle handle, uint32_t req_size, uint32_t req_alignment,
                                       std::string_view tag, const BufferMain *init_buf = nullptr,
                                       CommandBuffer cmd_buf = {}, uint32_t init_off = 0);
    bool FreeBufferSubRegion(BufferHandle handle, SubAllocation alloc);
    void ReleaseBuffer(BufferHandle handle, bool immediately = false);
    void ReleaseBuffers();

    void InitDefaultBuffers();
    void ReleaseDefaultBuffers();
    void ReleaseAll();

    int next_frontend_frame = 0;
    int in_flight_frontend_frame[MaxFramesInFlight];
    int backend_frame() const;
    int active_present_image() const;

    ImageHandle backbuffer_img() const;

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
#endif
};

#if defined(REN_GL_BACKEND)
void ResetGLState();
void CheckError(const char *op, ILog *log);
#endif
} // namespace Ren