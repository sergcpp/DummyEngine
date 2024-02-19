#include "RpRTGI.h"

#include <Ren/Context.h>
#include <Ren/RastState.h>
#include <Ren/Texture.h>

#include "../../utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"

void Eng::RpRTGI::Execute(RpBuilder &builder) {
    if (builder.ctx().capabilities.ray_query) {
        Execute_HWRT_Inline(builder);
    } else {
        Execute_HWRT_Pipeline(builder);
    }
}