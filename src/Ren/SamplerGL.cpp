#include "SamplerGL.h"

namespace Ren {
const uint32_t g_gl_min_filter[] = {
    GL_NEAREST,               // NoFilter
    GL_LINEAR_MIPMAP_NEAREST, // Bilinear
    GL_LINEAR_MIPMAP_LINEAR,  // Trilinear
    GL_LINEAR,                // BilinearNoMipmap
};
static_assert(sizeof(g_gl_min_filter) / sizeof(g_gl_min_filter[0]) ==
                  size_t(eTexFilter::_Count),
              "!");

const uint32_t g_gl_mag_filter[] = {
    GL_NEAREST, // NoFilter
    GL_LINEAR,  // Bilinear
    GL_LINEAR,  // Trilinear
    GL_LINEAR,  // BilinearNoMipmap
};
static_assert(sizeof(g_gl_mag_filter) / sizeof(g_gl_mag_filter[0]) ==
                  size_t(eTexFilter::_Count),
              "!");

const uint32_t g_gl_wrap_mode[] = {
    GL_REPEAT,          // Repeat
    GL_CLAMP_TO_EDGE,   // ClampToEdge
    GL_CLAMP_TO_BORDER, // ClampToBorder
};
static_assert(sizeof(g_gl_wrap_mode) / sizeof(g_gl_wrap_mode[0]) ==
                  size_t(eTexRepeat::WrapModesCount),
              "!");

const float AnisotropyLevel = 4.0f;
} // namespace Ren

Ren::Sampler::Sampler(Sampler &&rhs) {
    id_ = exchange(rhs.id_, 0);
    params_ = exchange(rhs.params_, {});
}

Ren::Sampler &Ren::Sampler::operator=(Sampler &&rhs) {
    Destroy();

    id_ = exchange(rhs.id_, 0);
    params_ = exchange(rhs.params_, {});

    return (*this);
}

void Ren::Sampler::Destroy() {
    if (id_) {
        GLuint id = GLuint(exchange(id_, 0));
        glDeleteSamplers(1, &id);
    }
}

void Ren::Sampler::Init(const SamplingParams params) {
    Destroy();

    GLuint new_sampler;
    glGenSamplers(1, &new_sampler);

    glSamplerParameteri(new_sampler, GL_TEXTURE_MIN_FILTER,
                        g_gl_min_filter[size_t(params.filter)]);
    glSamplerParameteri(new_sampler, GL_TEXTURE_MAG_FILTER,
                        g_gl_mag_filter[size_t(params.filter)]);

    glSamplerParameteri(new_sampler, GL_TEXTURE_WRAP_S,
                        g_gl_wrap_mode[size_t(params.repeat)]);
    glSamplerParameteri(new_sampler, GL_TEXTURE_WRAP_T,
                        g_gl_wrap_mode[size_t(params.repeat)]);
    glSamplerParameteri(new_sampler, GL_TEXTURE_WRAP_R,
                        g_gl_wrap_mode[size_t(params.repeat)]);

#ifndef __ANDROID__
    glSamplerParameterf(new_sampler, GL_TEXTURE_LOD_BIAS, params.lod_bias.to_float());
#endif

    glSamplerParameterf(new_sampler, GL_TEXTURE_MIN_LOD, params.min_lod.to_float());
    glSamplerParameterf(new_sampler, GL_TEXTURE_MAX_LOD, params.max_lod.to_float());

    glSamplerParameterf(new_sampler, GL_TEXTURE_MAX_ANISOTROPY_EXT, AnisotropyLevel);

    id_ = uint32_t(new_sampler);
    params_ = params;
}