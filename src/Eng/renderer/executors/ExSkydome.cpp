#include "ExSkydome.h"

#include <Ren/Camera.h>
#include <Ren/Context.h>
#include <Ren/DrawCall.h>

#include "../../utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"
#include "../framegraph/FgBuilder.h"
#include "../shaders/skydome_downsample_interface.h"
#include "../shaders/skydome_interface.h"

namespace ExSkydomeCubeInternal {
// PMJ samples stretched to 4x4 region
const Ren::Vec2i g_sample_positions[16] = {Ren::Vec2i{1, 0}, Ren::Vec2i{3, 2}, Ren::Vec2i{0, 3}, Ren::Vec2i{2, 1},
                                           Ren::Vec2i{0, 1}, Ren::Vec2i{2, 3}, Ren::Vec2i{1, 2}, Ren::Vec2i{3, 0},
                                           Ren::Vec2i{0, 0}, Ren::Vec2i{2, 2}, Ren::Vec2i{1, 3}, Ren::Vec2i{3, 1},
                                           Ren::Vec2i{1, 1}, Ren::Vec2i{3, 3}, Ren::Vec2i{0, 2}, Ren::Vec2i{2, 0}};
} // namespace ExSkydomeCubeInternal

void Eng::ExSkydomeCube::Execute(const FgContext &fg) {
    LazyInit(fg);

    const Ren::BufferROHandle unif_sh_data = fg.AccessROBuffer(args_->shared_data);
    const Ren::ImageROHandle transmittance_lut = fg.AccessROImage(args_->transmittance_lut);
    const Ren::ImageROHandle multiscatter_lut = fg.AccessROImage(args_->multiscatter_lut);
    const Ren::ImageROHandle moon = fg.AccessROImage(args_->moon);
    const Ren::ImageROHandle weather = fg.AccessROImage(args_->weather);
    const Ren::ImageROHandle cirrus = fg.AccessROImage(args_->cirrus);
    const Ren::ImageROHandle curl = fg.AccessROImage(args_->curl);
    const Ren::ImageROHandle noise3d = fg.AccessROImage(args_->noise3d);

    const Ren::ImageRWHandle color = fg.AccessRWImage(args_->color);

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

    // TODO: Get rid of this!
    const auto &[target_main, target_cold] = fg.ren_ctx().storages().images[color];

    Ren::RastState rast_state;
    rast_state.poly.cull = uint8_t(Ren::eCullFace::Front);
    rast_state.viewport[2] = target_cold.params.w;
    rast_state.viewport[3] = target_cold.params.h;

    const Ren::Binding bindings[] = {{Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, unif_sh_data},
                                     {Ren::eBindTarget::TexSampled, Skydome::TRANSMITTANCE_LUT_SLOT, transmittance_lut},
                                     {Ren::eBindTarget::TexSampled, Skydome::MULTISCATTER_LUT_SLOT, multiscatter_lut},
                                     {Ren::eBindTarget::TexSampled, Skydome::MOON_TEX_SLOT, moon},
                                     {Ren::eBindTarget::TexSampled, Skydome::WEATHER_TEX_SLOT, weather},
                                     {Ren::eBindTarget::TexSampled, Skydome::CIRRUS_TEX_SLOT, cirrus},
                                     {Ren::eBindTarget::TexSampled, Skydome::CURL_TEX_SLOT, curl},
                                     {Ren::eBindTarget::TexSampled, Skydome::NOISE3D_TEX_SLOT, noise3d}};

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

    const int mip_count = target_cold.params.mip_count;

    Ren::Camera temp_cam;
    temp_cam.Perspective(Ren::eZRange::OneToZero, 90.0f, 1.0f, 1.0f, 1000.0f);

    const int faceq_start = last_updated_faceq_ + 1;
    const int faceq_end = (generation_ == 0xffffffff) ? 24 : faceq_start + 1;

    assert(target_cold.params.w % 2 == 0);
    assert(target_cold.params.h % 2 == 0);
    const Ren::Vec4i quadrants[] = {
        Ren::Vec4i{0, 0, target_cold.params.w / 2, target_cold.params.h / 2},
        Ren::Vec4i{target_cold.params.w / 2, 0, target_cold.params.w / 2, target_cold.params.h / 2},
        Ren::Vec4i{0, target_cold.params.h / 2, target_cold.params.w / 2, target_cold.params.h / 2},
        Ren::Vec4i{target_cold.params.w / 2, target_cold.params.h / 2, target_cold.params.w / 2,
                   target_cold.params.h / 2}};

    for (int faceq = faceq_start; faceq < faceq_end; ++faceq) {
        rast_state.scissor.enabled = true;
        rast_state.scissor.rect = quadrants[faceq % 4];

        temp_cam.SetupView(Ren::Vec3f{0.0f}, axises[faceq / 4], ups[faceq / 4]);

        Skydome::Params uniform_params = {};
        uniform_params.clip_from_world = temp_cam.proj_matrix() * temp_cam.view_matrix();
        uniform_params.scale = 500.0f;

        const Ren::RenderTarget color_targets[] = {
            {color, uint8_t((faceq / 4) + 1), Ren::eLoadOp::Load, Ren::eStoreOp::Store}};
        prim_draw_.DrawPrim(fg.cmd_buf(), PrimDraw::ePrim::Sphere, prog_skydome_phys_, {}, color_targets, rast_state,
                            fg.rast_state(), bindings, &uniform_params, sizeof(uniform_params), 0, fg.framebuffers());

        last_updated_faceq_ = faceq;
    }

    const int face_start = (generation_ == 0xffffffff) ? 0 : (last_updated_faceq_ / 4);
    const int face_end = (generation_ == 0xffffffff) ? 6 : face_start + 1;

    for (int face = face_start; face < face_end; ++face) {
        for (int mip = 1; mip < mip_count; mip += 4) {
            const Ren::TransitionInfo transitions[] = {{color, Ren::eResState::UnorderedAccess}};
            TransitionResourceStates(fg.ren_ctx().api(), fg.storages(), fg.cmd_buf(), Ren::AllStages, Ren::AllStages,
                                     transitions);

            const Ren::Binding _bindings[] = {
                {Ren::eBindTarget::TexSampled, SkydomeDownsample::INPUT_TEX_SLOT, {color, (mip - 1) * 6 + face + 1}},
                {Ren::eBindTarget::ImageRW, SkydomeDownsample::OUTPUT_IMG_SLOT, 0, 1, {color, mip * 6 + face + 1}},
                {Ren::eBindTarget::ImageRW,
                 SkydomeDownsample::OUTPUT_IMG_SLOT,
                 1,
                 1,
                 {color, std::min(mip + 1, mip_count - 1) * 6 + face + 1}},
                {Ren::eBindTarget::ImageRW,
                 SkydomeDownsample::OUTPUT_IMG_SLOT,
                 2,
                 1,
                 {color, std::min(mip + 2, mip_count - 1) * 6 + face + 1}},
                {Ren::eBindTarget::ImageRW,
                 SkydomeDownsample::OUTPUT_IMG_SLOT,
                 3,
                 1,
                 {color, std::min(mip + 3, mip_count - 1) * 6 + face + 1}}};

            SkydomeDownsample::Params uniform_params = {};
            uniform_params.img_size[0] = (target_cold.params.w >> mip);
            uniform_params.img_size[1] = (target_cold.params.h >> mip);
            uniform_params.mip_count = std::min(4, mip_count - mip);

            const Ren::Vec3u grp_count =
                Ren::Vec3u(Ren::DivCeil(int(uniform_params.img_size[0]), SkydomeDownsample::GRP_SIZE_X),
                           Ren::DivCeil(int(uniform_params.img_size[1]), SkydomeDownsample::GRP_SIZE_Y), 1u);

            DispatchCompute(fg.cmd_buf(), pi_skydome_downsample_, fg.storages(), grp_count, _bindings, &uniform_params,
                            sizeof(uniform_params), fg.descr_alloc(), fg.log());
        }
    }

    if (last_updated_faceq_ == 23) {
        generation_ = generation_in_progress_;
    }

    const Ren::TransitionInfo transitions[] = {{color, Ren::eResState::RenderTarget}};
    TransitionResourceStates(fg.ren_ctx().api(), fg.storages(), fg.cmd_buf(), Ren::AllStages, Ren::AllStages,
                             transitions);
}

