#include "DrawTest.h"

#include <memory>

#include <Eng/Log.h>
#include <Eng/ViewerStateManager.h>
#include <Eng/scene/SceneManager.h>
#include <Eng/widgets/CmdlineUI.h>
#include <Gui/Renderer.h>
#include <Ray/Ray.h>
#include <Ren/Context.h>
#include <Sys/Time_.h>

#include "../Viewer.h"
#include "../widgets/FontStorage.h"

#define TINYEXR_IMPLEMENTATION
#define TINYEXR_USE_MINIZ 0
#define TINYEXR_USE_STB_ZLIB 1
#include <tinyexr/tinyexr.h>
#undef GetObject

#include <optick/optick.h>
#include <stb/stb_image.h>

namespace Ray {
extern const int LUT_DIMS;
extern const uint32_t *transform_luts[];
} // namespace Ray

namespace DrawTestInternal {
Ren::Span<const uint8_t> RayLUTByName(const std::string_view name) {
    const uint32_t *lut_data = nullptr;
    if (name == "agx") {
        lut_data = Ray::transform_luts[int(Ray::eViewTransform::AgX)];
    } else if (name == "agx_punchy") {
        lut_data = Ray::transform_luts[int(Ray::eViewTransform::AgX_Punchy)];
    } else if (name == "filmic_very_low_contrast") {
        lut_data = Ray::transform_luts[int(Ray::eViewTransform::Filmic_VeryLowContrast)];
    } else if (name == "filmic_low_contrast") {
        lut_data = Ray::transform_luts[int(Ray::eViewTransform::Filmic_LowContrast)];
    } else if (name == "filmic_med_low_contrast") {
        lut_data = Ray::transform_luts[int(Ray::eViewTransform::Filmic_MediumLowContrast)];
    } else if (name == "filmic_med_contrast" || name == "filmic") {
        lut_data = Ray::transform_luts[int(Ray::eViewTransform::Filmic_MediumContrast)];
    } else if (name == "filmic_med_high_contrast") {
        lut_data = Ray::transform_luts[int(Ray::eViewTransform::Filmic_MediumHighContrast)];
    } else if (name == "filmic_high_contrast") {
        lut_data = Ray::transform_luts[int(Ray::eViewTransform::Filmic_HighContrast)];
    } else if (name == "filmic_very_high_contrast") {
        lut_data = Ray::transform_luts[int(Ray::eViewTransform::Filmic_VeryHighContrast)];
    }
    return Ren::Span<const uint8_t>(reinterpret_cast<const uint8_t *>(lut_data),
                                    4 * Ray::LUT_DIMS * Ray::LUT_DIMS * Ray::LUT_DIMS);
}
} // namespace DrawTestInternal

#include <Ren/Utils.h>

namespace SceneManagerInternal {
int WriteImage(const uint8_t *out_data, int w, int h, int channels, bool flip_y, bool is_rgbm, const char *name);
}

DrawTest::DrawTest(Viewer *viewer) : BaseState(viewer) {}

void DrawTest::Enter() {
    using namespace DrawTestInternal;

    BaseState::Enter();

    cmdline_ui_->RegisterCommand("r_printCam", [this](Ren::Span<const Eng::CmdlineUI::ArgData> args) -> bool {
        log_->Info("View Pos: { %f, %f, %f }", view_origin_[0], view_origin_[1], view_origin_[2]);
        log_->Info("View Dir: { %f, %f, %f }", view_dir_[0], view_dir_[1], view_dir_[2]);
        return true;
    });

    log_->Info("DrawTest: Loading scene!");
    BaseState::LoadScene(viewer_->app_params.scene_name);
}

