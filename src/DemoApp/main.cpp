#include "DemoApp.h"

#undef main
int main(int argc, char *argv[]) {
    return DemoApp().Run(std::vector<std::string>(argv + 1, argv + argc));
}
