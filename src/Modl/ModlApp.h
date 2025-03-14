#pragma once

#include <cstdarg>

#include <string>
#include <vector>

#include <Phy/MMat.h>
#include <Ren/Camera.h>
#include <Ren/Context.h>
#include <Ren/Mesh.h>
#include <Ren/Pipeline.h>
#include <Ren/RenderPass.h>
#include <Ren/VertexInput.h>

#ifndef _WINDEF_
struct HWND__; // Forward or never
typedef HWND__ *HWND;
struct HDC__;
typedef HDC__ *HDC;
struct HGLRC__;
typedef HGLRC__ *HGLRC;
#endif

class LogStdout : public Ren::ILog {
  public:
    void Info(const char *fmt, ...) override {
        va_list vl;
        va_start(vl, fmt);
        vprintf(fmt, vl);
        va_end(vl);
        putc('\n', stdout);
    }
    void Warning(const char *fmt, ...) override {
        va_list vl;
        va_start(vl, fmt);
        vprintf(fmt, vl);
        va_end(vl);
        putc('\n', stdout);
    }
    void Error(const char *fmt, ...) override {
        va_list vl;
        va_start(vl, fmt);
        vprintf(fmt, vl);
        va_end(vl);
        putc('\n', stderr);
    }
};

class ModlApp {
  public:
    ModlApp();

    int Run(const std::vector<std::string> &args);

    int Init(int w, int h);
    void Frame();
    void Destroy();

    bool terminated() const { return quit_; }

    // private:
    enum class eCompileResult { RES_SUCCESS = 0, RES_PARSE_ERROR, RES_FILE_NOT_FOUND };
    eCompileResult CompileModel(const std::string &in_file_name, const std::string &out_file_name, bool optimize,
                                bool generate_occlusion);
    eCompileResult CompileAnim(const std::string &in_file_name, const std::string &out_file_name);

    std::vector<Phy::Vec4f> GenerateOcclusion(const std::vector<float> &positions, const std::vector<float> &normals,
                                              const std::vector<float> &tangents,
                                              const std::vector<std::vector<uint32_t>> &indices) const;

    bool quit_;
#if defined(_WIN32)
    HWND window_handle_ = {};
    HDC device_context_ = {};
    HGLRC gl_ctx_ = {};
#endif
    LogStdout log_;
#if defined(REN_GL_BACKEND)
    uint32_t simple_vao_ = 0, skinned_vao_ = 0;
    uint32_t last_vertex_buf1_ = 0, last_vertex_buf2_ = 0, last_skin_vertex_buffer_ = 0, last_delta_buffer_ = 0,
             last_index_buffer_ = 0;
    uint32_t uniform_buf_ = 0;

    void CheckInitVAOs();
#endif

    Ren::MeshRef view_mesh_;
    Ren::AnimSeqRef anim_seq_;
    float anim_time_ = 0.0f;
    Ren::Mat4f matr_palette_[256];
    Ren::Camera cam_;
    std::unique_ptr<Ren::Context> ctx_;

    Ren::ProgramRef diag_prog_, diag_colored_prog_, diag_skinned_prog_, skinning_prog_;
    Ren::TexRef checker_tex_;

    float angle_x_ = 0.0f, angle_y_ = 0.0f;
    float offset_x_ = 0.0f, offset_y_ = 0.0f;
    float view_dist_ = 10.0f;
    int grabbed_pointer_ = -1;

    int shape_key_index_ = -1;

    Ren::VertexInputRef draw_vi_;
    Ren::RenderPassRef rp_draw_;
    Ren::PipelineStorage pipelines_;

    enum class eViewMode {
        Diffuse,
        DiagNormals1,
        DiagTangent,
        DiagNormals2,
        DiagUVs1,
        DiagUVs2,
        DiagVtxColor
    } view_mode_ = eViewMode::DiagNormals1;

    void InitInternal();
    void DestroyInternal();
    void DrawMeshSimple(const Ren::MeshRef &ref);
    void DrawMeshColored(const Ren::MeshRef &ref);
    void DrawMeshSkeletal(Ren::MeshRef &ref, float dt_s);

    void PrintUsage();

    Ren::TexRef OnTextureNeeded(std::string_view name);
    Ren::SamplerRef OnSamplerNeeded(Ren::SamplingParams params);
    void OnPipelinesNeeded(uint32_t flags, std::string_view vs_shader, std::string_view fs_shader,
                           std::string_view arg3, std::string_view arg4,
                           Ren::SmallVectorImpl<Ren::PipelineRef> &out_pipelines);
    std::pair<Ren::MaterialRef, Ren::MaterialRef> OnMaterialNeeded(std::string_view name);

    static void ClearColorAndDepth(float r, float g, float b, float a);
};