void DrawTest::OnPreloadScene(Sys::JsObjectP &js_scene) {
    BaseState::OnPreloadScene(js_scene);

#if 0 // texture compression test
    std::ifstream src_stream("assets/textures/lenna.png",
                             std::ios::binary | std::ios::ate);
    assert(src_stream);
    auto src_size = size_t(src_stream.tellg());
    src_stream.seekg(0, std::ios::beg);

    std::unique_ptr<uint8_t[]> src_buf(new uint8_t[src_size]);
    src_stream.read((char *)&src_buf[0], src_size);

    int width, height, channels;
    unsigned char *image_data = stbi_load_from_memory(
        &src_buf[0], int(src_size), &width, &height, &channels, 0);

#ifdef NDEBUG
    const int RepeatCount = 1000;
#else
    const int RepeatCount = 1;
#endif

#if 0 // test DXT1/DXT5
    int dxt_size_total;
    if (channels == 3) {
        dxt_size_total = Ren::GetRequiredMemory_DXT1(width, height);
    } else {
        dxt_size_total = Ren::GetRequiredMemory_DXT5(width, height);
    }
    std::unique_ptr<uint8_t[]> img_dst(new uint8_t[dxt_size_total]);

    const uint64_t t1 = Sys::GetTimeUs();

    if (channels == 3) {
        for (int i = 0; i < RepeatCount; i++) {
            Ren::CompressImage_DXT1<3 /* Channels */>(image_data, width, height,
                                                      img_dst.get());
        }
    } else {
        for (int i = 0; i < RepeatCount; i++) {
            Ren::CompressImage_DXT5(image_data, width, height, img_dst.get());
        }
    }
#else // test YCoCg-DXT5
    assert(channels == 3);
    std::unique_ptr<uint8_t[]> image_data_YCoCg =
        Ren::ConvertRGB_to_CoCgxY(image_data, width, height);
    channels = 4;

    // SceneManagerInternal::WriteImage(image_data_YCoCg.get(), width, height, channels,
    //                                false, false,
    //                                "assets/textures/wall_picture_YCoCg.png");

    const int dxt_size_total = Ren::GetRequiredMemory_DXT5(width, height);
    std::unique_ptr<uint8_t[]> img_dst(new uint8_t[dxt_size_total]);

    const uint64_t t1 = Sys::GetTimeUs();

    for (int i = 0; i < RepeatCount; i++) {
        Ren::CompressImage_DXT5<true /* Is_YCoCg */>(image_data_YCoCg.get(), width,
                                                     height, img_dst.get());
    }
#endif
    const uint64_t t2 = Sys::GetTimeUs();

    const double elapsed_ms = double(t2 - t1) / (double(RepeatCount) * 1000.0);
    const double elapsed_s = elapsed_ms / 1000.0;

    log_->Info("Compressed in %f ms", elapsed_ms);
    log_->Info("Speed is %f Mpixels per second",
               double(width * height) / (1000000.0 * elapsed_s));

    free(image_data);

#ifdef NDEBUG
    system("pause");
#endif

    //
    // Write out file
    //

    Ren::DDSHeader header = {};
    header.dwMagic = (unsigned('D') << 0u) | (unsigned('D') << 8u) |
                     (unsigned('S') << 16u) | (unsigned(' ') << 24u);
    header.dwSize = 124;
    header.dwFlags = unsigned(DDSD_CAPS) | unsigned(DDSD_HEIGHT) | unsigned(DDSD_WIDTH) |
                     unsigned(DDSD_PIXELFORMAT) | unsigned(DDSD_LINEARSIZE) |
                     unsigned(DDSD_MIPMAPCOUNT);
    header.dwWidth = width;
    header.dwHeight = height;
    header.dwPitchOrLinearSize = dxt_size_total;
    header.dwMipMapCount = 1;
    header.sPixelFormat.dwSize = 32;
    header.sPixelFormat.dwFlags = DDPF_FOURCC;

    if (channels == 3) {
        header.sPixelFormat.dwFourCC = (unsigned('D') << 0u) | (unsigned('X') << 8u) |
                                       (unsigned('T') << 16u) | (unsigned('1') << 24u);
    } else {
        header.sPixelFormat.dwFourCC = (unsigned('D') << 0u) | (unsigned('X') << 8u) |
                                       (unsigned('T') << 16u) | (unsigned('5') << 24u);
    }

    header.sCaps.dwCaps1 = unsigned(DDSCAPS_TEXTURE) | unsigned(DDSCAPS_MIPMAP);

    std::ofstream out_stream("assets_pc/textures/wall_picture_YCoCg.dds",
                             std::ios::binary);
    out_stream.write((char *)&header, sizeof(header));
    out_stream.write((char *)img_dst.get(), dxt_size_total);
#endif

#if 0
    JsArray &js_objects = js_scene.at("objects").as_arr();
    if (js_objects.elements.size() < 2) return;

    JsObject 
        js_leaf_tree = js_objects.elements.begin()->as_obj(),
        js_palm_tree = (++js_objects.elements.begin())->as_obj();
    if (!js_leaf_tree.Has("name") || !js_palm_tree.Has("name")) return;

    if (js_leaf_tree.at("name").as_str().val != "leaf_tree" ||
        js_palm_tree.at("name").as_str().val != "palm_tree") return;
    
    for (int j = -9; j < 10; j++) {
        for (int i = -9; i < 10; i++) {
            if (j == 0 && i == 0) continue;

            js_leaf_tree.at("name").as_str().val = "leaf " + std::to_string(j) + ":" + std::to_string(i);
            js_palm_tree.at("name").as_str().val = "palm " + std::to_string(j) + ":" + std::to_string(i);

            {   // set leaf tree position
                JsObject &js_leaf_tree_tr = js_leaf_tree.at("transform").as_obj();
                JsArray &js_leaf_tree_pos = js_leaf_tree_tr.at("pos").as_arr();
                JsArray &js_leaf_tree_rot = js_leaf_tree_tr.at("rot").as_arr();

                JsNumber &js_leaf_posx = js_leaf_tree_pos.elements.begin()->as_num();
                JsNumber &js_leaf_posz = (++(++js_leaf_tree_pos.elements.begin()))->as_num();

                js_leaf_posx.val = double(i * 10);
                js_leaf_posz.val = double(j * 10);

                JsNumber &js_leaf_roty = (++js_leaf_tree_rot.elements.begin())->as_num();
                js_leaf_roty.val = double(i) * 43758.5453;
            }

            {   // set palm tree position
                JsObject &js_palm_tree_tr = js_palm_tree.at("transform").as_obj();
                JsArray &js_palm_tree_pos = js_palm_tree_tr.at("pos").as_arr();
                JsArray &js_palm_tree_rot = js_palm_tree_tr.at("rot").as_arr();

                JsNumber &js_palm_posx = js_palm_tree_pos.elements.begin()->as_num();
                JsNumber &js_palm_posz = (++(++js_palm_tree_pos.elements.begin()))->as_num();

                js_palm_posx.val = double(i * 10) + 5.0;
                js_palm_posz.val = double(j * 10);

                JsNumber &js_palm_roty = (++js_palm_tree_rot.elements.begin())->as_num();
                js_palm_roty.val = double(i) * 12.9898;
            }

            js_objects.elements.push_back(js_leaf_tree);
            js_objects.elements.push_back(js_palm_tree);
        }
    }
#endif

#if 0
    JsArrayP &js_objects = js_scene.at("objects").as_arr();

    JsArrayP js_new_objects(scene_manager_->mp_alloc());

    const int CountPerDim = 4;
    for (int z = -CountPerDim; z <= CountPerDim; ++z) {
        for (int y = -CountPerDim; y <= 0; ++y) {
            for (int x = -CountPerDim; x <= CountPerDim; ++x) {
                if (x == 0 && y == 0 && z == 0) {
                    continue;
                }

                for (size_t j = 0; j < js_objects.elements.size(); ++j) {
                    JsObjectP &js_obj_orig = js_objects[j].as_obj();
                    if (js_obj_orig.Has("probe")) {
                        continue;
                    }

                    JsObjectP js_obj_copy = js_obj_orig;

                    { // set new position
                        JsObjectP &js_tr = js_obj_copy.at("transform").as_obj();
                        JsArrayP &js_pos = js_tr.at("pos").as_arr();

                        JsNumber &js_posx = js_pos.elements.at(0).as_num();
                        JsNumber &js_posy = js_pos.elements.at(1).as_num();
                        JsNumber &js_posz = js_pos.elements.at(2).as_num();

                        js_posx.val += double(x * 15);
                        js_posy.val += double(y * 5);
                        js_posz.val += double(z * 15);
                    }

                    js_new_objects.Push(std::move(js_obj_copy));
                }
            }
        }
    }

    for (auto &js_obj : js_new_objects.elements) {
        js_objects.Push(std::move(js_obj));
    }
#endif

    std::fill_n(wolf_indices_, 32, 0xffffffff);
    std::fill_n(scooter_indices_, 16, 0xffffffff);
    std::fill_n(sophia_indices_, 2, 0xffffffff);
    std::fill_n(eric_indices_, 2, 0xffffffff);
    zenith_index_ = 0xffffffff;
    palm_index_ = 0xffffffff;
    leaf_tree_index_ = 0xffffffff;
}

