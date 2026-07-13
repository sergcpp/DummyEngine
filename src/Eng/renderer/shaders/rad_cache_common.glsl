#ifndef RAD_CACHE_COMMON_GLSL
#define RAD_CACHE_COMMON_GLSL

uint hash_jenkins32(uint a) {
    a = (a + 0x7ed55d16) + (a << 12);
    a = (a ^ 0xc761c23c) ^ (a >> 19);
    a = (a + 0x165667b1) + (a << 5);
    a = (a + 0xd3a2646c) ^ (a << 9);
    a = (a + 0xfd7046c5) + (a << 3);
    a = (a ^ 0xb55a4f09) ^ (a >> 16);
    return a;
}

float log_base(const float x, const float base) { return log2(x) / log2(base); }

uint calc_grid_level(const vec3 p, const vec3 cam_pos) {
    const float distance = length(cam_pos - p);
    const float ret =
        clamp(floor(log_base(distance, RAD_CACHE_GRID_LOG_BASE) + HASH_GRID_LEVEL_BIAS), 1.0, float(HASH_GRID_LEVEL_BIT_MASK));
    return uint(ret);
}

float calc_voxel_size(const uint grid_level) {
    return pow(RAD_CACHE_GRID_LOG_BASE, grid_level) / (RAD_CACHE_GRID_SCALE * pow(RAD_CACHE_GRID_LOG_BASE, HASH_GRID_LEVEL_BIAS));
}

ivec4 calc_grid_position_log(vec3 p, const vec3 cam_pos) {
    // NOTE: Bias down on Y axis because of sun leaking
    p += vec3(HASH_GRID_POSITION_BIAS, -HASH_GRID_POSITION_BIAS, HASH_GRID_POSITION_BIAS);

    const uint grid_level = calc_grid_level(p, cam_pos);
    const float voxel_size = calc_voxel_size(grid_level);
    ivec4 grid_position;
    grid_position.xyz = ivec3(floor(p / voxel_size));
    grid_position.w = int(grid_level);
    return grid_position;
}

uint hash_map_base_slot(const uint hash_key) {
    const uint hash = hash_jenkins32(hash_key);
    const uint slot = hash % HASH_GRID_CACHE_ENTRIES_COUNT;

    return min(slot, HASH_GRID_CACHE_ENTRIES_COUNT - HASH_GRID_HASH_MAP_BUCKET_SIZE);
}

uint compute_hash(const vec3 p, const bool backfacing, const vec3 cam_pos) {
    const uvec4 grid_pos = uvec4(calc_grid_position_log(p, cam_pos));

    uint hash_key =
        ((grid_pos.x & HASH_GRID_POSITION_BIT_MASK) << (HASH_GRID_POSITION_BIT_NUM * 0)) |
        ((grid_pos.y & HASH_GRID_POSITION_BIT_MASK) << (HASH_GRID_POSITION_BIT_NUM * 1)) |
        ((grid_pos.z & HASH_GRID_POSITION_BIT_MASK) << (HASH_GRID_POSITION_BIT_NUM * 2)) |
        ((grid_pos.w & HASH_GRID_LEVEL_BIT_MASK) << (HASH_GRID_POSITION_BIT_NUM * 3));

    hash_key |= ((backfacing ? 1u : 0u) << (HASH_GRID_POSITION_BIT_NUM * 3 + HASH_GRID_LEVEL_BIT_NUM));

    return hash_key;
}

vec3 GetColorFromHash32(const uint hash) {
    vec3 color;
    color.x = float((hash >> 0) & 0x3ff) / 1023.0;
    color.y = float((hash >> 11) & 0x7ff) / 2047.0;
    color.z = float((hash >> 22) & 0x7ff) / 2047.0;
    return color;
}

vec3 hash_grid_debug(const vec3 p, const bool backfacing, const vec3 cam_pos) {
    const uint hash_key = compute_hash(p, backfacing, cam_pos);
    return GetColorFromHash32(hash_jenkins32(hash_key));
}

#endif // RAD_CACHE_COMMON_GLSL