#include "Context.h"

#include <mutex>

#include "GL.h"
#include "GLCtx.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)
#endif

namespace Ren {
class DescrMultiPoolAlloc {};

void APIENTRY DebugCallback(const GLenum source, const GLenum type, const GLuint id, const GLenum severity,
                            const GLsizei length, const GLchar *message, const void *userParam) {
    auto *self = reinterpret_cast<const Context *>(userParam);
    if (severity != GL_DEBUG_SEVERITY_NOTIFICATION) {
        if (severity == GL_DEBUG_SEVERITY_HIGH) {
            self->log()->Error("%s", message);
        } else {
            self->log()->Warning("%s", message);
        }
    } else if (type != GL_DEBUG_TYPE_PUSH_GROUP && type != GL_DEBUG_TYPE_POP_GROUP && type != GL_DEBUG_TYPE_OTHER) {
        self->log()->Warning("%s", message);
    }
}
std::once_flag gl_initialize_once;
bool gl_initialized = false;
} // namespace Ren

Ren::Context::Context() {
    for (int i = 0; i < Ren::MaxFramesInFlight; ++i) {
        in_flight_frontend_frame[i] = -1;
    }
}

Ren::Context::~Context() {
    api_ctx_->present_image_refs.clear();
    for (int i = 0; i < MaxFramesInFlight; i++) {
        glDeleteQueries(MaxTimestampQueries, api_ctx_->queries[i]);
    }
    ReleaseAll();
}

bool Ren::Context::Init(const int w, const int h, ILog *log, const int validation_level, const bool nohwrt,
                        const bool nosubgroup, std::string_view) {
    std::call_once(gl_initialize_once, [&]() { gl_initialized = InitGLExtentions(log); });
    if (!gl_initialized) {
        return false;
    }

    w_ = w;
    h_ = h;
    log_ = log;

    api_ctx_ = std::make_unique<ApiContext>();
    for (int i = 0; i < MaxFramesInFlight; i++) {
        api_ctx_->in_flight_fences.emplace_back(MakeFence());
        glGenQueries(MaxTimestampQueries, api_ctx_->queries[i]);
    }

    log_->Info("============================================================================");
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

    {
        GLint i = 0;
        glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &i);
        capabilities.max_vertex_input = i;
        log_->Info("\tMax vtx attribs\t: %i", capabilities.max_vertex_input);

        glGetIntegerv(GL_MAX_VERTEX_OUTPUT_COMPONENTS, &i);
        i /= 4;
        capabilities.max_vertex_output = i;
        log_->Info("\tMax vtx output\t: %i", capabilities.max_vertex_output);
    }

    // determine compute work group sizes
    for (int i = 0; i < 3; i++) {
        GLint val = 0;
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, i, &val);
        capabilities.max_compute_work_group_size[i] = val;
    }

    if (IsExtensionSupported("GL_KHR_shader_subgroup") && !nosubgroup) {
        GLint stages = 0;
        glGetIntegerv(GL_SUBGROUP_SUPPORTED_STAGES_KHR, &stages);
        GLint features = 0;
        glGetIntegerv(GL_SUBGROUP_SUPPORTED_FEATURES_KHR, &features);

        capabilities.subgroup = (stages & GL_COMPUTE_SHADER_BIT) != 0;
        capabilities.subgroup &= (features & GL_SUBGROUP_FEATURE_BASIC_BIT_KHR) != 0;
        capabilities.subgroup &= (features & GL_SUBGROUP_FEATURE_BALLOT_BIT_KHR) != 0;
        capabilities.subgroup &= (features & GL_SUBGROUP_FEATURE_SHUFFLE_BIT_KHR) != 0;
        capabilities.subgroup &= (features & GL_SUBGROUP_FEATURE_VOTE_BIT_KHR) != 0;
        capabilities.subgroup &= (features & GL_SUBGROUP_FEATURE_ARITHMETIC_BIT_KHR) != 0;
        capabilities.subgroup &= (features & GL_SUBGROUP_FEATURE_QUAD_BIT_KHR) != 0;
    }

    log_->Info("============================================================================");

    if (validation_level && (IsExtensionSupported("GL_KHR_debug") || IsExtensionSupported("ARB_debug_output") ||
                             IsExtensionSupported("AMD_debug_output"))) {
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(DebugCallback, this);
    }

