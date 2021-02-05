#include "Utils.h"

namespace PhyInternal {
int CompareSigns(const real a, const real b) {
    if (a > real(0) && b > real(0)) {
        return 1;
    }
    if (a < real(0) && b < real(0)) {
        return 1;
    }
    return 0;
}

int CountValidPoints(const Vec4 &lambdas) {
    int count = 0;
    for (int i = 0; i < 4; i++) {
        if (lambdas[i] != real(0)) {
            ++count;
        }
    }
    return count;
}
} // namespace PhyInternal

void Phy::GetOrtho(const Vec3 &p, Vec3 &u, Vec3 &v) {
    const Vec3 n = Normalize(p);

    const Vec3 w = (n[2] * n[2] > real(0.9) * real(0.9)) ? Vec3{1, 0, 0} : Vec3{0, 0, 1};
    u = Normalize(Cross(w, n));
    v = Normalize(Cross(n, u));
    u = Normalize(Cross(v, n));
}

bool Phy::RaySphere(const Vec3 &ro, const Vec3 &rd, const Vec3 &center, const real radius,
                    real &t1, real &t2) {

    const Vec3 m = center - ro;
    const real a = Dot(rd, rd);
    const real b = Dot(m, rd);
    const real c = Dot(m, m) - radius * radius;

    const real discr = b * b - a * c;
    const real inv_a = real(1) / a;

    if (discr < real(0)) {
        return false;
    }

    const real discr_sqrt = std::sqrt(discr);
    t1 = inv_a * (b - discr_sqrt);
    t2 = inv_a * (b + discr_sqrt);

    return true;
}

Phy::real Phy::DistanceFromLine(const Vec3 &a, const Vec3 &b, const Vec3 &p) {
    const Vec3 ab = Normalize(b - a);
    const Vec3 ray = p - a;
    const Vec3 proj = ab * Dot(ray, ab);
    const Vec3 perp = ray - proj;
    return Length(perp);
}

Phy::real Phy::SignedDistanceFromTriangle(const Vec3 &a, const Vec3 &b, const Vec3 &c,
                                          const Vec3 &p) {
    const Vec3 ab = b - a;
    const Vec3 ac = c - a;
    const Vec3 n = Normalize(Cross(ab, ac));

    const Vec3 ray = p - a;
    return Dot(ray, n);
}

int Phy::FindPointFurthestInDir(const Vec3 pts[], const int count, const Vec3 &dir) {
    int max_ndx = 0;
    real max_dist = Dot(dir, pts[0]);
    for (int i = 1; i < count; i++) {
        const real dist = Dot(dir, pts[1]);
        if (dist > max_dist) {
            max_dist = dist;
            max_ndx = i;
        }
    }
    return max_ndx;
}

Phy::Vec3 Phy::FindPointFurthestFromLine(const Vec3 pts[], const int count, const Vec3 &a,
                                         const Vec3 &b) {
    int max_ndx = 0;
    real max_dist = DistanceFromLine(a, b, pts[0]);
    for (int i = 1; i < count; i++) {
        const real dist = DistanceFromLine(a, b, pts[i]);
        if (dist > max_dist) {
            max_dist = dist;
            max_ndx = i;
        }
    }
    return pts[max_ndx];
}

Phy::Vec3 Phy::FindPointFurthestFromTriangle(const Vec3 pts[], const int count,
                                             const Vec3 &a, const Vec3 &b,
                                             const Vec3 &c) {
    int max_ndx = 0;
    real max_dist2 = SignedDistanceFromTriangle(a, b, c, pts[0]);
    max_dist2 *= max_dist2;
    for (int i = 1; i < count; i++) {
        real dist2 = SignedDistanceFromTriangle(a, b, c, pts[i]);
        dist2 *= dist2;
        if (dist2 > max_dist2) {
            max_dist2 = dist2;
            max_ndx = i;
        }
    }
    return pts[max_ndx];
}