void DrawTest::OnPostloadScene(Sys::JsObjectP &js_scene) {
    using namespace DrawTestInternal;

    BaseState::OnPostloadScene(js_scene);

    cam_follow_path_.clear();
    cam_follow_point_ = 0;
    cam_follow_param_ = 0;

    if (js_scene.Has("camera")) {
        const Sys::JsObjectP &js_cam = js_scene.at("camera").as_obj();
        if (js_cam.Has("view_origin")) {
            const Sys::JsArrayP &js_orig = js_cam.at("view_origin").as_arr();
            initial_view_pos_[0] = float(js_orig.at(0).as_num().val);
            initial_view_pos_[1] = float(js_orig.at(1).as_num().val);
            initial_view_pos_[2] = float(js_orig.at(2).as_num().val);
        }

        if (js_cam.Has("view_dir")) {
            const Sys::JsArrayP &js_dir = js_cam.at("view_dir").as_arr();
            initial_view_dir_[0] = float(js_dir.at(0).as_num().val);
            initial_view_dir_[1] = float(js_dir.at(1).as_num().val);
            initial_view_dir_[2] = float(js_dir.at(2).as_num().val);
        } else if (js_cam.Has("view_rot")) {
            const Sys::JsArrayP &js_view_rot = js_cam.at("view_rot").as_arr();

            auto rx = float(js_view_rot.at(0).as_num().val);
            auto ry = float(js_view_rot.at(1).as_num().val);
            auto rz = float(js_view_rot.at(2).as_num().val);

            rx *= Ren::Pi<float>() / 180;
            ry *= Ren::Pi<float>() / 180;
            rz *= Ren::Pi<float>() / 180;

            Ren::Mat4f transform;
            transform = Rotate(transform, float(rz), Ren::Vec3f{0, 0, 1});
            transform = Rotate(transform, float(rx), Ren::Vec3f{1, 0, 0});
            transform = Rotate(transform, float(ry), Ren::Vec3f{0, 1, 0});

            auto view_vec = Ren::Vec4f{0, -1, 0, 0};
            view_vec = transform * view_vec;

            memcpy(&initial_view_dir_[0], ValuePtr(view_vec), 3 * sizeof(float));

            // Ren::Vec4f view_up_vec = {0, 0, -1, 0};
            // view_up_vec = transform * view_up_vec;

            // memcpy(&view_up[0], ValuePtr(view_up_vec), 3 * sizeof(float));
        }

        if (js_cam.Has("fwd_speed")) {
            const Sys::JsNumber &js_fwd_speed = js_cam.at("fwd_speed").as_num();
            max_fwd_speed_ = float(js_fwd_speed.val);
        }

        if (js_cam.Has("fov")) {
            const Sys::JsNumber &js_fov = js_cam.at("fov").as_num();
            view_fov_ = float(js_fov.val);
        }

        if (js_cam.Has("gamma")) {
            const Sys::JsNumber &js_gamma = js_cam.at("gamma").as_num();
            gamma_ = float(js_gamma.val);
        }

        if (js_cam.Has("min_exposure")) {
            const Sys::JsNumber &js_min_exposure = js_cam.at("min_exposure").as_num();
            min_exposure_ = float(js_min_exposure.val);
        }

        if (js_cam.Has("max_exposure")) {
            const Sys::JsNumber &js_max_exposure = js_cam.at("max_exposure").as_num();
            max_exposure_ = float(js_max_exposure.val);
        }

        if (js_cam.Has("shift")) {
            const Sys::JsArrayP &js_shift = js_cam.at("shift").as_arr();
            view_sensor_shift_[0] = float(js_shift[0].as_num().val);
            view_sensor_shift_[1] = float(js_shift[1].as_num().val);
        }

        if (js_cam.Has("filter")) {
            const Sys::JsStringP &js_filter = js_cam.at("filter").as_str();
            if (js_filter.val == "box") {
                renderer_->settings.pixel_filter = Eng::ePixelFilter::Box;
            } else if (js_filter.val == "gaussian") {
                renderer_->settings.pixel_filter = Eng::ePixelFilter::Gaussian;
            } else if (js_filter.val == "blackman-harris") {
                renderer_->settings.pixel_filter = Eng::ePixelFilter::BlackmanHarris;
                renderer_->settings.pixel_filter_width = 1.5f;
                if (js_cam.Has("filter_width")) {
                    const Sys::JsNumber &js_filter_width = js_cam.at("filter_width").as_num();
                    renderer_->settings.pixel_filter_width = float(js_filter_width.val);
                }
            }
        }

        if (js_cam.Has("view_transform")) {
            const Sys::JsStringP &js_view_transform = js_cam.at("view_transform").as_str();
            if (js_view_transform.val == "standard") {
                renderer_->settings.tonemap_mode = Eng::eTonemapMode::Standard;
            } else if (js_view_transform.val == "off") {
                renderer_->settings.tonemap_mode = Eng::eTonemapMode::Off;
            } else {
                renderer_->settings.tonemap_mode = Eng::eTonemapMode::LUT;
                renderer_->SetTonemapLUT(Ray::LUT_DIMS, Ren::eTexFormat::RawRGB10_A2,
                                         RayLUTByName(js_view_transform.val));
            }
        }

        if (js_cam.Has("follow_path")) {
            const Sys::JsArrayP &js_points = js_cam.at("follow_path").as_arr();
            for (const Sys::JsElementP &el : js_points.elements) {
                const Sys::JsArrayP &js_point = el.as_arr();

                const Sys::JsNumber &x = js_point.at(0).as_num(), &y = js_point.at(1).as_num(),
                                    &z = js_point.at(2).as_num();

                cam_follow_path_.emplace_back(float(x.val), float(y.val), float(z.val));
            }
        }
    }

    if (viewer_->app_params.exposure.has_value()) {
        min_exposure_ = max_exposure_ = viewer_->app_params.exposure.value();
    }

    next_view_origin_ = prev_view_origin_ = initial_view_pos_;
    view_dir_ = initial_view_dir_;

    Eng::SceneData &scene = scene_manager_->scene_data();

    {
        char wolf_name[] = "wolf00";

        for (int j = 0; j < 4; j++) {
            for (int i = 0; i < 8; i++) {
                const int index = j * 8 + i;

                wolf_name[4] = char('0' + j);
                wolf_name[5] = char('0' + i);

                const uint32_t wolf_index = scene_manager_->FindObject(wolf_name);
                wolf_indices_[index] = wolf_index;

                if (wolf_index != 0xffffffff) {
                    Eng::SceneObject *wolf = scene_manager_->GetObject(wolf_index);

                    const uint32_t mask = Eng::CompDrawableBit | Eng::CompAnimStateBit;
                    if ((wolf->comp_mask & mask) == mask) {
                        auto *as = (Eng::AnimState *)scene.comp_store[Eng::CompAnimState]->Get(
                            wolf->components[Eng::CompAnimState]);
                        as->anim_time_s = 4 * (float(rand()) / float(RAND_MAX)); // NOLINT
                    }
                }
            }
        }
    }

    {
        char scooter_name[] = "scooter_00";

        for (int j = 0; j < 2; j++) {
            for (int i = 0; i < 8; i++) {
                scooter_name[8] = char('0' + j);
                scooter_name[9] = char('0' + i);

                const uint32_t scooter_index = scene_manager_->FindObject(scooter_name);
                scooter_indices_[j * 8 + i] = scooter_index;
            }
        }
    }

    {
        char sophia_name[] = "sophia_00";

        for (int i = 0; i < 2; i++) {
            sophia_name[8] = char('0' + i);

            const uint32_t sophia_index = scene_manager_->FindObject(sophia_name);
            sophia_indices_[i] = sophia_index;
        }
    }

    {
        char eric_name[] = "eric_00";

        for (int i = 0; i < 2; i++) {
            eric_name[6] = char('0' + i);

            const uint32_t eric_index = scene_manager_->FindObject(eric_name);
            eric_indices_[i] = eric_index;
        }
    }

    zenith_index_ = scene_manager_->FindObject("zenith");
    palm_index_ = scene_manager_->FindObject("palm");
    leaf_tree_index_ = scene_manager_->FindObject("leaf_tree");
}

