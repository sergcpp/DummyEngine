#include "ExReadExposure.h"

#include <Ren/Context.h>

#include "../../utils/ShaderLoader.h"

void Eng::ExReadExposure::Execute(FgBuilder &builder) {
    FgAllocTex &input_tex = builder.GetReadTexture(args_->input_tex);
    FgAllocBuf &output_buf = builder.GetWriteBuffer(args_->output_buf);

    auto &ctx = builder.ctx();

    { // Retrieve result of readback from previous frame
        const auto *mapped_ptr = (const float *)output_buf.ref->Map();
        if (mapped_ptr) {
            exposure_ = mapped_ptr[ctx.backend_frame()];
            output_buf.ref->Unmap();
        }
    }

    input_tex.ref->CopyTextureData(*output_buf.ref, ctx.current_cmd_buf(), sizeof(float) * ctx.backend_frame(),
                                   sizeof(float));
}
