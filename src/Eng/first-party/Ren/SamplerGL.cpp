#include "SamplerGL.h"

#include "GL.h"

namespace Ren {
#define X(_0, _1, _2, _3, _4) _3,
extern const uint32_t g_min_filter_gl[] = {
#include "TextureFilter.inl"
};
#undef X

#define X(_0, _1, _2, _3, _4) _4,
extern const uint32_t g_mag_filter_gl[] = {
#include "TextureFilter.inl"
};
#undef X

#define X(_0, _1, _2) _2,
extern const uint32_t g_wrap_mode_gl[] = {
#include "../TextureWrap.inl"
};
#undef X

#define X(_0, _1, _2) _2,
extern const uint32_t g_compare_func_gl[] = {
#include "../TextureCompare.inl"
};
#undef X

extern const float AnisotropyLevel = 4;
} // namespace Ren

Ren::Sampler &Ren::Sampler::operator=(Sampler &&rhs) {
    if (&rhs == this) {
        return (*this);
    }

    RefCounter::operator=(static_cast<RefCounter &&>(rhs));

    Destroy();

    id_ = std::exchange(rhs.id_, 0);
    params_ = std::exchange(rhs.params_, {});

    return (*this);
}

void Ren::Sampler::Destroy() {
    if (id_) {
        GLuint id = GLuint(std::exchange(id_, 0));
        glDeleteSamplers(1, &id);
    }
}

void Ren::Sampler::Init(ApiContext *api_ctx, const SamplingParams params) {
    Destroy();

    GLuint new_sampler;
    glGenSamplers(1, &new_sampler);

    glSamplerParameteri(new_sampler, GL_TEXTURE_MIN_FILTER, g_min_filter_gl[size_t(params.filter)]);
    glSamplerParameteri(new_sampler, GL_TEXTURE_MAG_FILTER, g_mag_filter_gl[size_t(params.filter)]);

    glSamplerParameteri(new_sampler, GL_TEXTURE_WRAP_S, g_wrap_mode_gl[size_t(params.wrap)]);
    glSamplerParameteri(new_sampler, GL_TEXTURE_WRAP_T, g_wrap_mode_gl[size_t(params.wrap)]);
    glSamplerParameteri(new_sampler, GL_TEXTURE_WRAP_R, g_wrap_mode_gl[size_t(params.wrap)]);

    if (params.compare != eTexCompare::None) {
        glSamplerParameteri(new_sampler, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        glSamplerParameteri(new_sampler, GL_TEXTURE_COMPARE_FUNC, g_compare_func_gl[int(params.compare)]);
    } else {
        glSamplerParameteri(new_sampler, GL_TEXTURE_COMPARE_MODE, GL_NONE);
    }

#ifndef __ANDROID__
    glSamplerParameterf(new_sampler, GL_TEXTURE_LOD_BIAS, params.lod_bias.to_float());
#endif

    glSamplerParameterf(new_sampler, GL_TEXTURE_MIN_LOD, params.min_lod.to_float());
    glSamplerParameterf(new_sampler, GL_TEXTURE_MAX_LOD, params.max_lod.to_float());

    glSamplerParameterf(new_sampler, GL_TEXTURE_MAX_ANISOTROPY_EXT, AnisotropyLevel);

    id_ = uint32_t(new_sampler);
    params_ = params;
}

void Ren::GLUnbindSamplers(const int start, const int count) {
    for (int i = start; i < start + count; i++) {
        glBindSampler(GLuint(i), 0);
    }
}