void DrawTest::Exit() { BaseState::Exit(); }

void DrawTest::Draw() { BaseState::Draw(); }

void DrawTest::DrawUI(Gui::Renderer *r, Gui::BaseElement *root) { BaseState::DrawUI(r, root); }

void DrawTest::UpdateFixed(const uint64_t dt_us) {
    using namespace DrawTestInternal;

    OPTICK_EVENT("DrawTest::UpdateFixed");
    BaseState::UpdateFixed(dt_us);

    const Ren::Vec3f up = Ren::Vec3f{0, 1, 0}, side = Normalize(Cross(view_dir_, up));

    prev_view_origin_ = next_view_origin_;

    if (cam_follow_path_.size() < 3) {
        const float fwd_speed =
                        std::max(std::min(fwd_press_speed_ + fwd_touch_speed_, max_fwd_speed_), -max_fwd_speed_),
                    side_speed =
                        std::max(std::min(side_press_speed_ + side_touch_speed_, max_fwd_speed_), -max_fwd_speed_);

        next_view_origin_ += view_dir_ * fwd_speed;
        next_view_origin_ += side * side_speed;
    } else {
        int next_point = (cam_follow_point_ + 1) % int(cam_follow_path_.size());

        { // update param
            const Ren::Vec3f &p1 = cam_follow_path_[cam_follow_point_], &p2 = cam_follow_path_[next_point];

            cam_follow_param_ += 0.000005f * dt_us / Distance(p1, p2);
            while (cam_follow_param_ > 1) {
                cam_follow_point_ = (cam_follow_point_ + 1) % int(cam_follow_path_.size());
                cam_follow_param_ -= 1;
            }
        }

        next_point = (cam_follow_point_ + 1) % int(cam_follow_path_.size());

        const Ren::Vec3f &p1 = cam_follow_path_[cam_follow_point_], &p2 = cam_follow_path_[next_point];

        next_view_origin_ = 0.95f * next_view_origin_ + 0.05f * Mix(p1, p2, cam_follow_param_);
        view_dir_ = 0.9f * view_dir_ + 0.1f * Normalize(p2 - next_view_origin_);
        view_dir_ = Normalize(view_dir_);
    }

    Eng::SceneData &scene = scene_manager_->scene_data();

    if (scooter_indices_[0] != 0xffffffff) {
        const auto rot_center = Ren::Vec3f{44.5799f, 0.15f, 24.7763f};

        scooters_angle_ += 0.0000005f * dt_us;

        for (int i = 0; i < 16; i++) {
            if (scooter_indices_[i] == 0xffffffff) {
                break;
            }

            Eng::SceneObject *scooter = scene_manager_->GetObject(scooter_indices_[i]);

            const uint32_t mask = Eng::CompTransform;
            if ((scooter->comp_mask & mask) == mask) {
                auto *tr = (Eng::Transform *)scene.comp_store[Eng::CompTransform]->Get(
                    scooter->components[Eng::CompTransform]);

                tr->world_from_object_prev = tr->world_from_object;
                tr->world_from_object = Ren::Mat4f{1};
                tr->world_from_object = Translate(tr->world_from_object, rot_center);

                if (i < 8) {
                    // inner circle
                    tr->world_from_object =
                        Rotate(tr->world_from_object, scooters_angle_ + float(i) * 0.25f * Ren::Pi<float>(),
                               Ren::Vec3f{0, 1, 0});
                    tr->world_from_object = Translate(tr->world_from_object, Ren::Vec3f{6.5f, 0.0f, 0.0f});
                } else {
                    // outer circle
                    tr->world_from_object =
                        Rotate(tr->world_from_object, -scooters_angle_ + float(i - 8) * 0.25f * Ren::Pi<float>(),
                               Ren::Vec3f{0, 1, 0});
                    tr->world_from_object = Translate(tr->world_from_object, Ren::Vec3f{-8.5f, 0.0f, 0.0f});
                }
            }
        }

        scene_manager_->InvalidateObjects(scooter_indices_, Eng::CompTransformBit);
    }

    wind_update_time_ += dt_us;

    if (wind_update_time_ > 400000) {
        wind_update_time_ = 0;
        // update wind vector
        // const float next_wind_strength = 0.15f * random_->GetNormalizedFloat();
        // wind_vector_goal_ = next_wind_strength * random_->GetUnitVec3();
    }

    scene.env.wind_vec = 0.99f * scene.env.wind_vec + 0.01f * wind_vector_goal_;
    scene.env.wind_turbulence = 2 * Length(scene.env.wind_vec);
}

