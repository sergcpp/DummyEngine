#include "TextureArray.h"

#include "Context.h"
#include "GL.h"
#include "Utils.h"

namespace Ren {
extern const uint32_t g_gl_min_filter[];
extern const uint32_t g_gl_mag_filter[];
extern const uint32_t g_gl_wrap_mode[];
} // namespace Ren

Ren::Texture2DArray::Texture2DArray(ApiContext *api_ctx, const std::string_view name, const int w, const int h,
                                    const int layer_count, const int mip_count, const eTexFormat format,
                                    const eTexFilter filter, const eTexUsage usage)
    : api_ctx_(api_ctx), name_(name), w_(w), h_(h), layer_count_(layer_count), format_(format), filter_(filter) {
    GLuint tex_id;
    ren_glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &tex_id);

    // TODO: add srgb here

    ren_glTextureStorage3D_Comp(GL_TEXTURE_2D_ARRAY, tex_id, mip_count,
                                GLInternalFormatFromTexFormat(format, false /* is_srgb */), w, h, layer_count);

    ren_glTextureParameteri_Comp(GL_TEXTURE_2D_ARRAY, tex_id, GL_TEXTURE_MIN_FILTER, g_gl_min_filter[size_t(filter_)]);
    ren_glTextureParameteri_Comp(GL_TEXTURE_2D_ARRAY, tex_id, GL_TEXTURE_MAG_FILTER, g_gl_mag_filter[size_t(filter_)]);

    tex_id_ = uint32_t(tex_id);
}

void Ren::Texture2DArray::Free() {
    if (tex_id_ != 0xffffffff) {
        auto tex_id = GLuint(tex_id_);
        glDeleteTextures(1, &tex_id);
        tex_id_ = 0xffffffff;
    }
}

void Ren::Texture2DArray::FreeImmediate() { Free(); }

Ren::Texture2DArray &Ren::Texture2DArray::operator=(Texture2DArray &&rhs) noexcept {
    if (this == &rhs) {
        return (*this);
    }

    if (tex_id_ != 0xffffffff) {
        auto tex_id = (GLuint)tex_id_;
        glDeleteTextures(1, &tex_id);
    }

    mip_count_ = std::exchange(rhs.mip_count_, 0);
    layer_count_ = std::exchange(rhs.layer_count_, 0);
    format_ = std::exchange(rhs.format_, eTexFormat::Undefined);
    filter_ = std::exchange(rhs.filter_, eTexFilter::Nearest);

    resource_state = std::exchange(rhs.resource_state, eResState::Undefined);

    tex_id_ = std::exchange(rhs.tex_id_, 0xffffffff);

    return (*this);
}

void Ren::Texture2DArray::SetSubImage(const int level, const int layer, const int offsetx, const int offsety,
                                      const int sizex, const int sizey, const eTexFormat format, const Buffer &sbuf,
                                      const int data_off, const int data_len, void *) {
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, sbuf.id());

    ren_glTextureSubImage3D_Comp(GL_TEXTURE_2D_ARRAY, GLuint(tex_id_), level, offsetx, offsety, layer, sizex, sizey, 1,
                                 GLFormatFromTexFormat(format), GLTypeFromTexFormat(format),
                                 reinterpret_cast<const void *>(uintptr_t(data_off)));

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
}

void Ren::Texture2DArray::Clear(const float rgba[4], void *) {
    glClearTexImage(GLuint(tex_id_), 0, GL_RGBA, GL_FLOAT, rgba);
}