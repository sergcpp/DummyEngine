#include "DrawCall.h"

#include <cassert>

#include "Buffer.h"
#include "GL.h"
#include "ProbeStorage.h"
#include "Texture.h"

namespace Ren {
const uint32_t gl_binding_targets[] = {
    GL_TEXTURE_2D,             // Tex2D
    GL_TEXTURE_2D_MULTISAMPLE, // Tex2DMs
    GL_TEXTURE_CUBE_MAP_ARRAY, // TexCubeArray
    GL_TEXTURE_BUFFER,         // TBuf
    GL_UNIFORM_BUFFER,         // UBuf
    GL_SHADER_STORAGE_BUFFER,  // SBuf
    0xffffffff,                // Image
    0xffffffff                 // AccStruct
};
static_assert(COUNT_OF(gl_binding_targets) == size_t(eBindTarget::_Count), "!");

extern const uint32_t g_gl_internal_formats[];

int g_param_buf_binding;
} // namespace Ren

uint32_t Ren::GLBindTarget(const eBindTarget binding) { return gl_binding_targets[size_t(binding)]; }

void Ren::DispatchCompute(const Pipeline &comp_pipeline, Vec3u grp_count, Span<const Binding> bindings,
                          const void *uniform_data, int uniform_data_len,
                          DescrMultiPoolAlloc *descr_alloc, ILog *log) {
    for (const auto &b : bindings) {
        if (b.trg == eBindTarget::Tex2D) {
            ren_glBindTextureUnit_Comp(GLBindTarget(b.trg), GLuint(b.loc), GLuint(b.handle.tex->id()));
        } else if (b.trg == eBindTarget::UBuf || b.trg == eBindTarget::SBuf) {
            if (b.offset) {
                assert(b.size != 0);
                glBindBufferRange(GLBindTarget(b.trg), b.loc, b.handle.buf->id(), b.offset, b.size);
            } else {
                glBindBufferBase(GLBindTarget(b.trg), b.loc, b.handle.buf->id());
            }
        } else if (b.trg == eBindTarget::TBuf) {
            ren_glBindTextureUnit_Comp(GLBindTarget(b.trg), GLuint(b.loc), GLuint(b.handle.tex_buf->id()));
        } else if (b.trg == eBindTarget::TexCubeArray) {
            ren_glBindTextureUnit_Comp(GLBindTarget(b.trg), GLuint(b.loc), GLuint(b.handle.cube_arr->handle().id));
        } else if (b.trg == eBindTarget::Image) {
            glBindImageTexture(GLuint(b.loc), GLuint(b.handle.tex_buf->id()), 0, GL_FALSE, 0, GL_READ_WRITE,
                               GLInternalFormatFromTexFormat(b.handle.tex->params.format, false));
        }
    }

    Buffer temp_unif_buffer, temp_stage_buffer;
    if (uniform_data && uniform_data_len) {
        temp_unif_buffer = Buffer("Temp uniform buf", nullptr, eBufType::Uniform, uniform_data_len, 16);
        temp_stage_buffer = Buffer("Temp stage buf", nullptr, eBufType::Stage, uniform_data_len, 16);
        {
            uint8_t *stage_data = temp_stage_buffer.Map(BufMapWrite);
            memcpy(stage_data, uniform_data, uniform_data_len);

            temp_stage_buffer.FlushMappedRange(0, uniform_data_len);
            temp_stage_buffer.Unmap();
        }
        CopyBufferToBuffer(temp_stage_buffer, 0, temp_unif_buffer, 0, uniform_data_len, nullptr);
    }
    glBindBufferBase(GL_UNIFORM_BUFFER, GLuint(g_param_buf_binding), temp_unif_buffer.id());

    glUseProgram(comp_pipeline.prog()->id());

    glDispatchCompute(grp_count[0], grp_count[1], grp_count[2]);
}

void Ren::DispatchComputeIndirect(const Pipeline &comp_pipeline, const Buffer &indir_buf,
                                  const uint32_t indir_buf_offset, Span<const Binding> bindings,
                                  const void *uniform_data, int uniform_data_len, DescrMultiPoolAlloc *descr_alloc,
                                  ILog *log) {
    for (const auto &b : bindings) {
        if (b.trg == eBindTarget::Tex2D) {
            ren_glBindTextureUnit_Comp(GLBindTarget(b.trg), GLuint(b.loc), GLuint(b.handle.tex->id()));
        } else if (b.trg == eBindTarget::UBuf || b.trg == eBindTarget::SBuf) {
            if (b.offset) {
                assert(b.size != 0);
                glBindBufferRange(GLBindTarget(b.trg), b.loc, b.handle.buf->id(), b.offset, b.size);
            } else {
                glBindBufferBase(GLBindTarget(b.trg), b.loc, b.handle.buf->id());
            }
        } else if (b.trg == eBindTarget::TBuf) {
            ren_glBindTextureUnit_Comp(GLBindTarget(b.trg), GLuint(b.loc), GLuint(b.handle.tex_buf->id()));
        } else if (b.trg == eBindTarget::TexCubeArray) {
            ren_glBindTextureUnit_Comp(GLBindTarget(b.trg), GLuint(b.loc), GLuint(b.handle.cube_arr->handle().id));
        } else if (b.trg == eBindTarget::Image) {
            glBindImageTexture(GLuint(b.loc), GLuint(b.handle.tex_buf->id()), 0, GL_FALSE, 0, GL_READ_WRITE,
                               GLInternalFormatFromTexFormat(b.handle.tex->params.format, false));
        }
    }

    Buffer temp_unif_buffer, temp_stage_buffer;
    if (uniform_data) {
        temp_unif_buffer = Buffer("Temp uniform buf", nullptr, eBufType::Uniform, uniform_data_len, 16);
        temp_stage_buffer = Buffer("Temp stage buf", nullptr, eBufType::Stage, uniform_data_len, 16);
        {
            uint8_t *stage_data = temp_stage_buffer.Map(BufMapWrite);
            memcpy(stage_data, uniform_data, uniform_data_len);

            temp_stage_buffer.FlushMappedRange(0, uniform_data_len);
            temp_stage_buffer.Unmap();
        }
        CopyBufferToBuffer(temp_stage_buffer, 0, temp_unif_buffer, 0, uniform_data_len, nullptr);
    }
    glBindBufferBase(GL_UNIFORM_BUFFER, GLuint(g_param_buf_binding), temp_unif_buffer.id());

    glUseProgram(comp_pipeline.prog()->id());

    glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, indir_buf.id());
    glDispatchComputeIndirect(GLintptr(indir_buf_offset));
    glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, 0);
}
