#include "Shape.h"

#include "Utils.h"

namespace PhyInternal {
using namespace Phy;

const real CollisionEps = real(0.001);
const real HullPtsEps = real(0.01);

void RemoveInternalPoints(const Vec3 hull_pts[], const int pts_count,
                          const tri_t hull_tris[], const int tris_count,
                          std::vector<Vec3> &check_pts) {
    for (auto it = check_pts.begin(); it != check_pts.end();) {
        const Vec3 &p = (*it);

        bool is_external = false;
        for (int ti = 0; ti < tris_count; ti++) {
            const tri_t &tri = hull_tris[ti];

            const Vec3 &a = hull_pts[tri.a];
            const Vec3 &b = hull_pts[tri.b];
            const Vec3 &c = hull_pts[tri.c];

            const real dist = SignedDistanceFromTriangle(a, b, c, p);
            if (dist > real(0)) {
                is_external = true;
                break;
            }
        }

        bool is_duplicate = false;
        for (int j = 0; j < pts_count && !is_external; j++) {
            if (Distance2(p, hull_pts[j]) < HullPtsEps * HullPtsEps) {
                is_duplicate = true;
                break;
            }
        }

        if (is_external || is_duplicate) {
            it = check_pts.erase(it);
        } else {
            ++it;
        }
    }
}

bool IsEdgeUnique(const tri_t tris[], const int facing_tris[], const int facing_count,
                  const int ignore_ndx, const edge_t edge) {
    for (int i = 0; i < facing_count; i++) {
        const int tri_ndx = facing_tris[i];
        if (tri_ndx == ignore_ndx) {
            continue;
        }

        const tri_t &tri = tris[tri_ndx];

        const edge_t tri_edges[3] = {{tri.a, tri.b}, {tri.b, tri.c}, {tri.c, tri.a}};

        for (int j = 0; j < 3; j++) {
            if (tri_edges[j] == edge) {
                return false;
            }
        }
    }
    return true;
}

void AddPoint(std::vector<Vec3> &hull_points, std::vector<tri_t> &hull_tris,
              const Vec3 &pt) {
    // Point is assumed to be outside of hull
    // Find all the triangles that face this point
    std::vector<int> facing_tris;
    for (int i = int(hull_tris.size()) - 1; i >= 0; i--) {
        const tri_t &tri = hull_tris[i];

        const Vec3 &a = hull_points[tri.a];
        const Vec3 &b = hull_points[tri.b];
        const Vec3 &c = hull_points[tri.c];

        const real dist = SignedDistanceFromTriangle(a, b, c, pt);
        if (dist > real(0)) {
            facing_tris.push_back(i);
        }
    }

    // Gather unique edges
    std::vector<edge_t> unique_edges;
    for (int tri_ndx : facing_tris) {
        const tri_t &tri = hull_tris[tri_ndx];

        const edge_t edges[] = {edge_t{tri.a, tri.b}, edge_t{tri.b, tri.c},
                                edge_t{tri.c, tri.a}};

        for (int e = 0; e < 3; e++) {
            if (IsEdgeUnique(hull_tris.data(), facing_tris.data(),
                             int(facing_tris.size()), tri_ndx, edges[e])) {
                unique_edges.push_back(edges[e]);
            }
        }
    }

    // Remove facing tris (they are already in reverse order)
    for (int tri_ndx : facing_tris) {
        hull_tris.erase(hull_tris.begin() + tri_ndx);
    }

    // Add new point
    const int new_pt_ndx = int(hull_points.size());
    hull_points.push_back(pt);

    // Add triangles for each unique edge
    for (const edge_t &e : unique_edges) {
        tri_t tri;
        tri.a = e.a;
        tri.b = e.b;
        tri.c = new_pt_ndx;
        hull_tris.push_back(tri);
    }
}

void RemoveUnreferencedVerts(std::vector<Vec3> &hull_points,
                             std::vector<tri_t> &hull_tris) {
    for (int i = 0; i < int(hull_points.size()); i++) {
        bool is_used = false;
        for (const tri_t &tri : hull_tris) {
            if (tri.a == i || tri.b == i || tri.c == i) {
                is_used = true;
                break;
            }
        }

        if (!is_used) {
            // Remove point
            hull_points.erase(hull_points.begin() + i);
            i--;

            // Fix indices
            for (tri_t &tri : hull_tris) {
                if (tri.a > i) {
                    --tri.a;
                }
                if (tri.b > i) {
                    --tri.b;
                }
                if (tri.c > i) {
                    --tri.c;
                }
            }
        }
    }
}

void ExpandConvexHull(const Vec3 verts[], const int count, std::vector<Vec3> &hull_pts,
                      std::vector<tri_t> &hull_tris) {
    std::vector<Vec3> external_verts(verts, verts + count);
    RemoveInternalPoints(hull_pts.data(), int(hull_pts.size()), hull_tris.data(),
                         int(hull_tris.size()), external_verts);

    while (!external_verts.empty()) {
        const int pt_ndx = FindPointFurthestInDir(
            external_verts.data(), int(external_verts.size()), external_verts[0]);
        const Vec3 pt = external_verts[pt_ndx];

        external_verts.erase(external_verts.begin() + pt_ndx);

        AddPoint(hull_pts, hull_tris, pt);

        RemoveInternalPoints(hull_pts.data(), int(hull_pts.size()), hull_tris.data(),
                             int(hull_tris.size()), external_verts);
    }

    RemoveUnreferencedVerts(hull_pts, hull_tris);
}

bool IsExternal(const Vec3 pts[], const tri_t tris[], const int tris_count,
                const Vec3 &pt) {
    for (int i = 0; i < tris_count; i++) {
        const tri_t &tri = tris[i];

        const Vec3 &a = pts[tri.a];
        const Vec3 &b = pts[tri.b];
        const Vec3 &c = pts[tri.c];

        const real dist = SignedDistanceFromTriangle(a, b, c, pt);
        if (dist > real(0)) {
            return true;
        }
    }
    return false;
}

Vec3 CalculateCenterOfMass(const Vec3 pts[], const int pts_count, const tri_t tris[],
                           const int tris_count) {
    const int SamplesPerDim = 100;

    Bounds bounds;
    bounds.Expand(pts, pts_count);

    const real dx = bounds.width(0) / real(SamplesPerDim);
    const real dy = bounds.width(1) / real(SamplesPerDim);
    const real dz = bounds.width(2) / real(SamplesPerDim);

    auto cm = Vec3(real(0));

    int total_samples = 0;
    for (real x = bounds.mins[0]; x < bounds.maxs[0]; x += dx) {
        for (real y = bounds.mins[1]; y < bounds.maxs[1]; y += dy) {
            for (real z = bounds.mins[2]; z < bounds.maxs[2]; z += dz) {
                const auto pt = Vec3{x, y, z};

                if (IsExternal(pts, tris, tris_count, pt)) {
                    continue;
                }

                cm += pt;
                ++total_samples;
            }
        }
    }

    return (cm / real(total_samples));
}

Mat3 CalculateInertiaTensor(const Vec3 pts[], const int pts_count, const tri_t tris[],
                            const int tris_count, const Vec3 &cm) {
    const int SamplesPerDim = 100;

    Bounds bounds;
    bounds.Expand(pts, pts_count);

    const real dx = bounds.width(0) / real(SamplesPerDim);
    const real dy = bounds.width(1) / real(SamplesPerDim);
    const real dz = bounds.width(2) / real(SamplesPerDim);

    auto tensor = Mat3{real(0)};

    int total_samples = 0;
    for (real x = bounds.mins[0]; x < bounds.maxs[0]; x += dx) {
        for (real y = bounds.mins[1]; y < bounds.maxs[1]; y += dy) {
            for (real z = bounds.mins[2]; z < bounds.maxs[2]; z += dz) {
                const auto pt = Vec3{x, y, z};

                if (IsExternal(pts, tris, tris_count, pt)) {
                    continue;
                }

                const Vec3 delta = pt - cm;

                tensor[0][0] += delta[1] * delta[1] + delta[2] * delta[2];
                tensor[1][1] += delta[2] * delta[2] + delta[0] * delta[0];
                tensor[2][2] += delta[0] * delta[0] + delta[1] * delta[1];

                tensor[0][1] -= delta[0] * delta[1];
                tensor[0][2] -= delta[0] * delta[2];
                tensor[1][2] -= delta[1] * delta[2];

                tensor[1][0] -= delta[0] * delta[1];
                tensor[2][0] -= delta[0] * delta[2];
                tensor[2][1] -= delta[1] * delta[2];

                ++total_samples;
            }
        }
    }

    return (tensor / real(total_samples));
}

} // namespace PhyInternal

