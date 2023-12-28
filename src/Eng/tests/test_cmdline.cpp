#include "test_common.h"

#include "../utils/Cmdline.h"

void test_cmdline() {
    printf("Test cmdline            | ");

    Eng::Cmdline cmdline;

    double result;

    cmdline.RegisterCommand("add", [&result](int argc, Eng::Cmdline::ArgData *argv) -> bool {
        require(argc == 3);
        require(argv[1].type == Eng::Cmdline::eArgType::ArgNumber);
        require(argv[2].type == Eng::Cmdline::eArgType::ArgNumber);

        result = argv[1].val + argv[2].val;

        return true;
    });

    require(cmdline.Execute("add 22.78925 -45.89898"));
    require(result == Approx(-23.10973));

    printf("OK\n");
}