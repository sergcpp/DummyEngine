#include "ExSkydome.h"

#include <Ren/Context.h>
#include <Ren/DrawCall.h>

#include "../../utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"
#include "../shaders/skydome_downsample_interface.h"
#include "../shaders/skydome_interface.h"

namespace ExSkydomeCubeInternal {
// PMJ samples stretched to 4x4 region
const Ren::Vec2i g_sample_positions[16] = {Ren::Vec2i{1, 0}, Ren::Vec2i{3, 2}, Ren::Vec2i{0, 3}, Ren::Vec2i{2, 1},
                                           Ren::Vec2i{0, 1}, Ren::Vec2i{2, 3}, Ren::Vec2i{1, 2}, Ren::Vec2i{3, 0},
                                           Ren::Vec2i{0, 0}, Ren::Vec2i{2, 2}, Ren::Vec2i{1, 3}, Ren::Vec2i{3, 1},
                                           Ren::Vec2i{1, 1}, Ren::Vec2i{3, 3}, Ren::Vec2i{0, 2}, Ren::Vec2i{2, 0}};
} // namespace ExSkydomeCubeInternal

void Eng::ExSkydomeCube::Execute(FgContext &fg) {
    LazyInit(fg.ren_ctx(), fg.sh());

    const Ren::Buffer &unif_sh_data_buf = fg.AccessROBuffer(args_->shared_data);
    const Ren::Texture &transmittance_lut = fg.AccessROTexture(args_->transmittance_lut);
    const Ren::Texture &multiscatter_lut = fg.AccessROTexture(args_->multiscatter_lut);
    const Ren::Texture &moon_tex = fg.AccessROTexture(args_->moon_tex);
    const Ren::Texture &weather_tex = fg.AccessROTexture(args_->weather_tex);
    const Ren::Texture &cirrus_tex = fg.AccessROTexture(args_->cirrus_tex);
    const Ren::Texture &curl_tex = fg.AccessROTexture(args_->curl_tex);
    const Ren::Texture &noise3d_tex = fg.AccessROTexture(args_->noise3d_tex);
    Ren::WeakTexRef color_tex = fg.AccessRWTextureRef(args_->color_tex);

    if (view_state_->env_generation == generation_) {
        return;
    }

    if (view_state_->env_generation == 0xffffffff) {
        // full update
        generation_ = generation_in_progress_ = 0xffffffff;
        last_updated_faceq_ = 23;
    }

    if (last_updated_faceq_ == 23) {
        last_updated_faceq_ = -1;
        generation_in_progress_ = view_state_->env_generation;
    }

    Ren::RastState rast_state;
    rast_state.poly.cull = uint8_t(Ren::eCullFace::Front);
    rast_state.viewport[2] = color_tex->params.w;
    rast_state.viewport[3] = color_tex->params.h;

    const Ren::Binding bindings[] = {{Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, unif_sh_data_buf},
                                     {Ren::eBindTarget::TexSampled, Skydome::TRANSMITTANCE_LUT_SLOT, transmittance_lut},
                                     {Ren::eBindTarget::TexSampled, Skydome::MULTISCATTER_LUT_SLOT, multiscatter_lut},
                                     {Ren::eBindTarget::TexSampled, Skydome::MOON_TEX_SLOT, moon_tex},
                                     {Ren::eBindTarget::TexSampled, Skydome::WEATHER_TEX_SLOT, weather_tex},
                                     {Ren::eBindTarget::TexSampled, Skydome::CIRRUS_TEX_SLOT, cirrus_tex},
                                     {Ren::eBindTarget::TexSampled, Skydome::CURL_TEX_SLOT, curl_tex},
                                     {Ren::eBindTarget::TexSampled, Skydome::NOISE3D_TEX_SLOT, noise3d_tex}};

#if defined(REN_GL_BACKEND)
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

    const int mip_count = color_tex->params.mip_count;

    Ren::Camera temp_cam;
    temp_cam.Perspective(Ren::eZRange::OneToZero, 90.0f, 1.0f, 1.0f, 1000.0f);

    const int faceq_start = last_updated_faceq_ + 1;
    const int faceq_end = (generation_ == 0xffffffff) ? 24 : faceq_start + 1;

    assert(color_tex->params.w % 2 == 0);
    assert(color_tex->params.h % 2 == 0);
    const Ren::Vec4i quadrants[] = {
        Ren::Vec4i{0, 0, color_tex->params.w / 2, color_tex->params.h / 2},
        Ren::Vec4i{color_tex->params.w / 2, 0, color_tex->params.w / 2, color_tex->params.h / 2},
        Ren::Vec4i{0, color_tex->params.h / 2, color_tex->params.w / 2, color_tex->params.h / 2},
        Ren::Vec4i{color_tex->params.w / 2, color_tex->params.h / 2, color_tex->params.w / 2, color_tex->params.h / 2}};

    for (int faceq = faceq_start; faceq < faceq_end; ++faceq) {
        rast_state.scissor.enabled = true;
        rast_state.scissor.rect = quadrants[faceq % 4];

        temp_cam.SetupView(Ren::Vec3f{0.0f}, axises[faceq / 4], ups[faceq / 4]);

        Skydome::Params uniform_params = {};
        uniform_params.clip_from_world = temp_cam.proj_matrix() * temp_cam.view_matrix();
        uniform_params.scale = 500.0f;

        const Ren::RenderTarget color_targets[] = {
            {color_tex, uint8_t((faceq / 4) + 1), Ren::eLoadOp::Load, Ren::eStoreOp::Store}};
        prim_draw_.DrawPrim(PrimDraw::ePrim::Sphere, prog_skydome_phys_, {}, color_targets, rast_state, fg.rast_state(),
                            bindings, &uniform_params, sizeof(uniform_params), 0);

        last_updated_faceq_ = faceq;
    }

    const int face_start = (generation_ == 0xffffffff) ? 0 : (last_updated_faceq_ / 4);
    const int face_end = (generation_ == 0xffffffff) ? 6 : face_start + 1;

    for (int face = face_start; face < face_end; ++face) {
        for (int mip = 1; mip < mip_count; mip += 4) {
            const Ren::TransitionInfo transitions[] = {{color_tex.get(), Ren::eResState::UnorderedAccess}};
            TransitionResourceStates(fg.ren_ctx().api_ctx(), fg.cmd_buf(), Ren::AllStages, Ren::AllStages, transitions);

            const Ren::Binding _bindings[] = {
                {Ren::eBindTarget::TexSampled,
                 SkydomeDownsample::INPUT_TEX_SLOT,
                 {*color_tex, (mip - 1) * 6 + face + 1}},
                {Ren::eBindTarget::ImageRW, SkydomeDownsample::OUTPUT_IMG_SLOT, 0, 1, {*color_tex, mip * 6 + face + 1}},
                {Ren::eBindTarget::ImageRW,
                 SkydomeDownsample::OUTPUT_IMG_SLOT,
                 1,
                 1,
                 {*color_tex, std::min(mip + 1, mip_count - 1) * 6 + face + 1}},
                {Ren::eBindTarget::ImageRW,
                 SkydomeDownsample::OUTPUT_IMG_SLOT,
                 2,
                 1,
                 {*color_tex, std::min(mip + 2, mip_count - 1) * 6 + face + 1}},
                {Ren::eBindTarget::ImageRW,
                 SkydomeDownsample::OUTPUT_IMG_SLOT,
                 3,
                 1,
                 {*color_tex, std::min(mip + 3, mip_count - 1) * 6 + face + 1}}};

            SkydomeDownsample::Params uniform_params = {};
            uniform_params.img_size[0] = (color_tex->params.w >> mip);
            uniform_params.img_size[1] = (color_tex->params.h >> mip);
            uniform_params.mip_count = std::min(4, mip_count - mip);

            const Ren::Vec3u grp_count = Ren::Vec3u{
                (uniform_params.img_size[0] + SkydomeDownsample::GRP_SIZE_X - 1) / SkydomeDownsample::GRP_SIZE_X,
                (uniform_params.img_size[1] + SkydomeDownsample::GRP_SIZE_Y - 1) / SkydomeDownsample::GRP_SIZE_Y, 1u};

            DispatchCompute(*pi_skydome_downsample_, grp_count, _bindings, &uniform_params, sizeof(uniform_params),
                            fg.descr_alloc(), fg.log());
        }
    }

    if (last_updated_faceq_ == 23) {
        generation_ = generation_in_progress_;
    }

    const Ren::TransitionInfo transitions[] = {{color_tex.get(), Ren::eResState::RenderTarget}};
    Ren::TransitionResourceStates(fg.ren_ctx().api_ctx(), fg.cmd_buf(), Ren::AllStages, Ren::AllStages, transitions);
}

