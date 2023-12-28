#include "SWcompress.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

typedef struct SWtex_block {
    SWubyte col[4][4];
    SWint counter;
} SWtex_block;

SWint swTexBlockCompare(const SWtex_block *blck1, const SWtex_block *blck2) {
    SWint i, j, res = 0;
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            res += sw_abs((SWint)blck1->col[i][j] - blck2->col[i][j]);
        }
    }
    return res;
}

static int tex_block_cmp(const void *blck1, const void *blck2) {
    // return ((const SWtex_block *)blck1)->counter < ((const SWtex_block
    // *)blck2)->counter;

    const SWtex_block *b1 = (const SWtex_block *)blck1;
    const SWtex_block *b2 = (const SWtex_block *)blck2;

    SWfloat brightness1 = 1, brightness2 = 1;

    SWint i, j;

    extern SWfloat _sw_ubyte_to_float_table[256];
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            brightness1 += _sw_ubyte_to_float_table[b1->col[i][j]];
            brightness2 += _sw_ubyte_to_float_table[b2->col[i][j]];
        }
    }

    return b1->counter * brightness1 < b2->counter * brightness2;
}

SWint swTexBlockFind(SWtex_block *blocks, const SWint num_blocks, const SWtex_block *blck,
                     const SWint tolerance) {
    SWint i;
    for (i = 0; i < num_blocks; i++) {
        SWint res = swTexBlockCompare(&blocks[i], blck);
        if (res < tolerance)
            return i;
    }
    return -1;
}

SWint swTexBlockClosest(SWtex_block *blocks, const SWint num_blocks,
                        const SWtex_block *blck) {
    SWint i, best = 0, best_res = 255 * 4;
    for (i = 0; i < num_blocks; i++) {
        SWint res = swTexBlockCompare(&blocks[i], blck);
        if (res < best_res) {
            if (!res)
                return i;
            best = i;
            best_res = res;
        }
    }
    return best;
}

void swTexCompress(const void *data, const SWenum mode, const SWint w, const SWint h,
                   void **out_data, SWint *out_size) {
    assert(w > 1 && h > 1);
    assert(w % 2 == 0 && h % 2 == 0);

    const SWubyte *pixels = (const SWubyte *)data;
    SWint max_num_blocks = sw_max(w / 2, 1) * sw_max(h / 2, 1);
    SWtex_block *blocks = (SWtex_block *)malloc(max_num_blocks * sizeof(SWtex_block));
    SWint num_blocks = 0;
    SWint step = (mode == SW_RGB) ? 3 : 4;
    SWint i, j, k;

#define InitBlock(block)                                                                 \
    block.counter = 1;                                                                   \
    memcpy(&block.col[0][0], &pixels[step * ((j + 0) * w + (i + 0))], step);             \
    memcpy(&block.col[1][0], &pixels[step * ((j + 0) * w + (i + 1))], step);             \
    memcpy(&block.col[2][0], &pixels[step * ((j + 1) * w + (i + 0))], step);             \
    memcpy(&block.col[3][0], &pixels[step * ((j + 1) * w + (i + 1))], step);             \
    if (mode == SW_RGB) {                                                                \
        for (k = 0; k < 4; k++) {                                                        \
            block.col[k][3] = 255;                                                       \
        }                                                                                \
    }

    for (j = 0; j < h; j += 2) {
        for (i = 0; i < w; i += 2) {
            SWtex_block cur_block;
            InitBlock(cur_block);

            const SWint index = swTexBlockFind(blocks, num_blocks, &cur_block, 72);
            if (index == -1) {
                blocks[num_blocks++] = cur_block;
            } else {
                blocks[index].counter++;
            }
        }
    }

    qsort(blocks, num_blocks, sizeof(SWtex_block), tex_block_cmp);

    (*out_size) = 4 * 4 * 256 + (w / 2) * (h / 2);
    (*out_data) = malloc((size_t)(*out_size));
    SWubyte *p = (SWubyte *)(*out_data);

    for (i = 0; i < sw_min(256, num_blocks); i++) {
        memcpy(p, &blocks[i], 4 * 4);
        p += 4 * 4;
    }

    for (i = num_blocks; i < 256; i++) {
        SWubyte col[4] = {0, 0, 0, 255};
        for (j = 0; j < 4; j++) {
            memcpy(&p[j * 4], col, 4);
        }
        p += 4 * 4;
    }

    for (j = 0; j < h; j += 2) {
        for (i = 0; i < w; i += 2) {
            SWtex_block cur_block;
            InitBlock(cur_block);

            SWubyte index = (SWubyte)swTexBlockClosest(blocks, 256, &cur_block);
            (*p++) = index;
        }
    }

    free(blocks);

#undef InitBlock
}

SWenum swTexDecompress(const void *data, const SWint w, const SWint h, void **out_data,
                       SWint *out_size) {
    assert(w > 1 && h > 1);
    assert(w % 2 == 0 && h % 2 == 0);

    const SWubyte *blocks = (const SWubyte *)data;
    const SWubyte *pixels = (const SWubyte *)data + 4 * 4 * 256;

    SWint has_alpha = 0;
    SWint i, j;
    for (i = 0; i < 256; i++) {
        const SWubyte *block = &blocks[i * 16];
        if (block[4 * 0 + 3] != 255 || block[4 * 1 + 3] != 255 ||
            block[4 * 2 + 3] != 255 || block[4 * 3 + 3] != 255) {
            has_alpha = 1;
            break;
        }
    }

    (*out_size) = w * h * (3 + has_alpha);
    (*out_data) = malloc((size_t)(*out_size));

    SWubyte *p = (SWubyte *)(*out_data);
    const SWuint step = has_alpha ? 4 : 3;
    for (j = 0; j < h / 2; j++) {
        for (i = 0; i < w / 2; i++) {
            SWuint index = pixels[j * (w / 2) + i];
            const SWubyte *block = &blocks[index * 16];

            const SWint x = i * 2, y = j * 2;
            memcpy(&p[step * (y * w + x)], &block[0], step);
            memcpy(&p[step * (y * w + x + 1)], &block[4], step);
            memcpy(&p[step * ((y + 1) * w + x)], &block[8], step);
            memcpy(&p[step * ((y + 1) * w + x + 1)], &block[12], step);
        }
    }

    return has_alpha ? SW_RGBA : SW_RGB;
}
