#include "FreeCamController.h"

Eng::FreeCamController::FreeCamController(const int width, const int height, const float move_region_frac)
    : move_region_frac_(move_region_frac), width_(width), height_(height) {}

void Eng::FreeCamController::Update(uint64_t dt_us) {
    using namespace Ren;

    const Vec3f up = Vec3f{0, 1, 0}, side = Normalize(Cross(view_dir, up));

    const float fwd_speed = std::max(std::min(fwd_press_speed_ + fwd_touch_speed_, max_fwd_speed), -max_fwd_speed),
                side_speed = std::max(std::min(side_press_speed_ + side_touch_speed_, max_fwd_speed), -max_fwd_speed);

    view_origin += view_dir * fwd_speed;
    view_origin += side * side_speed;

    if (std::abs(fwd_speed) > 0.0f || std::abs(side_speed) > 0.0f) {
        invalidate_view = true;
    }
}

void Eng::FreeCamController::Resize(const int w, const int h) {
    width_ = w;
    height_ = h;
}

bool Eng::FreeCamController::HandleInput(const input_event_t &evt) {
    using namespace Ren;

    switch (evt.type) {
    case eInputEvent::P1Down:
        if (evt.point[0] < (move_region_frac_ * float(width_)) && move_pointer_ == 0) {
            move_pointer_ = 1;
        } else if (view_pointer_ == 0) {
            view_pointer_ = 1;
        }
        break;
    case eInputEvent::P2Down:
        if (evt.point[0] < (move_region_frac_ * float(width_)) && move_pointer_ == 0) {
            move_pointer_ = 2;
        } else if (view_pointer_ == 0) {
            view_pointer_ = 2;
        }
        break;
    case eInputEvent::P1Up:
        if (move_pointer_ == 1) {
            move_pointer_ = 0;
            fwd_touch_speed_ = 0;
            side_touch_speed_ = 0;
        } else if (view_pointer_ == 1) {
            view_pointer_ = 0;
        }
        break;
    case eInputEvent::P2Up:
        if (move_pointer_ == 2) {
            move_pointer_ = 0;
            fwd_touch_speed_ = 0;
            side_touch_speed_ = 0;
        } else if (view_pointer_ == 2) {
            view_pointer_ = 0;
        }
        break;
    case eInputEvent::P1Move:
        if (move_pointer_ == 1) {
            side_touch_speed_ += evt.move[0] * 0.002f;
            side_touch_speed_ = std::max(std::min(side_touch_speed_, max_fwd_speed), -max_fwd_speed);

            fwd_touch_speed_ += evt.move[1] * 0.002f;
            fwd_touch_speed_ = std::max(std::min(fwd_touch_speed_, max_fwd_speed), -max_fwd_speed);
        } else if (view_pointer_ == 1) {
            auto up = Vec3f{0, 1, 0};
            Vec3f side = Normalize(Cross(view_dir, up));
            up = Cross(side, view_dir);

            Mat4f rot;
            rot = Rotate(rot, -0.005f * evt.move[0], up);
            rot = Rotate(rot, 0.005f * evt.move[1], side);

            auto rot_m3 = Mat3f(rot);
            view_dir = rot_m3 * view_dir;

            invalidate_view = true;
        }
        break;
    case eInputEvent::P2Move:
        if (move_pointer_ == 2) {
            side_touch_speed_ += evt.move[0] * 0.002f;
            side_touch_speed_ = std::max(std::min(side_touch_speed_, max_fwd_speed), -max_fwd_speed);

            fwd_touch_speed_ += evt.move[1] * 0.002f;
            fwd_touch_speed_ = std::max(std::min(fwd_touch_speed_, max_fwd_speed), -max_fwd_speed);
        } else if (view_pointer_ == 2) {
            auto up = Vec3f{0, 1, 0};
            Vec3f side = Normalize(Cross(view_dir, up));
            up = Cross(side, view_dir);

            Mat4f rot;
            rot = Rotate(rot, 0.01f * evt.move[0], up);
            rot = Rotate(rot, -0.01f * evt.move[1], side);

            auto rot_m3 = Mat3f(rot);
            view_dir = rot_m3 * view_dir;

            invalidate_view = true;
        }
        break;
    case eInputEvent::KeyDown: {
        if (evt.key_code == Eng::eKey::Up || (evt.key_code == Eng::eKey::W && view_pointer_)) {
            fwd_press_speed_ = max_fwd_speed;
        } else if (evt.key_code == Eng::eKey::Down || (evt.key_code == Eng::eKey::S && view_pointer_)) {
            fwd_press_speed_ = -max_fwd_speed;
        } else if (evt.key_code == Eng::eKey::Left || (evt.key_code == Eng::eKey::A && view_pointer_)) {
            side_press_speed_ = -max_fwd_speed;
        } else if (evt.key_code == Eng::eKey::Right || (evt.key_code == Eng::eKey::D && view_pointer_)) {
            side_press_speed_ = max_fwd_speed;
        }
    } break;
    case eInputEvent::KeyUp: {
        if (evt.key_code == Eng::eKey::Up || (evt.key_code == Eng::eKey::W && view_pointer_)) {
            fwd_press_speed_ = 0;
        } else if (evt.key_code == Eng::eKey::Down || (evt.key_code == Eng::eKey::S && view_pointer_)) {
            fwd_press_speed_ = 0;
        } else if (evt.key_code == Eng::eKey::Left || (evt.key_code == Eng::eKey::A && view_pointer_)) {
            side_press_speed_ = 0;
        } else if (evt.key_code == Eng::eKey::Right || (evt.key_code == Eng::eKey::D && view_pointer_)) {
            side_press_speed_ = 0;
        }
    }
    }

    return true;
}