#pragma once

#include "Anim.h"
#include "Buffer.h"
#include "Material.h"
#include "Mesh.h"
#include "Program.h"
#include "Shader.h"
#include "TaskExecutor.h"
#include "Texture.h"
#include "TextureAtlas.h"
#include "TextureRegion.h"

struct SWcontext;

namespace Ren {
const int TextureAtlasWidth = 1024, TextureAtlasHeight = 512, TextureAtlasLayers = 4;

class Context : public TaskExecutor {
  protected:
    int w_ = 0, h_ = 0;
    ILog *log_ = nullptr;

    MeshStorage meshes_;
    MaterialStorage materials_;
    ProgramStorage programs_;
    ShaderStorage shaders_;
    Texture2DStorage textures_;
    Texture1DStorage textures_1D_;
    TextureRegionStorage texture_regions_;
    AnimSeqStorage anims_;
    BufferStorage buffers_;

    BufferRef default_vertex_buf1_, default_vertex_buf2_, default_skin_vertex_buf_,
        default_delta_buf_, default_indices_buf_;

    TextureAtlasArray texture_atlas_;

#if defined(USE_SW_RENDER)
    SWcontext *sw_ctx_;
#endif
  public:
    Context() = default;
    ~Context();

    Context(const Context &rhs) = delete;

    void Init(int w, int h, ILog *log);

    int w() const { return w_; }
    int h() const { return h_; }

    ILog *log() const { return log_; }

    Texture2DStorage &textures() { return textures_; }
    Texture1DStorage &textures1D() { return textures_1D_; }
    MaterialStorage &materials() { return materials_; }

    BufferRef default_vertex_buf1() const { return default_vertex_buf1_; }
    BufferRef default_vertex_buf2() const { return default_vertex_buf2_; }
    BufferRef default_skin_vertex_buf() const { return default_skin_vertex_buf_; }
    BufferRef default_delta_buf() const { return default_delta_buf_; }
    BufferRef default_indices_buf() const { return default_indices_buf_; }

    TextureAtlasArray &texture_atlas() { return texture_atlas_; }

    void Resize(int w, int h);

    /*** Mesh ***/
    MeshRef LoadMesh(const char *name, const float *positions, int vtx_count,
                     const uint32_t *indices, int ndx_count,
                     eMeshLoadStatus *load_status);
    MeshRef LoadMesh(const char *name, const float *positions, int vtx_count,
                     const uint32_t *indices, int ndx_count, BufferRef &vertex_buf1,
                     BufferRef &vertex_buf2, BufferRef &index_buf,
                     eMeshLoadStatus *load_status);
    MeshRef LoadMesh(const char *name, std::istream *data,
                     const material_load_callback &on_mat_load,
                     eMeshLoadStatus *load_status);
    MeshRef LoadMesh(const char *name, std::istream *data,
                     const material_load_callback &on_mat_load, BufferRef &vertex_buf1,
                     BufferRef &vertex_buf2, BufferRef &index_buf,
                     BufferRef &skin_vertex_buf, BufferRef &delta_buf,
                     eMeshLoadStatus *load_status);

    /*** Material ***/
    MaterialRef LoadMaterial(const char *name, const char *mat_src,
                             eMatLoadStatus *status,
                             const program_load_callback &on_prog_load,
                             const texture_load_callback &on_tex_load);
    int NumMaterialsNotReady();
    void ReleaseMaterials();

    /*** Program ***/
#if defined(USE_GL_RENDER)
    ShaderRef LoadShaderGLSL(const char *name, const char *shader_src, eShaderType type,
                             eShaderLoadStatus *load_status);
#ifndef __ANDROID__
    ShaderRef LoadShaderSPIRV(const char *name, const uint8_t *shader_data, int data_size,
                              eShaderType type, eShaderLoadStatus *load_status);
#endif

    ProgramRef LoadProgram(const char *name, ShaderRef vs_ref, ShaderRef fs_ref,
                           ShaderRef tcs_ref, ShaderRef tes_ref,
                           eProgLoadStatus *load_status);
    ProgramRef LoadProgram(const char *name, ShaderRef cs_source,
                           eProgLoadStatus *load_status);
#elif defined(USE_SW_RENDER)
    ProgramRef LoadProgramSW(const char *name, void *vs_shader, void *fs_shader,
                             int num_fvars, const Attribute *attrs, const Uniform *unifs,
                             eProgLoadStatus *load_status);
#endif
    ProgramRef GetProgram(uint32_t index);
    int NumProgramsNotReady();
    void ReleasePrograms();

    /*** Texture ***/
    Tex2DRef LoadTexture2D(const char *name, const Tex2DParams &p,
                           eTexLoadStatus *load_status);
    Tex2DRef LoadTexture2D(const char *name, const void *data, int size,
                           const Tex2DParams &p, eTexLoadStatus *load_status);
    Tex2DRef LoadTextureCube(const char *name, const void *data[6], const int size[6],
                             const Tex2DParams &p, eTexLoadStatus *load_status);

    void VisitTextures(uint32_t mask,
                       const std::function<void(Texture2D &tex)> &callback);
    int NumTexturesNotReady();
    void Release2DTextures();

    /*** Texture 1D ***/
    Tex1DRef CreateTexture1D(const char *name, BufferRef buf, eTexFormat format,
                             uint32_t offset, uint32_t size);
    void Release1DTextures();

    /** Texture regions **/
    TextureRegionRef LoadTextureRegion(const char *name, const void *data, int size,
                                       const Tex2DParams &p, eTexLoadStatus *load_status);

    void ReleaseTextureRegions();

    /*** Anims ***/
    AnimSeqRef LoadAnimSequence(const char *name, std::istream &data);
    int NumAnimsNotReady();
    void ReleaseAnims();

    /*** Buffers ***/
    BufferRef CreateBuffer(const char *name, eBufferType type, eBufferAccessType access,
                           eBufferAccessFreq freq, uint32_t initial_size);
    void ReleaseBuffers();

    void ReleaseAll();

#if defined(USE_GL_RENDER)
    struct { // NOLINT
        float max_anisotropy = 0.0f;
        int max_uniform_vec4 = 0;
        int max_vertex_input = 0, max_vertex_output = 0;
        bool gl_spirv = false;
        bool persistent_buf_mapping = false;
        int max_compute_work_group_size[3] = {};
        int tex_buf_offset_alignment = 1;
        int unif_buf_offset_alignment = 1;
    } capabilities;

    static bool IsExtensionSupported(const char *ext);
#elif defined(USE_SW_RENDER)
    int max_uniform_vec4 = 0;
#endif
};

#if defined(USE_GL_RENDER)
void ResetGLState();
void CheckError(const char *op, ILog *log);
#endif
} // namespace Ren
