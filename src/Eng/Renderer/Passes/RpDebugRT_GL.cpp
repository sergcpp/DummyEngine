#include "RpDebugRT.h"

#include <Ren/Context.h>
#include <Ren/RastState.h>
#include <Ren/Texture.h>

#include "../../Utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"

#include "../assets/shaders/internal/rt_debug_interface.glsl"

void RpDebugRT::Execute(RpBuilder &builder) {
    LazyInit(builder.ctx(), builder.sh());

    RpAllocBuf &geo_data_buf = builder.GetReadBuffer(geo_data_buf_);
    RpAllocBuf &materials_buf = builder.GetReadBuffer(materials_buf_);
    RpAllocBuf &vtx_buf1 = builder.GetReadBuffer(vtx_buf1_);
    RpAllocBuf &vtx_buf2 = builder.GetReadBuffer(vtx_buf2_);
    RpAllocBuf &ndx_buf = builder.GetReadBuffer(ndx_buf_);
    RpAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(shared_data_buf_);
    RpAllocTex &env_tex = builder.GetReadTexture(env_tex_);
    RpAllocTex &dummy_black = builder.GetReadTexture(dummy_black_);
    RpAllocTex *lm_tex[5];
    for (int i = 0; i < 5; ++i) {
        if (lm_tex_[i]) {
            lm_tex[i] = &builder.GetReadTexture(lm_tex_[i]);
        } else {
            lm_tex[i] = &dummy_black;
        }
    }

    RpAllocTex *output_tex = nullptr;
    if (output_tex_) {
        output_tex = &builder.GetWriteTexture(output_tex_);
    }

    // TODO: software fallback for raytracing
}

void RpDebugRT::LazyInit(Ren::Context &ctx, ShaderLoader &sh) {
    if (!initialized) {
        
        initialized = true;
    }
}