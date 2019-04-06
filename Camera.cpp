#include "Camera.h"

Ren::Plane::Plane(const Ren::Vec3f &v0, const Ren::Vec3f &v1, const Ren::Vec3f &v2) : n(Uninitialize) {
    const Ren::Vec3f e1 = { v1[0] - v0[0], v1[1] - v0[1], v1[2] - v0[2] },
                     e2 = { v2[0] - v0[0], v2[1] - v0[1], v2[2] - v0[2] };

    n = Ren::Normalize(Ren::Cross(e1, e2));
    d = -(v0[0] * n[0] + v0[1] * n[1] + v0[2] * n[2]);
}

int Ren::Plane::ClassifyPoint(const float point[3]) const {
    const float epsilon = 0.002f;

    float result = Dot(MakeVec3(point), n) + d;
    if (result > epsilon) {
        return Front;
    } else if (result < -epsilon) {
        return Back;
    }
    return OnPlane;
}

Ren::eVisibilityResult Ren::Frustum::CheckVisibility(const float bbox[8][3]) const {
    eVisibilityResult res = FullyVisible;

    for (int pl = LeftPlane; pl <= FarPlane; pl++) {
        int in_count = 8;

        for (int i = 0; i < 8; i++) {
            switch (planes[pl].ClassifyPoint(&bbox[i][0])) {
            case Back:
                in_count--;
                break;
            }
        }
        if (in_count == 0) {
            res = Invisible;
            break;
        }

        if (in_count != 8) {
            res = PartiallyVisible;
        }
    }

    return res;
}

Ren::eVisibilityResult Ren::Frustum::CheckVisibility(const Vec3f &bbox_min, const Vec3f &bbox_max) const {
    const float bbox_points[8][3] = {
        bbox_min[0], bbox_min[1], bbox_min[2],
        bbox_max[0], bbox_min[1], bbox_min[2],
        bbox_min[0], bbox_min[1], bbox_max[2],
        bbox_max[0], bbox_min[1], bbox_max[2],
        bbox_min[0], bbox_max[1], bbox_min[2],
        bbox_max[0], bbox_max[1], bbox_min[2],
        bbox_min[0], bbox_max[1], bbox_max[2],
        bbox_max[0], bbox_max[1], bbox_max[2]
    };

    return CheckVisibility(bbox_points);
}

Ren::Camera::Camera(const Vec3f &center, const Vec3f &target, const Vec3f &up)
    : is_orthographic_(false), angle_(0.0f), aspect_(0.0f), near_(0.0f), far_(0.0f) {
    SetupView(center, target, up);
}

void Ren::Camera::SetupView(const Vec3f &center, const Vec3f &target, const Vec3f &up) {
    LookAt(view_matrix_, center, target, up);

    world_position_[0] = -Dot(view_matrix_[0], view_matrix_[3]);
    world_position_[1] = -Dot(view_matrix_[1], view_matrix_[3]);
    world_position_[2] = -Dot(view_matrix_[2], view_matrix_[3]);
}

void Ren::Camera::Perspective(float angle, float aspect, float nearr, float farr) {
    is_orthographic_ = false;
    angle_ = angle;
    aspect_ = aspect;
    near_ = nearr;
    far_ = farr;
    PerspectiveProjection(proj_matrix_, angle, aspect, nearr, farr);
}

void Ren::Camera::Orthographic(float left, float right, float top, float down, float nearr, float farr) {
    is_orthographic_ = true;
    OrthographicProjection(proj_matrix_, left, right, top, down, nearr, farr);
}

void Ren::Camera::Move(const Vec3f &v, float delta_time) {
    view_matrix_[3][0] -= v[0] * delta_time;
    view_matrix_[3][1] -= v[1] * delta_time;
    view_matrix_[3][2] -= v[2] * delta_time;

    //world_position_[0] = -(view_matrix_[0]*view_matrix_[12] + view_matrix_[1]*view_matrix_[13] + view_matrix_[2]*view_matrix_[14]);
    //world_position_[1] = -(view_matrix_[4]*view_matrix_[12] + view_matrix_[5]*view_matrix_[13] + view_matrix_[6]*view_matrix_[14]);
    //world_position_[2] = -(view_matrix_[8]*view_matrix_[12] + view_matrix_[9]*view_matrix_[13] + view_matrix_[10]*view_matrix_[14]);
}