void Phy::BuildTetrahedron(const Vec3 verts[], const int count, Vec3 tet_pts[4],
                           tri_t tet_tris[4]) {
    const int ndx0 =
        FindPointFurthestInDir(verts, count, Vec3(real(1), real(0), real(0)));
    tet_pts[0] = verts[ndx0];
    const int ndx1 = FindPointFurthestInDir(verts, count, -tet_pts[0]);
    tet_pts[1] = verts[ndx1];
    tet_pts[2] = FindPointFurthestFromLine(verts, count, tet_pts[0], tet_pts[1]);
    tet_pts[3] =
        FindPointFurthestFromTriangle(verts, count, tet_pts[0], tet_pts[1], tet_pts[2]);

    const real dist =
        SignedDistanceFromTriangle(tet_pts[0], tet_pts[1], tet_pts[2], tet_pts[3]);
    if (dist > real(0)) {
        // ensure CCW order
        std::swap(tet_pts[0], tet_pts[1]);
    }
    tet_tris[0] = {0, 1, 2};
    tet_tris[1] = {0, 2, 3};
    tet_tris[2] = {2, 1, 3};
    tet_tris[3] = {1, 0, 3};
}

Phy::Vec2 Phy::SignedVolume1D(const Vec3 &a, const Vec3 &b) {
    const Vec3 ab = b - a;
    const Vec3 ap = Vec3{real(0)} - a;
    const Vec3 p = a + ab * Dot(ab, ap) / Length2(ab);

    // Choose the axis with the greatest difference/length
    int axis = 0;
    real mu_max = real(0);
    for (int i = 0; i < 3; i++) {
        const real mu = b[i] - a[i];
        if (mu * mu > mu_max * mu_max) {
            mu_max = mu;
            axis = i;
        }
    }

    // Project the simplex points and projected origin onto the axi with greatest length
    const real _a = a[axis];
    const real _b = b[axis];
    const real _p = p[axis];

    // Get signed distance from a to p and from p to b
    const real c1 = _p - _a;
    const real c2 = _b - _p;

    // if p is between a and b
    if ((_p > _a && _p < _b) || (_p > _b && _p < _a)) {
        return Vec2{c2 / mu_max, c1 / mu_max};
    }

    // if p is on the far side of a
    if ((_a <= _b && _p <= _a) || (_a >= _b && _p >= _a)) {
        return Vec2{real(1), real(0)};
    }

    // p must be on the far side of b
    return Vec2{real(0), real(1)};
}

Phy::Vec3 Phy::SignedVolume2D(const Vec3 &a, const Vec3 &b, const Vec3 &c) {
    using namespace PhyInternal;

    const Vec3 normal = Cross(b - a, c - a);
    const Vec3 p = normal * Dot(a, normal) / Length2(normal);

    // Choose the axis with the greatest projected area
    int axis = 0;
    real area_max = real(0);
    for (int i = 0; i < 3; i++) {
        const int j = (i + 1) % 3;
        const int k = (i + 2) % 3;

        const Vec2 _a = Vec2{a[j], a[k]};
        const Vec2 _b = Vec2{b[j], b[k]};
        const Vec2 _c = Vec2{c[j], c[k]};
        const Vec2 ab = _b - _a;
        const Vec2 ac = _c - _a;

        const real area = ab[0] * ac[1] - ab[1] * ac[0];
        if (area * area > area_max * area_max) {
            axis = i;
            area_max = area;
        }
    }

    // Project onto a chosen axis
    const int j = (axis + 1) % 3;
    const int k = (axis + 2) % 3;

    const Vec2 s[] = {Vec2{a[j], a[k]}, Vec2{b[j], b[k]}, Vec2{c[j], c[k]}};
    const Vec2 _p = Vec2{p[j], p[k]};

    // Get sub-areas of the triangles formed from the projected origin and the edges
    Vec3 areas;
    for (int i = 0; i < 3; i++) {
        const int j = (i + 1) % 3;
        const int k = (i + 2) % 3;

        const Vec2 _a = _p;
        const Vec2 _b = s[j];
        const Vec2 _c = s[k];
        const Vec2 ab = _b - _a;
        const Vec2 ac = _c - _a;

        areas[i] = ab[0] * ac[1] - ab[1] * ac[0];
    }

    // If the projected origin is inside the triangle, then return the barycentric
    // coordinates
    if (CompareSigns(area_max, areas[0]) > 0 && CompareSigns(area_max, areas[1]) > 0 &&
        CompareSigns(area_max, areas[2]) > 0) {
        return areas / area_max;
    }

    // Project point onto edges
    const Vec3 edges_pts[] = {a, b, c};
    real min_dist2 = std::numeric_limits<real>::max();
    auto lambdas = Vec3{1, 0, 0};
    for (int i = 0; i < 3; i++) {
        const int j = (i + 1) % 3;
        const int k = (i + 2) % 3;

        const Vec2 lambda_edge = SignedVolume1D(edges_pts[j], edges_pts[k]);
        const Vec3 pt = edges_pts[j] * lambda_edge[0] + edges_pts[k] * lambda_edge[1];
        if (Length2(pt) < min_dist2) {
            min_dist2 = Length2(pt);
            lambdas[i] = real(0);
            lambdas[j] = lambda_edge[0];
            lambdas[k] = lambda_edge[1];
        }
    }

    return lambdas;
}

