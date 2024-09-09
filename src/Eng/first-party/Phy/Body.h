#pragma once

#include <memory>

#include "Shape.h"

namespace Phy {
struct point_t;

class Body {
  public:
    Vec3 pos;
    real inv_mass;
    Quat rot;
    Vec3 vel_lin;
    real elasticity;
    Vec3 vel_ang;
    real friction;
    std::unique_ptr<Shape> shape;

    Mat3 GetInverseInertiaTensorWs() const;
    Vec3 GetCenterOfMassWs() const;
    Bounds GetBounds() const;

    [[nodiscard]] Vec3 WorldSpaceToBodySpace(const Vec3 &point_ws) const;

    void ApplyImpulse(const Vec3 &point, const Vec3 &impulse);
    void ApplyImpulseLinear(const Vec3 &impulse);
    void ApplyImpulseAngular(const Vec3 &impulse);

    void Update(real dt_s);

    static void SupportOfMinkowskiSum(const Body &a, const Body &b, Vec3 dir,
                                      real bias, point_t &out_point);
};

/////////////////////////////////////////////////////////////////////////////////////////

struct contact_t {
    Vec3 pt_on_a_ws;
    Vec3 pt_on_b_ws;
    Vec3 pt_on_a_ls;
    Vec3 pt_on_b_ls;

    Vec3 normal_ws;
    real separation_dist; // negative when penetrating
    real time_of_impact;

    Body *body_a;
    Body *body_b;
};

inline bool operator<(const contact_t &c1, const contact_t &c2) {
    return (c1.time_of_impact < c2.time_of_impact);
}

bool Intersect(Body *a, Body *b, contact_t &out_contact);
bool Intersect(Body *a, Body *b, real dt, contact_t &out_contact);
void ResolveContact(contact_t &contact);

bool ConservativeAdvance(Body *a, Body *b, real dt, contact_t &out_contact);

// Expanding Polytope Algorithm (EPA)
real EPA_Expand(const Body &a, const Body &b, real bias, const point_t simplex_pts[4],
                Vec3 &pt_on_a, Vec3 &pt_on_b);

// Gilbert-Johnson-Keerthi (GJK)
void GJK_ClosestPoints(const Body &a, const Body &b, Vec3 &pt_on_a, Vec3 &pt_on_b);
bool GJK_DoesIntersect(const Body &a, const Body &b, real bias, Vec3 &pt_on_a,
                       Vec3 &pt_on_b);

/////////////////////////////////////////////////////////////////////////////////////////

struct pseudo_body_t {
    int id;
    float val;
    bool ismin;
};

inline bool operator<(const pseudo_body_t &b1, const pseudo_body_t &b2) {
    return (b1.val < b2.val);
}

void SortBodiesBounds(const Body bodies[], int count, real dt_s,
                      pseudo_body_t out_sorted[]);

/////////////////////////////////////////////////////////////////////////////////////////

struct collision_pair_t {
    int b1, b2;
};

inline bool operator==(const collision_pair_t &p1, const collision_pair_t &p2) {
    return (p1.b1 == p2.b1 && p1.b2 == p2.b2) || (p1.b1 == p2.b2 && p1.b2 == p2.b1);
}

inline bool operator!=(const collision_pair_t &p1, const collision_pair_t &p2) {
    return !(p1 == p2);
}

int BuildCollisionPairs(const pseudo_body_t sorted_bodies[], int count,
                        collision_pair_t out_pairs[]);

} // namespace Phy