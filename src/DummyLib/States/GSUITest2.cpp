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
}

void GSUITest2::Enter() {
    using namespace GSUITest2Internal;

    GSBaseState::Enter();

    log_->Info("GSUITest: Loading scene!");
    GSBaseState::LoadScene(SCENE_NAME);

    test_image_.reset(new Gui::Image{
        *ctx_,
#if defined(__ANDROID__)
        "assets/textures/ui/flag_ua.uncompressed.png",
#else
        "assets_pc/textures/ui/flag_ua.uncompressed.png",
#endif
        Ren::Vec2f{ -0.5f, -0.5f }, Ren::Vec2f{ 0.5f, 0.5f }, ui_root_.get()
    });

    test_image2_.reset(new Gui::Image{
            *ctx_,
#if defined(__ANDROID__)
            "assets/textures/ui/flag_gb.uncompressed.png",
#else
            "assets_pc/textures/ui/flag_gb.uncompressed.png",
#endif
            Ren::Vec2f{ -0.5f, -0.5f }, Ren::Vec2f{ 0.5f, 0.5f }, ui_root_.get()
    });

    test_image3_.reset(new Gui::Image{
            *ctx_,
#if defined(__ANDROID__)
            "assets/textures/ui/flag_ru.uncompressed.png",
#else
            "assets_pc/textures/ui/flag_ru.uncompressed.png",
#endif
            Ren::Vec2f{ -0.5f, -0.5f }, Ren::Vec2f{ 0.5f, 0.5f }, ui_root_.get()
    });

    test_image4_.reset(new Gui::Image{
            *ctx_,
#if defined(__ANDROID__)
            "assets/textures/ui/flag_us.uncompressed.png",
#else
            "assets_pc/textures/ui/flag_us.uncompressed.png",
#endif
            Ren::Vec2f{ -0.5f, -0.5f }, Ren::Vec2f{ 0.5f, 0.5f }, ui_root_.get()
    });

    test_frame_.reset(new Gui::Image9Patch{
        *ctx_,
#if defined(__ANDROID__)
        "assets/textures/ui/frame_01.uncompressed.png",
#else
        "assets_pc/textures/ui/frame_01.uncompressed.png",
#endif
        Ren::Vec2f{ 3.0f, 3.0f }, 1.0f, Ren::Vec2f{ 0.0f, -0.1f }, Ren::Vec2f{ 0.48f, 0.46f }, ui_root_.get()
    });

    test_frame3_.reset(new Gui::Image9Patch{
            *ctx_,
#if defined(__ANDROID__)
            "assets/textures/ui/frame_01.uncompressed.png",
#else
            "assets_pc/textures/ui/frame_01.uncompressed.png",
#endif
            Ren::Vec2f{ 3.0f, 3.0f }, 1.0f, Ren::Vec2f{ 0.48f, -0.1f }, Ren::Vec2f{ 0.3f, 0.46f }, ui_root_.get()
    });

    test_frame4_.reset(new Gui::Image9Patch{
            *ctx_,
#if defined(__ANDROID__)
            "assets/textures/ui/frame_01.uncompressed.png",
#else
            "assets_pc/textures/ui/frame_01.uncompressed.png",
#endif
            Ren::Vec2f{ 3.0f, 3.0f }, 1.0f, Ren::Vec2f{ 0.78f, -0.1f }, Ren::Vec2f{ 0.22f, 0.46f }, ui_root_.get()
    });

    test_frame2_.reset(new Gui::Image9Patch{
            *ctx_,
#if defined(__ANDROID__)
            "assets/textures/ui/frame_02.uncompressed.png",
#else
            "assets_pc/textures/ui/frame_02.uncompressed.png",
#endif
            Ren::Vec2f{ 20.0f, 20.0f }, 1.0f, Ren::Vec2f{ 0.0f, -0.5f }, Ren::Vec2f{ 1.0f, 1.0f }, ui_root_.get()
    });

    zenith_index_ = scene_manager_->FindObject("zenith");
}