void Ren::Camera::Rotate(float rx, float ry, float delta_time) {
    Vec3f front;
    front[0] = -view_matrix_[0][2];
    front[1] = -view_matrix_[1][2];
    front[2] = -view_matrix_[2][2];

    Mat4f rot_matrix(1.0f);

    rot_matrix = Ren::Rotate(rot_matrix, rx * delta_time, Vec3f{ view_matrix_[0][0], view_matrix_[1][0], view_matrix_[2][0] });
    rot_matrix = Ren::Rotate(rot_matrix, ry * delta_time, Vec3f{ view_matrix_[0][1], view_matrix_[1][1], view_matrix_[2][1] });

    Vec3f tr_front;

    tr_front[0] = Dot(front, Vec3f{ view_matrix_[0][0], view_matrix_[0][1], view_matrix_[0][2] });
    tr_front[1] = Dot(front, Vec3f{ view_matrix_[1][0], view_matrix_[1][1], view_matrix_[1][2] });
    tr_front[2] = Dot(front, Vec3f{ view_matrix_[2][0], view_matrix_[2][1], view_matrix_[2][2] });

    LookAt(view_matrix_, world_position_, world_position_ + tr_front, Vec3f{ 0.0f, 1.0f, 0.0f });
}

void Ren::Camera::UpdatePlanes() {
    Mat4f combo_matrix = proj_matrix_ * view_matrix_;

    frustum_.planes[LeftPlane].n[0] = combo_matrix[0][3] + combo_matrix[0][0];
    frustum_.planes[LeftPlane].n[1] = combo_matrix[1][3] + combo_matrix[1][0];
    frustum_.planes[LeftPlane].n[2] = combo_matrix[2][3] + combo_matrix[2][0];
    frustum_.planes[LeftPlane].d = combo_matrix[3][3] + combo_matrix[3][0];

    frustum_.planes[RightPlane].n[0] = combo_matrix[0][3] - combo_matrix[0][0];
    frustum_.planes[RightPlane].n[1] = combo_matrix[1][3] - combo_matrix[1][0];
    frustum_.planes[RightPlane].n[2] = combo_matrix[2][3] - combo_matrix[2][0];
    frustum_.planes[RightPlane].d = combo_matrix[3][3] - combo_matrix[3][0];

    frustum_.planes[TopPlane].n[0] = combo_matrix[0][3] - combo_matrix[0][1];
    frustum_.planes[TopPlane].n[1] = combo_matrix[1][3] - combo_matrix[1][1];
    frustum_.planes[TopPlane].n[2] = combo_matrix[2][3] - combo_matrix[2][1];
    frustum_.planes[TopPlane].d = combo_matrix[3][3] - combo_matrix[3][1];

    frustum_.planes[BottomPlane].n[0] = combo_matrix[0][3] + combo_matrix[0][1];
    frustum_.planes[BottomPlane].n[1] = combo_matrix[1][3] + combo_matrix[1][1];
    frustum_.planes[BottomPlane].n[2] = combo_matrix[2][3] + combo_matrix[2][1];
    frustum_.planes[BottomPlane].d = combo_matrix[3][3] + combo_matrix[3][1];

    frustum_.planes[NearPlane].n[0] = combo_matrix[0][3] + combo_matrix[0][2];
    frustum_.planes[NearPlane].n[1] = combo_matrix[1][3] + combo_matrix[1][2];
    frustum_.planes[NearPlane].n[2] = combo_matrix[2][3] + combo_matrix[2][2];
    frustum_.planes[NearPlane].d = combo_matrix[3][3] + combo_matrix[3][2];

    frustum_.planes[FarPlane].n[0] = combo_matrix[0][3] - combo_matrix[0][2];
    frustum_.planes[FarPlane].n[1] = combo_matrix[1][3] - combo_matrix[1][2];
    frustum_.planes[FarPlane].n[2] = combo_matrix[2][3] - combo_matrix[2][2];
    frustum_.planes[FarPlane].d = combo_matrix[3][3] - combo_matrix[3][2];

    for (int pl = LeftPlane; pl <= FarPlane; pl++) {
        float inv_l = 1.0f
                      / std::sqrt(
                          frustum_.planes[pl].n[0] * frustum_.planes[pl].n[0]
                          + frustum_.planes[pl].n[1] * frustum_.planes[pl].n[1]
                          + frustum_.planes[pl].n[2] * frustum_.planes[pl].n[2]);
        frustum_.planes[pl].n[0] *= inv_l;
        frustum_.planes[pl].n[1] *= inv_l;
        frustum_.planes[pl].n[2] *= inv_l;
        frustum_.planes[pl].d *= inv_l;
    }

    world_position_[0] = -Dot(view_matrix_[0], view_matrix_[3]);
    world_position_[1] = -Dot(view_matrix_[1], view_matrix_[3]);
    world_position_[2] = -Dot(view_matrix_[2], view_matrix_[3]);
}

