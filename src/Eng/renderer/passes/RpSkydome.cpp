#include "RpSkydome.h"

#include <Ren/Context.h>
#include <Ren/DrawCall.h>

#include "../../utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"
#include "../shaders/skydome_downsample_interface.h"
#include "../shaders/skydome_interface.h"

void Eng::RpSkydomeCube::Execute(RpBuilder &builder) {
    LazyInit(builder.ctx(), builder.sh());

    RpAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(pass_data_->shared_data);
    RpAllocTex &transmittance_lut = builder.GetReadTexture(pass_data_->transmittance_lut);
    RpAllocTex &multiscatter_lut = builder.GetReadTexture(pass_data_->multiscatter_lut);
    RpAllocTex &moon_tex = builder.GetReadTexture(pass_data_->moon_tex);
    RpAllocTex &weather_tex = builder.GetReadTexture(pass_data_->weather_tex);
    RpAllocTex &cirrus_tex = builder.GetReadTexture(pass_data_->cirrus_tex);
    RpAllocTex &noise3d_tex = builder.GetReadTexture(pass_data_->noise3d_tex);
    RpAllocTex &color_tex = builder.GetWriteTexture(pass_data_->color_tex);

    Ren::RastState rast_state;
    rast_state.poly.cull = uint8_t(Ren::eCullFace::Front);
    rast_state.viewport[2] = color_tex.ref->params.w;
    rast_state.viewport[3] = color_tex.ref->params.h;

    const Ren::Binding bindings[] = {
        {Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, *unif_sh_data_buf.ref},
        {Ren::eBindTarget::Tex2DSampled, Skydome::TRANSMITTANCE_LUT_SLOT, *transmittance_lut.ref},
        {Ren::eBindTarget::Tex2DSampled, Skydome::MULTISCATTER_LUT_SLOT, *multiscatter_lut.ref},
        {Ren::eBindTarget::Tex2DSampled, Skydome::MOON_TEX_SLOT, *moon_tex.ref},
        {Ren::eBindTarget::Tex2DSampled, Skydome::WEATHER_TEX_SLOT, *weather_tex.ref},
        {Ren::eBindTarget::Tex2DSampled, Skydome::CIRRUS_TEX_SLOT, *cirrus_tex.ref},
        {Ren::eBindTarget::Tex3DSampled, Skydome::NOISE3D_TEX_SLOT, *noise3d_tex.tex3d}};

    Ren::Mat4f scale_matrix;
    scale_matrix = Scale(scale_matrix, Ren::Vec3f{5000.0f, 5000.0f, 5000.0f});

    Skydome::Params uniform_params = {};
    uniform_params.xform = scale_matrix;

#if defined(USE_GL_RENDER)
    static const Ren::Vec3f axises[] = {Ren::Vec3f{1.0f, 0.0f, 0.0f}, Ren::Vec3f{-1.0f, 0.0f, 0.0f},
                                        Ren::Vec3f{0.0f, 1.0f, 0.0f}, Ren::Vec3f{0.0f, -1.0f, 0.0f},
                                        Ren::Vec3f{0.0f, 0.0f, 1.0f}, Ren::Vec3f{0.0f, 0.0f, -1.0f}};
    static const Ren::Vec3f ups[] = {Ren::Vec3f{0.0f, -1.0f, 0.0f}, Ren::Vec3f{0.0f, -1.0f, 0.0f},
                                     Ren::Vec3f{0.0f, 0.0f, 1.0f},  Ren::Vec3f{0.0f, 0.0f, -1.0f},
                                     Ren::Vec3f{0.0f, -1.0f, 0.0f}, Ren::Vec3f{0.0f, -1.0f, 0.0f}};
#else
    static const Ren::Vec3f axises[] = {Ren::Vec3f{1.0f, 0.0f, 0.0f},  Ren::Vec3f{-1.0f, 0.0f, 0.0f},
                                        Ren::Vec3f{0.0f, 1.0f, 0.0f},  Ren::Vec3f{0.0f, -1.0f, 0.0f},
                                        Ren::Vec3f{0.0f, 0.0f, -1.0f}, Ren::Vec3f{0.0f, 0.0f, 1.0f}};
    static const Ren::Vec3f ups[] = {Ren::Vec3f{0.0f, 1.0f, 0.0f}, Ren::Vec3f{0.0f, 1.0f, 0.0f},
                                     Ren::Vec3f{0.0f, 0.0f, 1.0f}, Ren::Vec3f{0.0f, 0.0f, -1.0f},
                                     Ren::Vec3f{0.0f, 1.0f, 0.0f}, Ren::Vec3f{0.0f, 1.0f, 0.0f}};
#endif

    Ren::Camera temp_cam;
    temp_cam.Perspective(Ren::eZRange::OneToZero, 90.0f, 1.0f, 1.0f, 10000.0f);
    for (int i = 0; i < 6; i++) {
        temp_cam.SetupView(Ren::Vec3f{0.0f}, axises[i], ups[i]);

        uniform_params.clip_from_world = temp_cam.proj_matrix() * temp_cam.view_matrix();

        const Ren::RenderTarget color_targets[] = {
            {color_tex.ref, uint8_t(i + 1), Ren::eLoadOp::DontCare, Ren::eStoreOp::Store}};
        prim_draw_.DrawPrim(PrimDraw::ePrim::Sphere, prog_skydome_phys_, color_targets, {}, rast_state,
                            builder.rast_state(), bindings, &uniform_params, sizeof(uniform_params), 0);
    }

    const int mip_count = color_tex.ref->params.mip_count;
    for (int face = 0; face < 6; ++face) {
        for (int mip = 1; mip < mip_count; mip += 4) {
            const Ren::TransitionInfo transitions[] = {{color_tex.ref.get(), Ren::eResState::UnorderedAccess}};
            Ren::TransitionResourceStates(builder.ctx().api_ctx(), builder.ctx().current_cmd_buf(), Ren::AllStages,
                                            Ren::AllStages, transitions);

            const Ren::Binding bindings[] = {{Ren::eBindTarget::Tex2DSampled,
                                                SkydomeDownsample::INPUT_TEX_SLOT,
                                                {*color_tex.ref, (mip - 1) * 6 + face + 1}},
                                                {Ren::eBindTarget::Image2D,
                                                SkydomeDownsample::OUTPUT_IMG_SLOT,
                                                0,
                                                1,
                                                {*color_tex.ref, mip * 6 + face + 1}},
                                                {Ren::eBindTarget::Image2D,
                                                SkydomeDownsample::OUTPUT_IMG_SLOT,
                                                1,
                                                1,
                                                {*color_tex.ref, std::min(mip + 1, mip_count - 1) * 6 + face + 1}},
                                                {Ren::eBindTarget::Image2D,
                                                SkydomeDownsample::OUTPUT_IMG_SLOT,
                                                2,
                                                1,
                                                {*color_tex.ref, std::min(mip + 2, mip_count - 1) * 6 + face + 1}},
                                                {Ren::eBindTarget::Image2D,
                                                SkydomeDownsample::OUTPUT_IMG_SLOT,
                                                3,
                                                1,
                                                {*color_tex.ref, std::min(mip + 3, mip_count - 1) * 6 + face + 1}}};

            SkydomeDownsample::Params uniform_params = {};
            uniform_params.img_size[0] = (color_tex.ref->params.w >> mip);
            uniform_params.img_size[1] = (color_tex.ref->params.h >> mip);
            uniform_params.mip_count = std::min(4, mip_count - mip);

            const Ren::Vec3u grp_count =
                Ren::Vec3u{(uniform_params.img_size[0] + SkydomeDownsample::LOCAL_GROUP_SIZE_X - 1) /
                                SkydomeDownsample::LOCAL_GROUP_SIZE_X,
                            (uniform_params.img_size[1] + SkydomeDownsample::LOCAL_GROUP_SIZE_Y - 1) /
                                SkydomeDownsample::LOCAL_GROUP_SIZE_Y,
                            1u};

            Ren::DispatchCompute(pi_skydome_downsample_, grp_count, bindings, &uniform_params,
                                    sizeof(uniform_params), builder.ctx().default_descr_alloc(), builder.ctx().log());
        }
    }

    const Ren::TransitionInfo transitions[] = {{color_tex.ref.get(), Ren::eResState::RenderTarget}};
    Ren::TransitionResourceStates(builder.ctx().api_ctx(), builder.ctx().current_cmd_buf(), Ren::AllStages,
                                  Ren::AllStages, transitions);
}