Phy::Mat3 Phy::ShapeBox::GetInverseInertiaTensor() const {
    const real dx = bounds.maxs[0] - bounds.mins[0];
    const real dy = bounds.maxs[1] - bounds.mins[1];
    const real dz = bounds.maxs[2] - bounds.mins[2];

    // Inertia tensor for box centered around zero
    Mat3 tensor;
    tensor[0][0] = (dy * dy + dz * dz) / real(12);
    tensor[1][1] = (dx * dx + dz * dz) / real(12);
    tensor[2][2] = (dx * dx + dy * dy) / real(12);

    // parallel axis theorem
    const Vec3 R =
        Vec3{0} - center_of_mass_; // the displacement from center of mass to origin
    const real R2 = Length2(R);

    Mat3 pat_tensor{Uninitialize};
    pat_tensor[0] = Vec3{R2 - R[0] * R[0], R[0] * R[1], R[0] * R[2]};
    pat_tensor[1] = Vec3{R[1] * R[0], R2 - R[1] * R[1], R[1] * R[2]};
    pat_tensor[2] = Vec3{R[2] * R[0], R[2] * R[1], R2 - R[2] * R[2]};

    // add com tensor ant parallel axis theorem tensor
    return tensor + pat_tensor;
}

Phy::Bounds Phy::ShapeBox::GetBounds(const Vec3 &pos, const Quat &rot) const {
    Vec3 corners[8];
    bounds.ToPoints(corners);

    const Quat inv_rot = Inverse(rot);

    Bounds rbounds;
    for (int i = 0; i < 8; i++) {
        { // Rotate corner
            const Quat q = {corners[i][0], corners[i][1], corners[i][2], real(0)};
            const Quat rq = rot * q * inv_rot;

            corners[i][0] = rq.x;
            corners[i][1] = rq.y;
            corners[i][2] = rq.z;
        }

        rbounds.Expand(corners[i]);
    }

    return rbounds;
}

