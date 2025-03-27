#ifndef PMJ_COMMON_GLSL
#define PMJ_COMMON_GLSL

const int RAND_SAMPLES_COUNT = 256;
const int RAND_DIMS_COUNT = 8;

const int RAND_DIM_LIGHT_PICK = 0;
const int RAND_DIM_LIGHT_UV = 1;
const int RAND_DIM_FOG_OFFSET = 2;

uint hash_combine(uint seed, uint v) { return seed ^ (v + (seed << 6) + (seed >> 2)); }

uint laine_karras_permutation(uint x, uint seed) {
    x += seed;
    x ^= x * 0x6c50b47cu;
    x ^= x * 0xb82f1e52u;
    x ^= x * 0xc7afe638u;
    x ^= x * 0x8d22f6e6u;
    return x;
}

uint nested_uniform_scramble_base2(uint x, uint seed) {
    x = bitfieldReverse(x);
    x = laine_karras_permutation(x, seed);
    x = bitfieldReverse(x);
    return x;
}

float scramble_unorm(const uint seed, uint val) {
    val = nested_uniform_scramble_base2(val, seed);
    return float(val >> 8) / 16777216.0;
}

vec2 get_scrambled_2d_rand(usamplerBuffer random_seq, const uint dim, const uint seed, const int _sample) {
    const uint i_seed = hash_combine(seed, dim),
               x_seed = hash_combine(seed, 2 * dim + 0),
               y_seed = hash_combine(seed, 2 * dim + 1);

    const uint shuffled_dim = dim;//uint(nested_uniform_scramble_base2(dim, seed) & (RAND_DIMS_COUNT - 1));
    const uint shuffled_i = uint(nested_uniform_scramble_base2(_sample, i_seed) & (RAND_SAMPLES_COUNT - 1));
    return vec2(scramble_unorm(x_seed, texelFetch(random_seq, int(shuffled_dim * 2 * RAND_SAMPLES_COUNT + 2 * shuffled_i + 0)).x),
                scramble_unorm(y_seed, texelFetch(random_seq, int(shuffled_dim * 2 * RAND_SAMPLES_COUNT + 2 * shuffled_i + 1)).x));
}

#endif // PMJ_COMMON_GLSL