void DrawTest::UpdateAnim(const uint64_t dt_us) {
    using namespace Ren;

    OPTICK_EVENT();
    const float delta_time_s = dt_us * 0.000001f;

    // test test test
    TestUpdateAnims(delta_time_s);

    const Eng::FrameInfo &fr = fr_info_;
    const Vec3f smooth_view_origin = Mix(prev_view_origin_, next_view_origin_, float(fr.time_fract));

    // Invalidate view if camera has moved
    invalidate_view_ |= Distance(view_origin_, smooth_view_origin) > 0.00001f;
    view_origin_ = smooth_view_origin;

    // Make sure we use latest camera rotation (reduce lag)
    Vec3f view_dir = view_dir_;
    for (const Eng::input_event_t &evt : viewer_->input_manager()->peek_events()) {
        switch (evt.type) {
        case Eng::eInputEvent::P1Move:
            if (view_pointer_ == 1) {
                auto up = Vec3f{0, 1, 0};
                Vec3f side = Normalize(Cross(view_dir, up));
                up = Cross(side, view_dir);

                Mat4f rot;
                rot = Rotate(rot, -0.005f * evt.move[0], up);
                rot = Rotate(rot, 0.005f * evt.move[1], side);

                auto rot_m3 = Mat3f(rot);
                view_dir = rot_m3 * view_dir;

                invalidate_view_ = true;
            }
            break;
        case Eng::eInputEvent::P2Move:
            if (view_pointer_ == 2) {
                auto up = Vec3f{0, 1, 0};
                Vec3f side = Normalize(Cross(view_dir, up));
                up = Cross(side, view_dir);

                Mat4f rot;
                rot = Rotate(rot, 0.01f * evt.move[0], up);
                rot = Rotate(rot, -0.01f * evt.move[1], side);

                auto rot_m3 = Mat3f(rot);
                view_dir = rot_m3 * view_dir;

                invalidate_view_ = true;
            }
            break;
        default:
            break;
        }
    }

    const bool view_locked = !viewer_->app_params.ref_name.empty();
    if (view_locked) {
        view_dir = view_dir_;
    }

    // Update camera
    scene_manager_->SetupView(view_origin_, (view_origin_ + view_dir), Vec3f{0, 1, 0}, view_fov_, view_sensor_shift_,
                              gamma_, min_exposure_, max_exposure_);

    BaseState::UpdateAnim(dt_us);
}