Ren::eVisibilityResult Ren::Camera::CheckFrustumVisibility(const float bbox[8][3]) const {
    return frustum_.CheckVisibility(bbox);
}

Ren::eVisibilityResult Ren::Camera::CheckFrustumVisibility(const Vec3f &bbox_min, const Vec3f &bbox_max) const {
    return frustum_.CheckVisibility(bbox_min, bbox_max);
}

float Ren::Camera::GetBoundingSphere(Vec3f &out_center) const {
    float f = far_,
          n = near_;

    Vec3f fwd = Vec3f{ -view_matrix_[0][2], -view_matrix_[1][2], -view_matrix_[2][2] };

    float k = std::sqrt(1 + (1.0f / (aspect_ * aspect_))) * aspect_ * std::tan(0.5f * angle_ * Ren::Pi<float>() / 180.0f);
    float k_sqr = k * k;
    if (k_sqr >= (f - n) / (f + n)) {
        out_center = world_position_ + fwd * f;
        return f * k;
    } else {
        out_center = world_position_ + fwd * 0.5f * (f + n) * (1 + k_sqr);
        return 0.5f * std::sqrt((f - n) * (f - n) + 2.0f * (f * f + n * n) * k_sqr + (f + n) * (f + n) * k_sqr * k_sqr);
    }
}

void Ren::Camera::ExtractSubFrustums(int resx, int resy, int resz, Frustum *sub_frustums) const {
    // grid size by x and y in clip space
    const float grid_size_cs[2] = { 2.0f / resx, 2.0f / resy };

    const Mat4f world_from_clip = Ren::Inverse(proj_matrix_ * view_matrix_);

    {   // Construct cells for the first depth slice
        const float znear = near_,
                    zfar = near_ * std::pow(far_ / near_, 1.0f / resz);

        for (int y = 0; y < resy; y++) {
            float ybot = -1.0f + y * grid_size_cs[1],
                  ytop = -1.0f + (y + 1) * grid_size_cs[1];

            for (int x = 0; x < resx; x++) {
                Ren::Vec4f p0 = { -1.0f + x * grid_size_cs[0],        ybot, 0.0f, 1.0f },
                           p1 = { -1.0f + x * grid_size_cs[0],        ytop, 0.0f, 1.0f },
                           p2 = { -1.0f + (x + 1) * grid_size_cs[0],  ytop, 0.0f, 1.0f },
                           p3 = { -1.0f + (x + 1) * grid_size_cs[0],  ybot, 0.0f, 1.0f };

                p0 = world_from_clip * p0;
                p1 = world_from_clip * p1;
                p2 = world_from_clip * p2;
                p3 = world_from_clip * p3;

                const Ren::Vec3f _p0 = Ren::Vec3f{ p0 / p0[3] },
                                 _p1 = Ren::Vec3f{ p1 / p1[3] },
                                 _p2 = Ren::Vec3f{ p2 / p2[3] },
                                 _p3 = Ren::Vec3f{ p3 / p3[3] };

                Ren::Frustum &sf = sub_frustums[y * resx + x];
                sf.planes[Ren::LeftPlane] = { world_position_, _p0, _p1 };
                sf.planes[Ren::RightPlane] = { world_position_, _p2, _p3 };
                sf.planes[Ren::TopPlane] = { world_position_, _p1, _p2 };
                sf.planes[Ren::BottomPlane] = { world_position_, _p3, _p0 };
                sf.planes[Ren::NearPlane] = frustum_.planes[Ren::NearPlane];
                sf.planes[Ren::NearPlane].d -= (znear - near_);
                sf.planes[Ren::FarPlane] = frustum_.planes[Ren::FarPlane];
                sf.planes[Ren::FarPlane].d = -frustum_.planes[Ren::NearPlane].d + (zfar - near_);
            }
        }
    }

    // Construct cells for the rest slices
    for (int z = 1; z < resz; z++) {
        const float znear = near_ * std::pow(far_ / near_, float(z) / resz),
                    zfar = near_ * std::pow(far_ / near_, float(z + 1) / resz);

        memcpy(&sub_frustums[z * resy * resx], &sub_frustums[0], resy * resx * sizeof(Frustum));

        for (int i = 0; i < resy * resx; i++) {
            Ren::Frustum &sf = sub_frustums[z * resy * resx + i];
            sf.planes[Ren::NearPlane].d -= (znear - near_);
            sf.planes[Ren::FarPlane] = frustum_.planes[Ren::FarPlane];
            sf.planes[Ren::FarPlane].d = -frustum_.planes[Ren::NearPlane].d + (zfar - near_);
        }
    }
}