void GSUITest2::OnPostloadScene(JsObject &js_scene) {
    using namespace GSUITest2Internal;

    GSBaseState::OnPostloadScene(js_scene);

    Ren::Vec3f view_origin, view_dir = Ren::Vec3f{ 0.0f, 0.0f, 1.0f };
    float view_fov = 45.0f, max_exposure = 1000.0f;

    if (js_scene.Has("camera")) {
        const JsObject &js_cam = (const JsObject &)js_scene.at("camera");
        if (js_cam.Has("view_origin")) {
            const JsArray &js_orig = (const JsArray &)js_cam.at("view_origin");
            view_origin[0] = (float)((const JsNumber &)js_orig.at(0)).val;
            view_origin[1] = (float)((const JsNumber &)js_orig.at(1)).val;
            view_origin[2] = (float)((const JsNumber &)js_orig.at(2)).val;
        }

        if (js_cam.Has("view_dir")) {
            const JsArray &js_dir = (const JsArray &)js_cam.at("view_dir");
            view_dir[0] = (float)((const JsNumber &)js_dir.at(0)).val;
            view_dir[1] = (float)((const JsNumber &)js_dir.at(1)).val;
            view_dir[2] = (float)((const JsNumber &)js_dir.at(2)).val;
        }

        /*if (js_cam.Has("fwd_speed")) {
            const JsNumber &js_fwd_speed = (const JsNumber &)js_cam.at("fwd_speed");
            max_fwd_speed_ = (float)js_fwd_speed.val;
        }*/

        if (js_cam.Has("fov")) {
            const JsNumber &js_fov = (const JsNumber &)js_cam.at("fov");
            view_fov = (float)js_fov.val;
        }

        if (js_cam.Has("max_exposure")) {
            const JsNumber &js_max_exposure = (const JsNumber &)js_cam.at("max_exposure");
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
    using namespace GSUITestInternal;

    GSBaseState::DrawUI(r, root);

    if (is_visible_) {
        test_frame2_->Draw(r);

        test_frame_->Draw(r);
        test_frame3_->Draw(r);
        test_frame4_->Draw(r);

        const uint8_t
                color_white[4] = {255, 255, 255, 255},
                color_green[4] = {0, 255, 0, 255},
                color_yellow[4] = {255, 255, 0, 255};

        const auto img_size = Ren::Vec2f{ 2.0f * 64.0f / root->size_px()[0], 2.0f * 32.0f / root->size_px()[1] };

        dialog_font_->set_scale(std::max(root->size_px()[0] / 1536.0f, 1.0f));
        const float font_height1 = dialog_font_->height(root);

        float y_offset = 0.5f;

        y_offset -= font_height1;
        dialog_font_->DrawText(r, "CURRENT EMPLOYEES", Ren::Vec2f{ 0.25f, y_offset }, color_white, root);

        dialog_font_->set_scale(std::max(root->size_px()[0] / 2048.0f, 1.0f));
        const float font_height2 = dialog_font_->height(root);

        y_offset -= font_height2;
        dialog_font_->DrawText(r, "NAME", Ren::Vec2f{ 0.01f, y_offset }, color_white, root);
        dialog_font_->DrawText(r, "SPEC.", Ren::Vec2f{ 0.5f, y_offset }, color_white, root);
        dialog_font_->DrawText(r, "STATUS", Ren::Vec2f{ 0.8f, y_offset }, color_white, root);

        dialog_font_->set_scale(std::max(root->size_px()[0] / 4096.0f, 1.0f));
        const float font_height3 = dialog_font_->height(root);

        y_offset -= font_height3;
        test_image_->Resize(Ren::Vec2f{ 0.01f, y_offset }, img_size, root);
        test_image_->Draw(r);
        dialog_font_->DrawText(r, "Alexander Kadeniuk", Ren::Vec2f{ img_size[0] + 0.02f, y_offset }, color_white, root);
        dialog_font_->DrawText(r, "MED", Ren::Vec2f{ 0.5f, y_offset }, color_white, root);
        dialog_font_->DrawText(r, "Available", Ren::Vec2f{ 0.8f, y_offset }, color_green, root);

        y_offset -= font_height3;
        test_image2_->Resize(Ren::Vec2f{ 0.01f, y_offset }, img_size, root);
        test_image2_->Draw(r);
        dialog_font_->DrawText(r, "Benedict Foale", Ren::Vec2f{ img_size[0] + 0.02f, y_offset }, color_white, root);
        dialog_font_->DrawText(r, "IT", Ren::Vec2f{ 0.5f, y_offset }, color_white, root);
        dialog_font_->DrawText(r, "Available", Ren::Vec2f{ 0.8f, y_offset }, color_green, root);

        y_offset -= font_height3;
        test_image3_->Resize(Ren::Vec2f{ 0.01f, y_offset}, img_size, root);
        test_image3_->Draw(r);
        dialog_font_->DrawText(r, "Maria Serova", Ren::Vec2f{ img_size[0] + 0.02f, y_offset }, color_white, root);
        dialog_font_->DrawText(r, "ART", Ren::Vec2f{ 0.5f, y_offset }, color_white, root);
        dialog_font_->DrawText(r, "Tired", Ren::Vec2f{ 0.8f, y_offset }, color_yellow, root);

        y_offset -= font_height3;
        test_image4_->Resize(Ren::Vec2f{ 0.01f, y_offset}, img_size, root);
        test_image4_->Draw(r);
        dialog_font_->DrawText(r, "Kaidan Shepard", Ren::Vec2f{ img_size[0] + 0.02f, y_offset }, color_white, root);
        dialog_font_->DrawText(r, "BIO", Ren::Vec2f{ 0.5f, y_offset }, color_white, root);
        dialog_font_->DrawText(r, "Available", Ren::Vec2f{ 0.8f, y_offset }, color_green, root);
    }
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
        test_frame_->Resize(ui_root_.get());
        test_frame2_->Resize(ui_root_.get());
        test_frame3_->Resize(ui_root_.get());
        test_frame4_->Resize(ui_root_.get());
        break;
    default:
        break;
    }

    if (!input_processed) {
        GSBaseState::HandleInput(evt);
    }

    return true;
}
