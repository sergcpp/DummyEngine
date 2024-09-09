#pragma once

#include <cstdint>

#include <vector>

#include <Ren/Span.h>

namespace Phy {
class Body;

struct collision_pair_t;
struct contact_t;
struct pseudo_body_t;
} // namespace Phy

namespace Eng {
struct SceneData;
class PhysicsManager {
    std::vector<uint32_t> updated_objects_;
    std::vector<Phy::contact_t> contacts_;

    std::vector<Phy::pseudo_body_t> temp_sorted_bodies_;
    std::vector<Phy::collision_pair_t> temp_collision_pairs_;

    void SweepAndPrune1D(const Phy::Body bodies[], int count, float dt_s,
                         std::vector<Phy::collision_pair_t> &collision_pairs);

  public:
    void Update(SceneData &scene, float dt_s);

    [[nodiscard]] Ren::Span<const uint32_t> updated_objects() const {
        return updated_objects_;
    }
};
} // namespace Eng