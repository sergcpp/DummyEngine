#include "test_common.h"

#include <math.h>
#include <string.h>

#include "../SWtypes.h"
#include "../SWpixels.h"

static const SWubyte px_RGB888[] = { 0, 0, 0,        1, 0, 0,        0, 1, 0,        0, 0, 255,
                                     1, 0, 0,        0, 1, 0,        0, 0, 1,        0, 0, 0,
                                     1, 1, 0,        11, 13, 14,     190, 111, 20,   20, 20, 20,
                                     10, 111, 12,    190, 111, 20,   0, 1, 0,        0, 0, 1
                                   };

static const SWubyte px_RGBA8888[] = { 0, 0, 0, 1,        1, 0, 0, 2,         0, 1, 0, 3,        0, 0, 255, 4,
                                       1, 0, 0, 13,       0, 1, 0, 14,        0, 0, 1, 15,         0, 0, 0, 16,
                                       1, 1, 0, 5,        11, 13, 14, 6,      190, 111, 20, 7,    20, 20, 20, 8,
                                       10, 111, 12, 9,    190, 111, 20, 10,   0, 1, 0, 11,        0, 0, 1, 12
                                     };

static const SWfloat px_FRGBA[] = { 0, 0, 0, 1,         1, 0, 0, 0,             1, 0, 0, 1,         1, 0, 0, 1,
                                    0.5f, 0, 0, 0,      0, 0.5f, 0, 1,          0, 0, 0, 0,         1, 1, 1, 0,
                                    0, 0.5f, 0, 0,      0, 0.5f, 0.5f, 0,       0, 0, 0, 0,         1, 1, 1, 0,
                                    1, 1, 1, 0,         0, 0, 0, 0,             0, 0, 0, 0,         1, 0, 1, 0
                                  };

#define REQUIRE_VEC4_EQ(vec, _0, _1, _2, _3)    \
    require((vec)[0] == (_0));                  \
    require((vec)[1] == (_1));                  \
    require((vec)[2] == (_2));                  \
    require((vec)[3] == (_3));

#define REQUIRE_FVEC4_EQ(vec, _0, _1, _2, _3)   \
    require(fabs((vec)[0] - (_0)) < 0.1f);      \
    require(fabs((vec)[1] - (_1)) < 0.1f);      \
    require(fabs((vec)[2] - (_2)) < 0.1f);      \
    require(fabs((vec)[3] - (_3)) < 0.1f);

