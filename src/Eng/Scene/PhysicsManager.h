#pragma once

#include <cstdint>

#include <vector>

struct SceneData;

namespace Phy {
class Body;

struct collision_pair_t;
struct contact_t;
struct pseudo_body_t;
} // namespace Phy

class PhysicsManager {
    std::vector<uint32_t> updated_objects_;
    std::vector<Phy::contact_t> contacts_;

    std::vector<Phy::pseudo_body_t> temp_sorted_bodies_;
    std::vector<Phy::collision_pair_t> temp_collision_pairs_;

    void SweepAndPrune1D(const Phy::Body bodies[], int count, float dt_s,
                         std::vector<Phy::collision_pair_t> &collision_pairs);

  public:
    void Update(SceneData &scene, float dt_s);

    const uint32_t *updated_objects(uint32_t &out_count) const {
        out_count = uint32_t(updated_objects_.size());
        return updated_objects_.data();
    }
};