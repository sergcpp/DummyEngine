#include "RpDebugRT.h"

#include <Ren/Context.h>
#include <Ren/RastState.h>
#include <Ren/Texture.h>

#include "../../Utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"

void RpDebugRT::Execute(RpBuilder &builder) {
    if (builder.ctx().capabilities.raytracing) {
        Execute_HWRT(builder);
    } else {
        Execute_SWRT(builder);
    }
}

void RpDebugRT::Execute_SWRT(RpBuilder &builder) { LazyInit(builder.ctx(), builder.sh()); }