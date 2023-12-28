#pragma once

#include <vector>

#include "Core.h"

namespace Phy {
void GetOrtho(const Vec3 &p, Vec3 &u, Vec3 &v);

bool RaySphere(const Vec3 &ro, const Vec3 &rd, const Vec3 &center, const real radius,
               real &t1, real &t2);

real DistanceFromLine(const Vec3 &a, const Vec3 &b, const Vec3 &p);
real SignedDistanceFromTriangle(const Vec3 &a, const Vec3 &b, const Vec3 &c,
                                const Vec3 &p);

int FindPointFurthestInDir(const Vec3 pts[], const int count, const Vec3 &dir);
Vec3 FindPointFurthestFromLine(const Vec3 pts[], const int count, const Vec3 &a,
                               const Vec3 &b);
Vec3 FindPointFurthestFromTriangle(const Vec3 pts[], const int count, const Vec3 &a,
                                   const Vec3 &b, const Vec3 &c);

struct tri_t {
    int a, b, c;
};

void BuildTetrahedron(const Vec3 verts[], const int count, Vec3 tet_pts[4],
                      tri_t tet_tris[4]);

Vec2 SignedVolume1D(const Vec3 &a, const Vec3 &b);
Vec3 SignedVolume2D(const Vec3 &a, const Vec3 &b, const Vec3 &c);
Vec4 SignedVolume3D(const Vec3 &a, const Vec3 &b, const Vec3 &c, const Vec3 &d);

struct point_t {
    Vec3 pt_s; // The point on the minkowski sum
    Vec3 pt_a; // The point on body a
    Vec3 pt_b; // The point on body b
};
inline bool operator==(const point_t &p1, const point_t &p2) {
    return (p1.pt_a == p2.pt_a) && (p1.pt_b == p2.pt_b) && (p1.pt_s == p2.pt_s);
}

Vec3 GetNormalDirection(const tri_t& tri, const point_t points[]);

bool SimplexSignedVolumes(const point_t pts[], int pts_count, Vec3 &new_dir,
                          Vec4 &out_lambdas);

bool HasPoint(const point_t simplex_pts[4], const point_t &new_pt);
bool HasPoint(const Vec3 &w, const tri_t tris[], int tris_count, const point_t points[]);

int SortValidPoints(point_t simplex_pts[4], Vec4 &lambdas);

Vec3 BarycentricCoordinates(Vec3 s1, Vec3 s2, Vec3 s3, const Vec3 &pt);

real SignedDistanceToTriangle(const tri_t &tri, const Vec3 &pt, const point_t points[]);

int FindTriangleClosestToOrigin(const tri_t tris[], int tris_count,
                                const point_t points[]);

int RemoveTrianglesFacingPoint(const Vec3 &pt, std::vector<tri_t> &tris,
                               const point_t points[]);

struct edge_t {
    int a, b;
};
inline bool operator==(const edge_t& e1, const edge_t& e2) {
    return (e1.a == e2.a && e1.b == e2.b) || (e1.a == e2.b && e1.b == e2.a);
}

void FindDanglingEdges(const tri_t tris[], int tris_count, std::vector<edge_t> &out_edges);
} // namespace Phy
