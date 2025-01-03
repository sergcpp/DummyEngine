#include "DrawCall.h"

#include <cassert>

#include "Buffer.h"
#include "GL.h"
#include "Pipeline.h"
#include "ProbeStorage.h"
#include "Sampler.h"
#include "Texture.h"
#include "TextureArray.h"

namespace Ren {
const uint32_t gl_binding_targets[] = {
    GL_TEXTURE_2D,             // Tex2D
    GL_TEXTURE_2D,             // Tex2DSampled
    GL_TEXTURE_2D_ARRAY,       // Tex2DArraySampled
    GL_TEXTURE_2D_MULTISAMPLE, // Tex2DMs
    GL_TEXTURE_CUBE_MAP_ARRAY, // TexCubeArray
    GL_TEXTURE_3D,             // Tex3DSampled
    0xffffffff,                // Sampler
    GL_UNIFORM_BUFFER,         // UBuf
    GL_TEXTURE_BUFFER,         // UTBuf
    GL_SHADER_STORAGE_BUFFER,  // SBufRO
    GL_SHADER_STORAGE_BUFFER,  // SBufRW
    0xffffffff,                // STBufRO
    0xffffffff,                // STBufRW
    0xffffffff,                // Image2D
    0xffffffff,                // Image2DArray
    0xffffffff                 // AccStruct
};
static_assert(std::size(gl_binding_targets) == size_t(eBindTarget::_Count), "!");

extern const uint32_t g_gl_internal_formats[];

int g_param_buf_binding;
} // namespace Ren

uint32_t Ren::GLBindTarget(const eBindTarget binding) { return gl_binding_targets[size_t(binding)]; }