Phy::Vec4 Phy::SignedVolume3D(const Vec3 &a, const Vec3 &b, const Vec3 &c,
                              const Vec3 &d) {
    using namespace PhyInternal;

    const auto M = Mat4{Vec4{a[0], b[0], c[0], d[0]}, Vec4{a[1], b[1], c[1], d[1]},
                        Vec4{a[2], b[2], c[2], d[2]}, Vec4{1, 1, 1, 1}};

    const auto C4 =
        Vec4{Cofactor(M, 3, 0), Cofactor(M, 3, 1), Cofactor(M, 3, 2), Cofactor(M, 3, 3)};

    const real det_M = C4[0] + C4[1] + C4[2] + C4[3];

    // If the barycentric coordinates put the origin inside the simples, then return them
    if (CompareSigns(det_M, C4[0]) > 0 && CompareSigns(det_M, C4[1]) > 0 &&
        CompareSigns(det_M, C4[2]) > 0 && CompareSigns(det_M, C4[3]) > 0) {
        return (C4 / det_M);
    }

    // Project origin onto the faces and determine the closes one
    const Vec3 face_pts[] = {a, b, c, d};
    real min_dist2 = std::numeric_limits<real>::max();
    Vec4 lambdas;
    for (int i = 0; i < 4; i++) {
        const int j = (i + 1) % 4;
        const int k = (i + 2) % 4;
        const int l = (i + 3) % 4;

        const Vec3 lambda_face = SignedVolume2D(face_pts[i], face_pts[j], face_pts[k]);
        const Vec3 pt = face_pts[i] * lambda_face[0] + face_pts[j] * lambda_face[1] +
                        face_pts[k] * lambda_face[2];
        if (Length2(pt) < min_dist2) {
            min_dist2 = Length2(pt);
            lambdas[i] = lambda_face[0];
            lambdas[j] = lambda_face[1];
            lambdas[k] = lambda_face[2];
            lambdas[l] = real(0);
        }
    }

    return lambdas;
}

Phy::Vec3 Phy::GetNormalDirection(const tri_t &tri, const point_t points[]) {
    const Vec3 &a = points[tri.a].pt_s;
    const Vec3 &b = points[tri.b].pt_s;
    const Vec3 &c = points[tri.c].pt_s;

    const Vec3 ab = b - a;
    const Vec3 ac = c - a;

    return Normalize(Cross(ab, ac));
}

