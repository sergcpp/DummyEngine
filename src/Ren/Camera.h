#pragma once

#include "MMat.h"

namespace Ren {
enum ePointPos { Front, Back, OnPlane };

struct Plane {
    Vec3f   n;
    float   d;

    Plane() : d(0.0f) {}
    Plane(const Ren::Vec3f &v0, const Ren::Vec3f &v1, const Ren::Vec3f &v2);
    Plane(eUninitialized) : n(Uninitialize) {}

    int ClassifyPoint(const float point[3]) const;
};

enum eCamPlane {
    LeftPlane, RightPlane, TopPlane, BottomPlane, NearPlane, FarPlane
};

enum eVisibilityResult { Invisible, FullyVisible, PartiallyVisible };

struct Frustum {
    Ren::Plane planes[8] = { { Ren::Uninitialize }, { Ren::Uninitialize }, { Ren::Uninitialize }, { Ren::Uninitialize },
                             { Ren::Uninitialize }, { Ren::Uninitialize }, { Ren::Uninitialize }, { Ren::Uninitialize } };
    int planes_count = 6;

    eVisibilityResult CheckVisibility(const Vec3f &point) const;
    eVisibilityResult CheckVisibility(const float bbox[8][3]) const;
    eVisibilityResult CheckVisibility(const Vec3f &bbox_min, const Vec3f &bbox_max) const;
};

class Camera {
protected:
    Mat4f view_matrix_;
    Mat4f proj_matrix_;

    Vec3f world_position_;

    Frustum frustum_;
    bool is_orthographic_;

    float angle_, aspect_, near_, far_;
public:
    Camera() {}
    Camera(const Vec3f &center, const Vec3f &target, const Vec3f &up);

    const Mat4f &view_matrix() const {
        return view_matrix_;
    }
    const Mat4f &proj_matrix() const {
        return proj_matrix_;
    }

    const Vec3f &world_position() const {
        return world_position_;
    }

    Vec3f view_dir() const {
        return Vec3f{ view_matrix_[0][2], view_matrix_[1][2], view_matrix_[2][2] };
    }

    float angle() const {
        return angle_;
    }
    float aspect() const {
        return aspect_;
    }
    float near() const {
        return near_;
    }
    float far() const {
        return far_;
    }

    const Plane &frustum_plane(int i) const {
        return frustum_.planes[i];
    }

    const bool is_orthographic() const {
        return is_orthographic_;
    }

    void Perspective(float angle, float aspect, float near, float far);
    void Orthographic(float left, float right, float top, float down, float near, float far);

    void SetupView(const Vec3f &center, const Vec3f &target, const Vec3f &up);

    void UpdatePlanes();

    eVisibilityResult CheckFrustumVisibility(const Vec3f &point) const;
    eVisibilityResult CheckFrustumVisibility(const float bbox[8][3]) const;
    eVisibilityResult CheckFrustumVisibility(const Vec3f &bbox_min, const Vec3f &bbox_max) const;

    // returns radius
    float GetBoundingSphere(Vec3f &out_center) const;

    void ExtractSubFrustums(int resx, int resy, int resz, Frustum *sub_frustums) const;

    void Move(const Vec3f &v, float delta_time);
    void Rotate(float rx, float ry, float delta_time);
};
}
