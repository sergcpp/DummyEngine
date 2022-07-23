#include "test_common.h"

#include "membuf.h"

#include "../Buffer.h"
#include "../Context.h"
#include "../Utils.h"

void test_buffer() {
    { // Test suballocation
        TestContext test;

        auto buf = Ren::Buffer{"buf", test.api_ctx(), Ren::eBufType::Uniform, 256};

        require(buf.AllocSubRegion(16, "temp") == 0);
        require(buf.AllocSubRegion(32, "temp") == 16);
        require(buf.AllocSubRegion(64, "temp") == 16 + 32);
        require(buf.AllocSubRegion(16, "temp") == 16 + 32 + 64);

        buf.FreeSubRegion(0, 16);

        require(buf.AllocSubRegion(16, "temp") == 0);
    }
}
