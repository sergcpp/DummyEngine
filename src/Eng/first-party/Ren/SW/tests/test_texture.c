#include "test_common.h"

#include <stdlib.h>
#include <string.h>

#include "../SWtexture.h"

static SWubyte test_texture_tex[] = { 0, 0, 0,        1, 0, 0,        0, 1, 0,        0, 0, 255,
                         1, 1, 0,        11, 13, 14,     190, 111, 20,   20, 20, 20,
                         10, 111, 12,    190, 111, 20,   0, 1, 0,        0, 0, 1,
                         1, 0, 0,        0, 1, 0,        0, 0, 1,        0, 0, 0
                       };

void test_texture() {

    {
        // Texture init move
        SWtexture t;
        swTexInitMove(&t, SW_RGB, SW_UNSIGNED_BYTE, 4, 4, test_texture_tex, NULL);
        require(t.pixels == test_texture_tex);
        swTexDestroy(&t);
    }

    {
        // Texture init malloced
        SWtexture t;
        void *tex_data = malloc(sizeof(test_texture_tex));
        memcpy(tex_data, test_texture_tex, sizeof(test_texture_tex));
        swTexInitMove_malloced(&t, SW_RGB, SW_UNSIGNED_BYTE, 4, 4, tex_data);
        require(t.pixels == tex_data);
        swTexDestroy(&t);
    }

    {
        // Texture swTexGetColorFloat_RGBA
        SWtexture t_;
        swTexInit(&t_, SW_RGB, SW_UNSIGNED_BYTE, 4, 4, test_texture_tex);
        require(t_.pixels != NULL);
        require(((SWubyte*)t_.pixels)[3] == 1);

        SWfloat rgba[4];
        swTexGetColorFloat_RGBA(&t_, 0.9f, 0.0f, rgba);
        require(rgba[0] == 0);
        require(rgba[1] == 0);
        require(rgba[2] == 1);
        require(rgba[3] == 1);

        swTexDestroy(&t_);
        require(t_.pixels == NULL);
    }
}