bool Phy::SimplexSignedVolumes(const point_t pts[], const int pts_count, Vec3 &new_dir,
                               Vec4 &out_lambdas) {
    const real EpsilonSqr = real(0.0001) * real(0.0001);

    bool does_intersect = false;
    switch (pts_count) {
    default:
    case 2: {
        const Vec2 lambdas = SignedVolume1D(pts[0].pt_s, pts[1].pt_s);
        auto v = Vec3(0);
        for (int i = 0; i < 2; i++) {
            v += pts[i].pt_s * lambdas[i];
        }
        new_dir = -v;
        does_intersect = (Length2(v) < EpsilonSqr);
        out_lambdas[0] = lambdas[0];
        out_lambdas[1] = lambdas[1];
        out_lambdas[2] = real(0);
        out_lambdas[3] = real(0);
    } break;
    case 3: {
        const Vec3 lambdas = SignedVolume2D(pts[0].pt_s, pts[1].pt_s, pts[2].pt_s);
        auto v = Vec3(0);
        for (int i = 0; i < 3; i++) {
            v += pts[i].pt_s * lambdas[i];
        }
        new_dir = -v;
        does_intersect = (Length2(v) < EpsilonSqr);
        out_lambdas[0] = lambdas[0];
        out_lambdas[1] = lambdas[1];
        out_lambdas[2] = lambdas[2];
        out_lambdas[3] = real(0);
    } break;
    case 4: {
        const Vec4 lambdas =
            SignedVolume3D(pts[0].pt_s, pts[1].pt_s, pts[2].pt_s, pts[3].pt_s);
        auto v = Vec3(0);
        for (int i = 0; i < 4; i++) {
            v += pts[i].pt_s * lambdas[i];
        }
        new_dir = -v;
        does_intersect = (Length2(v) < EpsilonSqr);
        out_lambdas[0] = lambdas[0];
        out_lambdas[1] = lambdas[1];
        out_lambdas[2] = lambdas[2];
        out_lambdas[3] = lambdas[3];
    } break;
    }

    return does_intersect;
}

bool Phy::HasPoint(const point_t simplex_pts[4], const point_t &new_pt) {
    const real Precision = real(1e-6);

    for (int i = 0; i < 4; i++) {
        const Vec3 delta = simplex_pts[i].pt_s - new_pt.pt_s;
        if (Length2(delta) < Precision * Precision) {
            return true;
        }
    }

    return false;
}

bool Phy::HasPoint(const Vec3 &w, const tri_t tris[], int tris_count,
                   const point_t points[]) {
    const real EpsilonSqr = real(0.001) * real(0.001);

    for (int i = 0; i < tris_count; i++) {
        const tri_t &tri = tris[i];

        if (Distance2(points[tri.a].pt_s, w) < EpsilonSqr ||
            Distance2(points[tri.b].pt_s, w) < EpsilonSqr ||
            Distance2(points[tri.c].pt_s, w) < EpsilonSqr) {
            return true;
        }
    }

    return false;
}

int Phy::SortValidPoints(point_t simplex_pts[4], Vec4 &lambdas) {
    bool valids[4];
    for (int i = 0; i < 4; i++) {
        valids[i] = (lambdas[i] != real(0));
    }

    auto valid_lambdas = Vec4(0);
    int valid_count = 0;

    // TODO: simplify this

    point_t valid_pts[4];
    for (int i = 0; i < 4; i++) {
        if (valids[i]) {
            valid_pts[valid_count] = simplex_pts[i];
            valid_lambdas[valid_count] = lambdas[i];
            ++valid_count;
        }
    }

    for (int i = 0; i < 4; i++) {
        simplex_pts[i] = valid_pts[i];
        lambdas[i] = valid_lambdas[i];
    }

    return valid_count;
}

