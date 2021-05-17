#include "FreeCamController.h"

FreeCamController::FreeCamController(const int width, const int height,
                                     const float move_region_frac)
    : move_region_frac_(move_region_frac), width_(width), height_(height) {}

void FreeCamController::Update(uint64_t dt_us) {
    using namespace Ren;
    
    const Vec3f up = Vec3f{0, 1, 0}, side = Normalize(Cross(view_dir, up));

    const float fwd_speed = std::max(
                    std::min(fwd_press_speed_ + fwd_touch_speed_, max_fwd_speed),
                    -max_fwd_speed),
                side_speed = std::max(
                    std::min(side_press_speed_ + side_touch_speed_, max_fwd_speed),
                    -max_fwd_speed);

    view_origin += view_dir * fwd_speed;
    view_origin += side * side_speed;

    if (std::abs(fwd_speed) > 0.0f || std::abs(side_speed) > 0.0f) {
        invalidate_view = true;
    }
}

void FreeCamController::Resize(const int w, const int h) {
    width_ = w;
    height_ = h;
}

bool FreeCamController::HandleInput(const InputManager::Event &evt) {
    using namespace Ren;

    switch (evt.type) {
    case RawInputEv::P1Down:
        if (evt.point.x < (move_region_frac_ * float(width_)) && move_pointer_ == 0) {
            move_pointer_ = 1;
        } else if (view_pointer_ == 0) {
            view_pointer_ = 1;
        }
        break;
    case RawInputEv::P2Down:
        if (evt.point.x < (move_region_frac_ * float(width_)) && move_pointer_ == 0) {
            move_pointer_ = 2;
        } else if (view_pointer_ == 0) {
            view_pointer_ = 2;
        }
        break;
    case RawInputEv::P1Up:
        if (move_pointer_ == 1) {
            move_pointer_ = 0;
            fwd_touch_speed_ = 0;
            side_touch_speed_ = 0;
        } else if (view_pointer_ == 1) {
            view_pointer_ = 0;
        }
        break;
    case RawInputEv::P2Up:
        if (move_pointer_ == 2) {
            move_pointer_ = 0;
            fwd_touch_speed_ = 0;
            side_touch_speed_ = 0;
        } else if (view_pointer_ == 2) {
            view_pointer_ = 0;
        }
        break;
    case RawInputEv::P1Move:
        if (move_pointer_ == 1) {
            side_touch_speed_ += evt.move.dx * 0.002f;
            side_touch_speed_ =
                std::max(std::min(side_touch_speed_, max_fwd_speed), -max_fwd_speed);

            fwd_touch_speed_ -= evt.move.dy * 0.002f;
            fwd_touch_speed_ =
                std::max(std::min(fwd_touch_speed_, max_fwd_speed), -max_fwd_speed);
        } else if (view_pointer_ == 1) {
            auto up = Vec3f{0, 1, 0};
            Vec3f side = Normalize(Cross(view_dir, up));
            up = Cross(side, view_dir);

            Mat4f rot;
            rot = Rotate(rot, -0.005f * evt.move.dx, up);
            rot = Rotate(rot, -0.005f * evt.move.dy, side);

            auto rot_m3 = Mat3f(rot);
            view_dir = rot_m3 * view_dir;

            invalidate_view = true;
        }
        break;
    case RawInputEv::P2Move:
        if (move_pointer_ == 2) {
            side_touch_speed_ += evt.move.dx * 0.002f;
            side_touch_speed_ =
                std::max(std::min(side_touch_speed_, max_fwd_speed), -max_fwd_speed);

            fwd_touch_speed_ -= evt.move.dy * 0.002f;
            fwd_touch_speed_ =
                std::max(std::min(fwd_touch_speed_, max_fwd_speed), -max_fwd_speed);
        } else if (view_pointer_ == 2) {
            auto up = Vec3f{0, 1, 0};
            Vec3f side = Normalize(Cross(view_dir, up));
            up = Cross(side, view_dir);

            Mat4f rot;
            rot = Rotate(rot, 0.01f * evt.move.dx, up);
            rot = Rotate(rot, 0.01f * evt.move.dy, side);

            auto rot_m3 = Mat3f(rot);
            view_dir = rot_m3 * view_dir;

            invalidate_view = true;
        }
        break;
    case RawInputEv::KeyDown: {
        if (evt.key_code == KeyUp ||
            (evt.key_code == KeyW && view_pointer_)) {
            fwd_press_speed_ = max_fwd_speed;
        } else if (evt.key_code == KeyDown ||
                   (evt.key_code == KeyS && view_pointer_)) {
            fwd_press_speed_ = -max_fwd_speed;
        } else if (evt.key_code == KeyLeft ||
                   (evt.key_code == KeyA && view_pointer_)) {
            side_press_speed_ = -max_fwd_speed;
        } else if (evt.key_code == KeyRight ||
                   (evt.key_code == KeyD && view_pointer_)) {
            side_press_speed_ = max_fwd_speed;
        }
    } break;
    case RawInputEv::KeyUp: {
        if (evt.key_code == KeyUp ||
            (evt.key_code == KeyW && view_pointer_)) {
            fwd_press_speed_ = 0;
        } else if (evt.key_code == KeyDown ||
                   (evt.key_code == KeyS && view_pointer_)) {
            fwd_press_speed_ = 0;
        } else if (evt.key_code == KeyLeft ||
                   (evt.key_code == KeyA && view_pointer_)) {
            side_press_speed_ = 0;
        } else if (evt.key_code == KeyRight ||
                   (evt.key_code == KeyD && view_pointer_)) {
            side_press_speed_ = 0;
        }
    }
    }

    return true;
}