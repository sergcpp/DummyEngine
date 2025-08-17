#include "DrawCall.h"

#include <cassert>

#include "Buffer.h"
#include "GL.h"
#include "Pipeline.h"
#include "ProbeStorage.h"
#include "Sampler.h"
#include "Texture.h"

namespace Ren {
extern const uint32_t g_internal_formats_gl[];

int g_param_buf_binding;
} // namespace Ren

uint32_t Ren::GLBindTarget(const Texture &tex, const int view) {
    if (view == 0) {
        // NOTE: Assume all additional views are 2D textures
        return GL_TEXTURE_2D;
    }
    if (Bitmask<eTexFlags>{tex.params.flags} & eTexFlags::Array) {
        return GL_TEXTURE_2D_ARRAY;
    }
    if (tex.params.d != 0) {
        return GL_TEXTURE_3D;
    }
    return GL_TEXTURE_2D;
}

void Ren::DispatchCompute(CommandBuffer, const Pipeline &comp_pipeline, Vec3u grp_count, Span<const Binding> bindings,
                          const void *uniform_data, int uniform_data_len, DescrMultiPoolAlloc *descr_alloc, ILog *log) {
    for (const auto &b : bindings) {
        if (b.trg == eBindTarget::Tex || b.trg == eBindTarget::TexSampled) {
            auto texture_id = GLuint(b.handle.tex->id());
            if (b.handle.view_index) {
                texture_id = GLuint(b.handle.tex->handle().views[b.handle.view_index - 1]);
            }
            ren_glBindTextureUnit_Comp(GLBindTarget(*b.handle.tex, b.handle.view_index), GLuint(b.loc + b.offset),
                                       texture_id);
            if (b.handle.sampler) {
                ren_glBindSampler(GLuint(b.loc + b.offset), b.handle.sampler->id());
            } else {
                ren_glBindSampler(GLuint(b.loc + b.offset), 0);
            }
        } else if (b.trg == eBindTarget::UBuf) {
            glBindBufferRange(GL_UNIFORM_BUFFER, b.loc, b.handle.buf->id(), b.offset,
                              b.size ? b.size : b.handle.buf->size());
        } else if (b.trg == eBindTarget::SBufRO || b.trg == eBindTarget::SBufRW) {
            glBindBufferRange(GL_SHADER_STORAGE_BUFFER, b.loc, b.handle.buf->id(), b.offset,
                              b.size ? b.size : b.handle.buf->size());
        } else if (b.trg == eBindTarget::UTBuf) {
            ren_glBindTextureUnit_Comp(GL_TEXTURE_BUFFER, GLuint(b.loc),
                                       GLuint(b.handle.buf->view(b.handle.view_index).second));
        } else if (b.trg == eBindTarget::STBufRO) {
            glBindImageTexture(GLuint(b.loc + b.offset), GLuint(b.handle.buf->view(b.handle.view_index).second), 0,
                               GL_FALSE, 0, GL_READ_ONLY,
                               GLInternalFormatFromTexFormat(b.handle.buf->view(b.handle.view_index).first));
        } else if (b.trg == eBindTarget::STBufRW) {
            glBindImageTexture(GLuint(b.loc + b.offset), GLuint(b.handle.buf->view(b.handle.view_index).second), 0,
                               GL_FALSE, 0, GL_READ_WRITE,
                               GLInternalFormatFromTexFormat(b.handle.buf->view(b.handle.view_index).first));
        } else if (b.trg == eBindTarget::Sampler) {
            ren_glBindSampler(GLuint(b.loc + b.offset), b.handle.sampler->id());
        } else if (b.trg == eBindTarget::ImageRO || b.trg == eBindTarget::ImageRW) {
            auto texture_id = GLuint(b.handle.tex->id());
            if (b.handle.view_index) {
                texture_id = GLuint(b.handle.tex->handle().views[b.handle.view_index - 1]);
            }
            const bool layered = Bitmask<eTexFlags>(b.handle.tex->params.flags) & eTexFlags::Array;
            glBindImageTexture(GLuint(b.loc + b.offset), texture_id, 0, layered ? GL_TRUE : GL_FALSE, 0,
                               b.trg == eBindTarget::ImageRO ? GL_READ_ONLY : GL_READ_WRITE,
                               GLInternalFormatFromTexFormat(b.handle.tex->params.format));
        }
    }

    Buffer temp_unif_buffer, temp_stage_buffer;
    if (uniform_data && uniform_data_len) {
        temp_unif_buffer = Buffer("Temp uniform buf", nullptr, eBufType::Uniform, uniform_data_len);
        temp_stage_buffer = Buffer("Temp upload buf", nullptr, eBufType::Upload, uniform_data_len);
        {
            uint8_t *stage_data = temp_stage_buffer.Map();
            memcpy(stage_data, uniform_data, uniform_data_len);
            temp_stage_buffer.Unmap();
        }
        CopyBufferToBuffer(temp_stage_buffer, 0, temp_unif_buffer, 0, uniform_data_len, nullptr);
    }
    glBindBufferBase(GL_UNIFORM_BUFFER, GLuint(g_param_buf_binding), temp_unif_buffer.id());

    glUseProgram(comp_pipeline.prog()->id());

    glDispatchCompute(grp_count[0], grp_count[1], grp_count[2]);
}

