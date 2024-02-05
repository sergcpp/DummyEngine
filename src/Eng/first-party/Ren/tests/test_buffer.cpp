#include "test_common.h"

#include "membuf.h"

#include "../Buffer.h"
#include "../Context.h"
#include "../Utils.h"

void test_buffer() {
    printf("Test buffer             | ");

    { // Test suballocation
        TestContext test;

        auto buf = Ren::Buffer{"buf", test.api_ctx(), Ren::eBufType::Uniform, 256};

        const Ren::SubAllocation a1 = buf.AllocSubRegion(16, "temp");
        require(a1.offset == 0);
        require(buf.AllocSubRegion(32, "temp").offset == 16);
        require(buf.AllocSubRegion(64, "temp").offset == 16 + 32);
        require(buf.AllocSubRegion(16, "temp").offset == 16 + 32 + 64);
        require(buf.AllocSubRegion(256 - (16 + 32 + 64 + 16), "temp").offset == 16 + 32 + 64 + 16);

        buf.FreeSubRegion(a1);

        require(buf.AllocSubRegion(16, "temp").offset == 0);
    }

    printf("OK\n");
}
