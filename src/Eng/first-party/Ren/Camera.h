#pragma once

#include <cstdint>

#include "MMat.h"

namespace Ren {
enum class eZRange { NegOneToOne, ZeroToOne, OneToZero };
enum class ePointPos { Front, Back, OnPlane };

struct Plane {
    Vec3f n;
    float d;

    Plane() : d(0) {}
    Plane(const Vec3f &v0, const Vec3f &v1, const Vec3f &v2);
    explicit Plane(eUninitialized) : n(Uninitialize) {} // NOLINT

    ePointPos ClassifyPoint(const float point[3]) const;
};

enum class eCamPlane { Left, Right, Top, Bottom, Near, Far, _Count };

enum class eVisResult { Invisible, FullyVisible, PartiallyVisible };

struct Frustum {
    Plane planes[8] = {Plane{Uninitialize}, Plane{Uninitialize}, Plane{Uninitialize},
                       Plane{Uninitialize}, Plane{Uninitialize}, Plane{Uninitialize},
                       Plane{Uninitialize}, Plane{Uninitialize}};
    int planes_count = 6;

    void UpdateFromMatrix(const Mat4f &xform);

    [[nodiscard]] eVisResult CheckVisibility(const Vec3f &point) const;
    [[nodiscard]] eVisResult CheckVisibility(const float bbox[8][3]) const;
    [[nodiscard]] eVisResult CheckVisibility(const Vec3f &bbox_min, const Vec3f &bbox_max) const;
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

    float angle_ = 0, aspect_ = 1, near_ = 0, far_ = 0;
    mutable Vec2f px_offset_;

  public:
    float min_exposure = -1000, max_exposure = 1000;
    float focus_distance = 4, focus_depth = 2;
    float focus_near_mul = 0, focus_far_mul = 0;
    float fade = 0;
    float gamma = 1;

    Camera() = default;
    Camera(const Vec3f &center, const Vec3f &target, const Vec3f &up);

    [[nodiscard]] const Mat4f &view_matrix() const { return view_matrix_; }
    [[nodiscard]] const Mat4f &proj_matrix() const { return proj_matrix_; }
    [[nodiscard]] const Mat4f &proj_matrix_offset() const { return proj_matrix_offset_; }

    [[nodiscard]] const Vec3f &world_position() const { return world_position_; }
    [[nodiscard]] Vec2f px_offset() const { return px_offset_; }

    [[nodiscard]] Vec3f fwd() const { return Vec3f{-view_matrix_[0][2], -view_matrix_[1][2], -view_matrix_[2][2]}; }
    [[nodiscard]] Vec3f up() const { return Vec3f{view_matrix_[0][1], view_matrix_[1][1], view_matrix_[2][1]}; }

    [[nodiscard]] uint32_t render_mask() const { return render_mask_; }
    void set_render_mask(const uint32_t mask) { render_mask_ = mask; }

    [[nodiscard]] Vec3f view_dir() const { return Vec3f{view_matrix_[0][2], view_matrix_[1][2], view_matrix_[2][2]}; }

    [[nodiscard]] Vec4f clip_info() const {
        return Vec4f{near_ * far_, near_, far_, std::log2(1.0f + far_ / near_)};
    }

    [[nodiscard]] float angle() const { return angle_; }
    [[nodiscard]] float aspect() const { return aspect_; }
    [[nodiscard]] float near() const { return near_; }
    [[nodiscard]] float far() const { return far_; }

    [[nodiscard]] const Frustum &frustum() const { return frustum_; }

    [[nodiscard]] const Plane &frustum_plane(const int i) const { return frustum_.planes[i]; }
    [[nodiscard]] const Plane &frustum_plane(const eCamPlane i) const { return frustum_.planes[int(i)]; }

    [[nodiscard]] const Plane &frustum_plane_vs(const int i) const { return frustum_vs_.planes[i]; }
    [[nodiscard]] const Plane &frustum_plane_vs(const eCamPlane i) const { return frustum_vs_.planes[int(i)]; }

    void set_frustum_plane(const eCamPlane i, const Plane &plane) { frustum_.planes[int(i)] = plane; }

    [[nodiscard]] bool is_orthographic() const { return is_orthographic_; }

    void Perspective(eZRange mode, float angle, float aspect, float near, float far);
    void Orthographic(eZRange mode, float left, float right, float top, float down, float near, float far);

    void SetupView(const Vec3f &center, const Vec3f &target, const Vec3f &up);
    void SetPxOffset(Vec2f px_offset) const;

    void UpdatePlanes();

    [[nodiscard]] eVisResult CheckFrustumVisibility(const Vec3f &point) const;
    [[nodiscard]] eVisResult CheckFrustumVisibility(const float bbox[8][3]) const;
    [[nodiscard]] eVisResult CheckFrustumVisibility(const Vec3f &bbox_min, const Vec3f &bbox_max) const;

    // returns radius
    float GetBoundingSphere(Vec3f &out_center) const;

    void ExtractSubFrustums(int resx, int resy, int resz, Frustum sub_frustums[]) const;

    void Move(const Vec3f &v, float delta_time);
    void Rotate(float rx, float ry, float delta_time);
};
} // namespace Ren
