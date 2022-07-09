
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