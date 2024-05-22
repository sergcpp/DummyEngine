#pragma once

#include <cstdint>

#include "MMat.h"

namespace Ren {
enum class ePointPos { Front, Back, OnPlane };

struct Plane {
    Vec3f n;
    float d;

    Plane() : d(0.0f) {}
    Plane(const Ren::Vec3f &v0, const Ren::Vec3f &v1, const Ren::Vec3f &v2);
    explicit Plane(eUninitialized) : n(Uninitialize) {} // NOLINT

    ePointPos ClassifyPoint(const float point[3]) const;
};

enum class eCamPlane { Left, Right, Top, Bottom, Near, Far, _Count };

enum class eVisResult { Invisible, FullyVisible, PartiallyVisible };

struct Frustum {
    Plane planes[8] = {Plane{Ren::Uninitialize}, Plane{Ren::Uninitialize}, Plane{Ren::Uninitialize},
                       Plane{Ren::Uninitialize}, Plane{Ren::Uninitialize}, Plane{Ren::Uninitialize},
                       Plane{Ren::Uninitialize}, Plane{Ren::Uninitialize}};
    int planes_count = 6;

    void UpdateFromMatrix(const Mat4f &xform);

    eVisResult CheckVisibility(const Vec3f &point) const;
    eVisResult CheckVisibility(const float bbox[8][3]) const;
    eVisResult CheckVisibility(const Vec3f &bbox_min, const Vec3f &bbox_max) const;
};

class Camera {
  protected:
    Mat4f view_matrix_;
    Mat4f proj_matrix_;
    mutable Mat4f proj_matrix_offset_;

    Vec3f world_position_;
    uint32_t render_mask_ = 0xffffffff;

    Frustum frustum_, frustum_vs_;
    bool is_orthographic_ = false;

    float angle_ = 0.0f, aspect_ = 1.0f, near_ = 0.0f, far_ = 0.0f;
    mutable Vec2f px_offset_;

  public:
    float min_exposure = -1000.0f, max_exposure = 1000.0f;
    float focus_distance = 4.0f, focus_depth = 2.0f;
    float focus_near_mul = 0.0f, focus_far_mul = 0.0f;
    float fade = 0.0f;
    float gamma = 1.0f;

    Camera() = default;
    Camera(const Vec3f &center, const Vec3f &target, const Vec3f &up);

    const Mat4f &view_matrix() const { return view_matrix_; }
    const Mat4f &proj_matrix() const { return proj_matrix_; }
    const Mat4f &proj_matrix_offset() const { return proj_matrix_offset_; }

    const Vec3f &world_position() const { return world_position_; }
    Vec2f px_offset() const { return px_offset_; }

    Vec3f fwd() const { return Vec3f{-view_matrix_[0][2], -view_matrix_[1][2], -view_matrix_[2][2]}; }

    Vec3f up() const { return Vec3f{view_matrix_[0][1], view_matrix_[1][1], view_matrix_[2][1]}; }

    uint32_t render_mask() const { return render_mask_; }
    void set_render_mask(const uint32_t mask) { render_mask_ = mask; }

    Vec3f view_dir() const { return Vec3f{view_matrix_[0][2], view_matrix_[1][2], view_matrix_[2][2]}; }

    Vec4f clip_info() const { return Ren::Vec4f{near_ * far_, near_, far_, std::log2(1.0f + far_ / near_)}; }

    float angle() const { return angle_; }
    float aspect() const { return aspect_; }
    float near() const { return near_; }
    float far() const { return far_; }

    const Frustum &frustum() const { return frustum_; }

    const Plane &frustum_plane(const int i) const { return frustum_.planes[i]; }
    const Plane &frustum_plane(const eCamPlane i) const { return frustum_.planes[int(i)]; }

    const Plane &frustum_plane_vs(const int i) const { return frustum_vs_.planes[i]; }
    const Plane &frustum_plane_vs(const eCamPlane i) const { return frustum_vs_.planes[int(i)]; }

    void set_frustum_plane(const eCamPlane i, const Plane &plane) { frustum_.planes[int(i)] = plane; }

    bool is_orthographic() const { return is_orthographic_; }

    void Perspective(float angle, float aspect, float near, float far);
    void Orthographic(float left, float right, float top, float down, float near, float far);

    void SetupView(const Vec3f &center, const Vec3f &target, const Vec3f &up);
    void SetPxOffset(Vec2f px_offset) const;

    void UpdatePlanes();

    eVisResult CheckFrustumVisibility(const Vec3f &point) const;
    eVisResult CheckFrustumVisibility(const float bbox[8][3]) const;
    eVisResult CheckFrustumVisibility(const Vec3f &bbox_min, const Vec3f &bbox_max) const;

    // returns radius
    float GetBoundingSphere(Vec3f &out_center) const;

    void ExtractSubFrustums(int resx, int resy, int resz, Frustum sub_frustums[]) const;

    void Move(const Vec3f &v, float delta_time);
    void Rotate(float rx, float ry, float delta_time);
};
} // namespace Ren