void Ren::DispatchCompute(CommandBuffer, const Pipeline &comp_pipeline, Vec3u grp_count, Span<const Binding> bindings,
                          const void *uniform_data, int uniform_data_len, DescrMultiPoolAlloc *descr_alloc, ILog *log) {
    for (const auto &b : bindings) {
        if (b.trg == eBindTarget::Tex2D || b.trg == eBindTarget::Tex2DSampled) {
            auto texture_id = GLuint(b.handle.tex->id());
            if (b.handle.view_index) {
                texture_id = GLuint(b.handle.tex->handle().views[b.handle.view_index - 1]);
            }
            ren_glBindTextureUnit_Comp(GLBindTarget(b.trg), GLuint(b.loc + b.offset), texture_id);
            if (b.handle.sampler) {
                ren_glBindSampler(GLuint(b.loc + b.offset), b.handle.sampler->id());
            } else {
                ren_glBindSampler(GLuint(b.loc + b.offset), 0);
            }
        } else if (b.trg == eBindTarget::Tex2DArraySampled) {
            ren_glBindTextureUnit_Comp(GLBindTarget(b.trg), GLuint(b.loc + b.offset), GLuint(b.handle.tex2d_arr->id()));
            if (b.handle.sampler) {
                ren_glBindSampler(GLuint(b.loc + b.offset), b.handle.sampler->id());
            } else {
                ren_glBindSampler(GLuint(b.loc + b.offset), 0);
            }
        } else if (b.trg == eBindTarget::Tex3DSampled) {
            ren_glBindTextureUnit_Comp(GLBindTarget(b.trg), GLuint(b.loc + b.offset), GLuint(b.handle.tex3d->id()));
            if (b.handle.sampler) {
                ren_glBindSampler(GLuint(b.loc + b.offset), b.handle.sampler->id());
            } else {
                ren_glBindSampler(GLuint(b.loc + b.offset), 0);
            }
        } else if (b.trg == eBindTarget::UBuf || b.trg == eBindTarget::SBufRO || b.trg == eBindTarget::SBufRW) {
            if (b.offset) {
                assert(b.size != 0);
                glBindBufferRange(GLBindTarget(b.trg), b.loc, b.handle.buf->id(), b.offset, b.size);
            } else {
                glBindBufferBase(GLBindTarget(b.trg), b.loc, b.handle.buf->id());
            }
        } else if (b.trg == eBindTarget::UTBuf) {
            ren_glBindTextureUnit_Comp(GLBindTarget(b.trg), GLuint(b.loc), GLuint(b.handle.tex_buf->id()));
        } else if (b.trg == eBindTarget::STBufRO) {
            glBindImageTexture(GLuint(b.loc + b.offset), GLuint(b.handle.tex_buf->id()), 0, GL_FALSE, 0, GL_READ_ONLY,
                               GLInternalFormatFromTexFormat(b.handle.tex_buf->params().format, false));
        } else if (b.trg == eBindTarget::STBufRW) {
            glBindImageTexture(GLuint(b.loc + b.offset), GLuint(b.handle.tex_buf->id()), 0, GL_FALSE, 0, GL_READ_WRITE,
                               GLInternalFormatFromTexFormat(b.handle.tex_buf->params().format, false));
        } else if (b.trg == eBindTarget::TexCubeArray) {
            ren_glBindTextureUnit_Comp(GLBindTarget(b.trg), GLuint(b.loc), GLuint(b.handle.cube_arr->handle().id));
        } else if (b.trg == eBindTarget::Sampler) {
            ren_glBindSampler(GLuint(b.loc + b.offset), b.handle.sampler->id());
        } else if (b.trg == eBindTarget::Image2D) {
            auto texture_id = GLuint(b.handle.tex->id());
            if (b.handle.view_index) {
                texture_id = GLuint(b.handle.tex->handle().views[b.handle.view_index - 1]);
            }
            glBindImageTexture(GLuint(b.loc + b.offset), texture_id, 0, GL_FALSE, 0, GL_READ_WRITE,
                               GLInternalFormatFromTexFormat(b.handle.tex->params.format, false));
        } else if (b.trg == eBindTarget::Image2DArray) {
            glBindImageTexture(GLuint(b.loc), GLuint(b.handle.tex2d_arr->id()), 0, GL_TRUE, 0, GL_READ_WRITE,
                               GLInternalFormatFromTexFormat(b.handle.tex2d_arr->format(), false));
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
        if (b.trg == eBindTarget::Tex2D || b.trg == eBindTarget::Tex2DSampled) {
            ren_glBindTextureUnit_Comp(GLBindTarget(b.trg), GLuint(b.loc + b.offset), GLuint(b.handle.tex->id()));
            if (b.handle.sampler) {
                ren_glBindSampler(GLuint(b.loc + b.offset), b.handle.sampler->id());
            }
        } else if (b.trg == eBindTarget::Tex2DArraySampled) {
            ren_glBindTextureUnit_Comp(GLBindTarget(b.trg), GLuint(b.loc + b.offset), GLuint(b.handle.tex2d_arr->id()));
            if (b.handle.sampler) {
                ren_glBindSampler(GLuint(b.loc + b.offset), b.handle.sampler->id());
            }
        } else if (b.trg == eBindTarget::Tex3DSampled) {
            ren_glBindTextureUnit_Comp(GLBindTarget(b.trg), GLuint(b.loc + b.offset), GLuint(b.handle.tex3d->id()));
        } else if (b.trg == eBindTarget::UBuf || b.trg == eBindTarget::SBufRO || b.trg == eBindTarget::SBufRW) {
            if (b.offset) {
                assert(b.size != 0);
                glBindBufferRange(GLBindTarget(b.trg), b.loc, b.handle.buf->id(), b.offset, b.size);
            } else {
                glBindBufferBase(GLBindTarget(b.trg), b.loc, b.handle.buf->id());
            }
        } else if (b.trg == eBindTarget::UTBuf) {
            ren_glBindTextureUnit_Comp(GLBindTarget(b.trg), GLuint(b.loc), GLuint(b.handle.tex_buf->id()));
        } else if (b.trg == eBindTarget::TexCubeArray) {
            ren_glBindTextureUnit_Comp(GLBindTarget(b.trg), GLuint(b.loc), GLuint(b.handle.cube_arr->handle().id));
        } else if (b.trg == eBindTarget::Sampler) {
            ren_glBindSampler(GLuint(b.loc + b.offset), b.handle.sampler->id());
        } else if (b.trg == eBindTarget::Image2D) {
            glBindImageTexture(GLuint(b.loc + b.offset), GLuint(b.handle.tex_buf->id()), 0, GL_FALSE, 0, GL_READ_WRITE,
                               GLInternalFormatFromTexFormat(b.handle.tex->params.format, false));
        } else if (b.trg == eBindTarget::Image2DArray) {
            glBindImageTexture(GLuint(b.loc + b.offset), GLuint(b.handle.tex2d_arr->id()), 0, GL_TRUE, 0, GL_READ_WRITE,
                               GLInternalFormatFromTexFormat(b.handle.tex2d_arr->format(), false));
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
