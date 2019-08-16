#include "RpReadBrightness.h"

#include <Ren/Context.h>

#include "../../Utils/ShaderLoader.h"

void RpReadBrightness::Setup(RpBuilder &builder, const char intput_tex_name[], Ren::WeakBufferRef output_buf) {
    input_tex_ = builder.ReadTexture(intput_tex_name, Ren::eResState::CopySrc, Ren::eStageBits::Transfer, *this);

    output_buf_ = builder.WriteBuffer(output_buf, Ren::eResState::CopyDst, Ren::eStageBits::Transfer, *this);
}

void RpReadBrightness::Execute(RpBuilder &builder) {
    using namespace RpSampleBrightnessInternal;

    RpAllocTex &input_tex = builder.GetReadTexture(input_tex_);
    RpAllocBuf &output_buf = builder.GetWriteBuffer(output_buf_);

    auto &ctx = builder.ctx();

    float lum = 0.0f;
    const uint32_t read_size = input_tex.desc.w * input_tex.desc.h * sizeof(float);

    { // Retrieve result of glReadPixels call from previous frame
        auto *reduced_pixels =
            (float *)output_buf.ref->MapRange(Ren::BufMapRead, read_size * ctx.backend_frame(), read_size);
        if (reduced_pixels) {
            for (int i = 0; i < input_tex.desc.w * input_tex.desc.h; ++i) {
                if (!std::isnan(reduced_pixels[i])) {
                    lum += std::min(reduced_pixels[i], MaxValue);
                }
            }
            output_buf.ref->Unmap();
        }
    }

    lum /= float(input_tex.desc.w * input_tex.desc.h);
    reduced_average_ = AvgAlpha * lum + (1.0f - AvgAlpha) * reduced_average_;

    input_tex.ref->CopyTextureData(*output_buf.ref, ctx.current_cmd_buf(), read_size * ctx.backend_frame());

    /*{ // Start asynchronous memory read from framebuffer
        glBindFramebuffer(GL_FRAMEBUFFER, reduced_fb_.id());
        glReadBuffer(GL_COLOR_ATTACHMENT0);

        glBindBuffer(GL_PIXEL_PACK_BUFFER, GLuint(readback_buf_->id()));

        glReadPixels(0, 0, res_[0], res_[1], GL_RGBA, GL_FLOAT,
                     reinterpret_cast<GLvoid *>(uintptr_t(read_size * ctx.backend_frame())));

        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }*/

    /*{ // Start asynchronous memory read from framebuffer
        //glBindFramebuffer(GL_FRAMEBUFFER, reduced_fb_.id());
        //glReadBuffer(GL_COLOR_ATTACHMENT0);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, GLuint(input_tex.ref->id()));

        glBindBuffer(GL_PIXEL_PACK_BUFFER, GLuint(readback_buf_->id()));

        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT,
                      reinterpret_cast<GLvoid *>(uintptr_t(read_size * ctx.backend_frame())));

        //glReadPixels(0, 0, res_[0], res_[1], GL_RGBA, GL_FLOAT,
        //             reinterpret_cast<GLvoid *>(uintptr_t(read_size * ctx.backend_frame())));

        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        //glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
    }*/
}
