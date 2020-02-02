#include "GSUITest2.h"

#include <fstream>
#include <memory>

#include <Eng/GameStateManager.h>
#include <Eng/Renderer/Renderer.h>
#include <Eng/Scene/SceneManager.h>
#include <Eng/Utils/Cmdline.h>
#include <Eng/Gui/Image.h>
#include <Eng/Gui/Image9Patch.h>
#include <Eng/Gui/Renderer.h>
#include <Eng/Gui/Utils.h>
#include <Ren/Context.h>
#include <Ren/GL.h>
#include <Ren/Utils.h>
#include <Sys/AssetFile.h>
#include <Sys/Json.h>
#include <Sys/MemBuf.h>
#include <Sys/Time_.h>

#include "../Gui/FontStorage.h"
#include "../Gui/TextPrinter.h"
#include "../Utils/Dictionary.h"
#include "../Viewer.h"

namespace GSUITest2Internal {
#if defined(__ANDROID__)
const char SCENE_NAME[] = "assets/scenes/"
#else
const char SCENE_NAME[] = "assets_pc/scenes/"
#endif
    "zenith.json";
}

GSUITest2::GSUITest2(GameBase *game) : GSBaseState(game) {
    const std::shared_ptr<FontStorage> fonts = game->GetComponent<FontStorage>(UI_FONTS_KEY);
    dialog_font_ = fonts->FindFont("dialog_font");
    dialog_font_->set_scale(1.5f);

    dict_ = game->GetComponent<Dictionary>(DICT_KEY);
}

void GSUITest2::Enter() {
    using namespace GSUITest2Internal;

    GSBaseState::Enter();

    log_->Info("GSUITest: Loading scene!");
    GSBaseState::LoadScene(SCENE_NAME);

    {
        std::ifstream dict_file("assets_pc/scenes/test/test_dict/de-en.dict", std::ios::binary);
        dict_->Load(dict_file, log_.get());

        Dictionary::dict_entry_res_t result = {};
        if (dict_->Lookup("Gehilfe", result)) {
            volatile int ii = 0;
        }
    }

    zenith_index_ = scene_manager_->FindObject("zenith");
}

void GSUITest2::OnPostloadScene(JsObject &js_scene) {
    using namespace GSUITest2Internal;

    GSBaseState::OnPostloadScene(js_scene);

    Ren::Vec3f view_origin, view_dir = Ren::Vec3f{ 0.0f, 0.0f, 1.0f };
    float view_fov = 45.0f, max_exposure = 1000.0f;

    if (js_scene.Has("camera")) {
        const JsObject &js_cam = js_scene.at("camera").as_obj();
        if (js_cam.Has("view_origin")) {
            const JsArray &js_orig = js_cam.at("view_origin").as_arr();
            view_origin[0] = (float)js_orig.at(0).as_num().val;
            view_origin[1] = (float)js_orig.at(1).as_num().val;
            view_origin[2] = (float)js_orig.at(2).as_num().val;
        }

        if (js_cam.Has("view_dir")) {
            const JsArray &js_dir = js_cam.at("view_dir").as_arr();
            view_dir[0] = (float)js_dir.at(0).as_num().val;
            view_dir[1] = (float)js_dir.at(1).as_num().val;
            view_dir[2] = (float)js_dir.at(2).as_num().val;
        }

        /*if (js_cam.Has("fwd_speed")) {
            const JsNumber &js_fwd_speed = (const JsNumber &)js_cam.at("fwd_speed");
            max_fwd_speed_ = (float)js_fwd_speed.val;
        }*/

        if (js_cam.Has("fov")) {
            const JsNumber &js_fov = js_cam.at("fov").as_num();
            view_fov = (float)js_fov.val;
        }

        if (js_cam.Has("max_exposure")) {
            const JsNumber &js_max_exposure = js_cam.at("max_exposure").as_num();
            max_exposure = (float)js_max_exposure.val;
        }
    }

    scene_manager_->SetupView(
            view_origin, (view_origin + view_dir), Ren::Vec3f{ 0.0f, 1.0f, 0.0f },
            view_fov, max_exposure);
}