bool DrawTest::HandleInput(const Eng::input_event_t &evt, const std::vector<bool> &keys_state) {
    using namespace Ren;
    using namespace DrawTestInternal;

    const bool handled = BaseState::HandleInput(evt, keys_state);
    if (handled) {
        return true;
    }

    // pt switch for touch controls
    if (evt.type == Eng::eInputEvent::P1Down || evt.type == Eng::eInputEvent::P2Down) {
        if (evt.point[0] > float(ren_ctx_->w()) * 0.9f && evt.point[1] < float(ren_ctx_->h()) * 0.1f) {
            const uint64_t new_time = Sys::GetTimeMs();
            if (new_time - click_time_ < 400) {
                use_pt_ = !use_pt_;
                if (use_pt_) {
                    // scene_manager_->InitScene_PT();
                    invalidate_view_ = true;
                }

                click_time_ = 0;
            } else {
                click_time_ = new_time;
            }
        }
    }

    const Ren::Vec3f view_dir_before = view_dir_;

    switch (evt.type) {
    case Eng::eInputEvent::P1Down:
        if (evt.point[0] < (float(ren_ctx_->w()) / 3) && move_pointer_ == 0) {
            move_pointer_ = 1;
        } else if (view_pointer_ == 0) {
            view_pointer_ = 1;
        }
        break;
    case Eng::eInputEvent::P2Down:
        if (evt.point[0] < (float(ren_ctx_->w()) / 3) && move_pointer_ == 0) {
            move_pointer_ = 2;
        } else if ((keys_state[Eng::eKey::LeftShift] || keys_state[Eng::eKey::RightShift]) && sun_pointer_ == 0) {
            sun_pointer_ = 2;
        } else if (view_pointer_ == 0) {
            view_pointer_ = 2;
        }
        break;
    case Eng::eInputEvent::P1Up:
        if (move_pointer_ == 1) {
            move_pointer_ = 0;
            fwd_touch_speed_ = 0;
            side_touch_speed_ = 0;
        } else if (view_pointer_ == 1) {
            view_pointer_ = 0;
        }
        break;
    case Eng::eInputEvent::P2Up:
        if (move_pointer_ == 2) {
            move_pointer_ = 0;
            fwd_touch_speed_ = 0;
            side_touch_speed_ = 0;
        } else if (sun_pointer_ == 2) {
            sun_pointer_ = 0;
        } else if (view_pointer_ == 2) {
            view_pointer_ = 0;
        }
        break;
    case Eng::eInputEvent::P1Move:
        if (move_pointer_ == 1) {
            side_touch_speed_ += evt.move[0] * 0.002f;
            side_touch_speed_ = std::max(std::min(side_touch_speed_, max_fwd_speed_), -max_fwd_speed_);

            fwd_touch_speed_ += evt.move[1] * 0.002f;
            fwd_touch_speed_ = std::max(std::min(fwd_touch_speed_, max_fwd_speed_), -max_fwd_speed_);
        } else if (view_pointer_ == 1) {
            auto up = Vec3f{0, 1, 0};
            Vec3f side = Normalize(Cross(view_dir_, up));
            up = Cross(side, view_dir_);

            Mat4f rot;
            rot = Rotate(rot, -0.005f * evt.move[0], up);
            rot = Rotate(rot, 0.005f * evt.move[1], side);

            auto rot_m3 = Mat3f(rot);
            view_dir_ = rot_m3 * view_dir_;

            invalidate_view_ = true;
        } else if (sun_pointer_ == 2) {
            auto up = Vec3f{0, 1, 0};
            Vec3f side = Normalize(Cross(view_dir_, up));
            up = Cross(side, view_dir_);

            Mat4f rot;
            rot = Rotate(rot, -0.005f * evt.move[0], up);
            rot = Rotate(rot, 0.005f * evt.move[1], side);

            auto rot_m3 = Mat3f(rot);
            sun_dir_ = Normalize(rot_m3 * sun_dir_);
        }
        break;
    case Eng::eInputEvent::P2Move:
        if (move_pointer_ == 2) {
            side_touch_speed_ += evt.move[0] * 0.002f;
            side_touch_speed_ = std::max(std::min(side_touch_speed_, max_fwd_speed_), -max_fwd_speed_);

            fwd_touch_speed_ += evt.move[1] * 0.002f;
            fwd_touch_speed_ = std::max(std::min(fwd_touch_speed_, max_fwd_speed_), -max_fwd_speed_);
        } else if (view_pointer_ == 2) {
            auto up = Vec3f{0, 1, 0};
            Vec3f side = Normalize(Cross(view_dir_, up));
            up = Cross(side, view_dir_);

            Mat4f rot;
            rot = Rotate(rot, 0.01f * evt.move[0], up);
            rot = Rotate(rot, -0.01f * evt.move[1], side);

            auto rot_m3 = Mat3f(rot);
            view_dir_ = rot_m3 * view_dir_;

            invalidate_view_ = true;
        }
        break;
    case Eng::eInputEvent::KeyDown: {
        if ((evt.key_code == Eng::eKey::Up && !cmdline_ui_->enabled) ||
            (evt.key_code == Eng::eKey::W && (!cmdline_ui_->enabled || view_pointer_))) {
            fwd_press_speed_ = max_fwd_speed_;
        } else if ((evt.key_code == Eng::eKey::Down && !cmdline_ui_->enabled) ||
                   (evt.key_code == Eng::eKey::S && (!cmdline_ui_->enabled || view_pointer_))) {
            fwd_press_speed_ = -max_fwd_speed_;
        } else if (evt.key_code == Eng::eKey::Left ||
                   (evt.key_code == Eng::eKey::A && (!cmdline_ui_->enabled || view_pointer_))) {
            side_press_speed_ = -max_fwd_speed_;
        } else if (evt.key_code == Eng::eKey::Right ||
                   (evt.key_code == Eng::eKey::D && (!cmdline_ui_->enabled || view_pointer_))) {
            side_press_speed_ = max_fwd_speed_;
            //} else if (evt.key_code == KeySpace) {
            //    wind_vector_goal_ = Ren::Vec3f{1, 0, 0};
        }
    } break;
    case Eng::eInputEvent::KeyUp: {
        if (!cmdline_ui_->enabled || view_pointer_) {
            if (evt.key_code == Eng::eKey::Up || evt.key_code == Eng::eKey::W || evt.key_code == Eng::eKey::Down ||
                evt.key_code == Eng::eKey::S) {
                fwd_press_speed_ = 0;
            } else if (evt.key_code == Eng::eKey::Left || evt.key_code == Eng::eKey::A ||
                       evt.key_code == Eng::eKey::Right || evt.key_code == Eng::eKey::D) {
                side_press_speed_ = 0;
            }
        }
    } break;
    default:
        break;
    }

    const bool view_locked = !viewer_->app_params.ref_name.empty();
    if (view_locked) {
        view_dir_ = view_dir_before;
        fwd_touch_speed_ = side_touch_speed_ = 0;
        fwd_press_speed_ = side_press_speed_ = 0;
    }

    return true;
}

void DrawTest::SaveScene(Sys::JsObjectP &js_scene) {
    BaseState::SaveScene(js_scene);

    const auto &alloc = js_scene.elements.get_allocator();

    { // write camera
        Sys::JsObjectP js_camera(alloc);

        { // write view origin
            Sys::JsArrayP js_view_origin(alloc);

            js_view_origin.Push(Sys::JsNumber{initial_view_pos_[0]});
            js_view_origin.Push(Sys::JsNumber{initial_view_pos_[1]});
            js_view_origin.Push(Sys::JsNumber{initial_view_pos_[2]});

            js_camera.Insert("view_origin", std::move(js_view_origin));
        }

        { // write view direction
            Sys::JsArrayP js_view_dir(alloc);

            js_view_dir.Push(Sys::JsNumber{initial_view_dir_[0]});
            js_view_dir.Push(Sys::JsNumber{initial_view_dir_[1]});
            js_view_dir.Push(Sys::JsNumber{initial_view_dir_[2]});

            js_camera.Insert("view_dir", std::move(js_view_dir));
        }

        { // write forward speed
            js_camera.Insert("fwd_speed", Sys::JsNumber{max_fwd_speed_});
        }

        { // write fov
            js_camera.Insert("fov", Sys::JsNumber{view_fov_});
        }

        { // write max exposure
            js_camera.Insert("max_exposure", Sys::JsNumber{max_exposure_});
        }

        js_scene.Insert("camera", std::move(js_camera));
    }
}

