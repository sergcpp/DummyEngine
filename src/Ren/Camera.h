#pragma once

#include "MMat.h"

namespace Ren {
enum ePointPos { Front, Back, OnPlane };

struct Plane {
    Vec3f n;
    float d;

    Plane() : d(0.0f) {}
    Plane(const Ren::Vec3f &v0, const Ren::Vec3f &v1, const Ren::Vec3f &v2);
    explicit Plane(eUninitialized) : n(Uninitialize) {}

    int ClassifyPoint(const float point[3]) const;
};

enum class eCamPlane {
    LeftPlane,
    RightPlane,
    TopPlane,
    BottomPlane,
    NearPlane,
    FarPlane
};

enum class eVisResult { Invisible, FullyVisible, PartiallyVisible };

struct Frustum {
    Plane planes[8] = {Plane{Ren::Uninitialize}, Plane{Ren::Uninitialize},
                       Plane{Ren::Uninitialize}, Plane{Ren::Uninitialize},
                       Plane{Ren::Uninitialize}, Plane{Ren::Uninitialize},
                       Plane{Ren::Uninitialize}, Plane{Ren::Uninitialize}};
    int planes_count = 6;

    eVisResult CheckVisibility(const Vec3f &point) const;
    eVisResult CheckVisibility(const float bbox[8][3]) const;
    eVisResult CheckVisibility(const Vec3f &bbox_min, const Vec3f &bbox_max) const;
};

class Camera {
  protected:
    Mat4f view_matrix_;
    Mat4f proj_matrix_;

    Vec3f world_position_;
    uint32_t render_mask_ = 0xffffffff;

    Frustum frustum_;
    bool is_orthographic_;

    float angle_, aspect_, near_, far_;

  public:
    float max_exposure = 1000.0f;
    float focus_distance = 4.0f, focus_depth = 2.0f;
    float focus_near_mul = 0.0f, focus_far_mul = 0.0f;

    Camera() = default;
    Camera(const Vec3f &center, const Vec3f &target, const Vec3f &up);

    const Mat4f &view_matrix() const { return view_matrix_; }
    const Mat4f &proj_matrix() const { return proj_matrix_; }

    const Vec3f &world_position() const { return world_position_; }

    uint32_t render_mask() const { return render_mask_; }
    void set_render_mask(uint32_t mask) { render_mask_ = mask; }

    Vec3f view_dir() const {
        return Vec3f{view_matrix_[0][2], view_matrix_[1][2], view_matrix_[2][2]};
    }

    float angle() const { return angle_; }
    float aspect() const { return aspect_; }
    float near() const { return near_; }
    float far() const { return far_; }

    const Plane &frustum_plane(int i) const { return frustum_.planes[i]; }
    const Plane &frustum_plane(eCamPlane i) const { return frustum_.planes[int(i)]; }

    bool is_orthographic() const { return is_orthographic_; }

    void Perspective(float angle, float aspect, float near, float far);
    void Orthographic(float left, float right, float top, float down, float near,
                      float far);

    void SetupView(const Vec3f &center, const Vec3f &target, const Vec3f &up);

    void UpdatePlanes();

    eVisResult CheckFrustumVisibility(const Vec3f &point) const;
    eVisResult CheckFrustumVisibility(const float bbox[8][3]) const;
    eVisResult CheckFrustumVisibility(const Vec3f &bbox_min,
                                             const Vec3f &bbox_max) const;

    // returns radius
    float GetBoundingSphere(Vec3f &out_center) const;

    void ExtractSubFrustums(int resx, int resy, int resz, Frustum *sub_frustums) const;

    void Move(const Vec3f &v, float delta_time);
    void Rotate(float rx, float ry, float delta_time);
};
} // namespace Ren
