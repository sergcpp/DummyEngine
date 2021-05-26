#include "Context.h"

#include "GL.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)
#endif

Ren::Context::~Context() { ReleaseAll(); }

void Ren::Context::Init(int w, int h, ILog *log) {
    InitGLExtentions();
    RegisterAsMainThread();

    w_ = w;
    h_ = h;
    log_ = log;

    log_->Info("===========================================");
    log_->Info("Device info:");

    // print device info
#if !defined(EMSCRIPTEN) && !defined(__ANDROID__)
    GLint gl_major_version;
    glGetIntegerv(GL_MAJOR_VERSION, &gl_major_version);
    GLint gl_minor_version;
    glGetIntegerv(GL_MINOR_VERSION, &gl_minor_version);
    log_->Info("\tOpenGL version\t: %i.%i", int(gl_major_version), int(gl_minor_version));
#endif

    log_->Info("\tVendor\t\t: %s", glGetString(GL_VENDOR));
    log_->Info("\tRenderer\t: %s", glGetString(GL_RENDERER));
    log_->Info("\tGLSL version\t: %s", glGetString(GL_SHADING_LANGUAGE_VERSION));

    log_->Info("Capabilities:");

    // determine if anisotropy supported
    if (IsExtensionSupported("GL_EXT_texture_filter_anisotropic")) {
        GLfloat f;
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &f);
        capabilities.max_anisotropy = f;
        log_->Info("\tAnisotropy\t: %f", capabilities.max_anisotropy);
    }

    { // how many uniform vec4 vectors can be used
        GLint i = 0;
        glGetIntegerv(/*GL_MAX_VERTEX_UNIFORM_VECTORS*/ GL_MAX_VERTEX_UNIFORM_COMPONENTS,
                      &i);
        i /= 4;
        if (i == 0)
            i = 256;
        capabilities.max_uniform_vec4 = i;
        log_->Info("\tMax uniforms\t: %i", capabilities.max_uniform_vec4);
    }

    {
        GLint i = 0;
        glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &i);
        capabilities.max_vertex_input = i;
        log_->Info("\tMax vertex attribs\t: %i", capabilities.max_vertex_input);

        glGetIntegerv(GL_MAX_VERTEX_OUTPUT_COMPONENTS, &i);
        i /= 4;
        capabilities.max_vertex_output = i;
        log_->Info("\tMax vertex output\t: %i", capabilities.max_vertex_output);
    }

    // determine compute work group sizes
    for (int i = 0; i < 3; i++) {
        GLint val;
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, i, &val);
        capabilities.max_compute_work_group_size[i] = val;
    }

    // how many bones(mat4) can be used at time
    /*Mesh::max_gpu_bones = capabilities.max_uniform_vec4 / 8;
    log_->Info("\tBones per pass\t: %i", Mesh::max_gpu_bones);
    char buff[16];
    sprintf(buff, "%i", Mesh::max_gpu_bones);*/
    /*glsl_defines_ += "#define MAX_GPU_BONES ";
    glsl_defines_ += buff;
    glsl_defines_ += "\r\n";*/

    log_->Info("===========================================");

#if !defined(NDEBUG) && !defined(__APPLE__)
    if (IsExtensionSupported("GL_KHR_debug") ||
        IsExtensionSupported("ARB_debug_output") ||
        IsExtensionSupported("AMD_debug_output")) {

        auto gl_debug_proc = [](GLenum source, GLenum type, GLuint id, GLenum severity,
                                GLsizei length, const GLchar *message,
                                const void *userParam) {
            if (severity != GL_DEBUG_SEVERITY_NOTIFICATION) {
                auto *self = reinterpret_cast<const Context *>(userParam);
                self->log_->Error("%s", message);
                __debugbreak();
            }
        };

        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(gl_debug_proc, this);
    }
#endif

