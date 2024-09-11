#include "test_common.h"

#include "../widgets/CmdlineUI.h"

void test_cmdline() {
    printf("Test cmdline            | ");

    /*Eng::CmdlineUI cmdline;

    double result;

    cmdline.RegisterCommand("add", [&result](Ren::Span<const Eng::CmdlineUI::ArgData> args) -> bool {
        require(args.size() == 3);
        require(args[1].type == Eng::CmdlineUI::eArgType::Number);
        require(args[2].type == Eng::CmdlineUI::eArgType::Number);

        result = args[1].val + args[2].val;

        return true;
    });

    require(cmdline.Execute("add 22.78925 -45.89898"));
    require(result == Approx(-23.10973));*/

    printf("OK\n");
}