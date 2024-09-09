#pragma once

#include <cstdint>
#include <cstring>

#include <iosfwd>
#include <vector>

#include "MMat.h"
#include "MQuat.h"

#include "Span.h"
#include "Storage.h"
#include "String.h"

namespace Ren {
enum class eAnimBoneFlags { AnimHasTranslate = 1 };

struct AnimBone {
    char name[64];
    char parent_name[64];
    int id = -1;
    int offset = 0;
    uint32_t flags = 0;
    Vec3f cur_pos;
    Quatf cur_rot;

    AnimBone() { // NOLINT
        name[0] = parent_name[0] = '\0';
    }
};

struct AnimShape {
    char name[64];
    int offset = 0;
    float cur_weight = 0.0f;

    AnimShape() { // NOLINT
        name[0] = '\0';
    }
};

struct Bone;
struct ShapeKey;

class AnimSequence : public RefCounter {
    String name_, act_name_;
    int fps_ = 0;
    int len_ = 0;
    int frame_size_ = 0;
    float frame_dur_ = 0;
    float anim_dur_ = 0;
    std::vector<float> frames_;
    std::vector<AnimBone> bones_;
    std::vector<AnimShape> shapes_;
    bool ready_ = false;

  public:
    AnimSequence() = default;
    AnimSequence(std::string_view name, std::istream &data);

    const String &name() const { return name_; }
    const String &act_name() const { return act_name_; }
    int fps() const { return fps_; }
    int len() const { return len_; }
    int frame_size() const { return frame_size_; }
    float frame_dur() const { return frame_dur_; }
    float anim_dur() const { return anim_dur_; }
    size_t num_bones() const { return bones_.size(); }
    bool ready() const { return ready_; }

    const float *frames() const { return &frames_[0]; }
    const AnimBone *bone(const int i) { return &bones_[i]; }
    const AnimShape *shape(const int i) { return &shapes_[i]; }

    void Init(std::istream &data);
    void InitAnimBones(std::istream &data);

    void LinkBones(Span<const Bone> bones, int *out_bone_indices);
    void LinkShapes(Span<const ShapeKey> shapes, int *out_shape_indices);
    void Update(float time);
    void InterpolateFrames(int fr_0, int fr_1, float t);
};

typedef StrongRef<AnimSequence> AnimSeqRef;
typedef Storage<AnimSequence> AnimSeqStorage;

struct AnimLink {
    AnimSeqRef anim;
    std::vector<int> anim_bones;
    std::vector<int> anim_shapes;
};

struct Bone {
    char name[64];
    int id = -1;
    int parent_id = -1;
    bool dirty = false;
    Mat4f cur_matrix;
    Mat4f cur_comb_matrix;
    Mat4f bind_matrix;
    Mat4f inv_bind_matrix;
    Vec3f head_pos;

    Bone() { // NOLINT
        name[0] = '\0';
    }
};

struct ShapeKey {
    char name[64];
    uint32_t delta_offset, delta_count;
    uint16_t cur_weight_packed;
};

/*struct BoneGroup {
    std::vector<int> strip_ids;
    std::vector<int> bone_ids;
};*/

struct Skeleton {
    std::vector<Bone> bones;
    std::vector<ShapeKey> shapes;
    std::vector<AnimLink> anims;
    // std::vector<BoneGroup>  bone_groups;

    [[nodiscard]] const Bone *find_bone(std::string_view name) const {
        for (int i = 0; i < int(bones.size()); i++) {
            if (name == bones[i].name) {
                return &bones[i];
            }
        }
        return nullptr;
    }

    [[nodiscard]] Vec3f bone_pos(std::string_view name) const;
    [[nodiscard]] Vec3f bone_pos(int i) const;

    void bone_matrix(std::string_view name, Mat4f &mat) const;
    void bone_matrix(int i, Mat4f &mat) const;

    int AddAnimSequence(AnimSeqRef ref);

    void MarkChildren();
    void ApplyAnim(int id);
    void ApplyAnim(int anim_id1, int anim_id2, float t);
    void UpdateAnim(int anim_id, float t);
    void UpdateBones(Ren::Mat4f *matr_palette);
    int UpdateShapes(uint16_t *out_shape_palette);
};
} // namespace Ren