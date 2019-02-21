#include "test_common.h"

#include <stdlib.h>
#include <string.h>

#include "../SWcontext.h"

void test_context() {
    SWcontext *ctx = malloc(sizeof(SWcontext));
    swCtxInit(ctx, 100, 100);
    require(ctx->framebuffers[0].pixels != NULL);

    {
        // Context buffer creation
        SWint buf1 = swCtxCreateBuffer(ctx);
        SWint buf2 = swCtxCreateBuffer(ctx);
        require(buf2 == buf1 + 1);
        const char data1[] = "Data we put in buffer1";
        const char data2[] = "Data for buffer2";
        swCtxBindBuffer(ctx, SW_ARRAY_BUFFER, buf1);
        swCtxBindBuffer(ctx, SW_INDEX_BUFFER, buf2);
        swCtxBufferData(ctx, SW_ARRAY_BUFFER, sizeof(data1), data1);
        swCtxBufferData(ctx, SW_INDEX_BUFFER, sizeof(data2), data2);
        char data1_chk[sizeof(data1)];
        char data2_chk[sizeof(data2)];
        swCtxGetBufferSubData(ctx, SW_ARRAY_BUFFER, 0, sizeof(data1), data1_chk);
        swCtxGetBufferSubData(ctx, SW_INDEX_BUFFER, 0, sizeof(data2), data2_chk);
        require(strcmp(data1, data1_chk) == 0);
        require(strcmp(data2, data2_chk) == 0);
        swCtxDeleteBuffer(ctx, buf1);
        SWint buf3 = swCtxCreateBuffer(ctx);
        require(buf3 == buf1);
        swCtxDeleteBuffer(ctx, buf2);
        swCtxDeleteBuffer(ctx, buf3);
    }

    swCtxDestroy(ctx);
    free(ctx);
}
