#include "PhysicsManager.h"

#include <iterator>

#include <Ren/MMat.h>

#include "comp/Physics.h"
#include "comp/Transform.h"
#include "SceneData.h"

namespace PhysicsManagerInternal {
using Phy::real;
using Phy::Vec3;

const auto Gravity = Vec3{real(0.0), real(-9.8), real(0.0)};
} // namespace PhysicsManagerInternal

void Eng::PhysicsManager::Update(SceneData &scene, const float dt_s) {
    using namespace PhysicsManagerInternal;

    // retrieve pointers to components for fast access
    auto *transforms = (Transform *)scene.comp_store[CompTransform]->SequentialData();
    auto *physes = (Physics *)scene.comp_store[CompPhysics]->SequentialData();

    updated_objects_.clear();
    contacts_.clear();

    const uint32_t PhysMask = CompTransformBit | CompPhysicsBit;

    for (auto it = scene.objects.begin(); it != scene.objects.end(); ++it) {
        SceneObject &obj = (*it);

        if ((obj.comp_mask & PhysMask) == PhysMask) {
            Physics &ph = physes[obj.components[CompPhysics]];

            // I = dp, F = dp/dt => dp = F * dt => I = F * dt
            const real mass = real(1) / ph.body.inv_mass;
            const Phy::Vec3 impulse_gravity = Gravity * mass * dt_s;
            ph.body.ApplyImpulseLinear(impulse_gravity);

            updated_objects_.push_back(uint32_t(std::distance(scene.objects.begin(), it)));
        }
    }

    //
    // Broad phase
    //

    { // Sort bodies along diagonal axis
        const real BoundsEps = real(0.01);
        const Vec3 ProjAxis = Normalize(Vec3(real(1), real(1), real(1)));

        temp_sorted_bodies_.clear();
        temp_sorted_bodies_.resize(updated_objects_.size() * 2);

        for (uint32_t i = 0; i < uint32_t(updated_objects_.size()); i++) {
            const uint32_t ndx = updated_objects_[i];
            SceneObject &obj = scene.objects[ndx];
            Physics &ph = physes[obj.components[CompPhysics]];
            const Phy::Body &b = ph.body;

            Phy::Bounds bounds = b.GetBounds();

            bounds.Expand(bounds.mins + b.vel_lin * dt_s - Vec3(BoundsEps));
            bounds.Expand(bounds.maxs + b.vel_lin * dt_s + Vec3(BoundsEps));

            temp_sorted_bodies_[i * 2ull + 0].id = int(ndx);
            temp_sorted_bodies_[i * 2ull + 0].val = Dot(ProjAxis, bounds.mins);
            temp_sorted_bodies_[i * 2ull + 0].ismin = true;

            temp_sorted_bodies_[i * 2ull + 1].id = int(ndx);
            temp_sorted_bodies_[i * 2ull + 1].val = Dot(ProjAxis, bounds.maxs);
            temp_sorted_bodies_[i * 2ull + 1].ismin = false;
        }

        sort(begin(temp_sorted_bodies_), end(temp_sorted_bodies_));
    }

    { // Build potential collision pairs
        temp_collision_pairs_.resize(temp_sorted_bodies_.size() * temp_sorted_bodies_.size());
        const int pair_count = Phy::BuildCollisionPairs(temp_sorted_bodies_.data(), int(updated_objects_.size()),
                                                        temp_collision_pairs_.data());
        temp_collision_pairs_.resize(pair_count);
    }

    //
    // Narrow phase
    //

    for (const Phy::collision_pair_t &cp : temp_collision_pairs_) {
        SceneObject &obj1 = scene.objects[cp.b1];
        Physics &ph1 = physes[obj1.components[CompPhysics]];

        SceneObject &obj2 = scene.objects[cp.b2];
        Physics &ph2 = physes[obj2.components[CompPhysics]];

        if (ph1.body.inv_mass == real(0) && ph2.body.inv_mass == real(0)) {
            continue;
        }

        Phy::contact_t new_contact;
        if (Phy::Intersect(&ph1.body, &ph2.body, dt_s, new_contact)) {
            contacts_.push_back(new_contact);
        }
    }

    sort(begin(contacts_), end(contacts_));

    real accum_time = real(0);
    for (Phy::contact_t &contact : contacts_) {
        const real dt = contact.time_of_impact - accum_time;

        if (contact.body_a->inv_mass == real(0) && contact.body_b->inv_mass == real(0)) {
            continue;
        }

        // Update positions
        for (uint32_t i = 0; i < uint32_t(updated_objects_.size()); i++) {
            const uint32_t ndx = updated_objects_[i];
            SceneObject &obj = scene.objects[ndx];
            Physics &ph = physes[obj.components[CompPhysics]];

            ph.body.Update(dt);
        }

        Phy::ResolveContact(contact);
        accum_time += dt;
    }

    // Update the positions for the rest of this frame's time
    const real time_remaining = dt_s - accum_time;
    for (uint32_t i = 0; i < uint32_t(updated_objects_.size()) && time_remaining > real(0); i++) {
        const uint32_t ndx = updated_objects_[i];
        SceneObject &obj = scene.objects[ndx];
        Physics &ph = physes[obj.components[CompPhysics]];

        ph.body.Update(time_remaining);
    }
}

void Eng::PhysicsManager::SweepAndPrune1D(const Phy::Body bodies[], const int count, const float dt_s,
                                          std::vector<Phy::collision_pair_t> &collision_pairs) {
    temp_sorted_bodies_.clear();
    temp_sorted_bodies_.resize(count * 2ull);
    Phy::SortBodiesBounds(bodies, count, dt_s, temp_sorted_bodies_.data());

    collision_pairs.resize(temp_sorted_bodies_.size() * temp_sorted_bodies_.size());
    const int pair_count = Phy::BuildCollisionPairs(temp_sorted_bodies_.data(), count, collision_pairs.data());
    collision_pairs.resize(pair_count);
}