#include "Anim.h"

#include <istream>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)
#endif

Ren::AnimSequence::AnimSequence(const char *name, std::istream &data) {
    name_ = String{name};
    Init(data);
}

void Ren::AnimSequence::Init(std::istream &data) { InitAnimBones(data); }

void Ren::AnimSequence::InitAnimBones(std::istream &data) {
    if (!data) {
        ready_ = false;
        return;
    }

    char str[12];
    data.read(str, 12);
    assert(strcmp(str, "ANIM_SEQUEN\0") == 0);

    enum { SKELETON_CHUNK, SHAPES_CHUNK, ANIM_INFO_CHUNK, FRAMES_CHUNK };

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
        if (has_translate_anim) {
            bones_[i].flags |= uint32_t(eAnimBoneFlags::AnimHasTranslate);
        }
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
            shapes_[i].offset = offset;
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
    act_name_ = String{act_name};
    data.read((char *)&fps_, 4);
    data.read((char *)&len_, 4);

    frames_.resize(file_header.p[FramesChunk].length / 4);
    data.read((char *)&frames_[0], (size_t)file_header.p[FramesChunk].length);

    frame_dur_ = 1.0f / float(fps_);
    anim_dur_ = float(len_) * frame_dur_;

    ready_ = true;
}

void Ren::AnimSequence::LinkBones(const Bone *bones, const int bones_count, int *out_bone_indices) {
    for (int i = 0; i < bones_count; i++) {
        out_bone_indices[i] = -1;
        for (int j = 0; j < int(bones_.size()); j++) {
            if (strcmp(bones[i].name, bones_[j].name) == 0) {
                if (bones[i].parent_id != -1) {
                    assert(strcmp(bones[bones[i].parent_id].name, bones_[j].parent_name) == 0);
                }
                out_bone_indices[i] = j;
                break;
            }
        }
    }
}

void Ren::AnimSequence::LinkShapes(const ShapeKey *shapes, const int shapes_count, int *out_shape_indices) {
    for (int i = 0; i < shapes_count; i++) {
        out_shape_indices[i] = -1;
        for (int j = 0; j < int(shapes_.size()); j++) {
            if (strcmp(shapes[i].name, shapes_[j].name) == 0) {
                out_shape_indices[i] = j;
                break;
            }
        }
    }
}

void Ren::AnimSequence::Update(float time) {
    if (len_ < 2) {
        return;
    }

    while (time > anim_dur_) {
        time -= anim_dur_;
    }
    while (time < 0.0f) {
        time += anim_dur_;
    }

    const float frame = time * float(fps_);
    const float frame_fl = std::floor(frame);
    const int fr_0 = int(frame) % len_;
    const int fr_1 = int(std::ceil(frame)) % len_;
    const float t = frame - frame_fl;
    InterpolateFrames(fr_0, fr_1, t);
}

void Ren::AnimSequence::InterpolateFrames(const int fr_0, const int fr_1, const float t) {
    for (AnimBone &bone : bones_) {
        int offset = bone.offset;
        if (bone.flags & uint32_t(eAnimBoneFlags::AnimHasTranslate)) {
            const Vec3f p1 = MakeVec3(&frames_[fr_0 * frame_size_ + offset]);
            const Vec3f p2 = MakeVec3(&frames_[fr_1 * frame_size_ + offset]);
            bone.cur_pos = Mix(p1, p2, t);
            offset += 3;
        }
        const Quatf q1 = MakeQuat(&frames_[fr_0 * frame_size_ + offset]);
        const Quatf q2 = MakeQuat(&frames_[fr_1 * frame_size_ + offset]);
        bone.cur_rot = Mix(q1, q2, t);
    }

    for (AnimShape &shape : shapes_) {
        const int offset = shape.offset;
        const float w1 = frames_[fr_0 * frame_size_ + offset];
        const float w2 = frames_[fr_1 * frame_size_ + offset];
        shape.cur_weight = Mix(w1, w2, t);
    }
}

// skeleton

Ren::Vec3f Ren::Skeleton::bone_pos(const char *name) {
    const Bone *bone = find_bone(name);
    Vec3f ret;
    const float *m = ValuePtr(bone->cur_comb_matrix);
    /*ret[0] = -(m[0] * m[12] + m[1] * m[13] + m[2] * m[14]);
    ret[1] = -(m[4] * m[12] + m[5] * m[13] + m[6] * m[14]);
    ret[2] = -(m[8] * m[12] + m[9] * m[13] + m[10] * m[14]);*/

    ret[0] = m[12];
    ret[1] = m[13];
    ret[2] = m[14];

    return ret;
}

