//
// 2D sequence optimized to distribute error as blue noise in a 128x128 tile for heavyside function integrals
//

vec2 Sample2D_BN_PMJ_64SPP(usamplerBuffer bn_pmj_data, const uvec2 pixel, const uint dim_pair_index, uint sample_index) {
    const int SampleCount = 64;
    const int DimPairsCount = 8;
    const int TileRes = 128;
    const int SampleSizePerDimPair = 2 * SampleCount;
    const int ScramblingSizePerDimPair = 2 * TileRes * TileRes;
    const int SortingSizePerDimPair = TileRes * TileRes;

    const int ScramblingDataOffset = DimPairsCount * SampleSizePerDimPair;
    const int SortingDataOffset = ScramblingDataOffset + DimPairsCount * ScramblingSizePerDimPair;

    // wrap arguments
    const uint pixel_i = pixel.x & (TileRes - 1);
    const uint pixel_j = pixel.y & (TileRes - 1);
    sample_index = sample_index & (SampleCount - 1);

    // xor index based on optimized sorting
    const uint ranked_sample_index = sample_index ^ texelFetch(bn_pmj_data, int(SortingDataOffset + dim_pair_index * SortingSizePerDimPair + pixel_i + pixel_j * TileRes)).x;

    // fetch value in sequence
    uvec2 value = uvec2(texelFetch(bn_pmj_data, int(dim_pair_index * SampleSizePerDimPair + 2 * ranked_sample_index + 0)).x,
                        texelFetch(bn_pmj_data, int(dim_pair_index * SampleSizePerDimPair + 2 * ranked_sample_index + 1)).x);

    // xor sequence value based on optimized scrambling
    value = value ^ uvec2(texelFetch(bn_pmj_data, int(ScramblingDataOffset + dim_pair_index * ScramblingSizePerDimPair + 2 * (pixel_i + pixel_j * TileRes) + 0)).x,
                          texelFetch(bn_pmj_data, int(ScramblingDataOffset + dim_pair_index * ScramblingSizePerDimPair + 2 * (pixel_i + pixel_j * TileRes) + 1)).x);

    // convert to float and return
    return vec2(value >> 8) / 16777216.0;
}
