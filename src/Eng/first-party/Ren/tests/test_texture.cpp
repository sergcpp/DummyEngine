#include "test_common.h"

#include "../Context.h"
#include "../Texture.h"

static const unsigned char test_tga_img[] = {
    0x00, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x02, 0x00,
    0x20, 0x08, 0x01, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0x01, 0xff, 0xff, 0xff, 0xff,
    0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x54, 0x52, 0x55, 0x45,
    0x56, 0x49, 0x53, 0x49, 0x4f, 0x4e, 0x2d, 0x58, 0x46, 0x49, 0x4c, 0x45, 0x2e, 0x00};

void test_texture() {
    using namespace Ren;

    printf("Test texture            | ");

    { // TGA load
        TestContext test;

        eTexLoadStatus status;
        Tex2DParams p;
        p.usage = Bitmask(eTexUsage::Sampled) | eTexUsage::Transfer;

        Tex2DRef t_ref = test.LoadTexture2D("checker.tga", {}, p, test.default_mem_allocs(), &status);
        require(status == eTexLoadStatus::CreatedDefault);

        require(t_ref->name() == "checker.tga");
        const Tex2DParams &tp = t_ref->params;
        require(tp.w == 1);
        require(tp.h == 1);
        require(tp.format == eTexFormat::RGBA8);
        require(!t_ref->ready());

        {
            Tex2DRef t_ref2 = test.LoadTexture2D("checker.tga", {}, p, test.default_mem_allocs(), &status);
            require(status == eTexLoadStatus::Found);
            require(!t_ref2->ready());
        }

        {
            Tex2DRef t_ref3 = test.LoadTexture2D("checker.tga", test_tga_img, p, test.default_mem_allocs(), &status);
            require(status == eTexLoadStatus::CreatedFromData);
            const Tex2DParams &tp = t_ref3->params;
            require(tp.w == 2);
            require(tp.h == 2);
            require(tp.format == eTexFormat::RGBA8);
            require(t_ref3->ready());
        }
    }

    printf("OK\n");
}
