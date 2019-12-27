#pragma once

#include <cstddef>
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

struct DepthDrawBatch {
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
static_assert(offsetof(DepthDrawBatch, indices_count) == 4, "!");

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

struct ShadowList {
    int shadow_map_pos[2], shadow_map_size[2];
    int scissor_test_pos[2], scissor_test_size[2];
    uint32_t shadow_batch_start, shadow_batch_count;
    uint32_t solid_batches_count;

    // for debugging
    float cam_near, cam_far;
    Ren::Vec2f view_frustum_outline[12];
    int view_frustum_outline_count;
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

enum eRenderFlags : uint32_t {
    EnableZFill     = (1u << 0u),
    EnableCulling   = (1u << 1u),
    EnableSSR       = (1u << 2u),
    EnableSSAO      = (1u << 3u),
    EnableLightmap  = (1u << 4u),
    EnableLights    = (1u << 5u),
    EnableDecals    = (1u << 6u),
    EnableProbes    = (1u << 7u),
    EnableShadows   = (1u << 8u),
    EnableOIT       = (1u << 9u),
    EnableTonemap   = (1u << 10u),
    EnableBloom     = (1u << 11u),
    EnableFxaa      = (1u << 12u),
    EnableTimers    = (1u << 13u),
    DebugWireframe  = (1u << 14u),
    DebugCulling    = (1u << 15u),
    DebugShadow     = (1u << 16u),
    DebugReduce     = (1u << 17u),
    DebugLights     = (1u << 18u),
    DebugDeferred   = (1u << 19u),
    DebugBlur       = (1u << 20u),
    DebugDecals     = (1u << 21u),
    DebugSSAO       = (1u << 22u),
    DebugTimings    = (1u << 23u),
    DebugBVH        = (1u << 24u),
    DebugProbes     = (1u << 25u)
};

struct FrontendInfo {
    uint64_t start_timepoint_us = 0,
             end_timepoint_us = 0;
    uint32_t occluders_time_us = 0,
             main_gather_time_us = 0,
             shadow_gather_time_us = 0,
             drawables_sort_time_us = 0,
             items_assignment_time_us = 0;
};

struct BackendInfo {
    uint64_t cpu_start_timepoint_us = 0,
             cpu_end_timepoint_us = 0;
    uint64_t gpu_start_timepoint_us = 0,
             gpu_end_timepoint_us = 0;
    uint32_t skinning_time_us = 0,
             shadow_time_us = 0,
             depth_opaque_pass_time_us = 0,
             ao_pass_time_us = 0,
             opaque_pass_time_us = 0,
             transp_pass_time_us = 0,
             refl_pass_time_us = 0,
             blur_pass_time_us = 0,
             blit_pass_time_us = 0;
    int64_t gpu_cpu_time_diff_us = 0;

    uint32_t shadow_draw_calls_count = 0,
             depth_fill_draw_calls_count = 0,
             opaque_draw_calls_count = 0;

    uint32_t triangles_rendered = 0;
};

struct ItemsInfo {
    uint32_t light_sources_count = 0,
             decals_count = 0,
             probes_count = 0;
    uint32_t items_total = 0;
};