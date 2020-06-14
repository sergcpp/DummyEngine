#include "Anim.h"

#include <istream>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)
#endif

Ren::AnimSequence::AnimSequence(const char *name, std::istream &data) {
    name_ = String{ name };
    Init(data);
}

void Ren::AnimSequence::Init(std::istream& data) {
    InitAnimBones(data);
}

void Ren::AnimSequence::InitAnimBones(std::istream &data) {
    if (!data) {
        ready_ = false;
        return;
    }

    char str[12];
    data.read(str, 12);
    assert(strcmp(str, "ANIM_SEQUEN\0") == 0);

    enum {
        SKELETON_CHUNK,
        SHAPES_CHUNK,
        ANIM_INFO_CHUNK,
        FRAMES_CHUNK
    };

    struct ChunkPos {
        int offset;
        int length;
    };

    struct Header {
        int num_chunks;
        ChunkPos p[4];
    } file_header = {};

    data.read((char *)&file_header.num_chunks, sizeof(int));
    data.read((char *)&file_header.p[0], file_header.num_chunks * sizeof(ChunkPos));

    const size_t bones_count = (size_t)file_header.p[SKELETON_CHUNK].length / (64 + 64 + 4);
    bones_.resize(bones_count);
    int offset = 0;
    for (size_t i = 0; i < bones_count; i++) {
        bones_[i].id = (int)i;
        bones_[i].flags = 0;
        data.read(bones_[i].name, 64);
        data.read(bones_[i].parent_name, 64);
        int has_translate_anim = 0;
        data.read((char *)&has_translate_anim, 4);
        if (has_translate_anim) bones_[i].flags |= uint32_t(eAnimBoneFlags::AnimHasTranslate);
        bones_[i].offset = offset;
        if (has_translate_anim) {
            offset += 7;
        } else {
            offset += 4;
        }
    }

    if (file_header.num_chunks == 4) {
        const size_t shapes_count = (size_t)file_header.p[SHAPES_CHUNK].length / 64;
        shapes_.resize(shapes_count);
        for (size_t i = 0; i < shapes_count; i++) {
            data.read(shapes_[i].name, 64);
            shapes_[i].cur_weight = 0.0f;
            offset += 1;
        }
    }

    // support old layout
    const int AnimInfoChunk = (file_header.num_chunks == 4) ? ANIM_INFO_CHUNK : ANIM_INFO_CHUNK - 1;
    const int FramesChunk = (file_header.num_chunks == 4) ? FRAMES_CHUNK : FRAMES_CHUNK - 1;

    assert(file_header.p[AnimInfoChunk].length == 64 + 2 * sizeof(int32_t));

    frame_size_ = offset;
    char act_name[64];
    data.read(act_name, 64);
    act_name_ = String{ act_name };
    data.read((char *)&fps_, 4);
    data.read((char *)&len_, 4);

    frames_.resize(file_header.p[FramesChunk].length / 4);
    data.read((char *)&frames_[0], (size_t)file_header.p[FramesChunk].length);

    frame_dur_ = 1.0f / (float)fps_;
    anim_dur_ = (float)len_ * frame_dur_;

    ready_ = true;
}

std::vector<Ren::AnimBone *> Ren::AnimSequence::LinkBones(std::vector<Bone> &_bones) {
    std::vector<AnimBone *> anim_bones;
    anim_bones.reserve(_bones.size());
    for (size_t i = 0; i < _bones.size(); i++) {
        bool added = false;
        for (AnimBone &bone : this->bones_) {
            if (strcmp(_bones[i].name, bone.name) == 0) {
                if (_bones[i].parent_id != -1) {
                    assert(strcmp(_bones[_bones[i].parent_id].name, bone.parent_name) == 0);
                }
                anim_bones.push_back(&bone);
                added = true;
                break;
            }
        }
        if (!added) {
            anim_bones.push_back(nullptr);
        }
    }
    return anim_bones;
}

void Ren::AnimSequence::Update(float time) {
    if (len_ < 2) return;

    while (time > anim_dur_) time -= anim_dur_;
    while (time < 0.0f) time += anim_dur_;

    float frame = time * (float)fps_;
    float frame_fl = std::floor(frame);
    int fr_0 = (int)frame;
    int fr_1 = (int)std::ceil(frame);

    fr_0 = fr_0 % len_;
    fr_1 = fr_1 % len_;
    float t = frame - frame_fl;
    InterpolateFrames(fr_0, fr_1, t);
}

void Ren::AnimSequence::InterpolateFrames(int fr_0, int fr_1, float t) {
    for (AnimBone &bone : bones_) {
        int offset = bone.offset;
        if (bone.flags & uint32_t(eAnimBoneFlags::AnimHasTranslate)) {
            Vec3f p1 = MakeVec3(&frames_[fr_0 * frame_size_ + offset]);
            Vec3f p2 = MakeVec3(&frames_[fr_1 * frame_size_ + offset]);
            bone.cur_pos = Mix(p1, p2, t);
            offset += 3;
        }
        Quatf q1 = MakeQuat(&frames_[fr_0 * frame_size_ + offset]);
        Quatf q2 = MakeQuat(&frames_[fr_1 * frame_size_ + offset]);
        bone.cur_rot = Mix(q1, q2, t);
    }
}

