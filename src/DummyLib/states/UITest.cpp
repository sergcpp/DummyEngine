#include "UITest.h"

#include <fstream>
#include <memory>

#include <Eng/Log.h>
#include <Eng/ViewerStateManager.h>
#include <Eng/scene/SceneManager.h>
#include <Eng/widgets/CmdlineUI.h>
#include <Gui/Image.h>
#include <Gui/Image9Patch.h>
#include <Gui/Renderer.h>
#include <Gui/Utils.h>
#include <Ren/Context.h>
#include <Sys/AssetFile.h>
#include <Sys/MemBuf.h>
#include <Sys/Time_.h>

#include "../Viewer.h"
#include "../widgets/FontStorage.h"
#include "../widgets/WordPuzzleUI.h"

namespace UITestInternal {
#if defined(__ANDROID__)
const char SCENE_NAME[] = "assets/scenes/"
#else
const char SCENE_NAME[] = "assets_pc/scenes/"
#endif
                          "corridor.json";
} // namespace UITestInternal

UITest::UITest(Viewer *viewer) : BaseState(viewer) {
    dialog_font_ = viewer->font_storage()->FindFont("dialog_font");
    // dialog_font_->set_scale(1.5f);

    word_puzzle_ = std::make_unique<WordPuzzleUI>(*ren_ctx_, Gui::Vec2f{-0.995f, -0.995f}, Gui::Vec2f{1.99f, 1.1f},
                                                  ui_root_, *dialog_font_);
}

UITest::~UITest() = default;

void UITest::Enter() {
    using namespace UITestInternal;

    BaseState::Enter();

    log_->Info("GSUITest: Loading scene!");
    BaseState::LoadScene("scenes/empty.json");

#if defined(__ANDROID__)
    const char *dialog_name = "assets/scenes/test/test_puzzle.json";
#else
    const char *dialog_name = "assets_pc/scenes/test/test_puzzle.json";
#endif
    Sys::JsObject js_script;

    { // Load dialog data from file
        Sys::AssetFile in_scene(dialog_name);
        if (!in_scene) {
            log_->Error("Can not open dialog file %s", dialog_name);
        }

        const size_t scene_size = in_scene.size();

        std::unique_ptr<uint8_t[]> scene_data(new uint8_t[scene_size]);
        in_scene.Read((char *)&scene_data[0], scene_size);

        Sys::MemBuf mem(&scene_data[0], scene_size);
        std::istream in_stream(&mem);

        if (!js_script.Read(in_stream)) {
            throw std::runtime_error("Cannot load dialog!");
        }
    }

    word_puzzle_->Load(js_script);
    word_puzzle_->Restart();
}

void UITest::OnPostloadScene(Sys::JsObjectP &js_scene) {
    using namespace UITestInternal;

    BaseState::OnPostloadScene(js_scene);

    Ren::Vec3d view_origin, view_dir = Ren::Vec3d{0, 0, 1};
    float view_fov = 45, min_exposure = -1000, max_exposure = 1000;

    if (const size_t camera_ndx = js_scene.IndexOf("camera"); camera_ndx < js_scene.Size()) {
        const Sys::JsObjectP &js_cam = js_scene[camera_ndx].second.as_obj();
        if (const size_t view_origin_ndx = js_cam.IndexOf("view_origin"); view_origin_ndx < js_cam.Size()) {
            const Sys::JsArrayP &js_orig = js_cam[view_origin_ndx].second.as_arr();
            view_origin[0] = js_orig.at(0).as_num().val;
            view_origin[1] = js_orig.at(1).as_num().val;
            view_origin[2] = js_orig.at(2).as_num().val;
        }
        if (const size_t view_dir_ndx = js_cam.IndexOf("view_dir"); view_dir_ndx < js_cam.Size()) {
            const Sys::JsArrayP &js_dir = js_cam[view_dir_ndx].second.as_arr();
            view_dir[0] = js_dir.at(0).as_num().val;
            view_dir[1] = js_dir.at(1).as_num().val;
            view_dir[2] = js_dir.at(2).as_num().val;
        }
        if (const size_t fov_ndx = js_cam.IndexOf("fov"); fov_ndx < js_cam.Size()) {
            const Sys::JsNumber &js_fov = js_cam[fov_ndx].second.as_num();
            view_fov = float(js_fov.val);
        }
        if (const size_t min_exposure_ndx = js_cam.IndexOf("min_exposure"); min_exposure_ndx < js_cam.Size()) {
            const Sys::JsNumber &js_min_exposure = js_cam[min_exposure_ndx].second.as_num();
            min_exposure = float(js_min_exposure.val);
        }
        if (const size_t max_exposure_ndx = js_cam.IndexOf("max_exposure"); max_exposure_ndx < js_cam.Size()) {
            const Sys::JsNumber &js_max_exposure = js_cam[max_exposure_ndx].second.as_num();
            max_exposure = float(js_max_exposure.val);
        }
    }

    scene_manager_->SetupView(view_origin, (view_origin + view_dir), Ren::Vec3f{0, 1, 0}, view_fov, Ren::Vec2f{0.0f}, 1,
                              min_exposure, max_exposure);

    {
        char sophia_name[] = "sophia_00";

        for (int i = 0; i < 2; i++) {
            sophia_name[8] = char('0' + i);

            uint32_t sophia_index = scene_manager_->FindObject(sophia_name);
            sophia_indices_[i] = sophia_index;
        }
    }
}

void UITest::UpdateAnim(const uint64_t dt_us) {
    using namespace UITestInternal;

    BaseState::UpdateAnim(dt_us);

    const float delta_time_s = dt_us * 0.000001f;
    test_time_counter_s += delta_time_s;

    const float char_period_s = 0.025f;

    while (test_time_counter_s > char_period_s) {
        // word_puzzle_->incr_progress();
        test_time_counter_s -= char_period_s;
    }

    const Eng::SceneData &scene = scene_manager_->scene_data();

    if (sophia_indices_[0] != 0xffffffff) {
        for (int i = 0; i < 2; i++) { // NOLINT
            if (sophia_indices_[i] == 0xffffffff) {
                break;
            }

            Eng::SceneObject *sophia = scene_manager_->GetObject(sophia_indices_[i]);

            uint32_t mask = Eng::CompDrawableBit | Eng::CompAnimStateBit;
            if ((sophia->comp_mask & mask) == mask) {
                auto *dr =
                    (Eng::Drawable *)scene.comp_store[Eng::CompDrawable]->Get(sophia->components[Eng::CompDrawable]);
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
            }
        }
    }
}

void UITest::Exit() { BaseState::Exit(); }

void UITest::DrawUI(Gui::Renderer *r, Gui::BaseElement *root) {
    using namespace UITestInternal;

    BaseState::DrawUI(r, root);
}