Phy::real Phy::ShapeBox::GetFastestLinearSpeedDueToRotation(const Vec3 &vel_ang,
                                                            const Vec3 &dir) const {
                                                            return real(0);

    real max_speed = real(0);
    for (int i = 0; i < 8; i++) {
        const Vec3 r = points[i] - center_of_mass_;
        const Vec3 vel_lin = Cross(vel_ang, r);
        const real speed = Dot(dir, vel_lin);
        if (speed > max_speed) {
            max_speed = speed;
        }
    }
    return max_speed;
}

Phy::Vec3 Phy::ShapeBox::Support(const Vec3 &dir, const Vec3 &pos, const Quat &rot,
                                 const real bias) const {
    Vec3 max_pt;
    real max_dist2 = std::numeric_limits<real>::lowest();

    // Find the point that is the furthest in direction
    const Quat inv_rot = Inverse(rot);
    for (int i = 0; i < 8; i++) {
        Vec3 pt = points[i];

        { // Rotate point
            const Quat q = {pt[0], pt[1], pt[2], real(0)};
            const Quat rq = rot * q * inv_rot;

            pt[0] = rq.x;
            pt[1] = rq.y;
            pt[2] = rq.z;
        }

        pt += pos;

        const float dist2 = Dot(dir, pt);
        if (dist2 > max_dist2) {
            max_dist2 = dist2;
            max_pt = pt;
        }
    }

    return max_pt + Normalize(dir) * bias;
}

