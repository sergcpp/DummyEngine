#include "ExReadExposure.h"

#include <Ren/Context.h>

#include "../../utils/ShaderLoader.h"

void Eng::ExReadExposure::Execute(FgContext &fg) {
    FgAllocTex &input_tex = fg.AccessROTexture(args_->input_tex);
    FgAllocBuf &output_buf = fg.AccessRWBuffer(args_->output_buf);

    { // Retrieve result of readback from previous frame
        const auto *mapped_ptr = (const float *)output_buf.ref->Map();
        if (mapped_ptr) {
            exposure_ = mapped_ptr[fg.backend_frame()];
            output_buf.ref->Unmap();
        }
    }

    input_tex.ref->CopyTextureData(*output_buf.ref, fg.cmd_buf(), sizeof(float) * fg.backend_frame(), sizeof(float));
}