void Eng::ExSkydomeCube::LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh) {
    if (!initialized_) {
        prog_skydome_phys_ = sh.LoadProgram("internal/skydome_phys.vert.glsl", "internal/skydome_phys.frag.glsl");
        pi_skydome_downsample_ = sh.LoadPipeline("internal/skydome_downsample.comp.glsl");

        initialized_ = true;
    }
}

void Eng::ExSkydomeScreen::Execute(FgContext &fg) {
    LazyInit(fg.ren_ctx(), fg.sh());

    const Ren::Buffer &unif_sh_data_buf = fg.AccessROBuffer(args_->shared_data);

    Ren::WeakTexRef color_tex = fg.AccessRWTextureRef(args_->color_tex);

    Ren::RastState rast_state;
    rast_state.poly.cull = uint8_t(Ren::eCullFace::Front);
    rast_state.depth.test_enabled = true;
    rast_state.depth.write_enabled = false;
    rast_state.depth.compare_op = unsigned(Ren::eCompareOp::Greater);
    rast_state.blend.enabled = false;

    rast_state.stencil.enabled = true;
    rast_state.stencil.write_mask = 0xff;
    rast_state.stencil.pass = unsigned(Ren::eStencilOp::Replace);

    Ren::SmallVector<Ren::Binding, 8> bindings = {{Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, unif_sh_data_buf}};

    Skydome::Params uniform_params = {};
    uniform_params.clip_from_world = view_state_->clip_from_world_no_translation;
    uniform_params.sample_coord = sample_pos(view_state_->frame_index);
    uniform_params.img_size = view_state_->ren_res;
    uniform_params.texel_size = 1.0f / Ren::Vec2f(view_state_->ren_res);
    uniform_params.scale = 0.95f * view_state_->clip_info[2];

    const Ren::RenderTarget color_targets[] = {{color_tex, Ren::eLoadOp::Load, Ren::eStoreOp::Store}};

    if (args_->sky_quality == eSkyQuality::Ultra) {
        const Ren::Texture &transmittance_lut = fg.AccessROTexture(args_->phys.transmittance_lut);
        const Ren::Texture &multiscatter_lut = fg.AccessROTexture(args_->phys.multiscatter_lut);
        const Ren::Texture &moon_tex = fg.AccessROTexture(args_->phys.moon_tex);
        const Ren::Texture &weather_tex = fg.AccessROTexture(args_->phys.weather_tex);
        const Ren::Texture &cirrus_tex = fg.AccessROTexture(args_->phys.cirrus_tex);
        const Ren::Texture &curl_tex = fg.AccessROTexture(args_->phys.curl_tex);
        const Ren::Texture &noise3d_tex = fg.AccessROTexture(args_->phys.noise3d_tex);
        Ren::WeakTexRef depth_tex = fg.AccessRWTextureRef(args_->depth_tex);

        bindings.emplace_back(Ren::eBindTarget::TexSampled, Skydome::TRANSMITTANCE_LUT_SLOT, transmittance_lut);
        bindings.emplace_back(Ren::eBindTarget::TexSampled, Skydome::MULTISCATTER_LUT_SLOT, multiscatter_lut);
        bindings.emplace_back(Ren::eBindTarget::TexSampled, Skydome::MOON_TEX_SLOT, moon_tex);
        bindings.emplace_back(Ren::eBindTarget::TexSampled, Skydome::WEATHER_TEX_SLOT, weather_tex);
        bindings.emplace_back(Ren::eBindTarget::TexSampled, Skydome::CIRRUS_TEX_SLOT, cirrus_tex);
        bindings.emplace_back(Ren::eBindTarget::TexSampled, Skydome::CURL_TEX_SLOT, curl_tex);
        bindings.emplace_back(Ren::eBindTarget::TexSampled, Skydome::NOISE3D_TEX_SLOT, noise3d_tex);

        rast_state.viewport[2] = view_state_->ren_res[0];
        rast_state.viewport[3] = view_state_->ren_res[1];

        const Ren::RenderTarget depth_target = {depth_tex, Ren::eLoadOp::Load, Ren::eStoreOp::Store, Ren::eLoadOp::Load,
                                                Ren::eStoreOp::Store};
        prim_draw_.DrawPrim(PrimDraw::ePrim::Sphere, prog_skydome_phys_[0], depth_target, color_targets, rast_state,
                            fg.rast_state(), bindings, &uniform_params, sizeof(uniform_params), 0);
    } else if (args_->sky_quality == eSkyQuality::High) {
        const Ren::Texture &transmittance_lut = fg.AccessROTexture(args_->phys.transmittance_lut);
        const Ren::Texture &multiscatter_lut = fg.AccessROTexture(args_->phys.multiscatter_lut);
        const Ren::Texture &moon_tex = fg.AccessROTexture(args_->phys.moon_tex);
        const Ren::Texture &weather_tex = fg.AccessROTexture(args_->phys.weather_tex);
        const Ren::Texture &cirrus_tex = fg.AccessROTexture(args_->phys.cirrus_tex);
        const Ren::Texture &curl_tex = fg.AccessROTexture(args_->phys.curl_tex);
        const Ren::Texture &noise3d_tex = fg.AccessROTexture(args_->phys.noise3d_tex);
        const Ren::Texture &depth_tex = fg.AccessROTexture(args_->depth_tex);

        bindings.emplace_back(Ren::eBindTarget::TexSampled, Skydome::TRANSMITTANCE_LUT_SLOT, transmittance_lut);
        bindings.emplace_back(Ren::eBindTarget::TexSampled, Skydome::MULTISCATTER_LUT_SLOT, multiscatter_lut);
        bindings.emplace_back(Ren::eBindTarget::TexSampled, Skydome::MOON_TEX_SLOT, moon_tex);
        bindings.emplace_back(Ren::eBindTarget::TexSampled, Skydome::WEATHER_TEX_SLOT, weather_tex);
        bindings.emplace_back(Ren::eBindTarget::TexSampled, Skydome::CIRRUS_TEX_SLOT, cirrus_tex);
        bindings.emplace_back(Ren::eBindTarget::TexSampled, Skydome::CURL_TEX_SLOT, curl_tex);
        bindings.emplace_back(Ren::eBindTarget::TexSampled, Skydome::NOISE3D_TEX_SLOT, noise3d_tex);
        bindings.emplace_back(Ren::eBindTarget::TexSampled, Skydome::DEPTH_TEX_SLOT, Ren::OpaqueHandle{depth_tex, 1});

        rast_state.viewport[2] = color_tex->params.w;
        rast_state.viewport[3] = color_tex->params.h;

        prim_draw_.DrawPrim(PrimDraw::ePrim::Sphere, prog_skydome_phys_[1], {}, color_targets, rast_state,
                            fg.rast_state(), bindings, &uniform_params, sizeof(uniform_params), 0);
    } else {
        const Ren::Texture &env_tex = fg.AccessROTexture(args_->env_tex);
        Ren::WeakTexRef depth_tex = fg.AccessRWTextureRef(args_->depth_tex);

        bindings.emplace_back(Ren::eBindTarget::TexSampled, Skydome::ENV_TEX_SLOT, env_tex);

        rast_state.viewport[2] = view_state_->ren_res[0];
        rast_state.viewport[3] = view_state_->ren_res[1];

        const Ren::RenderTarget depth_target = {depth_tex, Ren::eLoadOp::Load, Ren::eStoreOp::Store, Ren::eLoadOp::Load,
                                                Ren::eStoreOp::Store};

        prim_draw_.DrawPrim(PrimDraw::ePrim::Sphere, prog_skydome_simple_, depth_target, color_targets, rast_state,
                            fg.rast_state(), bindings, &uniform_params, sizeof(uniform_params), 0);
    }
}

void Eng::ExSkydomeScreen::LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh) {
    if (!initialized) {
        prog_skydome_simple_ = sh.LoadProgram("internal/skydome_simple.vert.glsl", "internal/skydome_simple.frag.glsl");
        prog_skydome_phys_[0] =
            sh.LoadProgram("internal/skydome_phys.vert.glsl", "internal/skydome_phys@SCREEN.frag.glsl");
        prog_skydome_phys_[1] =
            sh.LoadProgram("internal/skydome_phys.vert.glsl", "internal/skydome_phys@SCREEN;SUBSAMPLE.frag.glsl");

        initialized = true;
    }
}

Ren::Vec2i Eng::ExSkydomeScreen::sample_pos(const int frame_index) {
    return ExSkydomeCubeInternal::g_sample_positions[frame_index % 16];
}