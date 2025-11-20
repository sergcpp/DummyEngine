#include "ExReadExposure.h"

#include <Ren/Context.h>

#include "../../utils/ShaderLoader.h"

void Eng::ExReadExposure::Execute(FgContext &fg) {
    const Ren::Image &input_tex = fg.AccessROImage(args_->input_tex);
    Ren::Buffer &output_buf = fg.AccessRWBuffer(args_->output_buf);

    { // Retrieve result of readback from previous frame
        const auto *mapped_ptr = (const float *)output_buf.Map();
        if (mapped_ptr) {
            exposure_ = mapped_ptr[fg.backend_frame()];
            output_buf.Unmap();
        }
    }

    input_tex.CopyTextureData(output_buf, fg.cmd_buf(), sizeof(float) * fg.backend_frame(), sizeof(float));
}