void Ren::DispatchCompute(const Pipeline &comp_pipeline, Vec3u grp_count, Span<const Binding> bindings,
                          const void *uniform_data, int uniform_data_len, DescrMultiPoolAlloc *descr_alloc, ILog *log) {
    DispatchCompute({}, comp_pipeline, grp_count, bindings, uniform_data, uniform_data_len, descr_alloc, log);
}

void Ren::DispatchComputeIndirect(CommandBuffer cmd_buf, const Pipeline &comp_pipeline, const Buffer &indir_buf,
                                  const uint32_t indir_buf_offset, Span<const Binding> bindings,
                                  const void *uniform_data, int uniform_data_len, DescrMultiPoolAlloc *descr_alloc,
                                  ILog *log) {
    for (const auto &b : bindings) {
        if (b.trg == eBindTarget::Tex || b.trg == eBindTarget::TexSampled) {
            auto texture_id = GLuint(b.handle.tex->id());
            if (b.handle.view_index) {
                texture_id = GLuint(b.handle.tex->handle().views[b.handle.view_index - 1]);
            }
            ren_glBindTextureUnit_Comp(GLBindTarget(*b.handle.tex, b.handle.view_index), GLuint(b.loc + b.offset),
                                       texture_id);
            if (b.handle.sampler) {
                ren_glBindSampler(GLuint(b.loc + b.offset), b.handle.sampler->id());
            } else {
                ren_glBindSampler(GLuint(b.loc + b.offset), 0);
            }
        } else if (b.trg == eBindTarget::UBuf) {
            glBindBufferRange(GL_UNIFORM_BUFFER, b.loc, b.handle.buf->id(), b.offset,
                              b.size ? b.size : b.handle.buf->size());
        } else if (b.trg == eBindTarget::SBufRO || b.trg == eBindTarget::SBufRW) {
            glBindBufferRange(GL_SHADER_STORAGE_BUFFER, b.loc, b.handle.buf->id(), b.offset,
                              b.size ? b.size : b.handle.buf->size());
        } else if (b.trg == eBindTarget::UTBuf) {
            ren_glBindTextureUnit_Comp(GL_TEXTURE_BUFFER, GLuint(b.loc),
                                       GLuint(b.handle.buf->view(b.handle.view_index).second));
        } else if (b.trg == eBindTarget::STBufRO) {
            glBindImageTexture(GLuint(b.loc + b.offset), GLuint(b.handle.buf->view(b.handle.view_index).second), 0,
                               GL_FALSE, 0, GL_READ_ONLY,
                               GLInternalFormatFromTexFormat(b.handle.buf->view(b.handle.view_index).first));
        } else if (b.trg == eBindTarget::STBufRW) {
            glBindImageTexture(GLuint(b.loc + b.offset), GLuint(b.handle.buf->view(b.handle.view_index).second), 0,
                               GL_FALSE, 0, GL_READ_WRITE,
                               GLInternalFormatFromTexFormat(b.handle.buf->view(b.handle.view_index).first));
        } else if (b.trg == eBindTarget::Sampler) {
            ren_glBindSampler(GLuint(b.loc + b.offset), b.handle.sampler->id());
        } else if (b.trg == eBindTarget::ImageRO || b.trg == eBindTarget::ImageRW) {
            auto texture_id = GLuint(b.handle.tex->id());
            if (b.handle.view_index) {
                texture_id = GLuint(b.handle.tex->handle().views[b.handle.view_index - 1]);
            }
            const bool layered = Bitmask<eTexFlags>(b.handle.tex->params.flags) & eTexFlags::Array;
            glBindImageTexture(GLuint(b.loc + b.offset), texture_id, 0, layered ? GL_TRUE : GL_FALSE, 0,
                               b.trg == eBindTarget::ImageRO ? GL_READ_ONLY : GL_READ_WRITE,
                               GLInternalFormatFromTexFormat(b.handle.tex->params.format));
        }
    }

    Buffer temp_unif_buffer;
    if (uniform_data) {
        temp_unif_buffer = Buffer("Temp uniform buf", nullptr, eBufType::Uniform, uniform_data_len);
        Buffer temp_stage_buffer = Buffer("Temp upload buf", nullptr, eBufType::Upload, uniform_data_len);
        {
            uint8_t *stage_data = temp_stage_buffer.Map();
            memcpy(stage_data, uniform_data, uniform_data_len);
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

void Ren::DispatchComputeIndirect(const Pipeline &comp_pipeline, const Buffer &indir_buf,
                                  const uint32_t indir_buf_offset, Span<const Binding> bindings,
                                  const void *uniform_data, int uniform_data_len, DescrMultiPoolAlloc *descr_alloc,
                                  ILog *log) {
    DispatchComputeIndirect({}, comp_pipeline, indir_buf, indir_buf_offset, bindings, uniform_data, uniform_data_len,
                            descr_alloc, log);
}
