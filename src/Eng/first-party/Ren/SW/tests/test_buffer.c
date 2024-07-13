#include "test_common.h"

#include <string.h>

#include "../SWbuffer.h"

void test_buffer() {
    printf("Test buffer             | ");

    {
        // Buffer init/destroy
        SWbuffer b;
        const char data1[] = "Data we put in buffer";
        swBufInit(&b, sizeof(data1), data1);
        require(b.data);
        swBufDestroy(&b);
        require(b.data == NULL);
    }

    {
        // Buffer data
        SWbuffer b;
        const char data1[] = "Data we put in buffer";
        swBufInit(&b, sizeof(data1), data1);
        char data1_chk[sizeof(data1)];
        swBufGetData(&b, 0, sizeof(data1), data1_chk);
        require(strcmp(data1, data1_chk) == 0);
        swBufDestroy(&b);
    }

    printf("OK\n");
}
