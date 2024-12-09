#include "ExSkinning.h"

#include "../../utils/ShaderLoader.h"
#include "../Renderer_Structs.h"

void Eng::ExSkinning::LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh) {
    if (!pi_skinning_) {
        pi_skinning_ = sh.LoadPipeline("internal/skinning.comp.glsl");
    }
}