Phy::Vec3 Phy::BarycentricCoordinates(Vec3 s1, Vec3 s2, Vec3 s3, const Vec3 &pt) {
    s1 = s1 - pt;
    s2 = s2 - pt;
    s3 = s3 - pt;

    const Vec3 normal = Cross(s2 - s1, s3 - s1);
    const Vec3 p0 = normal * Dot(s1, normal) / Length2(normal);

    // Find the axis with the greatest projected area
    int axis = 0;
    real area_max = 0;
    for (int i = 0; i < 3; i++) {
        const int j = (i + 1) % 3;
        const int k = (i + 2) % 3;

        const Vec2 a = Vec2{s1[j], s1[k]};
        const Vec2 b = Vec2{s2[j], s2[k]};
        const Vec2 c = Vec2{s3[j], s3[k]};
        const Vec2 ab = b - a;
        const Vec2 ac = c - a;

        const real area = ab[0] * ac[1] - ab[1] * ac[0];
        if (area * area > area_max * area_max) {
            axis = i;
            area_max = area;
        }
    }

    // Project onto the appropriate axis
    const int x = (axis + 1) % 3;
    const int y = (axis + 2) % 3;

    const Vec2 s[] = {Vec2{s1[x], s1[y]}, Vec2{s2[x], s2[y]}, Vec2{s3[x], s3[y]}};
    const Vec2 p = Vec2{p0[x], p0[y]};

    // Get the sub-areas of the triangles formed from the projected origin and the edges
    Vec3 areas{Uninitialize};
    for (int i = 0; i < 3; i++) {
        const int j = (i + 1) % 3;
        const int k = (i + 2) % 3;

        const Vec2 a = p;
        const Vec2 b = s[j];
        const Vec2 c = s[k];
        const Vec2 ab = b - a;
        const Vec2 ac = c - a;

        areas[i] = ab[0] * ac[1] - ab[1] * ac[0];
    }

    if (area_max == real(0)) {
        return Vec3{1, 0, 0};
    } else {
        return areas / area_max;
    }
}

Phy::real Phy::SignedDistanceToTriangle(const tri_t &tri, const Vec3 &pt,
                                        const point_t points[]) {
    const Vec3 normal = PhyInternal::GetNormalDirection(tri, points);
    const Vec3 &a = points[tri.a].pt_s;
    const Vec3 a2pt = pt - a;
    const real dist = Dot(normal, a2pt);
    return dist;
}

int Phy::FindTriangleClosestToOrigin(const tri_t tris[], int tris_count,
                                     const point_t points[]) {
    real min_dist2 = std::numeric_limits<real>::max();

    int ndx = -1;
    for (int i = 0; i < tris_count; i++) {
        const tri_t &tri = tris[i];

        const real dist = SignedDistanceToTriangle(tri, Vec3{0}, points);
        if (dist * dist < min_dist2) {
            ndx = i;
            min_dist2 = dist * dist;
        }
    }

    return ndx;
}

int Phy::RemoveTrianglesFacingPoint(const Vec3 &pt, std::vector<tri_t> &tris,
                                    const point_t points[]) {
    int removed_count = 0;
    for (auto it = tris.begin(); it != tris.end();) {
        const real dist = SignedDistanceToTriangle(*it, pt, points);
        if (dist > real(0)) {
            // This triangle faces the point, remove it.
            it = tris.erase(it);
            ++removed_count;
        } else {
            ++it;
        }
    }
    return removed_count;
}

void Phy::FindDanglingEdges(const tri_t tris[], const int tris_count,
                            std::vector<edge_t> &out_edges) {
    out_edges.clear();

    for (int i = 0; i < tris_count; i++) {
        const tri_t &tri = tris[i];
        const edge_t edges[] = {{tri.a, tri.b}, {tri.b, tri.c}, {tri.c, tri.a}};

        int counts[3] = {};

        for (int j = 0; j < tris_count; j++) {
            if (j == i) {
                continue;
            }

            const tri_t &tri2 = tris[j];
            const edge_t edges2[] = {
                {tri2.a, tri2.b}, {tri2.b, tri2.c}, {tri2.c, tri2.a}};

            for (int k = 0; k < 3; k++) {
                if (edges[k] == edges2[0]) {
                    ++counts[k];
                }
                if (edges[k] == edges2[1]) {
                    ++counts[k];
                }
                if (edges[k] == edges2[2]) {
                    ++counts[k];
                }
            }
        }

        // An edge that isn't shared is dangling
        for (int k = 0; k < 3; k++) {
            if (!counts[k]) {
                out_edges.push_back(edges[k]);
            }
        }
    }
}
