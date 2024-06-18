#include "RpRTGI.h"

#include <Ren/Context.h>
#include <Ren/RastState.h>
#include <Ren/Texture.h>

#include "../Renderer_Structs.h"

void Eng::RpRTGI::Execute(RpBuilder &builder) {
    LazyInit(builder.ctx(), builder.sh());

    if (builder.ctx().capabilities.hwrt) {
        Execute_HWRT_Inline(builder);
        // Execute_HWRT_Pipeline(builder);
    } else {
        Execute_SWRT(builder);
    }
}
