#include "RpSkydome.h"

#include <Ren/Context.h>
#include <Ren/DrawCall.h>

#include "../../utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"
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
    rast_state.viewport[2] = view_state_->act_res[0];
    rast_state.viewport[3] = view_state_->act_res[1];

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
    temp_cam.Perspective(90.0f, 1.0f, 1.0f, 10000.0f);
    for (int i = 0; i < 6; i++) {
        temp_cam.SetupView(Ren::Vec3f{0.0f}, axises[i], ups[i]);

        uniform_params.clip_from_world = temp_cam.proj_matrix() * temp_cam.view_matrix();

        const Ren::RenderTarget color_targets[] = {
            {color_tex.ref, uint8_t(i + 1), Ren::eLoadOp::DontCare, Ren::eStoreOp::Store}};
        prim_draw_.DrawPrim(PrimDraw::ePrim::Sphere, prog_skydome_phys_, color_targets, {}, rast_state,
                            builder.rast_state(), bindings, &uniform_params, sizeof(uniform_params), 0);
    }
}

void Eng::RpSkydomeCube::LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh) {
    if (!initialized) {
        prog_skydome_phys_ = sh.LoadProgram(ctx, "internal/skydome_phys.vert.glsl", "internal/skydome_phys.frag.glsl");
        assert(prog_skydome_phys_->ready());

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
    rast_state.depth.compare_op = unsigned(Ren::eCompareOp::Less);
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
    uniform_params.clip_from_world = view_state_->clip_from_world_no_translation;

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

        uniform_params.clip_from_world = view_state_->clip_from_world_no_translation;

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