#if !defined(__ANDROID__)
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
#endif

    capabilities.gl_spirv = IsExtensionSupported("GL_ARB_gl_spirv");
    capabilities.persistent_buf_mapping = IsExtensionSupported("GL_ARB_buffer_storage");

    const bool bindless_texture_arb = IsExtensionSupported("GL_ARB_bindless_texture");
    const bool bindless_texture_nv = IsExtensionSupported("GL_NV_bindless_texture");
    capabilities.bindless_texture = bindless_texture_arb || bindless_texture_nv;

    { // minimal texture buffer offset alignment
        GLint tex_buf_offset_alignment;
        glGetIntegerv(GL_TEXTURE_BUFFER_OFFSET_ALIGNMENT, &tex_buf_offset_alignment);
        capabilities.tex_buf_offset_alignment = tex_buf_offset_alignment;
    }

    { // minimal uniform buffer offset alignment
        GLint unif_buf_offset_alignment;
        glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &unif_buf_offset_alignment);
        capabilities.unif_buf_offset_alignment = unif_buf_offset_alignment;
    }

    default_vertex_buf1_ = buffers_.Add("default_vtx_buf1", eBufferType::VertexAttribs,
                                        eBufferAccessType::Draw,
                                        eBufferAccessFreq::Static, 64 * 1024 * 1024);
    default_vertex_buf2_ = buffers_.Add("default_vtx_buf2", eBufferType::VertexAttribs,
                                        eBufferAccessType::Draw,
                                        eBufferAccessFreq::Static, 64 * 1024 * 1024);
    default_skin_vertex_buf_ = buffers_.Add(
        "default_skin_vtx_buf", eBufferType::VertexAttribs, eBufferAccessType::Draw,
        eBufferAccessFreq::Static, 64 * 1024 * 1024);
    default_delta_buf_ = buffers_.Add("default_delta_buf", eBufferType::VertexAttribs,
                                      eBufferAccessType::Draw, eBufferAccessFreq::Static,
                                      64 * 1024 * 1024);
    default_indices_buf_ = buffers_.Add("default_ndx_buf2", eBufferType::VertexIndices,
                                        eBufferAccessType::Draw,
                                        eBufferAccessFreq::Static, 64 * 1024 * 1024);

    texture_atlas_ =
        TextureAtlasArray{TextureAtlasWidth, TextureAtlasHeight, TextureAtlasLayers,
                          eTexFormat::RawRGBA8888, eTexFilter::BilinearNoMipmap};
}

void Ren::Context::Resize(int w, int h) {
    w_ = w;
    h_ = h;
    glViewport(0, 0, w_, h_);
}

Ren::ShaderRef Ren::Context::LoadShaderGLSL(const char *name, const char *shader_src,
                                            eShaderType type,
                                            eShaderLoadStatus *load_status) {
    ShaderRef ref = shaders_.FindByName(name);

    if (!ref) {
        ref = shaders_.Add(name, shader_src, type, load_status, log_);
    } else {
        if (ref->ready()) {
            if (load_status) {
                (*load_status) = eShaderLoadStatus::Found;
            }
        } else if (shader_src) {
            ref->Init(shader_src, type, load_status, log_);
        }
    }

    return ref;
}

#ifndef __ANDROID__
Ren::ShaderRef Ren::Context::LoadShaderSPIRV(const char *name, const uint8_t *shader_data,
                                             int data_size, eShaderType type,
                                             eShaderLoadStatus *load_status) {
    ShaderRef ref = shaders_.FindByName(name);

    if (!ref) {
        ref = shaders_.Add(name, shader_data, data_size, type, load_status, log_);
    } else {
        if (ref->ready()) {
            if (load_status) {
                (*load_status) = eShaderLoadStatus::Found;
            }
        } else if (shader_data) {
            ref->Init(shader_data, data_size, type, load_status, log_);
        }
    }

    return ref;
}
#endif

Ren::ProgramRef Ren::Context::LoadProgram(const char *name, ShaderRef vs_ref,
                                          ShaderRef fs_ref, ShaderRef tcs_ref,
                                          ShaderRef tes_ref,
                                          eProgLoadStatus *load_status) {
    ProgramRef ref = programs_.FindByName(name);

    if (!ref) {
        ref = programs_.Add(name, std::move(vs_ref), std::move(fs_ref),
                            std::move(tcs_ref), std::move(tes_ref), load_status, log_);
    } else {
        if (ref->ready()) {
            if (load_status) {
                (*load_status) = eProgLoadStatus::Found;
            }
        } else if (!ref->ready() && vs_ref && fs_ref) {
            ref->Init(std::move(vs_ref), std::move(fs_ref), std::move(tcs_ref),
                      std::move(tes_ref), load_status, log_);
        }
    }

    return ref;
}

Ren::ProgramRef Ren::Context::LoadProgram(const char *name, ShaderRef cs_ref,
                                          eProgLoadStatus *load_status) {
    ProgramRef ref = programs_.FindByName(name);

    if (!ref) {
        ref = programs_.Add(name, std::move(cs_ref), load_status, log_);
    } else {
        if (ref->ready()) {
            if (load_status)
                *load_status = eProgLoadStatus::Found;
        } else if (!ref->ready() && cs_ref) {
            ref->Init(std::move(cs_ref), load_status, log_);
        }
    }

    return ref;
}

bool Ren::Context::IsExtensionSupported(const char *ext) {
    GLint ext_count = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &ext_count);

    for (GLint i = 0; i < ext_count; i++) {
        const char *extension = (const char *)glGetStringi(GL_EXTENSIONS, i);
        if (strcmp(extension, ext) == 0) {
            return true;
        }
    }

    return false;
}

void Ren::ResetGLState() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);

    Ren::GLUnbindTextureUnits(0, 24);
    Ren::GLUnbindSamplers(0, 24);
    Ren::GLUnbindBufferUnits(0, 24);
}

void Ren::CheckError(const char *op, ILog *log) {
    for (GLint error = glGetError(); error; error = glGetError()) {
        log->Error("after %s glError (0x%x)", op, error);
    }
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif
