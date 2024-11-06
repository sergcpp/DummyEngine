#include "Body.h"

#include "Utils.h"

Phy::Mat3 Phy::Body::GetInverseInertiaTensorWs() const {
    Mat3 orientation = ToMat3(rot);
    Mat3 orientation_transposed = Transpose(orientation);

    return orientation * shape->GetInverseInertiaTensor() * orientation_transposed *
           inv_mass;
}

Phy::Vec3 Phy::Body::GetCenterOfMassWs() const {
    Vec3 com = shape->center_of_mass();

    { // Rotate center
        const Quat q = {com[0], com[1], com[2], real(0)};
        const Quat rq = rot * q * Inverse(rot);

        com[0] = rq.x;
        com[1] = rq.y;
        com[2] = rq.z;
    }

    return pos + com;
}

Phy::Bounds Phy::Body::GetBounds() const { return shape->GetBounds(pos, rot); }

Phy::Vec3 Phy::Body::WorldSpaceToBodySpace(const Vec3 &point_ws) const {
    const Vec3 tmp = point_ws - GetCenterOfMassWs();
    const Quat inv_rot = Inverse(rot);
    const Quat point_ls = inv_rot * Quat{tmp[0], tmp[1], tmp[2], real(0)} * rot;
    return Vec3{point_ls.x, point_ls.y, point_ls.z};
}

void Phy::Body::ApplyImpulse(const Vec3 &point, const Vec3 &impulse) {
    if (inv_mass == real(0)) {
        return;
    }

    ApplyImpulseLinear(impulse);

    const Vec3 r = point - GetCenterOfMassWs();
    const Vec3 dL = Cross(r, impulse);

    ApplyImpulseAngular(dL);
}

void Phy::Body::ApplyImpulseLinear(const Vec3 &impulse) {
    if (inv_mass == real(0)) {
        return;
    }

    // p = m * v
    // dp = m * dv = J
    // => dv = J / m

    vel_lin += impulse * inv_mass;
}

void Phy::Body::ApplyImpulseAngular(const Vec3 &impulse) {
    if (inv_mass == real(0)) {
        return;
    }

    // L = I * w = r x p
    // dL = I * dw = r x J
    // => dw = I^-1 * (r x J)

    vel_ang += GetInverseInertiaTensorWs() * impulse;

    const float MaxAngularSpeed = 30;
    if (Length2(vel_ang) > MaxAngularSpeed * MaxAngularSpeed) {
        vel_ang = Normalize(vel_ang);
        vel_ang *= MaxAngularSpeed;
    }
}

void Phy::Body::Update(const real dt_s) {
    pos += vel_lin * dt_s;

    const Vec3 com_ws = GetCenterOfMassWs();
    const Vec3 com_to_pos = pos - com_ws;

    // Total torque is external applied torques + internal torque (precession)
    // T = T_external + omega x I * opega
    // T_external = 0 because it was applied in collision response function
    // T = Ia = w x I * w
    // a = I^-1 (w x I * w)
    const Mat3 rot_mat = ToMat3(rot);
    const Mat3 inertia_tensor =
        rot_mat * shape->GetInverseInertiaTensor() * Transpose(rot_mat);
    const Vec3 alpha = Inverse(inertia_tensor) * Cross(vel_ang, inertia_tensor * vel_ang);

    vel_ang += alpha * dt_s;

    // Update orientation
    const Vec3 dAngle = vel_ang * dt_s;
    const real dAngleMag = Length(dAngle);
    if (dAngleMag != real(0)) {
        const Quat dq = Quat{dAngle, dAngleMag};

        rot = dq * rot;
        rot = Normalize(rot);
    }

    { // offset position by rotated center-to-pos vector
        const Quat q = {com_to_pos[0], com_to_pos[1], com_to_pos[2], real(0)};
        const Quat rq = rot * q * Inverse(rot);

        pos = com_ws + Vec3{rq.x, rq.y, rq.z};
    }
}

void Phy::Body::SupportOfMinkowskiSum(const Body &a, const Body &b, Vec3 dir,
                                      const real bias, point_t &out_point) {
    dir = Normalize(dir);

    // Find the point on a furthest in direction
    out_point.pt_a = a.shape->Support(+dir, a.pos, a.rot, bias);
    // Find the point on b furthest in opposite direction
    out_point.pt_b = b.shape->Support(-dir, b.pos, b.rot, bias);
    // Find the point, in the minkowski sum, furthest in the direction
    out_point.pt_s = out_point.pt_a - out_point.pt_b;
}

/////////////////////////////////////////////////////////////////////////////////////////

bool Phy::Intersect(Body *a, Body *b, contact_t &out_contact) {
    const Vec3 ab = b->pos - a->pos;
    const real ab_len = Length(ab);

    out_contact.normal_ws = ab / ab_len;

    if (a->shape->type() == eShapeType::Sphere &&
        b->shape->type() == eShapeType::Sphere) {
        const auto *sph_a = static_cast<ShapeSphere *>(a->shape.get());
        const auto *sph_b = static_cast<ShapeSphere *>(b->shape.get());

        if (SphereSphereStatic(*sph_a, *sph_b, a->pos, b->pos, out_contact.pt_on_a_ws,
                               out_contact.pt_on_b_ws)) {
            out_contact.body_a = a;
            out_contact.body_b = b;

            // Convert world space contacts to local space
            out_contact.pt_on_a_ls = a->WorldSpaceToBodySpace(out_contact.pt_on_a_ws);
            out_contact.pt_on_b_ls = b->WorldSpaceToBodySpace(out_contact.pt_on_b_ws);

            // Calculate separation distance
            out_contact.separation_dist = ab_len - (sph_a->radius + sph_b->radius);

            return true;
        }
    } else {
        const real Bias = real(0.001);
        Vec3 pt_on_a, pt_on_b;

        const bool intersects = GJK_DoesIntersect(*a, *b, Bias, pt_on_a, pt_on_b);

        if (intersects) {
            // There was an intersection, so get the contact data
            const Vec3 normal = Normalize(pt_on_b - pt_on_a);

            pt_on_a -= normal * Bias;
            pt_on_b += normal * Bias;

            out_contact.normal_ws = normal;
        } else {
            // There was no collision, but we still want the contact data
            GJK_ClosestPoints(*a, *b, pt_on_a, pt_on_b);
        }

        out_contact.pt_on_a_ws = pt_on_a;
        out_contact.pt_on_b_ws = pt_on_b;

        out_contact.pt_on_a_ls = a->WorldSpaceToBodySpace(out_contact.pt_on_a_ws);
        out_contact.pt_on_b_ls = b->WorldSpaceToBodySpace(out_contact.pt_on_b_ws);

        const real dist = Distance(pt_on_a, pt_on_b);

        out_contact.separation_dist = intersects ? -dist : dist;

        return intersects;
    }
    return false;
}

bool Phy::Intersect(Body *a, Body *b, const real dt, contact_t &out_contact) {
    const Vec3 ab = b->pos - a->pos;
    const real ab_len = Length(ab);

    out_contact.normal_ws = ab / ab_len;

    if (a->shape->type() == eShapeType::Sphere &&
        b->shape->type() == eShapeType::Sphere) {
        const auto *sph_a = static_cast<ShapeSphere *>(a->shape.get());
        const auto *sph_b = static_cast<ShapeSphere *>(b->shape.get());

        if (SphereSphereDynamic(*sph_a, *sph_b, a->pos, b->pos, a->vel_lin, b->vel_lin,
                                dt, out_contact.pt_on_a_ws, out_contact.pt_on_b_ws,
                                out_contact.time_of_impact)) {
            out_contact.body_a = a;
            out_contact.body_b = b;

            // Step forward to get local collition points
            a->Update(out_contact.time_of_impact);
            b->Update(out_contact.time_of_impact);

            // Convert world space contacts to local space
            out_contact.pt_on_a_ls = a->WorldSpaceToBodySpace(out_contact.pt_on_a_ws);
            out_contact.pt_on_b_ls = b->WorldSpaceToBodySpace(out_contact.pt_on_b_ws);

            // Return to previous position
            a->Update(-out_contact.time_of_impact);
            b->Update(-out_contact.time_of_impact);

            // Calculate separation distance
            out_contact.separation_dist = ab_len - (sph_a->radius + sph_b->radius);

            return true;
        }
    } else {
        // Use GJK to perform conservative advancement
        return ConservativeAdvance(a, b, dt, out_contact);
    }
    return false;
}

void Phy::ResolveContact(contact_t &contact) {
    Body *a = contact.body_a;
    Body *b = contact.body_b;

    if (a->inv_mass == real(0) && b->inv_mass == real(0)) {
        return;
    }

    const Mat3 inv_inertia_tensor_a_ws = a->GetInverseInertiaTensorWs();
    const Mat3 inv_inertia_tensor_b_ws = b->GetInverseInertiaTensorWs();

    const Vec3 ra = contact.pt_on_a_ws - a->GetCenterOfMassWs();
    const Vec3 rb = contact.pt_on_b_ws - b->GetCenterOfMassWs();

    const Vec3 &n = contact.normal_ws;
    const Vec3 ang_J_a = Cross(inv_inertia_tensor_a_ws * Cross(ra, n), ra);
    const Vec3 ang_J_b = Cross(inv_inertia_tensor_b_ws * Cross(rb, n), rb);
    const real ang_fac = Dot(ang_J_a + ang_J_b, n);

    const Vec3 vel_a = a->vel_lin + Cross(a->vel_ang, ra);
    const Vec3 vel_b = b->vel_lin + Cross(b->vel_ang, rb);

    // Calculate the collision impulse
    const Vec3 vab = vel_a - vel_b;
    const real elasticity = a->elasticity * b->elasticity;
    const real impulse_J =
        -(real(1) + elasticity) * Dot(vab, n) / (a->inv_mass + b->inv_mass + ang_fac);
    const Vec3 vec_impulse_J = n * impulse_J;

    a->ApplyImpulse(contact.pt_on_a_ws, +vec_impulse_J);
    b->ApplyImpulse(contact.pt_on_b_ws, -vec_impulse_J);

    //
    // Calculate the impulse caused by friction
    //

    const real friction = a->friction * b->friction;

    // Velocity in the direction of collision normal
    const Vec3 vel_norm = n * Dot(n, vab);

    // Tangent velocity relative to collition normal
    const Vec3 vel_tang = vab - vel_norm;
    const real vel_tang_len = Length(vel_tang);

    if (vel_tang_len != real(0)) {
        const Vec3 rel_vel_tang = vel_tang / vel_tang_len;

        const Vec3 inertia_a =
            Cross(inv_inertia_tensor_a_ws * Cross(ra, rel_vel_tang), ra);
        const Vec3 inertia_b =
            Cross(inv_inertia_tensor_b_ws * Cross(rb, rel_vel_tang), rb);
        const real inv_inertia = Dot(inertia_a + inertia_b, rel_vel_tang);

        // Calculate the tangential impulse for friction
        const real reduced_mass = real(1) / (a->inv_mass + b->inv_mass + inv_inertia);
        const Vec3 impulse_friction = friction * vel_tang * reduced_mass;

        // Apply kinetic friction
        a->ApplyImpulse(contact.pt_on_a_ws, -impulse_friction);
        b->ApplyImpulse(contact.pt_on_b_ws, +impulse_friction);
    }

    // Move objects outside of each other
    if (contact.time_of_impact == real(0)) {
        const real ta = a->inv_mass / (a->inv_mass + b->inv_mass);
        const real tb = b->inv_mass / (a->inv_mass + b->inv_mass);

        const Vec3 ds = contact.pt_on_b_ws - contact.pt_on_a_ws;
        a->pos += ds * ta;
        b->pos -= ds * tb;
    }
}

bool Phy::ConservativeAdvance(Body *a, Body *b, real dt, contact_t &out_contact) {
    const int IterationsLimit = 10;

    out_contact.body_a = a;
    out_contact.body_b = b;

    real toi = real(0);
    int iter_count = 0;

    // Advance the positions of the bodies until they touch or there's no time left
    while (dt > real(0)) {
        // Check for intersection
        const bool did_intersect = Intersect(a, b, out_contact);
        if (did_intersect) {
            out_contact.time_of_impact = toi;
            a->Update(-toi);
            b->Update(-toi);
            return true;
        }

        if (++iter_count > IterationsLimit) {
            break;
        }

        // Get the vector from the closest point on A to the closest point on B
        const Vec3 ab = Normalize(out_contact.pt_on_b_ws - out_contact.pt_on_a_ws);

        // Project the relative velocity onto the ray of shortest distance
        const Vec3 rel_vel = a->vel_lin - b->vel_lin;
        real ortho_speed = Dot(rel_vel, ab);

        // Add maximum speed from rotation
        const real ang_speed_a =
            a->shape->GetFastestLinearSpeedDueToRotation(a->vel_ang, +ab);
        const real ang_speed_b =
            b->shape->GetFastestLinearSpeedDueToRotation(b->vel_ang, -ab);

        ortho_speed += ang_speed_a + ang_speed_b;
        if (ortho_speed <= real(0)) {
            break;
        }

        const real time_to_go = out_contact.separation_dist / ortho_speed;
        if (time_to_go > dt) {
            break;
        }

        dt -= time_to_go;
        toi += time_to_go;

        a->Update(time_to_go);
        b->Update(time_to_go);
    }

    // unwind
    a->Update(-toi);
    b->Update(-toi);

    return false;
}

Phy::real Phy::EPA_Expand(const Body &a, const Body &b, real bias,
                          const point_t simplex_pts[4], Vec3 &pt_on_a, Vec3 &pt_on_b) {
    std::vector<point_t> points;

    auto center = Vec3{0};
    for (int i = 0; i < 4; i++) {
        points.push_back(simplex_pts[i]);
        center += simplex_pts[i].pt_s;
    }
    center /= real(4);

    // Build the triangles
    std::vector<tri_t> tris;
    for (int i = 0; i < 4; i++) {
        const int j = (i + 1) % 4;
        const int k = (i + 2) % 4;

        tri_t tri = {i, j, k};

        const int unused_pt = (i + 3) % 4;
        const real dist =
            SignedDistanceToTriangle(tri, points[unused_pt].pt_s, points.data());
        // The unused point should be always on the negative side of the triangle
        if (dist > real(0)) {
            std::swap(tri.a, tri.b);
        }

        tris.push_back(tri);
    }

    // Expand the simplex to find the closest face of the CSO to the origin
    std::vector<edge_t> dangling_edges;
    while (true) {
        const int ndx =
            FindTriangleClosestToOrigin(tris.data(), int(tris.size()), points.data());
        const Vec3 normal = GetNormalDirection(tris[ndx], points.data());

        point_t new_pt;
        Body::SupportOfMinkowskiSum(a, b, normal, bias, new_pt);

        // if w already exists, then we can not expand any further
        if (HasPoint(new_pt.pt_s, tris.data(), int(tris.size()), points.data())) {
            break;
        }

        const real dist = SignedDistanceToTriangle(tris[ndx], new_pt.pt_s, points.data());
        if (dist <= real(0)) {
            // can not expand
            break;
        }

        const int new_ndx = int(points.size());
        points.push_back(new_pt);

        // Remove triangles that face this point
        const int removed_count =
            RemoveTrianglesFacingPoint(new_pt.pt_s, tris, points.data());
        if (!removed_count) {
            break;
        }

        // Find dangling edges
        dangling_edges.clear();
        FindDanglingEdges(tris.data(), int(tris.size()), dangling_edges);
        if (dangling_edges.empty()) {
            break;
        }

        // Edges are in CCW order, add 'a' in correct order
        for (const edge_t &edge : dangling_edges) {
            tri_t tri = {new_ndx, edge.b, edge.a};

            // Make sure it's in correct order
            const real dist = SignedDistanceToTriangle(tri, center, points.data());
            if (dist > real(0)) {
                std::swap(tri.b, tri.c);
            }

            tris.push_back(tri);
        }
    }

    // Get the projection of the origin on the closest triangle
    const int ndx =
        FindTriangleClosestToOrigin(tris.data(), int(tris.size()), points.data());
    const tri_t &tri = tris[ndx];
    const Vec3 pt_a_w = points[tri.a].pt_s;
    const Vec3 pt_b_w = points[tri.b].pt_s;
    const Vec3 pt_c_w = points[tri.c].pt_s;
    const Vec3 lambdas = BarycentricCoordinates(pt_a_w, pt_b_w, pt_c_w, Vec3{0});

    // Get the point on shape A
    const Vec3 pt_a_a = points[tri.a].pt_a;
    const Vec3 pt_b_a = points[tri.b].pt_a;
    const Vec3 pt_c_a = points[tri.c].pt_a;
    pt_on_a = pt_a_a * lambdas[0] + pt_b_a * lambdas[1] + pt_c_a * lambdas[2];

    // Get the point on shape B
    const Vec3 pt_a_b = points[tri.a].pt_b;
    const Vec3 pt_b_b = points[tri.b].pt_b;
    const Vec3 pt_c_b = points[tri.c].pt_b;
    pt_on_b = pt_a_b * lambdas[0] + pt_b_b * lambdas[1] + pt_c_b * lambdas[2];

    // Return the penetration distance
    return Distance(pt_on_a, pt_on_b);
}

void Phy::GJK_ClosestPoints(const Body &a, const Body &b, Vec3 &pt_on_a, Vec3 &pt_on_b) {
    // const Vec3 Origin = Vec3(0);

    int pts_count = 1;
    point_t simplex_pts[4];
    Body::SupportOfMinkowskiSum(a, b, Vec3{1, 1, 1}, 0 /* bias */, simplex_pts[0]);

    Vec4 lambdas;
    real closest_dist2 = std::numeric_limits<real>::max();
    Vec3 new_dir = -simplex_pts[0].pt_s;
    do {
        // Get the new point to check on
        point_t new_pt;
        Body::SupportOfMinkowskiSum(a, b, new_dir, 0 /* bias */, new_pt);

        // If the new point is the same as previous, then we can not expand any further
        if (HasPoint(simplex_pts, new_pt)) {
            break;
        }

        simplex_pts[pts_count++] = new_pt;

        SimplexSignedVolumes(simplex_pts, pts_count, new_dir, lambdas);
        pts_count = SortValidPoints(simplex_pts, lambdas);

        // Check that the new projection of the origin onto the simplex is closer than
        // the previous
        const real dist2 = Length2(new_dir);
        if (dist2 >= closest_dist2) {
            break;
        }
        closest_dist2 = dist2;
    } while (pts_count < 4);

    pt_on_a = Vec3{0};
    pt_on_b = Vec3{0};
    for (int i = 0; i < 4; i++) {
        pt_on_a += simplex_pts[i].pt_a * lambdas[i];
        pt_on_b += simplex_pts[i].pt_b * lambdas[i];
    }
}

bool Phy::GJK_DoesIntersect(const Body &a, const Body &b, const real bias, Vec3 &pt_on_a,
                            Vec3 &pt_on_b) {
    const Vec3 Origin = Vec3(0);

    int pts_count = 1;
    point_t simplex_pts[4];
    Body::SupportOfMinkowskiSum(a, b, Vec3{1, 1, 1}, 0 /* bias */, simplex_pts[0]);

    real closest_dist2 = std::numeric_limits<real>::max();
    bool does_contain_origin = false;
    Vec3 new_dir = -simplex_pts[0].pt_s;
    do {
        // Get the new point to check on
        point_t new_pt;
        Body::SupportOfMinkowskiSum(a, b, new_dir, 0 /* bias */, new_pt);

        // If the new point is the same as previous, then we can not expand any further
        if (HasPoint(simplex_pts, new_pt)) {
            break;
        }

        simplex_pts[pts_count++] = new_pt;

        const real _dot = Dot(new_dir, new_pt.pt_s - Origin);
        if (_dot < real(0)) {
            // Origin can not be in the set. There is no collision.
            break;
        }

        Vec4 lambdas;
        does_contain_origin =
            SimplexSignedVolumes(simplex_pts, pts_count, new_dir, lambdas);
        if (does_contain_origin) {
            break;
        }

        // Check that the new projection of the origin onto the simplex is closer than
        // the previous
        const real dist2 = Length2(new_dir);
        if (dist2 >= closest_dist2) {
            break;
        }
        closest_dist2 = dist2;

        pts_count = SortValidPoints(simplex_pts, lambdas);
        does_contain_origin = (pts_count == 4);
    } while (!does_contain_origin);

    if (!does_contain_origin) {
        return false;
    }

    // Ensure that we have a 3-simplex (EPA expects a tetrahedron)
    if (pts_count == 1) {
        const Vec3 search_dir = -simplex_pts[0].pt_s;

        point_t new_pt;
        Body::SupportOfMinkowskiSum(a, b, search_dir, 0 /* bias */, new_pt);
        simplex_pts[pts_count++] = new_pt;
    }
    if (pts_count == 2) {
        const Vec3 ab = simplex_pts[1].pt_s - simplex_pts[0].pt_s;
        Vec3 u, v;
        GetOrtho(ab, u, v);

        point_t new_pt;
        Body::SupportOfMinkowskiSum(a, b, u, 0 /* bias */, new_pt);
        simplex_pts[pts_count++] = new_pt;
    }
    if (pts_count == 3) {
        const Vec3 ab = simplex_pts[1].pt_s - simplex_pts[0].pt_s;
        const Vec3 ac = simplex_pts[2].pt_s - simplex_pts[0].pt_s;
        const Vec3 norm = Cross(ab, ac);

        point_t new_pt;
        Body::SupportOfMinkowskiSum(a, b, norm, 0 /* bias */, new_pt);
        simplex_pts[pts_count++] = new_pt;
    }

    //
    // Expand the simplex by the bias amount
    //

    // Get the center point of the simplex
    auto center = Vec3{0};
    for (int i = 0; i < 4; i++) {
        center += simplex_pts[i].pt_s;
    }
    center /= real(4);

    // Expand
    for (point_t &pt : simplex_pts) {
        const Vec3 dir = Normalize(pt.pt_s - center);

        pt.pt_a += dir * bias;
        pt.pt_b -= dir * bias;
        pt.pt_s = pt.pt_a - pt.pt_b;
    }

    // Perform EPA expansion of the simplex to find the closest face on the CSO
    // CSO is a configuration space obstacle (A + (-B))
    EPA_Expand(a, b, bias, simplex_pts, pt_on_a, pt_on_b);
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////

void Phy::SortBodiesBounds(const Body bodies[], const int count, const real dt_s,
                           pseudo_body_t out_sorted[]) {
    const real BoundsEps = real(0.01);
    const Vec3 ProjAxis = Normalize(Vec3(real(1), real(1), real(1)));

    for (int i = 0; i < count; i++) {
        const Body &b = bodies[i];

        Bounds bounds = b.GetBounds();

        bounds.Expand(bounds.mins + b.vel_lin * dt_s - Vec3(BoundsEps));
        bounds.Expand(bounds.maxs + b.vel_lin * dt_s + Vec3(BoundsEps));

        out_sorted[i * 2 + 0].id = i;
        out_sorted[i * 2 + 0].val = Dot(ProjAxis, bounds.mins);
        out_sorted[i * 2 + 0].ismin = true;

        out_sorted[i * 2 + 1].id = i;
        out_sorted[i * 2 + 1].val = Dot(ProjAxis, bounds.maxs);
        out_sorted[i * 2 + 1].ismin = false;
    }

    std::sort(out_sorted, out_sorted + count * 2ull);
}

/////////////////////////////////////////////////////////////////////////////////////////

int Phy::BuildCollisionPairs(const pseudo_body_t sorted_bodies[], const int count,
                             collision_pair_t out_pairs[]) {

    int pair_count = 0;

    for (int i = 0; i < count * 2; i++) {
        const pseudo_body_t &b1 = sorted_bodies[i];
        if (!b1.ismin) {
            continue;
        }

        for (int j = i + 1; j < count * 2; j++) {
            const pseudo_body_t &b2 = sorted_bodies[j];
            if (b2.id == b1.id) {
                // we hit max point of b1, can stop here
                break;
            }

            if (!b2.ismin) {
                continue;
            }

            out_pairs[pair_count++] = {b1.id, b2.id};
        }
    }

    return pair_count;
}