#include "Camera.h"

Ren::Plane::Plane(const Ren::Vec3f &v0, const Ren::Vec3f &v1, const Ren::Vec3f &v2)
    : n(Uninitialize) {
    const Ren::Vec3f e1 = v1 - v0, e2 = v2 - v0;

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

Ren::eVisResult Ren::Frustum::CheckVisibility(const Vec3f &point) const {
    for (int pl = 0; pl < planes_count; pl++) {
        if (planes[pl].ClassifyPoint(&point[0]) == Back) {
            return eVisResult::Invisible;
        }
    }

    return eVisResult::FullyVisible;
}

Ren::eVisResult Ren::Frustum::CheckVisibility(const float bbox[8][3]) const {
    eVisResult res = eVisResult::FullyVisible;

    for (int pl = 0; pl < planes_count; pl++) {
        int in_count = 8;

        for (int i = 0; i < 8; i++) {
            if (planes[pl].ClassifyPoint(&bbox[i][0]) == Back) {
                in_count--;
            }
        }
        if (in_count == 0) {
            res = eVisResult::Invisible;
            break;
        }

        if (in_count != 8) {
            res = eVisResult::PartiallyVisible;
        }
    }

    return res;
}

Ren::eVisResult Ren::Frustum::CheckVisibility(const Vec3f &bbox_min,
                                              const Vec3f &bbox_max) const {
    const float epsilon = 0.002f;
    const float *_bbox_min = ValuePtr(bbox_min), *_bbox_max = ValuePtr(bbox_max);

    eVisResult res = eVisResult::FullyVisible;

    for (int pl = 0; pl < planes_count; pl++) {
        int in_count = 8;

        const float *p_n = ValuePtr(planes[pl].n);
        const float p_d = planes[pl].d;

        if (p_n[0] * _bbox_min[0] + p_n[1] * _bbox_min[1] + p_n[2] * _bbox_min[2] + p_d <
            -epsilon)
            --in_count;
        if (p_n[0] * _bbox_max[0] + p_n[1] * _bbox_min[1] + p_n[2] * _bbox_min[2] + p_d <
            -epsilon)
            --in_count;
        if (p_n[0] * _bbox_min[0] + p_n[1] * _bbox_min[1] + p_n[2] * _bbox_max[2] + p_d <
            -epsilon)
            --in_count;
        if (p_n[0] * _bbox_max[0] + p_n[1] * _bbox_min[1] + p_n[2] * _bbox_max[2] + p_d <
            -epsilon)
            --in_count;
        if (p_n[0] * _bbox_min[0] + p_n[1] * _bbox_max[1] + p_n[2] * _bbox_min[2] + p_d <
            -epsilon)
            --in_count;
        if (p_n[0] * _bbox_max[0] + p_n[1] * _bbox_max[1] + p_n[2] * _bbox_min[2] + p_d <
            -epsilon)
            --in_count;
        if (p_n[0] * _bbox_min[0] + p_n[1] * _bbox_max[1] + p_n[2] * _bbox_max[2] + p_d <
            -epsilon)
            --in_count;
        if (p_n[0] * _bbox_max[0] + p_n[1] * _bbox_max[1] + p_n[2] * _bbox_max[2] + p_d <
            -epsilon)
            --in_count;

        if (in_count == 0) {
            res = eVisResult::Invisible;
            break;
        }

        if (in_count != 8) {
            res = eVisResult::PartiallyVisible;
        }
    }

    return res;
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

void Ren::Camera::SetPxOffset(const Vec2f px_offset) const {
    px_offset_ = px_offset;
    proj_matrix_offset_ = proj_matrix_;

    if (!is_orthographic_) {
        proj_matrix_offset_[2][0] += px_offset[0];
        proj_matrix_offset_[2][1] += px_offset[1];
    } else {
        // not implemented
    }
}

void Ren::Camera::Perspective(float angle, float aspect, float nearr, float farr) {
    is_orthographic_ = false;
    angle_ = angle;
    aspect_ = aspect;
    near_ = nearr;
    far_ = farr;
    PerspectiveProjection(proj_matrix_, angle, aspect, nearr, farr);
}

void Ren::Camera::Orthographic(float left, float right, float top, float down,
                               float nearr, float farr) {
    is_orthographic_ = true;
    near_ = nearr;
    far_ = farr;
    OrthographicProjection(proj_matrix_, left, right, top, down, nearr, farr);
}

void Ren::Camera::Move(const Vec3f &v, const float delta_time) {
    view_matrix_[3][0] -= v[0] * delta_time;
    view_matrix_[3][1] -= v[1] * delta_time;
    view_matrix_[3][2] -= v[2] * delta_time;
}

void Ren::Camera::Rotate(float rx, float ry, float delta_time) {
    Vec3f front;
    front[0] = -view_matrix_[0][2];
    front[1] = -view_matrix_[1][2];
    front[2] = -view_matrix_[2][2];

    Vec3f tr_front;

    tr_front[0] =
        Dot(front, Vec3f{view_matrix_[0][0], view_matrix_[0][1], view_matrix_[0][2]});
    tr_front[1] =
        Dot(front, Vec3f{view_matrix_[1][0], view_matrix_[1][1], view_matrix_[1][2]});
    tr_front[2] =
        Dot(front, Vec3f{view_matrix_[2][0], view_matrix_[2][1], view_matrix_[2][2]});

    LookAt(view_matrix_, world_position_, world_position_ + tr_front,
           Vec3f{0.0f, 1.0f, 0.0f});
}

void Ren::Camera::UpdatePlanes() {
    Mat4f combo_mat = proj_matrix_ * view_matrix_;

    frustum_.planes[(int)eCamPlane::LeftPlane].n[0] = combo_mat[0][3] + combo_mat[0][0];
    frustum_.planes[(int)eCamPlane::LeftPlane].n[1] = combo_mat[1][3] + combo_mat[1][0];
    frustum_.planes[(int)eCamPlane::LeftPlane].n[2] = combo_mat[2][3] + combo_mat[2][0];
    frustum_.planes[(int)eCamPlane::LeftPlane].d = combo_mat[3][3] + combo_mat[3][0];

    frustum_.planes[(int)eCamPlane::RightPlane].n[0] = combo_mat[0][3] - combo_mat[0][0];
    frustum_.planes[(int)eCamPlane::RightPlane].n[1] = combo_mat[1][3] - combo_mat[1][0];
    frustum_.planes[(int)eCamPlane::RightPlane].n[2] = combo_mat[2][3] - combo_mat[2][0];
    frustum_.planes[(int)eCamPlane::RightPlane].d = combo_mat[3][3] - combo_mat[3][0];

    frustum_.planes[(int)eCamPlane::TopPlane].n[0] = combo_mat[0][3] - combo_mat[0][1];
    frustum_.planes[(int)eCamPlane::TopPlane].n[1] = combo_mat[1][3] - combo_mat[1][1];
    frustum_.planes[(int)eCamPlane::TopPlane].n[2] = combo_mat[2][3] - combo_mat[2][1];
    frustum_.planes[(int)eCamPlane::TopPlane].d = combo_mat[3][3] - combo_mat[3][1];

    frustum_.planes[(int)eCamPlane::BottomPlane].n[0] = combo_mat[0][3] + combo_mat[0][1];
    frustum_.planes[(int)eCamPlane::BottomPlane].n[1] = combo_mat[1][3] + combo_mat[1][1];
    frustum_.planes[(int)eCamPlane::BottomPlane].n[2] = combo_mat[2][3] + combo_mat[2][1];
    frustum_.planes[(int)eCamPlane::BottomPlane].d = combo_mat[3][3] + combo_mat[3][1];

    frustum_.planes[(int)eCamPlane::NearPlane].n[0] = combo_mat[0][3] + combo_mat[0][2];
    frustum_.planes[(int)eCamPlane::NearPlane].n[1] = combo_mat[1][3] + combo_mat[1][2];
    frustum_.planes[(int)eCamPlane::NearPlane].n[2] = combo_mat[2][3] + combo_mat[2][2];
    frustum_.planes[(int)eCamPlane::NearPlane].d = combo_mat[3][3] + combo_mat[3][2];

    frustum_.planes[(int)eCamPlane::FarPlane].n[0] = combo_mat[0][3] - combo_mat[0][2];
    frustum_.planes[(int)eCamPlane::FarPlane].n[1] = combo_mat[1][3] - combo_mat[1][2];
    frustum_.planes[(int)eCamPlane::FarPlane].n[2] = combo_mat[2][3] - combo_mat[2][2];
    frustum_.planes[(int)eCamPlane::FarPlane].d = combo_mat[3][3] - combo_mat[3][2];

    for (int pl = (int)eCamPlane::LeftPlane; pl <= (int)eCamPlane::FarPlane; pl++) {
        float inv_l =
            1.0f / std::sqrt(frustum_.planes[pl].n[0] * frustum_.planes[pl].n[0] +
                             frustum_.planes[pl].n[1] * frustum_.planes[pl].n[1] +
                             frustum_.planes[pl].n[2] * frustum_.planes[pl].n[2]);
        frustum_.planes[pl].n[0] *= inv_l;
        frustum_.planes[pl].n[1] *= inv_l;
        frustum_.planes[pl].n[2] *= inv_l;
        frustum_.planes[pl].d *= inv_l;
    }

    frustum_.planes_count = 6;

    world_position_[0] = -Dot(view_matrix_[0], view_matrix_[3]);
    world_position_[1] = -Dot(view_matrix_[1], view_matrix_[3]);
    world_position_[2] = -Dot(view_matrix_[2], view_matrix_[3]);
}

Ren::eVisResult Ren::Camera::CheckFrustumVisibility(const Vec3f &point) const {
    return frustum_.CheckVisibility(point);
}

Ren::eVisResult Ren::Camera::CheckFrustumVisibility(const float bbox[8][3]) const {
    return frustum_.CheckVisibility(bbox);
}

Ren::eVisResult Ren::Camera::CheckFrustumVisibility(const Vec3f &bbox_min,
                                                    const Vec3f &bbox_max) const {
    return frustum_.CheckVisibility(bbox_min, bbox_max);
}

float Ren::Camera::GetBoundingSphere(Vec3f &out_center) const {
    float f = far_, n = near_;

    Vec3f fwd = Vec3f{-view_matrix_[0][2], -view_matrix_[1][2], -view_matrix_[2][2]};

    float k = std::sqrt(1 + (1.0f / (aspect_ * aspect_))) * aspect_ *
              std::tan(0.5f * angle_ * Ren::Pi<float>() / 180.0f);
    float k_sqr = k * k;
    if (k_sqr >= (f - n) / (f + n)) {
        out_center = world_position_ + fwd * f;
        return f * k;
    } else {
        out_center = world_position_ + fwd * 0.5f * (f + n) * (1 + k_sqr);
        return 0.5f * std::sqrt((f - n) * (f - n) + 2.0f * (f * f + n * n) * k_sqr +
                                (f + n) * (f + n) * k_sqr * k_sqr);
    }
}

void Ren::Camera::ExtractSubFrustums(int resx, int resy, int resz,
                                     Frustum *sub_frustums) const {
    // grid size by x and y in clip space
    const float grid_size_cs[2] = {2.0f / (float)resx, 2.0f / (float)resy};

    const Mat4f world_from_clip = Ren::Inverse(proj_matrix_ * view_matrix_);

    { // Construct cells for the first depth slice
        const float znear = near_,
                    zfar = near_ * std::pow(far_ / near_, 1.0f / (float)resz);

        for (int y = 0; y < resy; y++) {
            float ybot = -1.0f + (float)y * grid_size_cs[1],
                  ytop = -1.0f + float(y + 1) * grid_size_cs[1];

            for (int x = 0; x < resx; x++) {
                auto p0 =
                         Ren::Vec4f{-1.0f + (float)x * grid_size_cs[0], ybot, 0.0f, 1.0f},
                     p1 =
                         Ren::Vec4f{-1.0f + (float)x * grid_size_cs[0], ytop, 0.0f, 1.0f},
                     p2 = Ren::Vec4f{-1.0f + float(x + 1) * grid_size_cs[0], ytop, 0.0f,
                                     1.0f},
                     p3 = Ren::Vec4f{-1.0f + float(x + 1) * grid_size_cs[0], ybot, 0.0f,
                                     1.0f};

                p0 = world_from_clip * p0;
                p1 = world_from_clip * p1;
                p2 = world_from_clip * p2;
                p3 = world_from_clip * p3;

                const Ren::Vec3f _p0 = Ren::Vec3f{p0 / p0[3]},
                                 _p1 = Ren::Vec3f{p1 / p1[3]},
                                 _p2 = Ren::Vec3f{p2 / p2[3]},
                                 _p3 = Ren::Vec3f{p3 / p3[3]};

                Ren::Frustum &sf = sub_frustums[y * resx + x];
                sf.planes[(int)eCamPlane::LeftPlane] = {world_position_, _p0, _p1};
                sf.planes[(int)eCamPlane::RightPlane] = {world_position_, _p2, _p3};
                sf.planes[(int)eCamPlane::TopPlane] = {world_position_, _p1, _p2};
                sf.planes[(int)eCamPlane::BottomPlane] = {world_position_, _p3, _p0};
                sf.planes[(int)eCamPlane::NearPlane] =
                    frustum_.planes[(int)eCamPlane::NearPlane];
                sf.planes[(int)eCamPlane::NearPlane].d -= (znear - near_);
                sf.planes[(int)eCamPlane::FarPlane] =
                    frustum_.planes[(int)eCamPlane::FarPlane];
                sf.planes[(int)eCamPlane::FarPlane].d =
                    -frustum_.planes[(int)eCamPlane::NearPlane].d + (zfar - near_);
            }
        }
    }

    // Construct cells for the rest slices
    for (int z = 1; z < resz; z++) {
        const float znear = near_ * std::pow(far_ / near_, float(z) / float(resz)),
                    zfar = near_ * std::pow(far_ / near_, float(z + 1) / float(resz));

        memcpy(&sub_frustums[z * resy * resx], &sub_frustums[0],
               resy * resx * sizeof(Frustum));

        for (int i = 0; i < resy * resx; i++) {
            Ren::Frustum &sf = sub_frustums[z * resy * resx + i];
            sf.planes[(int)eCamPlane::NearPlane].d -= (znear - near_);
            sf.planes[(int)eCamPlane::FarPlane] =
                frustum_.planes[(int)eCamPlane::FarPlane];
            sf.planes[(int)eCamPlane::FarPlane].d =
                -frustum_.planes[(int)eCamPlane::NearPlane].d + (zfar - near_);
        }
    }
}