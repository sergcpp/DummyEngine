#include "RpCopyTex.h"

#include <Ren/Context.h>
#include <Ren/Texture.h>

#include "../../Utils/ShaderLoader.h"
#include "../Renderer_Structs.h"

void RpCopyTex::Setup(RpBuilder &builder, const Ren::Vec2i copy_res, const char input_tex_name[],
                      Ren::WeakTex2DRef history_tex) {
    copy_res_ = copy_res;

    input_tex_ = builder.ReadTexture(input_tex_name, Ren::eResState::CopySrc, Ren::eStageBits::Transfer, *this);
    output_tex_ = builder.WriteTexture(history_tex, Ren::eResState::CopyDst, Ren::eStageBits::Transfer, *this);
}

void RpCopyTex::Execute(RpBuilder &builder) {
    RpAllocTex &input_tex = builder.GetReadTexture(input_tex_);
    RpAllocTex &output_tex = builder.GetWriteTexture(output_tex_);

    Ren::CopyImageToImage(builder.ctx().current_cmd_buf(), *input_tex.ref, 0, 0, 0, *output_tex.ref, 0, 0, 0,
                          copy_res_[0], copy_res_[1]);
}