Ren::Vec3f Ren::Skeleton::bone_pos(const int i) {
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
    const Bone *bone = find_bone(name);
    assert(bone);
    mat = bone->cur_comb_matrix;
}

void Ren::Skeleton::bone_matrix(const int i, Mat4f &mat) { mat = bones[i].cur_comb_matrix; }

void Ren::Skeleton::UpdateBones(Ren::Mat4f *matr_palette) {
    for (int i = 0; i < bones_count; i++) {
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

int Ren::Skeleton::UpdateShapes(uint16_t *out_shape_palette) {
    int active_shapes_count = 0;

    for (int i = 0; i < shapes_count; i++) {
        const uint16_t weight_packed = shapes[i].cur_weight_packed;
        if (weight_packed) {
            out_shape_palette[2 * active_shapes_count + 0] = uint16_t(i);
            out_shape_palette[2 * active_shapes_count + 1] = weight_packed;
            ++active_shapes_count;
        }
    }

    return active_shapes_count;
}

int Ren::Skeleton::AddAnimSequence(AnimSeqRef ref) {
    for (int i = 0; i < int(anims.size()); i++) {
        if (anims[i].anim == ref) {
            return i;
        }
    }
    anims.emplace_back();
    AnimLink &a = anims.back();
    a.anim = std::move(ref);
    a.anim_bones.reset(new int[bones_count]);
    a.anim->LinkBones(&bones[0], bones_count, &a.anim_bones[0]);
    a.anim_shapes.reset(new int[bones_count]);
    a.anim->LinkShapes(&shapes[0], shapes_count, &a.anim_shapes[0]);
    return int(anims.size() - 1);
}

void Ren::Skeleton::MarkChildren() {
    for (int i = 0; i < bones_count; i++) {
        if (bones[i].parent_id != -1 && bones[bones[i].parent_id].dirty) {
            bones[i].dirty = true;
        }
    }
}

void Ren::Skeleton::ApplyAnim(const int id) {
    for (int i = 0; i < bones_count; i++) {
        const int ndx = anims[id].anim_bones[i];
        if (ndx != -1) {
            const AnimBone *abone = anims[id].anim->bone(ndx);
            Mat4f m = Mat4f{1.0f};
            if (abone->flags & uint32_t(eAnimBoneFlags::AnimHasTranslate)) {
                m = Translate(m, abone->cur_pos);
            } else {
                m = Translate(m, bones[i].head_pos);
            }
            m *= ToMat4(abone->cur_rot);
            bones[i].cur_matrix = m;
            bones[i].dirty = true;
        }
    }
    MarkChildren();

    for (int i = 0; i < shapes_count; i++) {
        const int ndx = anims[id].anim_shapes[i];
        if (ndx != -1) {
            const AnimShape *ashape = anims[id].anim->shape(ndx);
            shapes[i].cur_weight_packed = uint16_t(ashape->cur_weight * 65535);
        }
    }
}

void Ren::Skeleton::ApplyAnim(const int anim_id1, const int anim_id2, const float t) {
    for (int i = 0; i < bones_count; i++) {
        if (anims[anim_id1].anim_bones[i] != -1 || anims[anim_id2].anim_bones[i] != -1) {
            const int ndx1 = anims[anim_id1].anim_bones[i];
            const int ndx2 = anims[anim_id2].anim_bones[i];

            Mat4f m(1.0f);
            Vec3f pos;
            Quatf orient;
            if (ndx1 != -1) {
                const AnimBone *abone1 = anims[anim_id1].anim->bone(ndx1);
                if (abone1->flags & uint32_t(eAnimBoneFlags::AnimHasTranslate)) {
                    pos = abone1->cur_pos;
                } else {
                    pos = bones[i].head_pos;
                }
                orient = abone1->cur_rot;
            }
            if (ndx2 != -1) {
                const AnimBone *abone2 = anims[anim_id2].anim->bone(anims[anim_id2].anim_bones[i]);
                if (abone2->flags & uint32_t(eAnimBoneFlags::AnimHasTranslate)) {
                    pos = Mix(pos, abone2->cur_pos, t);
                }
                orient = Slerp(orient, abone2->cur_rot, t);
            }
            m = Translate(m, pos);
            m *= ToMat4(orient);
            bones[i].cur_matrix = m;
            bones[i].dirty = true;
        }
    }
    MarkChildren();
}

void Ren::Skeleton::UpdateAnim(const int anim_id, const float t) { anims[anim_id].anim->Update(t); }

#ifdef _MSC_VER
#pragma warning(pop)
#endif
