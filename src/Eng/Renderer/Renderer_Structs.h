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

struct EllipsItem {
    float position[3], radius;
    float axis[3]; uint32_t perp;
};
static_assert(sizeof(EllipsItem) == 8 * sizeof(float), "!");

struct CellData {
    uint32_t item_offset : 24;
    uint32_t light_count : 8;
    uint32_t decal_count : 8;
    uint32_t probe_count : 8;
    uint32_t ellips_count : 8;
    uint32_t _unused : 8;
};
static_assert(sizeof(CellData) == 8, "!");

struct ItemData {
    uint32_t light_index : 12;
    uint32_t decal_index : 12;
    uint32_t probe_index : 8;
    uint32_t ellips_index : 8;
    uint32_t _unused : 24;
};
static_assert(sizeof(ItemData) == 8, "!");

struct InstanceData {
    float model_matrix[3][4];
    union {
        float lmap_transform[4];
        struct {
            // float 0
            uint8_t movement_scale;
            uint8_t tree_mode;
            uint8_t bend_scale;
            uint8_t stretch;
            // float 1, 2
            uint16_t wind_dir_ls[3];
            uint16_t wind_turb;
        };
    };
};
static_assert(sizeof(InstanceData) == 64, "!");

struct DepthDrawBatch { // NOLINT
    static const uint32_t TypeSimple    = 0b00u;    // simple
    static const uint32_t TypeVege      = 0b01u;    // vegetation
    static const uint32_t TypeSkinned   = 0b10u;    // skeletal

    static const uint32_t BitsSimple    = (TypeSimple   << 30u);
    static const uint32_t BitsVege      = (TypeVege     << 30u);
    static const uint32_t BitsSkinned   = (TypeSkinned  << 30u);
    static const uint32_t BitAlphaTest  = (1u << 29u);
    static const uint32_t BitMoving     = (1u << 28u);
    static const uint32_t BitTwoSided   = (1u << 27u);
    static const uint32_t FlagBits      = (0b11111u << 27u);

    union {
        struct {
            uint32_t indices_offset : 27;
            uint32_t two_sided_bit  : 1;
            uint32_t moving_bit     : 1;    // object uses two transforms
            uint32_t alpha_test_bit : 1;
            uint32_t type_bits      : 2;
        };
        uint32_t sort_key = 0;
    };

    uint32_t indices_count, mat_id;
    int32_t base_vertex;
    int instance_indices[REN_MAX_BATCH_SIZE], instance_count;
};
static_assert(offsetof(DepthDrawBatch, indices_count) == 4, "!");

struct MainDrawBatch {  // NOLINT
    static const uint64_t BitAlphaBlend = (1ull << 63u);
    static const uint64_t BitAlphaTest  = (1ull << 62u);
    static const uint64_t BitTwoSided   = (1ull << 61u);
    static const uint64_t BitsProgId    = (0b11111111ull << 53u);
    static const uint64_t BitsMatId     = (0b11111111111111ull << 39u);
    static const uint64_t BitsCamDist   = (0b11111111ull << 31u);
    static const uint64_t FlagBits      = (0b111ull << 61u);

    union {
        struct {
            uint64_t _pad1              : 4;
            uint64_t indices_offset     : 27;
            uint64_t cam_dist           : 8;
            uint64_t mat_id             : 14;
            uint64_t prog_id            : 8;
            uint64_t two_sided_bit      : 1;
            uint64_t alpha_test_bit     : 1;
            uint64_t alpha_blend_bit    : 1;
        };
        uint64_t sort_key = 0;
    };

    uint32_t indices_count;
    int32_t base_vertex;
    int instance_indices[REN_MAX_BATCH_SIZE], instance_count;
};
static_assert(offsetof(MainDrawBatch, indices_count) == 8, "!");

struct ShadowList { // NOLINT
    int shadow_map_pos[2], shadow_map_size[2];
    int scissor_test_pos[2], scissor_test_size[2];
    uint32_t shadow_batch_start, shadow_batch_count;

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
static_assert(sizeof(SortSpan32) == 12, "!");

struct SortSpan64 {
    uint64_t key;
    uint32_t base;
    uint32_t count;
};
static_assert(sizeof(SortSpan64) == 16, "!");

struct SkinTransform {
    float matr[3][4];
};
static_assert(sizeof(SkinTransform) == 48, "!");

struct SkinRegion {
    uint32_t in_vtx_offset, out_vtx_offset;
    uint32_t xform_offset, vertex_count;
};

enum eRenderFlags : uint32_t {
    EnableZFill     = (1u << 0u),
    EnableCulling   = (1u << 1u),
    EnableSSR       = (1u << 2u),
    EnableSSAO      = (1u << 3u),
    EnableLightmap  = (1u << 4u),
    EnableLights    = (1u << 5u),
    EnableDecals    = (1u << 6u),
    EnableProbes    = (1u << 7u),
    EnableDOF       = (1u << 8u),
    EnableShadows   = (1u << 9u),
    EnableOIT       = (1u << 10u),
    EnableTonemap   = (1u << 11u),
    EnableBloom     = (1u << 12u),
    EnableMsaa      = (1u << 13u),
    EnableTaa       = (1u << 14u),
    EnableFxaa      = (1u << 15u),
    EnableTimers    = (1u << 16u),
    DebugWireframe  = (1u << 17u),
    DebugCulling    = (1u << 18u),
    DebugShadow     = (1u << 19u),
    DebugReduce     = (1u << 20u),
    DebugLights     = (1u << 21u),
    DebugDeferred   = (1u << 22u),
    DebugBlur       = (1u << 23u),
    DebugDecals     = (1u << 24u),
    DebugSSAO       = (1u << 25u),
    DebugTimings    = (1u << 26u),
    DebugBVH        = (1u << 27u),
    DebugProbes     = (1u << 28u),
    DebugEllipsoids = (1u << 29u)
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
             taa_pass_time_us = 0,
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
