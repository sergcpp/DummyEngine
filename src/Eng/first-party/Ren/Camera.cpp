#include "Camera.h"

#include <cstring>

Ren::Plane::Plane(const Vec3f &v0, const Vec3f &v1, const Vec3f &v2) : n(Uninitialize) {
    const Vec3f e1 = v1 - v0, e2 = v2 - v0;

    n = Normalize(Cross(e1, e2));
    // d = -(v0[0] * n[0] + v0[1] * n[1] + v0[2] * n[2]);
    d = -Dot(v0, n);
}

Ren::ePointPos Ren::Plane::ClassifyPoint(const float point[3], const float eps) const {
    float result = Dot(MakeVec3(point), n) + d;
    if (result > eps) {
        return ePointPos::Front;
    } else if (result < -eps) {
        return ePointPos::Back;
    }
    return ePointPos::OnPlane;
}

void Ren::Frustum::UpdateFromMatrix(const Mat4f &xform) {
    planes[int(eCamPlane::Left)].n[0] = xform[0][3] + xform[0][0];
    planes[int(eCamPlane::Left)].n[1] = xform[1][3] + xform[1][0];
    planes[int(eCamPlane::Left)].n[2] = xform[2][3] + xform[2][0];
    planes[int(eCamPlane::Left)].d = xform[3][3] + xform[3][0];

    planes[int(eCamPlane::Right)].n[0] = xform[0][3] - xform[0][0];
    planes[int(eCamPlane::Right)].n[1] = xform[1][3] - xform[1][0];
    planes[int(eCamPlane::Right)].n[2] = xform[2][3] - xform[2][0];
    planes[int(eCamPlane::Right)].d = xform[3][3] - xform[3][0];

    planes[int(eCamPlane::Top)].n[0] = xform[0][3] - xform[0][1];
    planes[int(eCamPlane::Top)].n[1] = xform[1][3] - xform[1][1];
    planes[int(eCamPlane::Top)].n[2] = xform[2][3] - xform[2][1];
    planes[int(eCamPlane::Top)].d = xform[3][3] - xform[3][1];

    planes[int(eCamPlane::Bottom)].n[0] = xform[0][3] + xform[0][1];
    planes[int(eCamPlane::Bottom)].n[1] = xform[1][3] + xform[1][1];
    planes[int(eCamPlane::Bottom)].n[2] = xform[2][3] + xform[2][1];
    planes[int(eCamPlane::Bottom)].d = xform[3][3] + xform[3][1];

    planes[int(eCamPlane::Far)].n[0] = xform[0][2];
    planes[int(eCamPlane::Far)].n[1] = xform[1][2];
    planes[int(eCamPlane::Far)].n[2] = xform[2][2];
    planes[int(eCamPlane::Far)].d = xform[3][2];

    /*planes[int(eCamPlane::Near)].n[0] = xform[0][3] + xform[0][2];
    planes[int(eCamPlane::Near)].n[1] = xform[1][3] + xform[1][2];
    planes[int(eCamPlane::Near)].n[2] = xform[2][3] + xform[2][2];
    planes[int(eCamPlane::Near)].d = xform[3][3] + xform[3][2];*/

    planes[int(eCamPlane::Near)].n[0] = xform[0][3] - xform[0][2];
    planes[int(eCamPlane::Near)].n[1] = xform[1][3] - xform[1][2];
    planes[int(eCamPlane::Near)].n[2] = xform[2][3] - xform[2][2];
    planes[int(eCamPlane::Near)].d = xform[3][3] - xform[3][2];

    for (int pl = 0; pl < int(eCamPlane::_Count); pl++) {
        const float inv_l = 1.0f / std::sqrt(planes[pl].n[0] * planes[pl].n[0] + planes[pl].n[1] * planes[pl].n[1] +
                                             planes[pl].n[2] * planes[pl].n[2]);
        planes[pl].n[0] *= inv_l;
        planes[pl].n[1] *= inv_l;
        planes[pl].n[2] *= inv_l;
        planes[pl].d *= inv_l;
    }

    planes_count = 6;
}