void Eng::RpSkydomeCube::LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh) {
    if (!initialized) {
        prog_skydome_phys_ = sh.LoadProgram(ctx, "internal/skydome_phys.vert.glsl", "internal/skydome_phys.frag.glsl");
        assert(prog_skydome_phys_->ready());

        Ren::ProgramRef prog = sh.LoadProgram(ctx, "internal/skydome_downsample.comp.glsl");
        assert(prog->ready());

        if (!pi_skydome_downsample_.Init(ctx.api_ctx(), std::move(prog), ctx.log())) {
            ctx.log()->Error("RpSkydomeCube: Failed to initialize pipeline!");
        }

        initialized = true;
    }
}

void Eng::RpSkydomeScreen::Execute(RpBuilder &builder) {
    LazyInit(builder.ctx(), builder.sh());

    RpAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(pass_data_->shared_data);

    RpAllocTex &color_tex = builder.GetWriteTexture(pass_data_->color_tex);
    RpAllocTex &depth_tex = builder.GetWriteTexture(pass_data_->depth_tex);

    Ren::RastState rast_state;
    rast_state.poly.cull = uint8_t(Ren::eCullFace::Front);
    rast_state.viewport[2] = view_state_->act_res[0];
    rast_state.viewport[3] = view_state_->act_res[1];
    rast_state.depth.test_enabled = true;
    rast_state.depth.write_enabled = false;
    rast_state.depth.compare_op = unsigned(Ren::eCompareOp::Greater);
    rast_state.blend.enabled = false;

    rast_state.stencil.enabled = true;
    rast_state.stencil.write_mask = 0xff;
    rast_state.stencil.pass = unsigned(Ren::eStencilOp::Replace);

    Ren::SmallVector<Ren::Binding, 8> bindings = {
        {Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, *unif_sh_data_buf.ref}};

    Ren::Mat4f scale_matrix;
    scale_matrix = Scale(scale_matrix, Ren::Vec3f{5000.0f, 5000.0f, 5000.0f});

    Skydome::Params uniform_params = {};
    uniform_params.xform = scale_matrix;
    uniform_params.clip_from_world = view_state_->clip_from_world;

    const Ren::RenderTarget color_targets[] = {{color_tex.ref, Ren::eLoadOp::Load, Ren::eStoreOp::Store}};
    const Ren::RenderTarget depth_target = {depth_tex.ref, Ren::eLoadOp::Load, Ren::eStoreOp::Store, Ren::eLoadOp::Load,
                                            Ren::eStoreOp::Store};

    if (pass_data_->sky_quality == eSkyQuality::High) {
        RpAllocTex &transmittance_lut = builder.GetReadTexture(pass_data_->phys.transmittance_lut);
        RpAllocTex &multiscatter_lut = builder.GetReadTexture(pass_data_->phys.multiscatter_lut);
        RpAllocTex &moon_tex = builder.GetReadTexture(pass_data_->phys.moon_tex);
        RpAllocTex &weather_tex = builder.GetReadTexture(pass_data_->phys.weather_tex);
        RpAllocTex &cirrus_tex = builder.GetReadTexture(pass_data_->phys.cirrus_tex);
        RpAllocTex &noise3d_tex = builder.GetReadTexture(pass_data_->phys.noise3d_tex);

        bindings.emplace_back(Ren::eBindTarget::Tex2DSampled, Skydome::TRANSMITTANCE_LUT_SLOT, *transmittance_lut.ref);
        bindings.emplace_back(Ren::eBindTarget::Tex2DSampled, Skydome::MULTISCATTER_LUT_SLOT, *multiscatter_lut.ref);
        bindings.emplace_back(Ren::eBindTarget::Tex2DSampled, Skydome::MOON_TEX_SLOT, *moon_tex.ref);
        bindings.emplace_back(Ren::eBindTarget::Tex2DSampled, Skydome::WEATHER_TEX_SLOT, *weather_tex.ref);
        bindings.emplace_back(Ren::eBindTarget::Tex2DSampled, Skydome::CIRRUS_TEX_SLOT, *cirrus_tex.ref);
        bindings.emplace_back(Ren::eBindTarget::Tex3DSampled, Skydome::NOISE3D_TEX_SLOT, *noise3d_tex.tex3d);

        uniform_params.clip_from_world = view_state_->clip_from_world;

        prim_draw_.DrawPrim(PrimDraw::ePrim::Sphere, prog_skydome_phys_, color_targets, depth_target, rast_state,
                            builder.rast_state(), bindings, &uniform_params, sizeof(uniform_params), 0);
    } else {
        RpAllocTex &env_tex = builder.GetReadTexture(pass_data_->env_tex);

        bindings.emplace_back(Ren::eBindTarget::Tex2DSampled, Skydome::ENV_TEX_SLOT, *env_tex.ref);

        prim_draw_.DrawPrim(PrimDraw::ePrim::Sphere, prog_skydome_simple_, color_targets, depth_target, rast_state,
                            builder.rast_state(), bindings, &uniform_params, sizeof(uniform_params), 0);
    }
}

void Eng::RpSkydomeScreen::LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh) {
    if (!initialized) {
        prog_skydome_simple_ =
            sh.LoadProgram(ctx, "internal/skydome_simple.vert.glsl", "internal/skydome_simple.frag.glsl");
        assert(prog_skydome_simple_->ready());

        prog_skydome_phys_ =
            sh.LoadProgram(ctx, "internal/skydome_phys.vert.glsl", "internal/skydome_phys.frag.glsl@SCREEN");
        assert(prog_skydome_phys_->ready());

        initialized = true;
    }
}