void Phy::ShapeBox::Build(const Vec3 pts[], const int count) {
    for (int i = 0; i < count; i++) {
        bounds.Expand(pts[i]);
    }

    bounds.ToPoints(points);

    center_of_mass_ = real(0.5) * (bounds.mins + bounds.maxs);
}

Phy::Vec3 Phy::ShapeConvex::Support(const Vec3 &dir, const Vec3 &pos, const Quat &rot,
                                    real bias) const {
    Vec3 max_pt;
    real max_dist = std::numeric_limits<real>::lowest();

    // Find the point that is the furthest in direction
    for (int i = 0; i < int(points.size()); i++) {
        Vec3 pt;
        { // Rotate point
            const Quat q = {pos[0], pos[1], pos[2], real(0)};
            const Quat rq = rot * q * Inverse(rot);

            pt[0] = rq.x;
            pt[1] = rq.y;
            pt[2] = rq.z;
        }

        const real dist = Dot(dir, pt);
        if (dist > max_dist) {
            max_dist = dist;
            max_pt = pt;
        }
    }

    return max_pt + Normalize(dir) * bias;
}

void Phy::ShapeConvex::Build(const Vec3 pts[], const int pts_count) {
    using namespace PhyInternal;

    // Expand into a convex hull
    std::vector<tri_t> hull_tris;
    ExpandConvexHull(pts, pts_count, points, hull_tris);

    // Expand the bounds
    bounds.Clear();
    bounds.Expand(points.data(), int(points.size()));

    center_of_mass_ = CalculateCenterOfMass(points.data(), int(points.size()),
                                            hull_tris.data(), int(hull_tris.size()));

    inertia_tensor =
        CalculateInertiaTensor(points.data(), int(points.size()), hull_tris.data(),
                               int(hull_tris.size()), center_of_mass_);
}

bool Phy::SphereSphereStatic(const ShapeSphere &a, const ShapeSphere &b,
                             const Vec3 &pos_a, const Vec3 &pos_b, Vec3 &pt_on_a,
                             Vec3 &pt_on_b) {
    const Vec3 ab = pos_b - pos_a;
    const Vec3 norm = Normalize(ab);

    pt_on_a = pos_a + norm * a.radius;
    pt_on_b = pos_b - norm * b.radius;

    const real radius_ab = a.radius + b.radius;
    return (Length2(ab) <= (radius_ab * radius_ab));
}

bool Phy::SphereSphereDynamic(const ShapeSphere &a, const ShapeSphere &b,
                              const Vec3 &pos_a, const Vec3 &pos_b, const Vec3 &vel_a,
                              const Vec3 &vel_b, const real dt, Vec3 &pt_on_a,
                              Vec3 &pt_on_b, real &toi) {
    using namespace PhyInternal;

    const Vec3 rel_vel = vel_a - vel_b;

    const Vec3 beg_a = pos_a;
    const Vec3 end_a = pos_a + rel_vel * dt;
    const Vec3 ray_dir = end_a - beg_a;

    real t0 = real(0), t1 = real(0);
    if (Length2(ray_dir) < CollisionEps * CollisionEps) {
        // ray is too short, check if start point is inside
        const Vec3 ab = pos_b - pos_a;
        const real radius = a.radius + b.radius + CollisionEps;
        if (Length2(ab) > radius * radius) {
            return false;
        }
    } else if (!RaySphere(pos_a, ray_dir, pos_b, a.radius + b.radius, t0, t1)) {
        return false;
    }

    // Scale from [0; 1] to [0; dt] range
    t0 *= dt;
    t1 *= dt;

    if (t1 < real(0)) {
        return false;
    }

    // time of impact
    toi = Max(real(0), t0);

    // check if collision happened within time delta
    if (toi > dt) {
        return false;
    }

    const Vec3 new_pos_a = pos_a + vel_a * toi;
    const Vec3 new_pos_b = pos_b + vel_b * toi;
    const Vec3 ab = Normalize(new_pos_b - new_pos_a);

    pt_on_a = new_pos_a + ab * a.radius;
    pt_on_b = new_pos_b - ab * b.radius;

    return true;
}