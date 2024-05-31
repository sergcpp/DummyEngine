#include "RpReadExposure.h"

#include <Ren/Context.h>

#include "../../utils/ShaderLoader.h"

void Eng::RpReadExposure::Execute(RpBuilder &builder) {
    RpAllocTex &input_tex = builder.GetReadTexture(pass_data_->input_tex);
    RpAllocBuf &output_buf = builder.GetWriteBuffer(pass_data_->output_buf);

    auto &ctx = builder.ctx();

    { // Retrieve result of readback from previous frame
        const auto *mapped_ptr = (const float *)output_buf.ref->Map();
        if (mapped_ptr) {
            exposure_ = mapped_ptr[ctx.backend_frame()];
            output_buf.ref->Unmap();
        }
    }

    input_tex.ref->CopyTextureData(*output_buf.ref, ctx.current_cmd_buf(), sizeof(float) * ctx.backend_frame());
}
