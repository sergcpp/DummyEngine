#include "test_common.h"

#include "../Utils/Cmdline.h"

void test_cmdline() {
    Cmdline cmdline;

    double result;

    cmdline.RegisterCommand("add",
                            [&result](int argc, Cmdline::ArgData *argv) -> bool {
                                require(argc == 3);
                                require(argv[1].type == Cmdline::ArgNumber);
                                require(argv[2].type == Cmdline::ArgNumber);

                                result = argv[1].val + argv[2].val;

                                return true;
                            });

    require(cmdline.Execute("add 22.78925 -45.89898"));
    require(result == Approx(-23.10973));
}