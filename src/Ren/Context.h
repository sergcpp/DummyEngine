#pragma once

#include "Anim.h"
#include "Buffer.h"
#include "Material.h"
#include "Mesh.h"
#include "Program.h"
#include "RenderThread.h"
#include "Texture.h"

struct SWcontext;

namespace Ren {
class Context : public RenderThread {
protected:
    int w_ = 0, h_ = 0;

    MeshStorage      meshes_;
    MaterialStorage  materials_;
    ProgramStorage   programs_;
    Texture2DStorage textures_;
    AnimSeqStorage   anims_;
    BufferStorage    buffers_;

    BufferRef       default_vertex_buf1_, default_vertex_buf2_, default_skin_vertex_buf_,
                    default_indices_buf_;

#if defined(USE_SW_RENDER)
    SWcontext       *sw_ctx_;
#endif

#if defined(USE_GL_RENDER)
    std::string glsl_defines_;
#endif
public:
    ~Context();

    void Init(int w, int h);

    int w() const {
        return w_;
    }
    int h() const {
        return h_;
    }

    BufferRef default_vertex_buf1() const { return default_vertex_buf1_; }
    BufferRef default_vertex_buf2() const { return default_vertex_buf2_; }
    BufferRef default_skin_vertex_buf() const { return default_skin_vertex_buf_; }
    BufferRef default_indices_buf() const { return default_indices_buf_; }

    void Resize(int w, int h);

    /*** Mesh ***/
    MeshRef LoadMesh(const char *name, std::istream &data, material_load_callback on_mat_load);
    MeshRef LoadMesh(const char *name, std::istream &data, material_load_callback on_mat_load,
                     BufferRef &vertex_buf1, BufferRef &vertex_buf2, BufferRef &index_buf, BufferRef &skin_vertex_buf);

    /*** Material ***/
    MaterialRef LoadMaterial(const char *name, const char *mat_src, eMatLoadStatus *status, const program_load_callback &on_prog_load,
                             const texture_load_callback &on_tex_load);
    MaterialRef GetMaterial(uint32_t index);
    int NumMaterialsNotReady();
    void ReleaseMaterials();

    /*** Program ***/
#if defined(USE_GL_RENDER)
    ProgramRef LoadProgramGLSL(const char *name, const char *vs_source, const char *fs_source, eProgLoadStatus *load_status);
    ProgramRef LoadProgramGLSL(const char *name, const char *cs_source, eProgLoadStatus *load_status);
#ifndef __ANDROID__
    ProgramRef LoadProgramSPIRV(const char *name, const uint8_t *vs_data, const int vs_data_size,
                                                  const uint8_t *fs_data, const int fs_data_size, eProgLoadStatus *load_status);
    ProgramRef LoadProgramSPIRV(const char *name, const uint8_t *cs_data, const int cs_data_size, eProgLoadStatus *load_status);
#endif
#elif defined(USE_SW_RENDER)
    ProgramRef LoadProgramSW(const char *name, void *vs_shader, void *fs_shader, int num_fvars,
                             const Attribute *attrs, const Uniform *unifs, eProgLoadStatus *load_status);
#endif
    ProgramRef GetProgram(uint32_t index);
    int NumProgramsNotReady();
    void ReleasePrograms();

    /*** Texture ***/
    Texture2DRef LoadTexture2D(const char *name, const void *data, int size, const Texture2DParams &p, eTexLoadStatus *load_status);
    Texture2DRef LoadTextureCube(const char *name, const void *data[6], const int size[6], const Texture2DParams &p, eTexLoadStatus *load_status);

    int NumTexturesNotReady();
    void ReleaseTextures();

    /*** Anims ***/
    AnimSeqRef LoadAnimSequence(const char *name, std::istream &data);
    int NumAnimsNotReady();
    void ReleaseAnims();

    /*** Buffers ***/
    BufferRef CreateBuffer(const char *name, uint32_t initial_size);
    void ReleaseBuffers();

    void ReleaseAll();

#if defined(USE_GL_RENDER)
    struct {
        float max_anisotropy = 0.0f;
        int max_uniform_vec4 = 0;
        int max_vertex_input = 0,
            max_vertex_output = 0;
        bool gl_spirv = false;
    } capabilities;
    
    static bool IsExtensionSupported(const char *ext);
#elif defined(USE_SW_RENDER)
    int max_uniform_vec4 = 0;
#endif
};

#if defined(USE_GL_RENDER)
void CheckError(const char *op);
#endif
}