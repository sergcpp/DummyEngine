#include "RpSampleBrightness.h"

#include <Ren/Context.h>

#include "../../Renderer/PrimDraw.h"
#include "../../Utils/ShaderLoader.h"

#include "../assets/shaders/internal/blit_reduced_interface.h"

namespace RpSampleBrightnessInternal {
const Ren::Vec2f poisson_disk[] = {
    Ren::Vec2f{-0.705374f, -0.668203f}, Ren::Vec2f{-0.780145f, 0.486251f},  Ren::Vec2f{0.566637f, 0.605213f},
    Ren::Vec2f{0.488876f, -0.783441f},  Ren::Vec2f{-0.613392f, 0.617481f},  Ren::Vec2f{0.170019f, -0.040254f},
    Ren::Vec2f{-0.299417f, 0.791925f},  Ren::Vec2f{0.645680f, 0.493210f},   Ren::Vec2f{-0.651784f, 0.717887f},
    Ren::Vec2f{0.421003f, 0.027070f},   Ren::Vec2f{-0.817194f, -0.271096f}, Ren::Vec2f{0.977050f, -0.108615f},
    Ren::Vec2f{0.063326f, 0.142369f},   Ren::Vec2f{0.203528f, 0.214331f},   Ren::Vec2f{-0.667531f, 0.326090f},
    Ren::Vec2f{-0.098422f, -0.295755f}, Ren::Vec2f{-0.885922f, 0.215369f},  Ren::Vec2f{0.039766f, -0.396100f},
    Ren::Vec2f{0.751946f, 0.453352f},   Ren::Vec2f{0.078707f, -0.715323f},  Ren::Vec2f{-0.075838f, -0.529344f},
    Ren::Vec2f{0.724479f, -0.580798f},  Ren::Vec2f{0.222999f, -0.215125f},  Ren::Vec2f{-0.467574f, -0.405438f},
    Ren::Vec2f{-0.248268f, -0.814753f}, Ren::Vec2f{0.354411f, -0.887570f},  Ren::Vec2f{0.175817f, 0.382366f},
    Ren::Vec2f{0.487472f, -0.063082f},  Ren::Vec2f{-0.084078f, 0.898312f},  Ren::Vec2f{0.470016f, 0.217933f},
    Ren::Vec2f{-0.696890f, -0.549791f}, Ren::Vec2f{-0.149693f, 0.605762f},  Ren::Vec2f{0.034211f, 0.979980f},
    Ren::Vec2f{0.503098f, -0.308878f},  Ren::Vec2f{-0.016205f, -0.872921f}, Ren::Vec2f{0.385784f, -0.393902f},
    Ren::Vec2f{-0.146886f, -0.859249f}, Ren::Vec2f{0.643361f, 0.164098f},   Ren::Vec2f{0.634388f, -0.049471f},
    Ren::Vec2f{-0.688894f, 0.007843f},  Ren::Vec2f{0.464034f, -0.188818f},  Ren::Vec2f{-0.440840f, 0.137486f},
    Ren::Vec2f{0.364483f, 0.511704f},   Ren::Vec2f{0.034028f, 0.325968f},   Ren::Vec2f{0.099094f, -0.308023f},
    Ren::Vec2f{0.693960f, -0.366253f},  Ren::Vec2f{0.678884f, -0.204688f},  Ren::Vec2f{0.001801f, 0.780328f},
    Ren::Vec2f{0.145177f, -0.898984f},  Ren::Vec2f{0.062655f, -0.611866f},  Ren::Vec2f{0.315226f, -0.604297f},
    Ren::Vec2f{-0.371868f, 0.882138f},  Ren::Vec2f{0.200476f, 0.494430f},   Ren::Vec2f{-0.494552f, -0.711051f},
    Ren::Vec2f{0.612476f, 0.705252f},   Ren::Vec2f{-0.578845f, -0.768792f}, Ren::Vec2f{-0.772454f, -0.090976f},
    Ren::Vec2f{0.504440f, 0.372295f},   Ren::Vec2f{0.155736f, 0.065157f},   Ren::Vec2f{0.391522f, 0.849605f},
    Ren::Vec2f{-0.620106f, -0.328104f}, Ren::Vec2f{0.789239f, -0.419965f},  Ren::Vec2f{-0.545396f, 0.538133f},
    Ren::Vec2f{-0.178564f, -0.596057f}};
} // namespace RpSampleBrightnessInternal

void RpSampleBrightness::Execute(RpBuilder &builder) {
    using namespace RpSampleBrightnessInternal;

    RpAllocTex &input_tex = builder.GetReadTexture(pass_data_->input_tex);
    RpAllocTex &reduced_tex = builder.GetWriteTexture(pass_data_->reduced_tex);

    Ren::Context &ctx = builder.ctx();
    LazyInit(ctx, builder.sh(), reduced_tex);

    const auto offset_step = Ren::Vec2f{1.0f / float(res_[0]), 1.0f / float(res_[1])};

    Ren::RastState rast_state;
    rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

    rast_state.viewport[2] = res_[0];
    rast_state.viewport[3] = res_[1];

    { // Sample texture
        const Ren::Binding bindings[] = {{Ren::eBindTarget::Tex2D, Reduced::SRC_TEX_SLOT, *input_tex.ref}};

        const Ren::RenderTarget render_targets[] = {{reduced_tex.ref, Ren::eLoadOp::DontCare, Ren::eStoreOp::Store}};

        Reduced::Params uniform_params;
        uniform_params.transform = Ren::Vec4f{0.0f, 0.0f, 1.0f, 1.0f};
        uniform_params.offset[0] = 0.5f * poisson_disk[cur_offset_][0] * offset_step[0];
        uniform_params.offset[1] = 0.5f * poisson_disk[cur_offset_][1] * offset_step[1];

        cur_offset_ = (cur_offset_ + 1) % 64;

        prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, blit_red_prog_, render_targets, {}, rast_state, builder.rast_state(),
                            bindings, &uniform_params, sizeof(Reduced::Params), 0);
    }
}

void RpSampleBrightness::LazyInit(Ren::Context &ctx, ShaderLoader &sh, RpAllocTex &reduced_tex) {
    if (!initialized_) {
        blit_red_prog_ =
            sh.LoadProgram(ctx, "blit_red2", "internal/blit_reduced.vert.glsl", "internal/blit_reduced.frag.glsl");
        assert(blit_red_prog_->ready());

        readback_buf_ = ctx.LoadBuffer("Brightness Readback", Ren::eBufType::Stage,
                                       4 * res_[0] * res_[1] * sizeof(float) * Ren::MaxFramesInFlight);

        initialized_ = true;
    }
}