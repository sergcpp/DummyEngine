#include "RpSSRTraceHQ.h"

#include <Ren/Context.h>
#include <Ren/Program.h>
#include <Ren/RastState.h>

#include "../../Scene/ProbeStorage.h"
#include "../../Utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"

void RpSSRTraceHQ::Setup(RpBuilder &builder, const ViewState *view_state, const char shared_data_buf[],
                         const char normal_tex[], const char depth_down_2x[], const char output_tex_name[]) {
    view_state_ = view_state;

    shared_data_buf_ =
        builder.ReadBuffer(shared_data_buf, Ren::eResState::UniformBuffer, Ren::eStageBits::ComputeShader, *this);
    normal_tex_ =
        builder.ReadTexture(normal_tex, Ren::eResState::ShaderResource, Ren::eStageBits::ComputeShader, *this);
    depth_down_2x_tex_ =
        builder.ReadTexture(depth_down_2x, Ren::eResState::ShaderResource, Ren::eStageBits::ComputeShader, *this);

    { // Auxilary texture for reflections (rg - uvs, b - influence)
        Ren::Tex2DParams params;
        params.w = view_state->scr_res[0];
        params.h = view_state->scr_res[1];
        params.format = Ren::eTexFormat::RawRGB10_A2;
        params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
        params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

        output_tex_ = builder.WriteTexture(output_tex_name, params, Ren::eResState::UnorderedAccess,
                                           Ren::eStageBits::ComputeShader, *this);
    }
}

void RpSSRTraceHQ::LazyInit(Ren::Context &ctx, ShaderLoader &sh) {
    if (!initialized) {
        Ren::ProgramRef ssr_trace_hq_prog = sh.LoadProgram(ctx, "ssr_trace_hq", "internal/ssr_trace_hq.comp.glsl");
        assert(ssr_trace_hq_prog->ready());

        if (!pi_ssr_trace_hq_.Init(ctx.api_ctx(), std::move(ssr_trace_hq_prog), ctx.log())) {
            ctx.log()->Error("RpSSRTraceHQ: failed to initialize pipeline!");
        }

        initialized = true;
    }
}
