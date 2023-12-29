#include "test_common.h"

#include "../SWframebuffer.h"
#include "../SWtexture.h"

SWubyte tex[] = { 0, 0, 0,        1, 0, 0,        0, 1, 0,        0, 0, 255,
                  1, 1, 0,        11, 13, 14,     190, 111, 20,   20, 20, 20,
                  10, 111, 12,    190, 111, 20,   0, 1, 0,        0, 0, 1,
                  1, 0, 0,        0, 1, 0,        0, 0, 1,        0, 0, 0
                };

void test_framebuffer() {

#define TEST_BEGIN                          \
    SWframebuffer f;                        \
    swFbufInit(&f, SW_BGRA8888, 5, 20, 1);  \
    require(f.pixels != NULL);              \
    require(f.zbuf != NULL)

#define TEST_END                            \
    swFbufDestroy(&f);                      \
    require(f.pixels == NULL);              \
    require(f.zbuf == NULL)

    {
        // Framebuffer swFbufClearColor_RGBA
        TEST_BEGIN;

        SWubyte col[4] = { 1, 2, 3, 4 };
        swFbufClearColor_RGBA(&f, col);
        for (int y = 0; y < f.h; y++) {
            for (int x = 0; x < f.w; x++) {
                require(((SWubyte*)f.pixels)[4 * (y * f.w + x) + 0] == 3);
                require(((SWubyte*)f.pixels)[4 * (y * f.w + x) + 1] == 2);
                require(((SWubyte*)f.pixels)[4 * (y * f.w + x) + 2] == 1);
                require(((SWubyte*)f.pixels)[4 * (y * f.w + x) + 3] == 4);
            }
        }

        TEST_END;
    }

    {
        // Framebuffer swFbufClearColorFloat
        TEST_BEGIN;

        swFbufClearColorFloat(&f, 1, 0.5f, 0.25f, 0.75f);
        for (int y = 0; y < f.h; y++) {
            for (int x = 0; x < f.w; x++) {
                require(((SWubyte*)f.pixels)[4 * (y * f.w + x) + 0] == 63);
                require(((SWubyte*)f.pixels)[4 * (y * f.w + x) + 1] == 127);
                require(((SWubyte*)f.pixels)[4 * (y * f.w + x) + 2] == 255);
                require(((SWubyte*)f.pixels)[4 * (y * f.w + x) + 3] == 191);
            }
        }

        TEST_END;
    }

    {
        // Framebuffer swFbufSetPixelFloat_RGBA
        TEST_BEGIN;

        swFbufClearColorFloat(&f, 1, 0.5f, 0.25f, 1);
        SWfloat col[4] = { 1, 0, 0.5f, 0.5f };
        swFbufSetPixel_FRGBA(&f, 4, 13, col);

        require(((SWubyte*)f.pixels)[4 * (13 * f.w + 4) + 0] == 127);
        require(((SWubyte*)f.pixels)[4 * (13 * f.w + 4) + 1] == 0);
        require(((SWubyte*)f.pixels)[4 * (13 * f.w + 4) + 2] == 255);
        require(((SWubyte*)f.pixels)[4 * (13 * f.w + 4) + 3] == 127);

        TEST_END;
    }

    {
        // Framebuffer swFbufBlitTexture
        TEST_BEGIN;

        swFbufClearColorFloat(&f, 1, 0.0f, 0.0f, 1);
        swFbufBlitPixels(&f, 1, 10, 0, SW_UNSIGNED_BYTE, SW_RGB, 4, 4, tex, 1);

        require(((SWubyte*)f.pixels)[4 * (11 * f.w + 2) + 0] == 14);
        require(((SWubyte*)f.pixels)[4 * (11 * f.w + 2) + 1] == 13);
        require(((SWubyte*)f.pixels)[4 * (11 * f.w + 2) + 2] == 11);
        require(((SWubyte*)f.pixels)[4 * (11 * f.w + 2) + 3] == 255);

        require(((SWubyte*)f.pixels)[4 * (12 * f.w + 2) + 0] == 20);
        require(((SWubyte*)f.pixels)[4 * (12 * f.w + 2) + 1] == 111);
        require(((SWubyte*)f.pixels)[4 * (12 * f.w + 2) + 2] == 190);
        require(((SWubyte*)f.pixels)[4 * (12 * f.w + 2) + 3] == 255);

        require(((SWubyte*)f.pixels)[4 * (5 * f.w + 3) + 0] == 0);
        require(((SWubyte*)f.pixels)[4 * (5 * f.w + 3) + 1] == 0);
        require(((SWubyte*)f.pixels)[4 * (5 * f.w + 3) + 2] == 255);
        require(((SWubyte*)f.pixels)[4 * (5 * f.w + 3) + 3] == 255);

        TEST_END;
    }
}
#undef TEST_BEGIN
#undef TEST_END