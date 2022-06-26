
#define TILE_SIZE_X 8
#define TILE_SIZE_Y 4

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
