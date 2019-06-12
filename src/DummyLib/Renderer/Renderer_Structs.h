#pragma once

#include <cstdint>

#include <Ren/MMat.h>

#include "Renderer_GL_Defines.inl"

struct LightSourceItem {
    float pos[3], radius;
    float col[3]; int shadowreg_index;
    float dir[3], spot;
};
static_assert(sizeof(LightSourceItem) == 48, "!");

struct DecalItem {
    float mat[3][4];
    float diff[4], norm[4], spec[4];
};
static_assert(sizeof(DecalItem) == 24 * sizeof(float), "!");

struct ProbeItem {
    float position[3], radius;
    float _unused[3], layer;
    float sh_coeffs[3][4];
};
static_assert(sizeof(ProbeItem) == 20 * sizeof(float), "!");

struct CellData {
    uint32_t item_offset : 24;
    uint32_t light_count : 8;
    uint32_t decal_count : 8;
    uint32_t probe_count : 8;
    uint32_t _unused : 16;
};
static_assert(sizeof(CellData) == 8, "!");

struct ItemData {
    uint32_t light_index : 12;
    uint32_t decal_index : 12;
    uint32_t probe_index : 8;
};
static_assert(sizeof(ItemData) == 4, "!");

struct InstanceData {
    float model_matrix[3][4];
    float lmap_transform[4];
};
static_assert(sizeof(InstanceData) == 64, "!");

struct ShadowDrawBatch {
    union {
        struct {
            uint32_t _pad1 : 3;
            uint32_t indices_offset : 28;
            uint32_t alpha_test_bit : 1;
        };
        uint32_t sort_key = 0;
    };

    uint32_t indices_count, mat_id;
    int32_t base_vertex;
    int instance_indices[REN_MAX_BATCH_SIZE], instance_count;
};
static_assert(sizeof(ShadowDrawBatch) == sizeof(uint32_t) + 3 * sizeof(uint32_t) + sizeof(int) * REN_MAX_BATCH_SIZE + sizeof(int), "!");

struct MainDrawBatch {
    union {
        struct {
            uint32_t _pad1              : 4;
            uint32_t indices_offset     : 28;
            uint32_t cam_dist           : 8;
            uint32_t mat_id             : 14;
            uint32_t alpha_test_bit     : 1;
            uint32_t prog_id            : 8;
            uint32_t alpha_blend_bit    : 1;
        };
        uint64_t sort_key = 0;
    };

    uint32_t indices_count;
    int32_t base_vertex;
    int instance_indices[REN_MAX_BATCH_SIZE], instance_count;
};
static_assert(offsetof(MainDrawBatch, indices_count) == 8, "!");
//static_assert(sizeof(MainDrawBatch) == sizeof(uint64_t) + 2 * sizeof(uint32_t) + sizeof(int) * REN_MAX_BATCH_SIZE + sizeof(int), "!");

struct ShadowList {
    int shadow_map_pos[2], shadow_map_size[2];
    uint32_t shadow_batch_start, shadow_batch_count;
    float cam_near, cam_far; // for debugging
};

struct ShadowMapRegion {
    Ren::Vec4f transform;
    Ren::Mat4f clip_from_world;
};
static_assert(sizeof(ShadowMapRegion) == 80, "!");

struct SortSpan32 {
    uint32_t key;
    uint32_t base;
    uint32_t count;
};

struct SortSpan64 {
    uint64_t key;
    uint32_t base;
    uint32_t count;
};

struct SkinTransform {
    float matr[3][4];
};
static_assert(sizeof(SkinTransform) == 48, "!");

struct SkinRegion {
    uint32_t in_vtx_offset, out_vtx_offset;
    uint16_t xform_offset, vertex_count;
};
static_assert(sizeof(SkinRegion) == 12, "!");