Ren::eVisResult Ren::Frustum::CheckVisibility(const Vec3f &point) const {
    for (int pl = 0; pl < planes_count; pl++) {
        if (planes[pl].ClassifyPoint(&point[0]) == ePointPos::Back) {
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
            if (planes[pl].ClassifyPoint(&bbox[i][0]) == ePointPos::Back) {
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

Ren::eVisResult Ren::Frustum::CheckVisibility(const Vec3f &bbox_min, const Vec3f &bbox_max) const {
    const float epsilon = 0.002f;
    const float *_bbox_min = ValuePtr(bbox_min), *_bbox_max = ValuePtr(bbox_max);

    eVisResult res = eVisResult::FullyVisible;

    for (int pl = 0; pl < planes_count; pl++) {
        int in_count = 8;

        const float *p_n = ValuePtr(planes[pl].n);
        const float p_d = planes[pl].d;

        if (p_n[0] * _bbox_min[0] + p_n[1] * _bbox_min[1] + p_n[2] * _bbox_min[2] + p_d < -epsilon) {
            --in_count;
        }
        if (p_n[0] * _bbox_max[0] + p_n[1] * _bbox_min[1] + p_n[2] * _bbox_min[2] + p_d < -epsilon) {
            --in_count;
        }
        if (p_n[0] * _bbox_min[0] + p_n[1] * _bbox_min[1] + p_n[2] * _bbox_max[2] + p_d < -epsilon) {
            --in_count;
        }
        if (p_n[0] * _bbox_max[0] + p_n[1] * _bbox_min[1] + p_n[2] * _bbox_max[2] + p_d < -epsilon) {
            --in_count;
        }
        if (p_n[0] * _bbox_min[0] + p_n[1] * _bbox_max[1] + p_n[2] * _bbox_min[2] + p_d < -epsilon) {
            --in_count;
        }
        if (p_n[0] * _bbox_max[0] + p_n[1] * _bbox_max[1] + p_n[2] * _bbox_min[2] + p_d < -epsilon) {
            --in_count;
        }
        if (p_n[0] * _bbox_min[0] + p_n[1] * _bbox_max[1] + p_n[2] * _bbox_max[2] + p_d < -epsilon) {
            --in_count;
        }
        if (p_n[0] * _bbox_max[0] + p_n[1] * _bbox_max[1] + p_n[2] * _bbox_max[2] + p_d < -epsilon) {
            --in_count;
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

Ren::Camera::Camera(const Vec3f &center, const Vec3f &target, const Vec3f &up)
    : is_orthographic_(false), angle_(0), aspect_(0), near_(0), far_(0) {
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

void Ren::Camera::Perspective(const eZRange mode, const float angle, const float aspect, const float nearr,
                              const float farr, const Vec2f sensor_shift) {
    is_orthographic_ = false;
    angle_ = angle;
    aspect_ = aspect;
    near_ = nearr;
    far_ = farr;
    sensor_shift_ = sensor_shift;
    proj_matrix_ = PerspectiveProjection(angle, aspect, nearr, farr, mode != eZRange::NegOneToOne);
    proj_matrix_[2][0] += 2 * sensor_shift_[0] / aspect_;
    proj_matrix_[2][1] += 2 * sensor_shift_[1];
    if (mode == eZRange::OneToZero) {
        const Mat4f reverse_z = Mat4f{Vec4f{1, 0, 0, 0},  //
                                      Vec4f{0, 1, 0, 0},  //
                                      Vec4f{0, 0, -1, 0}, //
                                      Vec4f{0, 0, 1, 1}};
        proj_matrix_ = reverse_z * proj_matrix_;
    }
}

void Ren::Camera::Orthographic(const eZRange mode, const float left, const float right, const float top,
                               const float down, const float nearr, const float farr) {
    is_orthographic_ = true;
    near_ = nearr;
    far_ = farr;
    proj_matrix_ = OrthographicProjection(left, right, top, down, nearr, farr, mode != eZRange::NegOneToOne);
    if (mode == eZRange::OneToZero) {
        const Mat4f reverse_z = Mat4f{Vec4f{1, 0, 0, 0},  //
                                      Vec4f{0, 1, 0, 0},  //
                                      Vec4f{0, 0, -1, 0}, //
                                      Vec4f{0, 0, 1, 1}};
        proj_matrix_ = reverse_z * proj_matrix_;
    }
}

void Ren::Camera::Move(const Vec3f &v, const float delta_time) {
    view_matrix_[3][0] -= v[0] * delta_time;
    view_matrix_[3][1] -= v[1] * delta_time;
    view_matrix_[3][2] -= v[2] * delta_time;
}

void Ren::Camera::Rotate(const float rx, const float ry, const float delta_time) {
    const auto front = Vec3f{-view_matrix_[0][2], -view_matrix_[1][2], -view_matrix_[2][2]};

    Vec3f tr_front;

    tr_front[0] = Dot(front, Vec3f{view_matrix_[0][0], view_matrix_[0][1], view_matrix_[0][2]});
    tr_front[1] = Dot(front, Vec3f{view_matrix_[1][0], view_matrix_[1][1], view_matrix_[1][2]});
    tr_front[2] = Dot(front, Vec3f{view_matrix_[2][0], view_matrix_[2][1], view_matrix_[2][2]});

    LookAt(view_matrix_, world_position_, world_position_ + tr_front, Vec3f{0, 1, 0});
}

void Ren::Camera::UpdatePlanes() {
    frustum_.UpdateFromMatrix(proj_matrix_ * view_matrix_);
    frustum_vs_.UpdateFromMatrix(proj_matrix_);

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

Ren::eVisResult Ren::Camera::CheckFrustumVisibility(const Vec3f &bbox_min, const Vec3f &bbox_max) const {
    return frustum_.CheckVisibility(bbox_min, bbox_max);
}

float Ren::Camera::GetBoundingSphere(Vec3f &out_center) const {
    const float f = far_, n = near_;

    const float k =
        std::sqrt(1 + (1.0f / (aspect_ * aspect_))) * std::tan(aspect_ * 0.5f * angle_ * Pi<float>() / 180.0f);
    const float k_sqr = k * k;
    if (k_sqr >= (f - n) / (f + n)) {
        out_center = world_position_ + fwd() * f;
        return f * k;
    } else {
        out_center = world_position_ + fwd() * 0.5f * (f + n) * (1 + k_sqr);
        return 0.5f * std::sqrt((f - n) * (f - n) + 2.0f * (f * f + n * n) * k_sqr + (f + n) * (f + n) * k_sqr * k_sqr);
    }
}

void Ren::Camera::ExtractSubFrustums(const int resx, const int resy, const int resz, Frustum sub_frustums[]) const {
    // grid size by x and y in clip space
    const float grid_size_cs[2] = {2.0f / float(resx), 2.0f / float(resy)};

    const Mat4f world_from_clip = Inverse(proj_matrix_ * view_matrix_);

    { // Construct cells for the first depth slice
        const float znear = near_, zfar = near_ * std::pow(far_ / near_, 1.0f / float(resz));

        for (int y = 0; y < resy; y++) {
            const float ybot = -1.0f + float(y) * grid_size_cs[1], ytop = -1.0f + float(y + 1) * grid_size_cs[1];

            for (int x = 0; x < resx; x++) {
                auto p0 = Vec4f{-1.0f + float(x) * grid_size_cs[0], ybot, 1.0f, 1.0f},
                     p1 = Vec4f{-1.0f + float(x) * grid_size_cs[0], ytop, 1.0f, 1.0f},
                     p2 = Vec4f{-1.0f + float(x + 1) * grid_size_cs[0], ytop, 1.0f, 1.0f},
                     p3 = Vec4f{-1.0f + float(x + 1) * grid_size_cs[0], ybot, 1.0f, 1.0f};

                p0 = world_from_clip * p0;
                p1 = world_from_clip * p1;
                p2 = world_from_clip * p2;
                p3 = world_from_clip * p3;

                const Vec3f _p0 = Vec3f{p0 / p0[3]}, _p1 = Vec3f{p1 / p1[3]}, _p2 = Vec3f{p2 / p2[3]},
                            _p3 = Vec3f{p3 / p3[3]};

                Frustum &sf = sub_frustums[y * resx + x];
                sf.planes[int(eCamPlane::Left)] = {world_position_, _p0, _p1};
                sf.planes[int(eCamPlane::Right)] = {world_position_, _p2, _p3};
                sf.planes[int(eCamPlane::Top)] = {world_position_, _p1, _p2};
                sf.planes[int(eCamPlane::Bottom)] = {world_position_, _p3, _p0};
                sf.planes[int(eCamPlane::Near)] = frustum_.planes[int(eCamPlane::Near)];
                sf.planes[int(eCamPlane::Near)].d -= (znear - near_);
                sf.planes[int(eCamPlane::Far)] = frustum_.planes[int(eCamPlane::Far)];
                sf.planes[int(eCamPlane::Far)].d = -frustum_.planes[int(eCamPlane::Near)].d + (zfar - near_);
            }
        }
    }

    // Construct cells for the rest slices
    for (int z = 1; z < resz; z++) {
        const float znear = near_ * std::pow(far_ / near_, float(z) / float(resz)),
                    zfar = near_ * std::pow(far_ / near_, float(z + 1) / float(resz));

        memcpy(&sub_frustums[z * resy * resx], &sub_frustums[0], sizeof(Frustum) * resy * resx);

        for (int i = 0; i < resy * resx; i++) {
            Frustum &sf = sub_frustums[z * resy * resx + i];
            sf.planes[int(eCamPlane::Near)].d -= (znear - near_);
            sf.planes[int(eCamPlane::Far)] = frustum_.planes[int(eCamPlane::Far)];
            sf.planes[int(eCamPlane::Far)].d = -frustum_.planes[int(eCamPlane::Near)].d + (zfar - near_);
        }
    }
}