//
// 1D sequence optimized to distribute error as blue noise in a 128x128 tile for step function integrals
//

float Sample1D_BN_PMJ_16SPP(usamplerBuffer bn_pmj_data, const uvec2 pixel, uint sample_index) {
    const int SampleCount = 16;
    const int TileRes = 128;
    const int ScramblingDataOffset = SampleCount;
    const int SortingDataOffset = SampleCount + TileRes * TileRes;

    // wrap arguments
    const uint pixel_i = pixel.x & (TileRes - 1);
    const uint pixel_j = pixel.y & (TileRes - 1);
    sample_index = sample_index & (SampleCount - 1);

    // xor index based on optimized sorting
    const uint ranked_sample_index = sample_index ^ texelFetch(bn_pmj_data, SortingDataOffset + int(pixel_i + pixel_j * TileRes)).x;

    // fetch value in sequence
    uint value = texelFetch(bn_pmj_data, int(ranked_sample_index)).x;

    // xor sequence value based on optimized scrambling
    value = value ^ texelFetch(bn_pmj_data, ScramblingDataOffset + int(pixel_i + pixel_j * TileRes)).x;

    // convert to float and return
    return float(value >> 8) / 16777216.0;
}
