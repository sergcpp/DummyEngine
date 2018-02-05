#include "GSOccTest.h"

#include <fstream>
#include <sstream>

#include <engine/GameStateManager.h>
#include <ren/Context.h>
#include <ren/Utils.h>
#include <ren/GL.h>
#include <sys/AssetFile.h>
#include <sys/Json.h>
#include <sys/Log.h>
#include <sys/Time_.h>
#include <ui/Renderer.h>

#include "../Viewer.h"
#include "../ui/FontStorage.h"

namespace {
	const float FORWARD_SPEED = 8.0f;
}

GSOccTest::GSOccTest(GameBase *game) : game_(game) {
    state_manager_  = game->GetComponent<GameStateManager>(STATE_MANAGER_KEY);
    ctx_            = game->GetComponent<ren::Context>(REN_CONTEXT_KEY);

    ui_renderer_    = game->GetComponent<ui::Renderer>(UI_RENDERER_KEY);
    ui_root_        = game->GetComponent<ui::BaseElement>(UI_ROOT_KEY);

    const auto fonts = game->GetComponent<FontStorage>(UI_FONTS_KEY);
    font_ = fonts->FindFont("main_font");

	swCullCtxInit(&cull_ctx_, 256, 128);

	//InitShaders();
}

GSOccTest::~GSOccTest() {
	swCullCtxDestroy(&cull_ctx_);
}

void GSOccTest::Enter() {
    using namespace math;

    
}

void GSOccTest::Exit() {

}

void GSOccTest::Draw(float dt_s) {
    
	{
		const SWfloat attribs[] = { 0,    0,    0.5f,
								    0.5f, 0,    0.5f,
									0.5f, 0.5f, 0.5f, };
		const SWubyte indices[] = { 0, 1, 2 };

		SWcull_surf s;
		s.type = SW_OCCLUDER;
		s.prim_type = SW_TRIANGLES;
		s.index_type = SW_UNSIGNED_BYTE;
		s.attribs = &attribs[0];
		s.indices = &indices[0];
		s.stride = 3 * sizeof(float);
		s.count = 3;

		float xform[16];

		//swCullCtxClear(&cull_ctx_);
		swCullCtxSubmitCullSurfs(&cull_ctx_, &s, 1, xform);
	}

	{
		
	}

	{
		
        BlitDepthBuf();
	}

    {
        // ui draw
        ui_renderer_->BeginDraw();

		static std::string s = "0";

		fps_counter_++;
		time_acc_ += dt_s;
		if (time_acc_ >= 1.0f) {
			s = std::to_string(fps_counter_);

			time_acc_ -= 1.0f;
			fps_counter_ = 0;
		}

		font_->DrawText(ui_renderer_.get(), s.c_str(), { -1, 1.0f - font_->height(ui_root_.get()) }, ui_root_.get());

        ui_renderer_->EndDraw();
    }

    ctx_->ProcessTasks();
}

void GSOccTest::Update(int dt_ms) {
    using namespace math;

    vec3 up = { 0, 1, 0 };
    vec3 side = normalize(cross(view_dir_, up));

    view_origin_ += view_dir_ * forward_speed_;
    view_origin_ += side * side_speed_;

    if (forward_speed_ != 0 || side_speed_ != 0) {
        invalidate_preview_ = true;
    }

}

void GSOccTest::HandleInput(InputManager::Event evt) {
    using namespace math;

    switch (evt.type) {
    case InputManager::RAW_INPUT_P1_DOWN:
        view_grabbed_ = true;
        break;
    case InputManager::RAW_INPUT_P1_UP:
        view_grabbed_ = false;
        break;
    case InputManager::RAW_INPUT_P1_MOVE:
        if (view_grabbed_) {
            vec3 up = { 0, 1, 0 };
            vec3 side = normalize(cross(view_dir_, up));
            up = cross(side, view_dir_);

            mat4 rot;
            rot = rotate(rot, 0.01f * evt.move.dx, up);
            rot = rotate(rot, 0.01f * evt.move.dy, side);

            mat3 rot_m3 = mat3(rot);

            if (!view_targeted_) {
                view_dir_ = view_dir_ * rot_m3;
            } else {
                vec3 dir = view_origin_ - view_target_;
                dir = dir * rot_m3;
                view_origin_ = view_target_ + dir;
                view_dir_ = normalize(-dir);
            }

            invalidate_preview_ = true;
        }
        break;
    case InputManager::RAW_INPUT_KEY_DOWN: {
        if (evt.key == InputManager::RAW_INPUT_BUTTON_UP) {
            forward_speed_ = FORWARD_SPEED;
        } else if (evt.key == InputManager::RAW_INPUT_BUTTON_DOWN) {
            forward_speed_ = -FORWARD_SPEED;
        } else if (evt.key == InputManager::RAW_INPUT_BUTTON_LEFT) {
            side_speed_ = -FORWARD_SPEED;
        } else if (evt.key == InputManager::RAW_INPUT_BUTTON_RIGHT) {
            side_speed_ = FORWARD_SPEED;
        } else if (evt.key == InputManager::RAW_INPUT_BUTTON_SPACE) {
            
        } else if (evt.raw_key == 'e' || evt.raw_key == 'q') {
            vec3 up = { 1, 0, 0 };
            vec3 side = normalize(cross(sun_dir_, up));
            up = cross(side, sun_dir_);

            mat4 rot;
            rot = rotate(rot, evt.raw_key == 'e' ? 0.025f : -0.025f, up);
            rot = rotate(rot, evt.raw_key == 'e' ? 0.025f : -0.025f, side);

            mat3 rot_m3 = mat3(rot);

            sun_dir_ = sun_dir_ * rot_m3;
        }
    }
    break;
    case InputManager::RAW_INPUT_KEY_UP: {
        if (evt.key == InputManager::RAW_INPUT_BUTTON_UP) {
            forward_speed_ = 0;
        } else if (evt.key == InputManager::RAW_INPUT_BUTTON_DOWN) {
            forward_speed_ = 0;
        } else if (evt.key == InputManager::RAW_INPUT_BUTTON_LEFT) {
            side_speed_ = 0;
        } else if (evt.key == InputManager::RAW_INPUT_BUTTON_RIGHT) {
            side_speed_ = 0;
        }
    }
    break;
    case InputManager::RAW_INPUT_RESIZE:

        break;
    default:
        break;
    }
}
