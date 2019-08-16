#include "RpTAACopyTex.h"

#include <Ren/Context.h>
#include <Ren/Texture.h>

#include "../../Utils/ShaderLoader.h"
#include "../Renderer_Structs.h"

void RpTAACopyTex::Setup(RpBuilder &builder, const ViewState *view_state, const char input_tex_name[],
                         Ren::WeakTex2DRef history_tex) {
    view_state_ = view_state;

    input_tex_ = builder.ReadTexture(input_tex_name, Ren::eResState::CopySrc, Ren::eStageBits::Transfer, *this);
    output_tex_ = builder.WriteTexture(history_tex, Ren::eResState::CopyDst, Ren::eStageBits::Transfer, *this);
}

void RpTAACopyTex::Execute(RpBuilder &builder) {
    RpAllocTex &input_tex = builder.GetReadTexture(input_tex_);
    RpAllocTex &output_tex = builder.GetWriteTexture(output_tex_);

    Ren::CopyImageToImage(builder.ctx().current_cmd_buf(), *input_tex.ref, 0, 0, 0, *output_tex.ref, 0, 0, 0,
                          uint32_t(view_state_->act_res[0]), uint32_t(view_state_->act_res[1]));

    //Ren::TransitionInfo info(output_tex.ref.get(), Ren::eResState::ShaderResource);
    //Ren::TransitionResourceStates(builder.ctx().current_cmd_buf(), Ren::AllStages, Ren::AllStages, &info, 1);
}