void DrawTest::TestUpdateAnims(const float delta_time_s) {
    Eng::SceneData &scene = scene_manager_->scene_data();

    if (wolf_indices_[0] != 0xffffffff) {
        for (uint32_t wolf_index : wolf_indices_) {
            if (wolf_index == 0xffffffff) {
                break;
            }

            Eng::SceneObject *wolf = scene_manager_->GetObject(wolf_index);

            const uint32_t mask = Eng::CompTransformBit | Eng::CompDrawableBit | Eng::CompAnimStateBit;
            if ((wolf->comp_mask & mask) == mask) {
                auto *tr =
                    (Eng::Transform *)scene.comp_store[Eng::CompTransform]->Get(wolf->components[Eng::CompTransform]);
                auto *dr =
                    (Eng::Drawable *)scene.comp_store[Eng::CompDrawable]->Get(wolf->components[Eng::CompDrawable]);
                auto *as =
                    (Eng::AnimState *)scene.comp_store[Eng::CompAnimState]->Get(wolf->components[Eng::CompAnimState]);

                // keep previous palette for velocity calculation
                std::swap(as->matr_palette_curr, as->matr_palette_prev);
                as->anim_time_s += delta_time_s;

                Ren::Mesh *mesh = dr->mesh.get();
                Ren::Skeleton *skel = mesh->skel();

                skel->UpdateAnim(0, as->anim_time_s);
                skel->ApplyAnim(0);
                skel->UpdateBones(&as->matr_palette_curr[0]);

                {
                    Ren::Mat4f xform;
                    // xform = Rotate(xform, 0.5f * delta_time_s, Ren::Vec3f{0, 1, 0});
                    xform = Translate(xform, delta_time_s * Ren::Vec3f{0.1f, 0.0f, 0.0f});

                    tr->world_from_object_prev = tr->world_from_object;
                    tr->world_from_object = xform * tr->world_from_object;
                    scene_manager_->InvalidateObjects({&wolf_index, 1}, Eng::CompTransformBit);
                }
            }
        }
    }

    if (scooter_indices_[0] != 0xffffffff) {
        for (int i = 0; i < 16; i++) {
            if (scooter_indices_[i] == 0xffffffff) {
                break;
            }

            Eng::SceneObject *scooter = scene_manager_->GetObject(scooter_indices_[i]);

            const uint32_t mask = Eng::CompDrawableBit | Eng::CompAnimStateBit;
            if ((scooter->comp_mask & mask) == mask) {
                auto *dr =
                    (Eng::Drawable *)scene.comp_store[Eng::CompDrawable]->Get(scooter->components[Eng::CompDrawable]);
                auto *as = (Eng::AnimState *)scene.comp_store[Eng::CompAnimState]->Get(
                    scooter->components[Eng::CompAnimState]);

                // keep previous palette for velocity calculation
                std::swap(as->matr_palette_curr, as->matr_palette_prev);
                as->anim_time_s += delta_time_s;

                Ren::Mesh *mesh = dr->mesh.get();
                Ren::Skeleton *skel = mesh->skel();

                const int anim_index = (i < 8) ? 2 : 3;

                skel->UpdateAnim(anim_index, as->anim_time_s);
                skel->ApplyAnim(anim_index);
                skel->UpdateBones(&as->matr_palette_curr[0]);
            }
        }
    }

    for (const uint32_t ndx : sophia_indices_) {
        if (ndx == 0xffffffff) {
            break;
        }

        Eng::SceneObject *sophia = scene_manager_->GetObject(ndx);

        const uint32_t mask = Eng::CompDrawableBit | Eng::CompAnimStateBit;
        if ((sophia->comp_mask & mask) == mask) {
            auto *dr = (Eng::Drawable *)scene.comp_store[Eng::CompDrawable]->Get(sophia->components[Eng::CompDrawable]);
            auto *as =
                (Eng::AnimState *)scene.comp_store[Eng::CompAnimState]->Get(sophia->components[Eng::CompAnimState]);

            // keep previous palette for velocity calculation
            std::swap(as->matr_palette_curr, as->matr_palette_prev);
            as->anim_time_s += delta_time_s;

            Ren::Mesh *mesh = dr->mesh.get();
            Ren::Skeleton *skel = mesh->skel();

            const int anim_index = 0;

            skel->UpdateAnim(anim_index, as->anim_time_s);
            skel->ApplyAnim(anim_index);
            skel->UpdateBones(&as->matr_palette_curr[0]);

            sophia->change_mask |= Eng::CompDrawableBit;
        }
    }

    for (const uint32_t ndx : eric_indices_) {
        if (ndx == 0xffffffff) {
            break;
        }

        Eng::SceneObject *eric = scene_manager_->GetObject(ndx);

        const uint32_t mask = Eng::CompDrawableBit | Eng::CompAnimStateBit;
        if ((eric->comp_mask & mask) == mask) {
            auto *dr = (Eng::Drawable *)scene.comp_store[Eng::CompDrawable]->Get(eric->components[Eng::CompDrawable]);
            auto *as =
                (Eng::AnimState *)scene.comp_store[Eng::CompAnimState]->Get(eric->components[Eng::CompAnimState]);

            // keep previous palette for velocity calculation
            std::swap(as->matr_palette_curr, as->matr_palette_prev);
            as->anim_time_s += delta_time_s;

            Ren::Mesh *mesh = dr->mesh.get();
            Ren::Skeleton *skel = mesh->skel();

            const int anim_index = 0;

            skel->UpdateAnim(anim_index, as->anim_time_s);
            skel->ApplyAnim(anim_index);
            skel->UpdateBones(&as->matr_palette_curr[0]);
        }
    }

    if (zenith_index_ != 0xffffffff) {
        Eng::SceneObject *zenith = scene_manager_->GetObject(zenith_index_);

        const uint32_t mask = Eng::CompDrawableBit | Eng::CompAnimStateBit;
        if ((zenith->comp_mask & mask) == mask) {
            auto *dr = (Eng::Drawable *)scene.comp_store[Eng::CompDrawable]->Get(zenith->components[Eng::CompDrawable]);
            auto *as =
                (Eng::AnimState *)scene.comp_store[Eng::CompAnimState]->Get(zenith->components[Eng::CompAnimState]);

            // keep previous palette for velocity calculation
            std::swap(as->matr_palette_curr, as->matr_palette_prev);
            as->anim_time_s += delta_time_s;

            Ren::Mesh *mesh = dr->mesh.get();
            Ren::Skeleton *skel = mesh->skel();

            const int anim_index = 0;

            skel->UpdateAnim(anim_index, as->anim_time_s);
            skel->ApplyAnim(anim_index);
            skel->UpdateBones(&as->matr_palette_curr[0]);
        }
    }

    if (palm_index_ != 0xffffffff) {
        Eng::SceneObject *palm = scene_manager_->GetObject(palm_index_);

        const uint32_t mask = Eng::CompDrawableBit | Eng::CompAnimStateBit;
        if ((palm->comp_mask & mask) == mask) {
            auto *dr = (Eng::Drawable *)scene.comp_store[Eng::CompDrawable]->Get(palm->components[Eng::CompDrawable]);
            auto *as =
                (Eng::AnimState *)scene.comp_store[Eng::CompAnimState]->Get(palm->components[Eng::CompAnimState]);

            // keep previous palette for velocity calculation
            std::swap(as->matr_palette_curr, as->matr_palette_prev);
            as->anim_time_s += delta_time_s;

            Ren::Mesh *mesh = dr->mesh.get();
            Ren::Skeleton *skel = mesh->skel();

            const int anim_index = 0;

            skel->UpdateAnim(anim_index, as->anim_time_s);
            skel->ApplyAnim(anim_index);
            skel->UpdateBones(&as->matr_palette_curr[0]);
        }
    }

    /*if (leaf_tree_index_ != 0xffffffff) {
        SceneObject *leaf_tree = scene_manager_->GetObject(leaf_tree_index_);

        uint32_t mask = CompDrawableBit | CompTransformBit;
        if ((leaf_tree->comp_mask & mask) == mask) {
            auto *tr = (Transform *)scene.comp_store[CompTransform]->Get(leaf_tree->components[CompTransform]);

            Ren::Mat4f rot_mat;
            rot_mat = Rotate(rot_mat, 0.5f * delta_time_s, Ren::Vec3f{0, 1, 0});
            //rot_mat = Translate(rot_mat, Ren::Vec3f{0.001f, 0.0f, 0.0f});

            tr->world_from_object_prev = tr->world_from_object;
            tr->world_from_object = rot_mat * tr->world_from_object;
            scene_manager_->InvalidateObjects(&leaf_tree_index_, 1, CompTransformBit);
        }
    }*/

    const auto wind_scroll_dir = 128 * Normalize(Ren::Vec2f{scene.env.wind_vec[0], scene.env.wind_vec[2]});
    scene.env.prev_wind_scroll_lf = scene.env.curr_wind_scroll_lf;
    scene.env.prev_wind_scroll_hf = scene.env.curr_wind_scroll_hf;

    scene.env.curr_wind_scroll_lf =
        Fract(scene.env.curr_wind_scroll_lf - (1.0f / 1536.0f) * delta_time_s * wind_scroll_dir);
    scene.env.curr_wind_scroll_hf =
        Fract(scene.env.curr_wind_scroll_hf - (1.0f / 32.0f) * delta_time_s * wind_scroll_dir);
}

#undef TINYEXR_IMPLEMENTATION
#undef TINYEXR_USE_MINIZ
#undef TINYEXR_USE_STB_ZLIB