void Eng::ExSkydomeCube::LazyInit(const FgContext &fg) {
    auto &sh = fg.sh();
    if (!initialized_) {
        prog_skydome_phys_ =
            sh.FindOrCreateProgram("internal/skydome_phys.vert.glsl", "internal/skydome_phys.frag.glsl");
        pi_skydome_downsample_ = sh.FindOrCreatePipeline("internal/skydome_downsample.comp.glsl");

        initialized_ = true;
    }
}

void Eng::ExSkydomeScreen::Execute(const FgContext &fg) {
    LazyInit(fg);

    const Ren::BufferROHandle unif_sh_data_buf = fg.AccessROBuffer(args_->shared_data);

    const Ren::ImageRWHandle color = fg.AccessRWImage(args_->color);

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

    const Ren::RenderTarget color_targets[] = {{color, Ren::eLoadOp::Load, Ren::eStoreOp::Store}};

    if (args_->sky_quality == eSkyQuality::Ultra) {
        const Ren::ImageROHandle transmittance_lut = fg.AccessROImage(args_->phys.transmittance_lut);
        const Ren::ImageROHandle multiscatter_lut = fg.AccessROImage(args_->phys.multiscatter_lut);
        const Ren::ImageROHandle moon = fg.AccessROImage(args_->phys.moon);
        const Ren::ImageROHandle weather = fg.AccessROImage(args_->phys.weather);
        const Ren::ImageROHandle cirrus = fg.AccessROImage(args_->phys.cirrus);
        const Ren::ImageROHandle curl = fg.AccessROImage(args_->phys.curl);
        const Ren::ImageROHandle noise3d = fg.AccessROImage(args_->phys.noise3d);

        const Ren::ImageRWHandle depth = fg.AccessRWImage(args_->depth_rw);

        bindings.emplace_back(Ren::eBindTarget::TexSampled, Skydome::TRANSMITTANCE_LUT_SLOT, transmittance_lut);
        bindings.emplace_back(Ren::eBindTarget::TexSampled, Skydome::MULTISCATTER_LUT_SLOT, multiscatter_lut);
        bindings.emplace_back(Ren::eBindTarget::TexSampled, Skydome::MOON_TEX_SLOT, moon);
        bindings.emplace_back(Ren::eBindTarget::TexSampled, Skydome::WEATHER_TEX_SLOT, weather);
        bindings.emplace_back(Ren::eBindTarget::TexSampled, Skydome::CIRRUS_TEX_SLOT, cirrus);
        bindings.emplace_back(Ren::eBindTarget::TexSampled, Skydome::CURL_TEX_SLOT, curl);
        bindings.emplace_back(Ren::eBindTarget::TexSampled, Skydome::NOISE3D_TEX_SLOT, noise3d);

        rast_state.viewport[2] = view_state_->ren_res[0];
        rast_state.viewport[3] = view_state_->ren_res[1];

        const Ren::RenderTarget depth_target = {depth, Ren::eLoadOp::Load, Ren::eStoreOp::Store, Ren::eLoadOp::Load,
                                                Ren::eStoreOp::Store};
        prim_draw_.DrawPrim(fg.cmd_buf(), PrimDraw::ePrim::Sphere, prog_skydome_phys_[0], depth_target, color_targets,
                            rast_state, fg.rast_state(), bindings, &uniform_params, sizeof(uniform_params), 0,
                            fg.framebuffers());
    } else if (args_->sky_quality == eSkyQuality::High) {
        const Ren::ImageROHandle transmittance_lut = fg.AccessROImage(args_->phys.transmittance_lut);
        const Ren::ImageROHandle multiscatter_lut = fg.AccessROImage(args_->phys.multiscatter_lut);
        const Ren::ImageROHandle moon = fg.AccessROImage(args_->phys.moon);
        const Ren::ImageROHandle weather = fg.AccessROImage(args_->phys.weather);
        const Ren::ImageROHandle cirrus = fg.AccessROImage(args_->phys.cirrus);
        const Ren::ImageROHandle curl = fg.AccessROImage(args_->phys.curl);
        const Ren::ImageROHandle noise3d = fg.AccessROImage(args_->phys.noise3d);
        const Ren::ImageROHandle depth = fg.AccessROImage(args_->depth_ro);

        bindings.emplace_back(Ren::eBindTarget::TexSampled, Skydome::TRANSMITTANCE_LUT_SLOT, transmittance_lut);
        bindings.emplace_back(Ren::eBindTarget::TexSampled, Skydome::MULTISCATTER_LUT_SLOT, multiscatter_lut);
        bindings.emplace_back(Ren::eBindTarget::TexSampled, Skydome::MOON_TEX_SLOT, moon);
        bindings.emplace_back(Ren::eBindTarget::TexSampled, Skydome::WEATHER_TEX_SLOT, weather);
        bindings.emplace_back(Ren::eBindTarget::TexSampled, Skydome::CIRRUS_TEX_SLOT, cirrus);
        bindings.emplace_back(Ren::eBindTarget::TexSampled, Skydome::CURL_TEX_SLOT, curl);
        bindings.emplace_back(Ren::eBindTarget::TexSampled, Skydome::NOISE3D_TEX_SLOT, noise3d);
        bindings.emplace_back(Ren::eBindTarget::TexSampled, Skydome::DEPTH_TEX_SLOT, Ren::OpaqueHandle{depth, 1});

        // TODO: Get rid of this!
        const auto &[color_main, color_cold] = fg.storages().images[color];
        rast_state.viewport[2] = color_cold.params.w;
        rast_state.viewport[3] = color_cold.params.h;

        prim_draw_.DrawPrim(fg.cmd_buf(), PrimDraw::ePrim::Sphere, prog_skydome_phys_[1], {}, color_targets, rast_state,
                            fg.rast_state(), bindings, &uniform_params, sizeof(uniform_params), 0);
    } else {
        const Ren::ImageROHandle env = fg.AccessROImage(args_->env);
        const Ren::ImageRWHandle depth = fg.AccessRWImage(args_->depth_rw);

        bindings.emplace_back(Ren::eBindTarget::TexSampled, Skydome::ENV_TEX_SLOT, env);

        rast_state.viewport[2] = view_state_->ren_res[0];
        rast_state.viewport[3] = view_state_->ren_res[1];

        const Ren::RenderTarget depth_target = {depth, Ren::eLoadOp::Load, Ren::eStoreOp::Store, Ren::eLoadOp::Load,
                                                Ren::eStoreOp::Store};

        prim_draw_.DrawPrim(fg.cmd_buf(), PrimDraw::ePrim::Sphere, prog_skydome_simple_, depth_target, color_targets,
                            rast_state, fg.rast_state(), bindings, &uniform_params, sizeof(uniform_params), 0,
                            fg.framebuffers());
    }
}

void Eng::ExSkydomeScreen::LazyInit(const FgContext &fg) {
    auto &sh = fg.sh();
    if (!initialized_) {
        prog_skydome_simple_ =
            sh.FindOrCreateProgram("internal/skydome_simple.vert.glsl", "internal/skydome_simple.frag.glsl");
        prog_skydome_phys_[0] =
            sh.FindOrCreateProgram("internal/skydome_phys.vert.glsl", "internal/skydome_phys@SCREEN.frag.glsl");
        prog_skydome_phys_[1] = sh.FindOrCreateProgram("internal/skydome_phys.vert.glsl",
                                                       "internal/skydome_phys@SCREEN;SUBSAMPLE.frag.glsl");

        initialized_ = true;
    }
}

Ren::Vec2i Eng::ExSkydomeScreen::sample_pos(const int frame_index) {
    return ExSkydomeCubeInternal::g_sample_positions[frame_index % 16];
}