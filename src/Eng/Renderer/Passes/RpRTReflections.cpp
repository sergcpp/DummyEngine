#include "RpRTReflections.h"

#include <Ren/Context.h>
#include <Ren/RastState.h>
#include <Ren/Texture.h>

#include "../../Utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"

void RpRTReflections::Execute(RpBuilder &builder) {
    LazyInit(builder.ctx(), builder.sh());

    if (builder.ctx().capabilities.raytracing) {
        if (builder.ctx().capabilities.ray_query) {
            ExecuteHWRTInline(builder);
        } else {
            ExecuteHWRTPipeline(builder);
        }
    } else {
        ExecuteSWRT(builder);
    }
}
