#pragma once

#include <cstddef>
#include <cstdint>

#include <Ren/MMat.h>
#include <Ren/SmallVector.h>

#include "Renderer_GL_Defines.inl"

struct LightSourceItem {
    float pos[3], radius;
    float col[3];
    int shadowreg_index;
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
    float axis[3];
    uint32_t perp;
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
    float normal_matrix[3][4];
    float _vec[4];
};
static_assert(sizeof(InstanceData) == 128, "!");

struct DepthDrawBatch {                        // NOLINT
    static const uint32_t TypeSimple = 0b00u;  // simple
    static const uint32_t TypeVege = 0b01u;    // vegetation
    static const uint32_t TypeSkinned = 0b10u; // skeletal
    // TODO: this probably should not be a type
    static const uint32_t TypeTaaResp = 0b11u; // video-textured etc.

    static const uint32_t BitsSimple = (TypeSimple << 30u);
    static const uint32_t BitsVege = (TypeVege << 30u);
    static const uint32_t BitsSkinned = (TypeSkinned << 30u);
    static const uint32_t BitsTaaResp = (TypeTaaResp << 30u);
    static const uint32_t BitAlphaTest = (1u << 29u);
    static const uint32_t BitMoving = (1u << 28u);
    static const uint32_t BitTwoSided = (1u << 27u);
    static const uint32_t FlagBits = (0b11111u << 27u);

    union {
        struct {
            uint32_t indices_offset : 27;
            uint32_t two_sided_bit : 1;
            uint32_t moving_bit : 1; // object uses two transforms
            uint32_t alpha_test_bit : 1;
            uint32_t type_bits : 2;
        };
        uint32_t sort_key = 0;
    };

    uint32_t indices_count;
    int32_t base_vertex;
    int32_t instance_indices[REN_MAX_BATCH_SIZE][2];
    uint32_t instance_count;
};
static_assert(offsetof(DepthDrawBatch, indices_count) == 4, "!");

struct MainDrawBatch { // NOLINT
    static const uint64_t BitAlphaBlend = (1ull << 63u);
    static const uint64_t BitAlphaTest = (1ull << 62u);
    static const uint64_t BitDepthWrite = (1ull << 61u);
    static const uint64_t BitTwoSided = (1ull << 60u);
    static const uint64_t BitsProgId = (0b11111111ull << 54u);
    static const uint64_t BitsMatId = (0b11111111111111ull << 40u);
    static const uint64_t BitsCamDist = (0b11111111ull << 32u);
    static const uint64_t FlagBits = (0b1111ull << 60u);

    union {
        struct {
            uint64_t _pad1 : 3;
            uint64_t indices_offset : 27;
            uint64_t cam_dist : 8;
            uint64_t mat_id : 14;
            uint64_t pipe_id : 8;
            uint64_t two_sided_bit : 1;
            uint64_t depth_write_bit : 1;
            uint64_t alpha_test_bit : 1;
            uint64_t alpha_blend_bit : 1;
        };
        uint64_t sort_key = 0;
    };

    uint32_t indices_count;
    int32_t base_vertex;
    int32_t instance_indices[REN_MAX_BATCH_SIZE][2];
    uint32_t instance_count;
};
static_assert(offsetof(MainDrawBatch, indices_count) == 8, "!");

struct ShadowList { // NOLINT
    int shadow_map_pos[2], shadow_map_size[2];
    int scissor_test_pos[2], scissor_test_size[2];
    uint32_t shadow_batch_start, shadow_batch_count;
    float bias[2];

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
    uint32_t xform_offset, delta_offset;
    uint32_t shape_key_offset_curr, shape_key_count_curr;
    uint32_t shape_key_offset_prev, shape_key_count_prev;
    uint32_t vertex_count, shape_keyed_vertex_count;
};

struct ShapeKeyData {
    uint16_t shape_index;
    uint16_t shape_weight;
};

enum eRenderFlags : uint32_t {
    EnableZFill = (1u << 0u),
    EnableCulling = (1u << 1u),
    EnableSSR = (1u << 2u),
    EnableSSR_HQ = (1u << 3u),
    EnableSSAO = (1u << 4u),
    EnableLightmap = (1u << 5u),
    EnableLights = (1u << 6u),
    EnableDecals = (1u << 7u),
    EnableProbes = (1u << 8u),
    EnableDOF = (1u << 9u),
    EnableShadows = (1u << 10u),
    EnableOIT = (1u << 11u),
    EnableTonemap = (1u << 12u),
    EnableBloom = (1u << 13u),
    EnableMsaa = (1u << 14u),
    EnableTaa = (1u << 15u),
    EnableFxaa = (1u << 16u),
    EnableTimers = (1u << 17u),
    DebugWireframe = (1u << 18u),
    DebugCulling = (1u << 19u),
    DebugShadow = (1u << 20u),
    DebugLights = (1u << 21u),
    DebugDeferred = (1u << 22u),
    DebugBlur = (1u << 23u),
    DebugDecals = (1u << 24u),
    DebugSSAO = (1u << 25u),
    DebugTimings = (1u << 26u),
    DebugBVH = (1u << 27u),
    DebugProbes = (1u << 28u),
    DebugEllipsoids = (1u << 29u),
    DebugRT = (1u << 30u),
    DebugDenoise = (1u << 31u)
};

struct FrontendInfo {
    uint64_t start_timepoint_us = 0, end_timepoint_us = 0;
    uint32_t occluders_time_us = 0, main_gather_time_us = 0, shadow_gather_time_us = 0, drawables_sort_time_us = 0,
             items_assignment_time_us = 0;
};

struct PassTiming {
    std::string name;
    uint64_t duration;
};

struct BackendInfo {
    uint64_t cpu_start_timepoint_us = 0, cpu_end_timepoint_us = 0;
    uint64_t gpu_start_timepoint_us = 0, gpu_end_timepoint_us = 0;
    int64_t gpu_cpu_time_diff_us = 0;

    Ren::SmallVector<PassTiming, 256> pass_timings;

    uint32_t shadow_draw_calls_count = 0, depth_fill_draw_calls_count = 0, opaque_draw_calls_count = 0;

    uint32_t tris_rendered = 0;
};

struct ItemsInfo {
    uint32_t light_sources_count = 0, decals_count = 0, probes_count = 0;
    uint32_t items_total = 0;
};

struct ViewState {
    Ren::Vec2i act_res, scr_res;
    float vertical_fov;
    int frame_index;
    Ren::Vec3f prev_cam_pos;
    Ren::Mat4f prev_clip_from_world, down_buf_view_from_world, prev_clip_from_view;
    mutable Ren::Vec4f clip_info;
    bool is_multisampled = false;
};

struct SharedDataBlock {
    Ren::Mat4f view_matrix, proj_matrix, view_proj_matrix, view_proj_prev_matrix;
    Ren::Mat4f inv_view_matrix, inv_proj_matrix, inv_view_proj_matrix, delta_matrix;
    ShadowMapRegion shadowmap_regions[REN_MAX_SHADOWMAPS_TOTAL];
    Ren::Vec4f sun_dir, sun_col, uTaaInfo, uFrustumInfo;
    Ren::Vec4f clip_info, cam_pos_and_gamma, cam_delta;
    Ren::Vec4f res_and_fres, transp_params_and_time;
    Ren::Vec4f wind_scroll, wind_scroll_prev;
    ProbeItem probes[REN_MAX_PROBES_TOTAL] = {};
    EllipsItem ellipsoids[REN_MAX_ELLIPSES_TOTAL] = {};
};
static_assert(sizeof(SharedDataBlock) == 7856, "!");

struct MaterialData {
    uint32_t texture_indices[5];
    uint32_t _pad[3];
    Ren::Vec4f params;
};
static_assert(sizeof(MaterialData) == 48, "!");

const uint32_t RTGeoProbeBits = 0xff;
const uint32_t RTGeoLightmappedBit = (1u << 8u);

struct RTGeoInstance {
    uint32_t indices_start;
    uint32_t vertices_start;
    uint32_t material_index;
    uint32_t flags;
    float lmap_transform[4];
};
static_assert(sizeof(RTGeoInstance) == 32, "!");

struct RTObjInstance {
    float xform[3][4];
    uint32_t custom_index : 24;
    uint32_t mask : 8;
    uint32_t _pad;
    uint64_t blas_ref;
};
static_assert(sizeof(RTObjInstance) == 64, "!");

#if defined(USE_VK_RENDER)
#include <Ren/VK.h>
#include <Ren/Buffer.h>
#include <Ren/Fwd.h>
#elif defined(USE_GL_RENDER)
#include <Ren/Buffer.h>
#endif

struct BindlessTextureData {
#if defined(USE_VK_RENDER)
    const Ren::SmallVectorImpl<VkDescriptorSet> *textures_descr_sets;
    VkDescriptorSet rt_textures_descr_set, rt_inline_textures_descr_set;
#elif defined(USE_GL_RENDER)
    Ren::WeakBufferRef textures_buf;
#endif
};

struct AccelerationStructureData {
    Ren::WeakBufferRef rt_instance_buf, rt_geo_data_buf, rt_tlas_buf;
    Ren::IAccStructure *rt_tlas = nullptr;
};

// Constant that controls buffers orphaning
const size_t SkinTransformsBufChunkSize = sizeof(SkinTransform) * REN_MAX_SKIN_XFORMS_TOTAL;
const size_t ShapeKeysBufChunkSize = sizeof(ShapeKeyData) * REN_MAX_SHAPE_KEYS_TOTAL;
const size_t SkinRegionsBufChunkSize = sizeof(SkinRegion) * REN_MAX_SKIN_REGIONS_TOTAL;
const size_t InstanceDataBufChunkSize = sizeof(InstanceData) * REN_MAX_INSTANCES_TOTAL;
const size_t LightsBufChunkSize = sizeof(LightSourceItem) * REN_MAX_LIGHTS_TOTAL;
const size_t DecalsBufChunkSize = sizeof(DecalItem) * REN_MAX_DECALS_TOTAL;
const size_t CellsBufChunkSize = sizeof(CellData) * REN_CELLS_COUNT;
const size_t ItemsBufChunkSize = sizeof(ItemData) * REN_MAX_ITEMS_TOTAL;
const size_t SharedDataBlockSize = 8 * 1024;

static_assert(sizeof(SharedDataBlock) <= SharedDataBlockSize, "!");
