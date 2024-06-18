#include "RpRTShadows.h"

#include <Ren/Context.h>
#include <Ren/RastState.h>
#include <Ren/Texture.h>

#include "../../utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"

void Eng::RpRTShadows::Execute(RpBuilder &builder) {
    LazyInit(builder.ctx(), builder.sh());

    if (builder.ctx().capabilities.hwrt) {
        Execute_HWRT_Inline(builder);
        // Execute_HWRT_Pipeline(builder);
    } else {
        Execute_SWRT(builder);
    }
}