void GSUITest2::OnUpdateScene() {
    using namespace GSUITest2Internal;

    GSBaseState::OnUpdateScene();

    const float delta_time_s = fr_info_.delta_time_us * 0.000001f;
    /*test_time_counter_s += delta_time_s;

    const float char_period_s = 0.025f;

    while (test_time_counter_s > char_period_s) {
        text_printer_->incr_progress();
        test_time_counter_s -= char_period_s;
    }*/

    const SceneData &scene = scene_manager_->scene_data();

    if (zenith_index_ != 0xffffffff) {
        SceneObject *zenith = scene_manager_->GetObject(zenith_index_);

        uint32_t mask = CompDrawableBit | CompAnimStateBit;
        if ((zenith->comp_mask & mask) == mask) {
            auto *dr = (Drawable *)scene.comp_store[CompDrawable]->Get(zenith->components[CompDrawable]);
            auto *as = (AnimState *)scene.comp_store[CompAnimState]->Get(zenith->components[CompAnimState]);

            as->anim_time_s += delta_time_s;

            Ren::Mesh *mesh = dr->mesh.get();
            Ren::Skeleton *skel = mesh->skel();

            const int anim_index = 0;

            skel->UpdateAnim(anim_index, as->anim_time_s);
            skel->ApplyAnim(anim_index);
            skel->UpdateBones(as->matr_palette);
        }
    }
}

void GSUITest2::Exit() {
    GSBaseState::Exit();
}

void GSUITest2::DrawUI(Gui::Renderer *r, Gui::BaseElement *root) {
    using namespace GSUITest2Internal;

    GSBaseState::DrawUI(r, root);
}

bool GSUITest2::HandleInput(const InputManager::Event &evt) {
    using namespace Ren;
    using namespace GSUITest2Internal;

    // pt switch for touch controls
    if (evt.type == RawInputEvent::EvP1Down || evt.type == RawInputEvent::EvP2Down) {
        if (evt.point.x > (float)ctx_->w() * 0.9f && evt.point.y < (float)ctx_->h() * 0.1f) {
            uint32_t new_time = Sys::GetTimeMs();
            if (new_time - click_time_ < 400) {
                use_pt_ = !use_pt_;
                if (use_pt_) {
                    scene_manager_->InitScene_PT();
                    invalidate_view_ = true;
                }

                click_time_ = 0;
            } else {
                click_time_ = new_time;
            }
        }
    }

    bool input_processed = true;

    switch (evt.type) {
    case RawInputEvent::EvP1Down: {
        Ren::Vec2f p = Gui::MapPointToScreen(Ren::Vec2i{ (int)evt.point.x, (int)evt.point.y }, Ren::Vec2i{ ctx_->w(), ctx_->h() });
        //text_printer_->Press(p, true);
    } break;
    case RawInputEvent::EvP2Down: {
        
    } break;
    case RawInputEvent::EvP1Up: {
        //text_printer_->skip();

        Ren::Vec2f p = Gui::MapPointToScreen(Ren::Vec2i{ (int)evt.point.x, (int)evt.point.y }, Ren::Vec2i{ ctx_->w(), ctx_->h() });
        //text_printer_->Press(p, false);

        is_visible_ = !is_visible_;
    } break;
    case RawInputEvent::EvP2Up: {

    } break;
    case RawInputEvent::EvP1Move: {
        Ren::Vec2f p = Gui::MapPointToScreen(Ren::Vec2i{ (int)evt.point.x, (int)evt.point.y }, Ren::Vec2i{ ctx_->w(), ctx_->h() });
        //text_printer_->Focus(p);
    } break;
    case RawInputEvent::EvP2Move: {

    } break;
    case RawInputEvent::EvKeyDown: {
        input_processed = false;
    } break;
    case RawInputEvent::EvKeyUp: {
        if (evt.key_code == KeyUp || (evt.key_code == KeyW && !cmdline_enabled_)) {
            //text_printer_->restart();
        } else {
            input_processed = false;
        }
    } break;
    case RawInputEvent::EvResize:
        //text_printer_->Resize(ui_root_.get());
        break;
    default:
        break;
    }

    if (!input_processed) {
        GSBaseState::HandleInput(evt);
    }

    return true;
}
