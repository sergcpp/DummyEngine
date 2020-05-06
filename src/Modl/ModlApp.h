#pragma once

#include <cstdarg>

#include <string>
#include <vector>

#include <Ren/Camera.h>
#include <Ren/Context.h>
#include <Ren/Mesh.h>

struct SDL_Window;

class LogStdout : public Ren::ILog {
  public:
    void Info(const char *fmt, ...) override {
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
    void PollEvents();
    void Destroy();

    bool terminated() const { return quit_; }

  private:
    enum class eCompileResult { RES_SUCCESS = 0, RES_PARSE_ERROR, RES_FILE_NOT_FOUND };
    eCompileResult CompileModel(const std::string &in_file_name,
                                const std::string &out_file_name, bool optimize,
                                bool generate_occlusion);
    eCompileResult CompileAnim(const std::string &in_file_name,
                               const std::string &out_file_name);

    std::vector<Ren::Vec4f>
    GenerateOcclusion(const std::vector<float> &positions,
                      const std::vector<float> &normals,
                      const std::vector<float> &tangents,
                      const std::vector<std::vector<uint32_t>> &indices) const;

    bool quit_;
    SDL_Window *window_ = nullptr;
    LogStdout log_;
#if defined(USE_GL_RENDER)
    void *gl_ctx_ = nullptr;
    uint32_t simple_vao_ = 0, skinned_vao_ = 0;
    uint32_t last_vertex_buf1_ = 0, last_vertex_buf2_, last_skin_vertex_buffer_ = 0,
             last_index_buffer_ = 0;

    void CheckInitVAOs();
#endif

    Ren::MeshRef view_mesh_;
    Ren::AnimSeqRef anim_seq_;
    float anim_time_ = 0.0f;
    Ren::Mat4f matr_palette_[160];
    Ren::Camera cam_;
    Ren::Context ctx_;

    Ren::ProgramRef diag_prog_, diag_colored_prog_, diag_skinned_prog_, skinning_prog_;
    Ren::Texture2DRef checker_tex_;

    float angle_x_ = 0.0f, angle_y_ = 0.0f;
    float view_dist_ = 10.0f;
    bool mouse_grabbed_ = false;

    enum class eViewMode {
        Diffuse, DiagNormals1, DiagNormals2, DiagUVs1, DiagUVs2, DiagVtxColor
    } view_mode_ = eViewMode::DiagNormals1;

    void InitInternal();
    void DestroyInternal();
    void DrawMeshSimple(Ren::MeshRef &ref);
    void DrawMeshColored(Ren::MeshRef &ref);
    void DrawMeshSkeletal(Ren::MeshRef &ref, float dt_s);

    void PrintUsage();

    Ren::Texture2DRef OnTextureNeeded(const char *name);
    Ren::ProgramRef OnProgramNeeded(const char *name, const char *vs_shader,
                                    const char *fs_shader);
    Ren::MaterialRef OnMaterialNeeded(const char *name);

    void ClearColorAndDepth(float r, float g, float b, float a);
};