// skeleton

Ren::Vec3f Ren::Skeleton::bone_pos(const char *name) {
    auto bone_it = bone(name);
    Vec3f ret;
    const float *m = ValuePtr(bone_it->cur_comb_matrix);
    /*ret[0] = -(m[0] * m[12] + m[1] * m[13] + m[2] * m[14]);
    ret[1] = -(m[4] * m[12] + m[5] * m[13] + m[6] * m[14]);
    ret[2] = -(m[8] * m[12] + m[9] * m[13] + m[10] * m[14]);*/

    ret[0] = m[12];
    ret[1] = m[13];
    ret[2] = m[14];

    return ret;
}

Ren::Vec3f Ren::Skeleton::bone_pos(int i) {
    auto bone_it = &bones[i];
    Vec3f ret;
    const float *m = ValuePtr(bone_it->cur_comb_matrix);
    /*ret[0] = -(m[0] * m[12] + m[1] * m[13] + m[2] * m[14]);
    ret[1] = -(m[4] * m[12] + m[5] * m[13] + m[6] * m[14]);
    ret[2] = -(m[8] * m[12] + m[9] * m[13] + m[10] * m[14]);*/

    ret[0] = m[12];
    ret[1] = m[13];
    ret[2] = m[14];

    return ret;
}

void Ren::Skeleton::bone_matrix(const char *name, Mat4f &mat) {
    auto bone_it = bone(name);
    assert(bone_it != bones.end());
    mat = bone_it->cur_comb_matrix;
}

void Ren::Skeleton::bone_matrix(int i, Mat4f &mat) {
    mat = bones[i].cur_comb_matrix;
}

void Ren::Skeleton::UpdateBones(Ren::Mat4f *matr_palette) {
    for (size_t i = 0; i < bones.size(); i++) {
        if (bones[i].dirty) {
            if (bones[i].parent_id != -1) {
                bones[i].cur_comb_matrix = bones[bones[i].parent_id].cur_comb_matrix * bones[i].cur_matrix;
            } else {
                bones[i].cur_comb_matrix = bones[i].cur_matrix;
            }
            bones[i].dirty = false;
        }
        matr_palette[i] = bones[i].cur_comb_matrix * bones[i].inv_bind_matrix;
    }
}

int Ren::Skeleton::AddAnimSequence(const AnimSeqRef &ref) {
    for (int i = 0; i < (int)anims.size(); i++) {
        if (anims[i].anim == ref) {
            return i;
        }
    }
    anims.emplace_back();
    AnimLink &a = anims.back();
    a.anim = ref;
    a.anim_bones = a.anim->LinkBones(bones);
    return int(anims.size() - 1);
}

void Ren::Skeleton::MarkChildren() {
    for (Bone &bone : bones) {
        if (bone.parent_id != -1 && bones[bone.parent_id].dirty) {
            bone.dirty = true;
        }
    }
}

void Ren::Skeleton::ApplyAnim(int id) {
    for (size_t i = 0; i < bones.size(); i++) {
        if (anims[id].anim_bones[i]) {
            Mat4f m = Mat4f{ 1.0f };
            if (anims[id].anim_bones[i]->flags & uint32_t(eAnimBoneFlags::AnimHasTranslate)) {
                m = Translate(m, anims[id].anim_bones[i]->cur_pos);
            } else {
                m = Translate(m, bones[i].head_pos);
            }
            m *= ToMat4(anims[id].anim_bones[i]->cur_rot);
            bones[i].cur_matrix = m;
            bones[i].dirty = true;
        }
    }
    MarkChildren();
}

void Ren::Skeleton::ApplyAnim(int anim_id1, int anim_id2, float t) {
    for (size_t i = 0; i < bones.size(); i++) {
        if (anims[anim_id1].anim_bones[i] || anims[anim_id2].anim_bones[i]) {
            Mat4f m(1.0f);
            Vec3f pos;
            Quatf orient;
            if (anims[anim_id1].anim_bones[i]) {
                if (anims[anim_id1].anim_bones[i]->flags & uint32_t(eAnimBoneFlags::AnimHasTranslate)) {
                    pos = anims[anim_id1].anim_bones[i]->cur_pos;
                } else {
                    pos = bones[i].head_pos;
                }
                orient = anims[anim_id1].anim_bones[i]->cur_rot;
            }
            if (anims[anim_id2].anim_bones[i]) {
                if (anims[anim_id2].anim_bones[i]->flags & uint32_t(eAnimBoneFlags::AnimHasTranslate)) {
                    pos = Mix(pos, anims[anim_id2].anim_bones[i]->cur_pos, t);
                }
                orient = Slerp(orient, anims[anim_id2].anim_bones[i]->cur_rot, t);
            }
            m = Translate(m, pos);
            m *= ToMat4(orient);
            bones[i].cur_matrix = m;
            bones[i].dirty = true;
        }
    }
    MarkChildren();
}

void Ren::Skeleton::UpdateAnim(int anim_id, float t) {
    anims[anim_id].anim->Update(t);
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif
