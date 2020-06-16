#pragma once

#include <cstdint>
#include <cstring>

#include <iosfwd>
#include <vector>

#include "MMat.h"
#include "MQuat.h"

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
    AnimSequence(const char *name, std::istream &data);

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

    void LinkBones(const Bone *bones, int bones_count, int *out_bone_indices);
    void LinkShapes(const ShapeKey *shapes, int shapes_count, int *out_shape_indices);
    void Update(float time);
    void InterpolateFrames(int fr_0, int fr_1, float t);
};

typedef StorageRef<AnimSequence> AnimSeqRef;
typedef Storage<AnimSequence> AnimSeqStorage;

struct AnimLink {
    AnimSeqRef anim;
    std::unique_ptr<int[]> anim_bones;
    std::unique_ptr<int[]> anim_shapes;
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
    std::unique_ptr<Bone[]> bones;
    int bones_count = 0;
    std::unique_ptr<ShapeKey[]> shapes;
    int shapes_count = 0;
    std::vector<AnimLink> anims;
    // std::vector<BoneGroup>  bone_groups;

    Bone *find_bone(const char *name) {
        for (int i = 0; i < bones_count; i++) {
            if (strcmp(bones[i].name, name) == 0) {
                return &bones[i];
            }
        }
        return nullptr;
    }

    Vec3f bone_pos(const char *name);
    Vec3f bone_pos(int i);

    void bone_matrix(const char *name, Mat4f &mat);
    void bone_matrix(int i, Mat4f &mat);

    int AddAnimSequence(const AnimSeqRef &ref);

    void MarkChildren();
    void ApplyAnim(int id);
    void ApplyAnim(int anim_id1, int anim_id2, float t);
    void UpdateAnim(int anim_id, float t);
    void UpdateBones(Ren::Mat4f *matr_palette);
    int UpdateShapes(uint16_t* out_shape_palette);
};
} // namespace Ren