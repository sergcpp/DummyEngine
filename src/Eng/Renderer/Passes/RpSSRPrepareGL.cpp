#include "RpSSRPrepare.h"

#include <Ren/Buffer.h>
#include <Ren/Context.h>
#include <Ren/GL.h>

#include "../Renderer_Structs.h"

void RpSSRPrepare::Execute(RpBuilder &builder) {
    LazyInit(builder.ctx(), builder.sh());

    RpAllocBuf &ray_counter_buf = builder.GetWriteBuffer(ray_counter_buf_);
    RpAllocTex &raylen_tex = builder.GetWriteTexture(raylen_tex_);

    uint32_t zero = 0;

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ray_counter_buf.ref->id());
    glClearBufferSubData(GL_SHADER_STORAGE_BUFFER, GL_R32UI, 0, ray_counter_buf.ref->size(), GL_RED, GL_UNSIGNED_INT,
                         &zero);

    const float zero_col[3] = {0.0f, 0.0f, 0.0f};
    glClearTexImage(raylen_tex.ref->id(), 0, GL_RED, GL_FLOAT, zero_col);
}
