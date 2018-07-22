#include "GSOccTest.h"

#include <Eng/GameStateManager.h>
#include <Gui/Renderer.h>
#include <Ren/Context.h>
#include <Ren/GL.h>
#include <Sys/Time_.h>

#include "../Viewer.h"
#include "../ui/FontStorage.h"

namespace GSOccTestInternal {
const float FORWARD_SPEED = 2.0f;

const float CAM_FOV = 45.0f;
const float CAM_CENTER[3] = { 100.0f, 100.0f, -500.0f };
const float CAM_TARGET[3] = { 0.0f, 0.0f, 0.0f };
const float CAM_UP[3] = { 0.0f, 1.0f, 0.0f };

const float NEAR_CLIP = 0.5f;
const float FAR_CLIP = 1000;

const int DEPTH_RES_W = 256, DEPTH_RES_H = 128;
}

GSOccTest::GSOccTest(GameBase *game) : game_(game),
    cam_(GSOccTestInternal::CAM_CENTER,
         GSOccTestInternal::CAM_TARGET,
         GSOccTestInternal::CAM_UP) {
    using namespace GSOccTestInternal;

    state_manager_  = game->GetComponent<GameStateManager>(STATE_MANAGER_KEY);
    ctx_            = game->GetComponent<Ren::Context>(REN_CONTEXT_KEY);

    ui_renderer_    = game->GetComponent<Gui::Renderer>(UI_RENDERER_KEY);
    ui_root_        = game->GetComponent<Gui::BaseElement>(UI_ROOT_KEY);

    const auto fonts = game->GetComponent<FontStorage>(UI_FONTS_KEY);
    font_ = fonts->FindFont("main_font");

    view_origin_ = math::make_vec3(CAM_CENTER);
    cam_.Perspective(CAM_FOV, float(game_->width)/game_->height, NEAR_CLIP, FAR_CLIP);

    SWfloat z = FAR_CLIP / (FAR_CLIP - NEAR_CLIP) + (NEAR_CLIP - (2.0f * NEAR_CLIP)) / (0.15f * (FAR_CLIP - NEAR_CLIP));
    swCullCtxInit(&cull_ctx_, DEPTH_RES_W, DEPTH_RES_H, z);

    InitShaders();
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
    using namespace GSOccTestInternal;
    using namespace math;

    double dt1_ms = 0, dt2_ms = 0;
    int num_visible = 0;

    const vec3 up = { 0, 1, 0 };

    SWfloat attribs[8][8][8][144] = { 0 };
    const SWubyte indices[] = { 0, 1, 2, 0, 2, 3,
                                6, 5, 4, 7, 6, 4,

                                10, 9, 8, 11, 10, 8,
                                12, 13, 14, 12, 14, 15,

                                18, 17, 16, 19, 18, 16,
                                20, 21, 22, 20, 22, 23
                              };

    auto set_bbox = [](const float pos[3], float attribs[144]) {
        float _min[3] = { -10 + pos[0], 0 + pos[1], -10 + pos[2] };
        float _max[3] = { 10 + pos[0], 30 + pos[1], 10 + pos[2] };

        attribs[0] = _min[0];
        attribs[1] = _min[1];
        attribs[2] = _min[2];
        attribs[3] = 0;
        attribs[4] = -1;
        attribs[5] = 0;
        attribs[6] = _max[0];
        attribs[7] = _min[1];
        attribs[8] = _min[2];
        attribs[9] = 0;
        attribs[10] = -1;
        attribs[11] = 0;
        attribs[12] = _max[0];
        attribs[13] = _min[1];
        attribs[14] = _max[2];
        attribs[15] = 0;
        attribs[16] = -1;
        attribs[17] = 0;
        attribs[18] = _min[0];
        attribs[19] = _min[1];
        attribs[20] = _max[2];
        attribs[21] = 0;
        attribs[22] = -1;
        attribs[23] = 0;

        attribs[24] = _min[0];
        attribs[25] = _max[1];
        attribs[26] = _min[2];
        attribs[27] = 0;
        attribs[28] = 1;
        attribs[29] = 0;
        attribs[30] = _max[0];
        attribs[31] = _max[1];
        attribs[32] = _min[2];
        attribs[33] = 0;
        attribs[34] = 1;
        attribs[35] = 0;
        attribs[36] = _max[0];
        attribs[37] = _max[1];
        attribs[38] = _max[2];
        attribs[39] = 0;
        attribs[40] = 1;
        attribs[41] = 0;
        attribs[42] = _min[0];
        attribs[43] = _max[1];
        attribs[44] = _max[2];
        attribs[45] = 0;
        attribs[46] = 1;
        attribs[47] = 0;

        attribs[48] = _min[0];
        attribs[49] = _min[1];
        attribs[50] = _min[2];
        attribs[51] = 1;
        attribs[52] = 0;
        attribs[53] = 0;
        attribs[54] = _min[0];
        attribs[55] = _max[1];
        attribs[56] = _min[2];
        attribs[57] = 1;
        attribs[58] = 0;
        attribs[59] = 0;
        attribs[60] = _min[0];
        attribs[61] = _max[1];
        attribs[62] = _max[2];
        attribs[63] = 1;
        attribs[64] = 0;
        attribs[65] = 0;
        attribs[66] = _min[0];
        attribs[67] = _min[1];
        attribs[68] = _max[2];
        attribs[69] = 1;
        attribs[70] = 0;
        attribs[71] = 0;

        attribs[72] = _max[0];
        attribs[73] = _min[1];
        attribs[74] = _min[2];
        attribs[75] = -1;
        attribs[76] = 0;
        attribs[77] = 0;
        attribs[78] = _max[0];
        attribs[79] = _max[1];
        attribs[80] = _min[2];
        attribs[81] = -1;
        attribs[82] = 0;
        attribs[83] = 0;
        attribs[84] = _max[0];
        attribs[85] = _max[1];
        attribs[86] = _max[2];
        attribs[87] = -1;
        attribs[88] = 0;
        attribs[89] = 0;
        attribs[90] = _max[0];
        attribs[91] = _min[1];
        attribs[92] = _max[2];
        attribs[93] = -1;
        attribs[94] = 0;
        attribs[95] = 0;

        attribs[96] = _min[0];
        attribs[97] = _min[1];
        attribs[98] = _min[2];
        attribs[99] = 0;
        attribs[100] = 0;
        attribs[101] = 1;
        attribs[102] = _max[0];
        attribs[103] = _min[1];
        attribs[104] = _min[2];
        attribs[105] = 0;
        attribs[106] = 0;
        attribs[107] = 1;
        attribs[108] = _max[0];
        attribs[109] = _max[1];
        attribs[110] = _min[2];
        attribs[111] = 0;
        attribs[112] = 0;
        attribs[113] = 1;
        attribs[114] = _min[0];
        attribs[115] = _max[1];
        attribs[116] = _min[2];
        attribs[117] = 0;
        attribs[118] = 0;
        attribs[119] = 1;

        attribs[120] = _min[0];
        attribs[121] = _min[1];
        attribs[122] = _max[2];
        attribs[123] = 0;
        attribs[124] = 0;
        attribs[125] = -1;
        attribs[126] = _max[0];
        attribs[127] = _min[1];
        attribs[128] = _max[2];
        attribs[129] = 0;
        attribs[130] = 0;
        attribs[131] = -1;
        attribs[132] = _max[0];
        attribs[133] = _max[1];
        attribs[134] = _max[2];
        attribs[135] = 0;
        attribs[136] = 0;
        attribs[137] = -1;
        attribs[138] = _min[0];
        attribs[139] = _max[1];
        attribs[140] = _max[2];
        attribs[141] = 0;
        attribs[142] = 0;
        attribs[143] = -1;
    };

    SWcull_surf s[8][8][8];

    {
        cam_.Perspective(CAM_FOV, float(game_->width)/game_->height, NEAR_CLIP, FAR_CLIP);
        cam_.SetupView(value_ptr(view_origin_), value_ptr(view_origin_ + view_dir_), value_ptr(up));

        mat4 world_from_object,
             view_from_world = make_mat4(cam_.view_matrix()),
             proj_from_view = make_mat4(cam_.projection_matrix());

        mat4 view_from_object = view_from_world * world_from_object,
             proj_from_object = proj_from_view * view_from_object;

        for (int i = 0; i < 8; i++) {
            for (int j = 0; j < 8; j++) {
                for (int k = 0; k < 8; k++) {
                    float pos[3] = { i * 30.0f, j * 40.0f, k * 30.0f };
                    set_bbox(pos, attribs[i][j][k]);

                    s[i][j][k].type = SW_OCCLUDER;
                    s[i][j][k].prim_type = SW_TRIANGLES;
                    s[i][j][k].index_type = SW_UNSIGNED_BYTE;
                    s[i][j][k].attribs = &attribs[i][j][k][0];
                    s[i][j][k].indices = &indices[0];
                    s[i][j][k].stride = 6 * sizeof(float);
                    s[i][j][k].count = 36;
                    s[i][j][k].xform = value_ptr(proj_from_object);
                    s[i][j][k].dont_skip = nullptr;
                }
            }
        }

        auto t1 = Sys::GetTimeNs();

        swCullCtxClear(&cull_ctx_);
        swCullCtxSubmitCullSurfs(&cull_ctx_, &s[0][0][0], 8 * 8 * 8);

        auto t2 = Sys::GetTimeNs();

        for (int i = 0; i < 8; i++) {
            for (int j = 0; j < 8; j++) {
                for (int k = 0; k < 8; k++) {
                    s[i][j][k].type = SW_OCCLUDEE;
                }
            }
        }

        auto t3 = Sys::GetTimeNs();

        swCullCtxSubmitCullSurfs(&cull_ctx_, &s[0][0][0], 8 * 8 * 8);

        num_visible = 0;
        for (int i = 0; i < 8; i++) {
            for (int j = 0; j < 8; j++) {
                for (int k = 0; k < 8; k++) {
                    num_visible += s[i][j][k].visible;
                }
            }
        }

        auto t4 = Sys::GetTimeNs();

        dt1_ms = (t2 - t1) / 1000000.0;
        dt2_ms = (t4 - t3) / 1000000.0;
    }

    {
        {
            // draw main view
            glClearColor(0, 0.2f, 0.2f, 1);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            DrawBoxes(&s[0][0][0], 8 * 8 * 8);
        }

        {
            // draw secondary view
            GLint viewport[4];
            glGetIntegerv(GL_VIEWPORT, viewport);

            glEnable(GL_SCISSOR_TEST);
            glViewport(0, DEPTH_RES_H, DEPTH_RES_W, DEPTH_RES_H);
            glScissor(0, DEPTH_RES_H, DEPTH_RES_W, DEPTH_RES_H);

            glClearColor(0, 0.2f, 0, 1);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            cam_.SetupView(value_ptr(view_origin_ + vec3{ 0, 500, 0 } + 20.0f * vec3{ view_dir_.x, 0, view_dir_.z }), value_ptr(view_origin_ + 200.0f * vec3{ view_dir_.x, 0, view_dir_.z }), value_ptr(up));

            DrawBoxes(&s[0][0][0], 8 * 8 * 8);
            DrawCam();

            glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
            glDisable(GL_SCISSOR_TEST);
        }
    }

    {
        BlitDepthBuf();
        BlitDepthTiles();
    }

    {
        // ui draw
        ui_renderer_->BeginDraw();

        static std::string s1, s2, s3, s4;

        fps_counter_++;
        time_acc_ += dt_s;
        if (time_acc_ >= 1.0f) {
            s1 = "           fps: ";
            s1 += std::to_string(fps_counter_);

            time_acc_ -= 1.0f;
            fps_counter_ = 0;
        }

        s2 = "occluders time: ";
        s2 += std::to_string(dt1_ms);
        s3 = "occludees time: ";
        s3 += std::to_string(dt2_ms);
        s4 = "   num visible: ";
        s4 += std::to_string(num_visible);
        s4 += "/512";

        font_->DrawText(ui_renderer_.get(), s1.c_str(), { -1, 1.0f - 1 * font_->height(ui_root_.get()) }, ui_root_.get());
        font_->DrawText(ui_renderer_.get(), s2.c_str(), { -1, 1.0f - 2 * font_->height(ui_root_.get()) }, ui_root_.get());
        font_->DrawText(ui_renderer_.get(), s3.c_str(), { -1, 1.0f - 3 * font_->height(ui_root_.get()) }, ui_root_.get());
        font_->DrawText(ui_renderer_.get(), s4.c_str(), { -1, 1.0f - 4 * font_->height(ui_root_.get()) }, ui_root_.get());

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

    cam_.SetupView(value_ptr(view_origin_), value_ptr(view_origin_ + view_dir_), value_ptr(up));

    if (forward_speed_ != 0 || side_speed_ != 0) {
        invalidate_preview_ = true;
    }

}

void GSOccTest::HandleInput(InputManager::Event evt) {
    using namespace GSOccTestInternal;
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
        } else if (evt.raw_key == 'f') {
            wireframe_ = !wireframe_;
        } else if (evt.raw_key == 'e') {
            cull_ = !cull_;
        }
    }
    break;
    case InputManager::RAW_INPUT_RESIZE:

        break;
    default:
        break;
    }
}