void test_pixels() {

    {
        // Get pixels RGB888
        SWubyte rgba[4], bgra[4];
        SWfloat frgba[4], fbgra[4];

        // First row
        swPx_RGB888_GetColor_RGBA8888(4, 4, px_RGB888, 0, 0, rgba);
        swPx_RGB888_GetColor_BGRA8888(4, 4, px_RGB888, 0, 0, bgra);
        swPx_RGB888_GetColor_FRGBA(4, 4, px_RGB888, 0, 0, frgba);
        swPx_RGB888_GetColor_FBGRA(4, 4, px_RGB888, 0, 0, fbgra);
        REQUIRE_VEC4_EQ(rgba, 0, 0, 0, 255);
        REQUIRE_VEC4_EQ(bgra, 0, 0, 0, 255);
        REQUIRE_FVEC4_EQ(frgba, 0, 0, 0, 1.0);
        REQUIRE_FVEC4_EQ(fbgra, 0, 0, 0, 1.0);

        swPx_RGB888_GetColor_RGBA8888(4, 4, px_RGB888, 3, 0, rgba);
        swPx_RGB888_GetColor_BGRA8888(4, 4, px_RGB888, 3, 0, bgra);
        swPx_RGB888_GetColor_FRGBA(4, 4, px_RGB888, 3, 0, frgba);
        swPx_RGB888_GetColor_FBGRA(4, 4, px_RGB888, 3, 0, fbgra);
        REQUIRE_VEC4_EQ(rgba, 0, 0, 255, 255);
        REQUIRE_VEC4_EQ(bgra, 255, 0, 0, 255);
        REQUIRE_FVEC4_EQ(frgba, 0, 0, 1.0, 1.0);
        REQUIRE_FVEC4_EQ(fbgra, 1.0, 0, 0, 1.0);

        // Last row
        swPx_RGB888_GetColor_RGBA8888(4, 4, px_RGB888, 0, 3, rgba);
        swPx_RGB888_GetColor_BGRA8888(4, 4, px_RGB888, 0, 3, bgra);
        swPx_RGB888_GetColor_FRGBA(4, 4, px_RGB888, 0, 3, frgba);
        swPx_RGB888_GetColor_FBGRA(4, 4, px_RGB888, 0, 3, fbgra);
        REQUIRE_VEC4_EQ(rgba, 10, 111, 12, 255);
        REQUIRE_VEC4_EQ(bgra, 12, 111, 10, 255);
        REQUIRE_FVEC4_EQ(frgba, 10.0 / 255, 111.0 / 255, 12.0 / 255, 1.0);
        REQUIRE_FVEC4_EQ(fbgra, 12.0 / 255, 111.0 / 255, 10.0 / 255, 1.0);

        swPx_RGB888_GetColor_RGBA8888(4, 4, px_RGB888, 3, 3, rgba);
        swPx_RGB888_GetColor_BGRA8888(4, 4, px_RGB888, 3, 3, bgra);
        swPx_RGB888_GetColor_FRGBA(4, 4, px_RGB888, 3, 3, frgba);
        swPx_RGB888_GetColor_FBGRA(4, 4, px_RGB888, 3, 3, fbgra);
        REQUIRE_VEC4_EQ(rgba, 0, 0, 1, 255);
        REQUIRE_VEC4_EQ(bgra, 1, 0, 0, 255);
        REQUIRE_FVEC4_EQ(frgba, 0, 0, 1.0 / 255, 1.0);
        REQUIRE_FVEC4_EQ(fbgra, 1.0 / 255, 0, 0, 1.0);
    }

    {
        // Get pixels RGB888
        SWubyte rgba[4], bgra[4];
        SWfloat frgba[4], fbgra[4];

        // First row
        swPx_RGB888_GetColor_RGBA8888(4, 4, px_RGB888, 0, 0, rgba);
        swPx_RGB888_GetColor_BGRA8888(4, 4, px_RGB888, 0, 0, bgra);
        swPx_RGB888_GetColor_FRGBA(4, 4, px_RGB888, 0, 0, frgba);
        swPx_RGB888_GetColor_FBGRA(4, 4, px_RGB888, 0, 0, fbgra);
        REQUIRE_VEC4_EQ(rgba, 0, 0, 0, 255);
        REQUIRE_VEC4_EQ(bgra, 0, 0, 0, 255);
        REQUIRE_FVEC4_EQ(frgba, 0, 0, 0, 1.0);
        REQUIRE_FVEC4_EQ(fbgra, 0, 0, 0, 1.0);

        swPx_RGB888_GetColor_RGBA8888(4, 4, px_RGB888, 3, 0, rgba);
        swPx_RGB888_GetColor_BGRA8888(4, 4, px_RGB888, 3, 0, bgra);
        swPx_RGB888_GetColor_FRGBA(4, 4, px_RGB888, 3, 0, frgba);
        swPx_RGB888_GetColor_FBGRA(4, 4, px_RGB888, 3, 0, fbgra);
        REQUIRE_VEC4_EQ(rgba, 0, 0, 255, 255);
        REQUIRE_VEC4_EQ(bgra, 255, 0, 0, 255);
        REQUIRE_FVEC4_EQ(frgba, 0, 0, 1.0, 1.0);
        REQUIRE_FVEC4_EQ(fbgra, 1.0, 0, 0, 1.0);

        // Last row
        swPx_RGB888_GetColor_RGBA8888(4, 4, px_RGB888, 0, 3, rgba);
        swPx_RGB888_GetColor_BGRA8888(4, 4, px_RGB888, 0, 3, bgra);
        swPx_RGB888_GetColor_FRGBA(4, 4, px_RGB888, 0, 3, frgba);
        swPx_RGB888_GetColor_FBGRA(4, 4, px_RGB888, 0, 3, fbgra);
        REQUIRE_VEC4_EQ(rgba, 10, 111, 12, 255);
        REQUIRE_VEC4_EQ(bgra, 12, 111, 10, 255);
        REQUIRE_FVEC4_EQ(frgba, 10.0 / 255, 111.0 / 255, 12.0 / 255, 1.0);
        REQUIRE_FVEC4_EQ(fbgra, 12.0 / 255, 111.0 / 255, 10.0 / 255, 1.0);

        swPx_RGB888_GetColor_RGBA8888(4, 4, px_RGB888, 3, 3, rgba);
        swPx_RGB888_GetColor_BGRA8888(4, 4, px_RGB888, 3, 3, bgra);
        swPx_RGB888_GetColor_FRGBA(4, 4, px_RGB888, 3, 3, frgba);
        swPx_RGB888_GetColor_FBGRA(4, 4, px_RGB888, 3, 3, fbgra);
        REQUIRE_VEC4_EQ(rgba, 0, 0, 1, 255);
        REQUIRE_VEC4_EQ(bgra, 1, 0, 0, 255);
        REQUIRE_FVEC4_EQ(frgba, 0, 0, 1.0 / 255, 1.0);
        REQUIRE_FVEC4_EQ(fbgra, 1.0 / 255, 0, 0, 1.0);
    }

    {
        // Get pixels RGB888 by uv
        SWubyte rgba[4], bgra[4];
        SWfloat frgba[4], fbgra[4];

        // First row
        swPx_RGB888_GetColor_RGBA8888_UV(4, 4, px_RGB888, 0.1f, 0.1f, rgba);
        swPx_RGB888_GetColor_BGRA8888_UV(4, 4, px_RGB888, 0.1f, 0.1f, bgra);
        swPx_RGB888_GetColor_FRGBA_UV(4, 4, px_RGB888, 0.1f, 0.1f, frgba);
        swPx_RGB888_GetColor_FBGRA_UV(4, 4, px_RGB888, 0.1f, 0.1f, fbgra);
        REQUIRE_VEC4_EQ(rgba, 0, 0, 0, 255);
        REQUIRE_VEC4_EQ(bgra, 0, 0, 0, 255);
        REQUIRE_FVEC4_EQ(frgba, 0, 0, 0, 1.0);
        REQUIRE_FVEC4_EQ(fbgra, 0, 0, 0, 1.0);

        swPx_RGB888_GetColor_RGBA8888_UV(4, 4, px_RGB888, 0.7f, 0.1f, rgba);
        swPx_RGB888_GetColor_BGRA8888_UV(4, 4, px_RGB888, 0.7f, 0.1f, bgra);
        swPx_RGB888_GetColor_FRGBA_UV(4, 4, px_RGB888, 0.7f, 0.1f, frgba);
        swPx_RGB888_GetColor_FBGRA_UV(4, 4, px_RGB888, 0.7f, 0.1f, fbgra);
        REQUIRE_VEC4_EQ(rgba, 0, 1, 0, 255);
        REQUIRE_VEC4_EQ(bgra, 0, 1, 0, 255);
        REQUIRE_FVEC4_EQ(frgba, 0, 1.0 / 255, 0, 1.0);
        REQUIRE_FVEC4_EQ(fbgra, 0, 1.0 / 255, 0, 1.0);

        // Last row
        swPx_RGB888_GetColor_RGBA8888_UV(4, 4, px_RGB888, 1.1f, 0.9f, rgba);
        swPx_RGB888_GetColor_BGRA8888_UV(4, 4, px_RGB888, 1.1f, 0.9f, bgra);
        swPx_RGB888_GetColor_FRGBA_UV(4, 4, px_RGB888, 1.1f, 0.9f, frgba);
        swPx_RGB888_GetColor_FBGRA_UV(4, 4, px_RGB888, 1.1f, 0.9f, fbgra);
        REQUIRE_VEC4_EQ(rgba, 10, 111, 12, 255);
        REQUIRE_VEC4_EQ(bgra, 12, 111, 10, 255);
        REQUIRE_FVEC4_EQ(frgba, 10.0 / 255, 111.0 / 255, 12.0 / 255, 1.0);
        REQUIRE_FVEC4_EQ(fbgra, 12.0 / 255, 111.0 / 255, 10.0 / 255, 1.0);

        swPx_RGB888_GetColor_RGBA8888_UV(4, 4, px_RGB888, 1.9f, 1.9f, rgba);
        swPx_RGB888_GetColor_BGRA8888_UV(4, 4, px_RGB888, 1.9f, 1.9f, bgra);
        swPx_RGB888_GetColor_FRGBA_UV(4, 4, px_RGB888, 1.9f, 1.9f, frgba);
        swPx_RGB888_GetColor_FBGRA_UV(4, 4, px_RGB888, 1.9f, 1.9f, fbgra);
        REQUIRE_VEC4_EQ(rgba, 0, 0, 1, 255);
        REQUIRE_VEC4_EQ(bgra, 1, 0, 0, 255);
        REQUIRE_FVEC4_EQ(frgba, 0, 0, 1.0 / 255, 1.0);
        REQUIRE_FVEC4_EQ(fbgra, 1.0 / 255, 0, 0, 1.0);
    }

    {
        // Get pixels RGBA8888
        SWubyte rgba[4], bgra[4];
        SWfloat frgba[4], fbgra[4];

        // First row
        swPx_RGBA8888_GetColor_RGBA8888(4, 4, px_RGBA8888, 0, 0, rgba);
        swPx_RGBA8888_GetColor_BGRA8888(4, 4, px_RGBA8888, 0, 0, bgra);
        swPx_RGBA8888_GetColor_FRGBA(4, 4, px_RGBA8888, 0, 0, frgba);
        swPx_RGBA8888_GetColor_FBGRA(4, 4, px_RGBA8888, 0, 0, fbgra);
        REQUIRE_VEC4_EQ(rgba, 0, 0, 0, 1);
        REQUIRE_VEC4_EQ(bgra, 0, 0, 0, 1);
        REQUIRE_FVEC4_EQ(frgba, 0, 0, 0, 1.0 / 255);
        REQUIRE_FVEC4_EQ(fbgra, 0, 0, 0, 1.0 / 255);

        swPx_RGBA8888_GetColor_RGBA8888(4, 4, px_RGBA8888, 3, 0, rgba);
        swPx_RGBA8888_GetColor_BGRA8888(4, 4, px_RGBA8888, 3, 0, bgra);
        swPx_RGBA8888_GetColor_FRGBA(4, 4, px_RGBA8888, 3, 0, frgba);
        swPx_RGBA8888_GetColor_FBGRA(4, 4, px_RGBA8888, 3, 0, fbgra);
        REQUIRE_VEC4_EQ(rgba, 0, 0, 255, 4);
        REQUIRE_VEC4_EQ(bgra, 255, 0, 0, 4);
        REQUIRE_FVEC4_EQ(frgba, 0, 0, 1.0, 4.0 / 255);
        REQUIRE_FVEC4_EQ(fbgra, 1.0, 0, 0, 4.0 / 255);

        // Last row
        swPx_RGBA8888_GetColor_RGBA8888(4, 4, px_RGBA8888, 0, 3, rgba);
        swPx_RGBA8888_GetColor_BGRA8888(4, 4, px_RGBA8888, 0, 3, bgra);
        swPx_RGBA8888_GetColor_FRGBA(4, 4, px_RGBA8888, 0, 3, frgba);
        swPx_RGBA8888_GetColor_FBGRA(4, 4, px_RGBA8888, 0, 3, fbgra);
        REQUIRE_VEC4_EQ(rgba, 10, 111, 12, 9);
        REQUIRE_VEC4_EQ(bgra, 12, 111, 10, 9);
        REQUIRE_FVEC4_EQ(frgba, 10.0 / 255, 111.0 / 255, 12.0 / 255, 9.0 / 255);
        REQUIRE_FVEC4_EQ(fbgra, 12.0 / 255, 111.0 / 255, 10.0 / 255, 9.0 / 255);

        swPx_RGBA8888_GetColor_RGBA8888(4, 4, px_RGBA8888, 3, 3, rgba);
        swPx_RGBA8888_GetColor_BGRA8888(4, 4, px_RGBA8888, 3, 3, bgra);
        swPx_RGBA8888_GetColor_FRGBA(4, 4, px_RGBA8888, 3, 3, frgba);
        swPx_RGBA8888_GetColor_FBGRA(4, 4, px_RGBA8888, 3, 3, fbgra);
        REQUIRE_VEC4_EQ(rgba, 0, 0, 1, 12);
        REQUIRE_VEC4_EQ(bgra, 1, 0, 0, 12);
        REQUIRE_FVEC4_EQ(frgba, 0, 0, 1.0 / 255, 12.0 / 255);
        REQUIRE_FVEC4_EQ(fbgra, 1.0 / 255, 0, 0, 12.0 / 255);
    }

    {
        // Get pixels RGBA8888 by uv
        SWubyte rgba[4], bgra[4];
        SWfloat frgba[4], fbgra[4];

        // First row
        swPx_RGBA8888_GetColor_RGBA8888_UV(4, 4, px_RGBA8888, 0.1f, 0.1f, rgba);
        swPx_RGBA8888_GetColor_BGRA8888_UV(4, 4, px_RGBA8888, 0.1f, 0.1f, bgra);
        swPx_RGBA8888_GetColor_FRGBA_UV(4, 4, px_RGBA8888, 0.1f, 0.1f, frgba);
        swPx_RGBA8888_GetColor_FBGRA_UV(4, 4, px_RGBA8888, 0.1f, 0.1f, fbgra);
        REQUIRE_VEC4_EQ(rgba, 0, 0, 0, 1);
        REQUIRE_VEC4_EQ(bgra, 0, 0, 0, 1);
        REQUIRE_FVEC4_EQ(frgba, 0, 0, 0, 1.0 / 255);
        REQUIRE_FVEC4_EQ(fbgra, 0, 0, 0, 1.0 / 255);

        swPx_RGBA8888_GetColor_RGBA8888_UV(4, 4, px_RGBA8888, 0.7f, 0.1f, rgba);
        swPx_RGBA8888_GetColor_BGRA8888_UV(4, 4, px_RGBA8888, 0.7f, 0.1f, bgra);
        swPx_RGBA8888_GetColor_FRGBA_UV(4, 4, px_RGBA8888, 0.7f, 0.1f, frgba);
        swPx_RGBA8888_GetColor_FBGRA_UV(4, 4, px_RGBA8888, 0.7f, 0.1f, fbgra);
        REQUIRE_VEC4_EQ(rgba, 0, 1, 0, 3);
        REQUIRE_VEC4_EQ(bgra, 0, 1, 0, 3);
        REQUIRE_FVEC4_EQ(frgba, 0, 1.0 / 255, 0, 3.0 / 255);
        REQUIRE_FVEC4_EQ(fbgra, 0, 1.0 / 255, 0, 3.0 / 255);

        // Last row
        swPx_RGBA8888_GetColor_RGBA8888_UV(4, 4, px_RGBA8888, -0.9f, 0.9f, rgba); // wrong but ok
        swPx_RGBA8888_GetColor_BGRA8888_UV(4, 4, px_RGBA8888, -0.9f, 0.9f, bgra);
        swPx_RGBA8888_GetColor_FRGBA_UV(4, 4, px_RGBA8888, -0.9f, 0.9f, frgba);
        swPx_RGBA8888_GetColor_FBGRA_UV(4, 4, px_RGBA8888, -0.9f, 0.9f, fbgra);
        REQUIRE_VEC4_EQ(rgba, 190, 111, 20, 10);
        REQUIRE_VEC4_EQ(bgra, 20, 111, 190, 10);
        REQUIRE_FVEC4_EQ(frgba, 190.0 / 255, 111.0 / 255, 20.0 / 255, 10.0 / 255);
        REQUIRE_FVEC4_EQ(fbgra, 20.0 / 255, 111.0 / 255, 190.0 / 255, 10.0 / 255);

        swPx_RGBA8888_GetColor_RGBA8888_UV(4, 4, px_RGBA8888, 1.9f, 1.9f, rgba);
        swPx_RGBA8888_GetColor_BGRA8888_UV(4, 4, px_RGBA8888, 1.9f, 1.9f, bgra);
        swPx_RGBA8888_GetColor_FRGBA_UV(4, 4, px_RGBA8888, 1.9f, 1.9f, frgba);
        swPx_RGBA8888_GetColor_FBGRA_UV(4, 4, px_RGBA8888, 1.9f, 1.9f, fbgra);
        REQUIRE_VEC4_EQ(rgba, 0, 0, 1, 12);
        REQUIRE_VEC4_EQ(bgra, 1, 0, 0, 12);
        REQUIRE_FVEC4_EQ(frgba, 0, 0, 1.0 / 255, 12.0 / 255);
        REQUIRE_FVEC4_EQ(fbgra, 1.0 / 255, 0, 0, 12.0 / 255);
    }

    {
        // Set pixels RGBA8888
        SWubyte bgra_buf[4 * 4 * 4];
        for (int x = 0; x < 4; x++) {
            for (int y = 0; y < 4; y++) {
                swPx_BGRA8888_SetColor_RGBA8888(4, 4, bgra_buf, x, y, &px_RGBA8888[4 * (4 * y + x)]);
            }
        }

        REQUIRE_VEC4_EQ(&bgra_buf[4 * (4 * 0 + 0)], 0, 0, 0, 1);
        REQUIRE_VEC4_EQ(&bgra_buf[4 * (4 * 0 + 3)], 255, 0, 0, 4);
        REQUIRE_VEC4_EQ(&bgra_buf[4 * (4 * 3 + 0)], 12, 111, 10, 9);
        REQUIRE_VEC4_EQ(&bgra_buf[4 * (4 * 3 + 3)], 1, 0, 0, 12);

        for (int x = 0; x < 4; x++) {
            for (int y = 0; y < 4; y++) {
                swPx_BGRA8888_SetColor_FRGBA(4, 4, bgra_buf, x, y, &px_FRGBA[4 * (4 * y + x)]);
            }
        }

        REQUIRE_VEC4_EQ(&bgra_buf[4 * (4 * 0 + 0)], 0, 0, 0, 255);
        REQUIRE_VEC4_EQ(&bgra_buf[4 * (4 * 0 + 3)], 0, 0, 255, 255);
        REQUIRE_VEC4_EQ(&bgra_buf[4 * (4 * 3 + 0)], 255, 255, 255, 0);
        REQUIRE_VEC4_EQ(&bgra_buf[4 * (4 * 3 + 3)], 255, 0, 255, 0);
    }
}

#undef REQUIRE_VEC4_EQ
#undef REQUIRE_FVEC4_EQ