#if !defined(__ANDROID__)
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
#endif

    /*const bool srgb_framebuffer = IsExtensionSupported("GL_ARB_framebuffer_sRGB") ||
                                  IsExtensionSupported("GLX_ARB_framebuffer_sRGB") ||
                                  IsExtensionSupported("WGL_ARB_framebuffer_sRGB");
    if (srgb_framebuffer) {
        glEnable(GL_FRAMEBUFFER_SRGB);
    } else {
        log->Warning("SRGB framebuffer is not supported!");
    }*/

    capabilities.spirv = IsExtensionSupported("GL_ARB_gl_spirv");
    capabilities.persistent_buf_mapping = IsExtensionSupported("GL_ARB_buffer_storage");

    const bool bindless_texture_arb = IsExtensionSupported("GL_ARB_bindless_texture");
    const bool bindless_texture_nv = IsExtensionSupported("GL_NV_bindless_texture");
    capabilities.bindless_texture = bindless_texture_arb || bindless_texture_nv;
    capabilities.swrt = capabilities.bindless_texture;

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

    { // RGB565 render target
        GLint color_renderable = 0;
        glGetInternalformativ(GL_RENDERBUFFER, GL_RGB565, GL_COLOR_RENDERABLE, 1, &color_renderable);
        capabilities.rgb565_render_target = (color_renderable != 0);
    }

    InitDefaultBuffers();

    { // create dummy texture
        char name_buf[] = "Present Image [0]";

        TexParams params;
        params.format = eTexFormat::RGBA8;
        params.flags = eTexFlags::NoOwnership;
        params.usage = Bitmask(eTexUsage::RenderTarget);

        api_ctx_->present_image_refs.emplace_back(
            textures_.Insert(name_buf, api_ctx_.get(), TexHandle{}, params, MemAllocation{}, log_));
    }

    texture_atlas_ =
        TextureAtlasArray{api_ctx_.get(),     "Texture Atlas",      TextureAtlasWidth,
                          TextureAtlasHeight, TextureAtlasLayers,   1,
                          eTexFormat::RGBA8,  eTexFilter::Bilinear, Bitmask(eTexUsage::Transfer) | eTexUsage::Sampled};

    return true;
}

void Ren::Context::Resize(const int w, const int h) {
    w_ = w;
    h_ = h;
    glViewport(0, 0, w_, h_);
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

Ren::DescrMultiPoolAlloc &Ren::Context::default_descr_alloc() const {
    static DescrMultiPoolAlloc stub;
    return stub;
}

void Ren::Context::BegSingleTimeCommands(CommandBuffer cmd_buf) {}
Ren::CommandBuffer Ren::Context::BegTempSingleTimeCommands() { return nullptr; }
Ren::SyncFence Ren::Context::EndSingleTimeCommands(CommandBuffer cmd_buf) { return MakeFence(); }
void Ren::Context::EndTempSingleTimeCommands(CommandBuffer cmd_buf) {}
void Ren::Context::InsertReadbackMemoryBarrier(CommandBuffer cmd_buf) { glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT); }
void *Ren::Context::current_cmd_buf() { return nullptr; }

int Ren::Context::WriteTimestamp(const bool) {
    const uint32_t query_index = api_ctx_->query_counts[api_ctx_->backend_frame]++;
    glQueryCounter(api_ctx_->queries[api_ctx_->backend_frame][query_index], GL_TIMESTAMP);

    return int(query_index);
}

uint64_t Ren::Context::GetTimestampIntervalDurationUs(const int query_beg, const int query_end) const {
    return uint64_t(float(api_ctx_->query_results[api_ctx_->backend_frame][query_end] -
                          api_ctx_->query_results[api_ctx_->backend_frame][query_beg]) /
                    1000.0f);
}

void Ren::Context::WaitIdle() {
    glFlush();
    glFinish();
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif
