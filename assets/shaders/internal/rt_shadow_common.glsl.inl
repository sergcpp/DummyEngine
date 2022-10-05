
#define TILE_SIZE_X 8
#define TILE_SIZE_Y 4

#define TILE_META_DATA_CLEAR_MASK 1u // 0b01u
#define TILE_META_DATA_LIGHT_MASK 2u // 0b10u

uvec4 PackTile(uvec2 tile_coord, uint mask, float min_t, float max_t) {
    return uvec4(
        (tile_coord.y << 16u) | tile_coord.x,
        mask,
        floatBitsToUint(min_t),
        floatBitsToUint(max_t)
    );
}

void UnpackTile(uvec4 tile, out uvec2 tile_coord, out uint mask, out float min_t, out float max_t) {
    tile_coord = uvec2(tile.x & 0xffff, (tile.x >> 16) & 0xffff);
    mask = tile.y;
    min_t = uintBitsToFloat(tile.z);
    max_t = uintBitsToFloat(tile.w);
}

uint LaneIdToBitShift(uvec2 id) {
    return id.y * TILE_SIZE_X + id.x;
}

uvec2 GetTileIndexFromPixelPosition(uvec2 pixel_pos) {
    return uvec2(pixel_pos.x / TILE_SIZE_X, pixel_pos.y / TILE_SIZE_Y);
}

uint LinearTileIndex(uvec2 tile_index, uint screen_width) {
    return tile_index.y * ((screen_width + TILE_SIZE_X - 1) / TILE_SIZE_X) + tile_index.x;
}

uint GetBitMaskFromPixelPosition(uvec2 pixel_pos) {
    uint lane_index = (pixel_pos.y % 4u) * 8u + (pixel_pos.x % 8u);
    return (1u << lane_index);
}

mat2x3 CreateTangentVectors(vec3 normal) {
	vec3 up = abs(normal.z) < 0.99999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);

	mat2x3 tangents;

	tangents[0] = normalize(cross(up, normal));
	tangents[1] = cross(normal, tangents[0]);

	return tangents;
}

vec3 MapToCone(vec2 u, vec3 n, float radius) {
	vec2 offset = 2.0 * u - vec2(1.0);

	if (offset.x == 0.0 && offset.y == 0.0) {
		return n;
	}

	float theta, r;

	if (abs(offset.x) > abs(offset.y)) {
		r = offset.x;
		theta = 0.25 * M_PI * (offset.y / offset.x);
	} else {
		r = offset.y;
		theta = 0.5 * M_PI * (1.0 - 0.5 * (offset.x / offset.y));
	}

	vec2 uv = vec2(radius * r * cos(theta), radius * r * sin(theta));

	mat2x3 tangents = CreateTangentVectors(n);

	return n + uv.x * tangents[0] + uv.y * tangents[1];
}