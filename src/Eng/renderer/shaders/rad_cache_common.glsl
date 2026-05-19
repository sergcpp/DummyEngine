#ifndef RAD_CACHE_COMMON_GLSL
#define RAD_CACHE_COMMON_GLSL
#if defined(VULKAN)
#extension GL_EXT_shader_explicit_arithmetic_types : require
#endif

// TODO: Move these constants to a common header

//
// Spatial hashing
//
const uint HASH_GRID_CACHE_ENTRIES_COUNT = (1u << 22);
const uint HASH_GRID_POSITION_BIT_NUM = 17u;
const uint HASH_GRID_POSITION_BIT_MASK = (1u << HASH_GRID_POSITION_BIT_NUM) - 1;
const uint HASH_GRID_LEVEL_BIT_NUM = 10u;
const uint HASH_GRID_LEVEL_BIT_MASK = (1u << HASH_GRID_LEVEL_BIT_NUM) - 1;
const uint HASH_GRID_NORMAL_BIT_NUM = 3u;
const uint HASH_GRID_NORMAL_BIT_MASK = (1u << HASH_GRID_NORMAL_BIT_NUM) - 1;
const uint HASH_GRID_HASH_MAP_BUCKET_SIZE = 32u;
const uint HASH_GRID_INVALID_CACHE_ENTRY = 0xFFFFFFFFu;
const uint HASH_GRID_LEVEL_BIAS = 2u; // positive bias adds extra levels with content magnification
const uint HASH_GRID_INVALID_HASH_KEY = 0u;
const bool HASH_GRID_USE_NORMALS = true;
const bool HASH_GRID_ALLOW_COMPACTION = (HASH_GRID_HASH_MAP_BUCKET_SIZE == 32u);

//
// Radiance caching
//
const int RAD_CACHE_SAMPLE_COUNT_MAX = 128;
const int RAD_CACHE_SAMPLE_COUNT_MIN = 8;
const float RAD_CACHE_RADIANCE_SCALE = 1e4f;
const int RAD_CACHE_SAMPLE_COUNTER_BIT_NUM = 20;
const uint RAD_CACHE_SAMPLE_COUNTER_BIT_MASK = ((1u << RAD_CACHE_SAMPLE_COUNTER_BIT_NUM) - 1);
const uint RAD_CACHE_FRAME_COUNTER_BIT_NUM = (32 - RAD_CACHE_SAMPLE_COUNTER_BIT_NUM);
const uint RAD_CACHE_FRAME_COUNTER_BIT_MASK = ((1u << RAD_CACHE_FRAME_COUNTER_BIT_NUM) - 1);
const float RAD_CACHE_GRID_LOGARITHM_BASE = 2.0f;
const int RAD_CACHE_STALE_FRAME_NUM_MAX = 128;
const int RAD_CACHE_DOWNSAMPLING_FACTOR = 4;
const bool RAD_CACHE_ENABLE_COMPACTION = true;
const bool RAD_CACHE_FILTER_ADJACENT_LEVELS = true;
const int RAD_CACHE_PROPAGATION_DEPTH = 4;
const float RAD_CACHE_GRID_SCALE = 50.0f;
const float RAD_CACHE_MIN_ROUGHNESS = 0.4f;

struct cache_voxel_t {
    vec3 radiance;
    uint sample_count;
    uint frame_count;
};

struct cache_grid_params_t {
    vec3 cam_pos_curr, cam_pos_prev;
    float log_base;
    float scale;
    float exposure;
};

uint hash_jenkins32(uint a) {
    a = (a + 0x7ed55d16) + (a << 12);
    a = (a ^ 0xc761c23c) ^ (a >> 19);
    a = (a + 0x165667b1) + (a << 5);
    a = (a + 0xd3a2646c) ^ (a << 9);
    a = (a + 0xfd7046c5) + (a << 3);
    a = (a ^ 0xb55a4f09) ^ (a >> 16);
    return a;
}

uint hash64(const uint64_t hash_key) {
    return hash_jenkins32(uint((hash_key >> 0) & 0xffffffff)) ^
           hash_jenkins32(uint((hash_key >> 32) & 0xffffffff));
}

float log_base(const float x, const float base) { return log2(x) / log2(base); }

uint calc_grid_level(const vec3 p, const cache_grid_params_t params) {
    const float distance = length(params.cam_pos_curr - p);
    const float ret =
        clamp(floor(log_base(distance, params.log_base) + HASH_GRID_LEVEL_BIAS), 1.0, float(HASH_GRID_LEVEL_BIT_MASK));
    return uint(ret);
}

float calc_voxel_size(uint grid_level, const cache_grid_params_t params) {
    return pow(params.log_base, grid_level) / (params.scale * pow(params.log_base, HASH_GRID_LEVEL_BIAS));
}

ivec4 calc_grid_position_log(const vec3 p, const cache_grid_params_t params) {
    const uint grid_level = calc_grid_level(p, params);
    const float voxel_size = calc_voxel_size(grid_level, params);
    ivec4 grid_position;
    grid_position.xyz = ivec3(floor(p / voxel_size));
    grid_position.w = int(grid_level);
    return grid_position;
}

uint64_t compute_hash(const vec3 p, const vec3 n, const cache_grid_params_t params) {
    const uvec4 grid_pos = uvec4(calc_grid_position_log(p, params));

    uint64_t hash_key =
        ((uint64_t(grid_pos.x) & HASH_GRID_POSITION_BIT_MASK) << (HASH_GRID_POSITION_BIT_NUM * 0)) |
        ((uint64_t(grid_pos.y) & HASH_GRID_POSITION_BIT_MASK) << (HASH_GRID_POSITION_BIT_NUM * 1)) |
        ((uint64_t(grid_pos.z) & HASH_GRID_POSITION_BIT_MASK) << (HASH_GRID_POSITION_BIT_NUM * 2)) |
        ((uint64_t(grid_pos.w) & HASH_GRID_LEVEL_BIT_MASK) << (HASH_GRID_POSITION_BIT_NUM * 3));

    if (HASH_GRID_USE_NORMALS) {
        const uint normal_bits = (n.x >= 0 ? 1 : 0) + (n.y >= 0 ? 2 : 0) + (n.z >= 0 ? 4 : 0);
        hash_key |= (uint64_t(normal_bits) << (HASH_GRID_POSITION_BIT_NUM * 3 + HASH_GRID_LEVEL_BIT_NUM));
    }

    return hash_key;
}

vec3 GetColorFromHash32(const uint hash) {
    vec3 color;
    color.x = float((hash >> 0) & 0x3ff) / 1023.0;
    color.y = float((hash >> 11) & 0x7ff) / 2047.0;
    color.z = float((hash >> 22) & 0x7ff) / 2047.0;
    return color;
}

vec3 hash_grid_debug(const vec3 p, const vec3 n, const cache_grid_params_t params) {
    const uint64_t hash_key = compute_hash(p, n, params);
    return GetColorFromHash32(hash64(hash_key));
}

#endif // RAD_CACHE_COMMON_GLSL