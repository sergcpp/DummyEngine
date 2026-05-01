#include "ExSkinning.h"

#include "../../utils/ShaderLoader.h"
#include "../Renderer_Structs.h"
#include "../framegraph/FgBuilder.h"

void Eng::ExSkinning::LazyInit(const FgContext &fg) {
    if (!pi_skinning_) {
        pi_skinning_ = fg.sh().FindOrCreatePipeline("internal/skinning.comp.glsl");
    }
}