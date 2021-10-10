#include "RpSSRPrepare.h"

#include <Ren/Buffer.h>
#include <Ren/Context.h>
#include <Ren/GL.h>

#include "../Renderer_Structs.h"

void RpSSRPrepare::Execute(RpBuilder &builder) {
    LazyInit(builder.ctx(), builder.sh());

    RpAllocBuf &temp_variance_mask_buf = builder.GetWriteBuffer(temp_variance_mask_buf_);
    RpAllocBuf &ray_counter_buf = builder.GetWriteBuffer(ray_counter_buf_);
    RpAllocTex &denoised_refl_tex = builder.GetWriteTexture(denoised_refl_tex_);

    uint32_t zero = 0;

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, temp_variance_mask_buf.ref->id());
    glClearBufferSubData(GL_SHADER_STORAGE_BUFFER, GL_R32UI, 0, temp_variance_mask_buf.ref->size(), GL_RED,
                         GL_UNSIGNED_INT, &zero);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ray_counter_buf.ref->id());
    glClearBufferSubData(GL_SHADER_STORAGE_BUFFER, GL_R32UI, 0, ray_counter_buf.ref->size(), GL_RED, GL_UNSIGNED_INT,
                         &zero);

    const float zero_col[3] = {0.0f, 0.0f, 0.0f};
    glClearTexImage(denoised_refl_tex.ref->id(), 0, GL_RGB, GL_FLOAT, zero_col);
}
