#include "DummyApp.h"

#undef main
int main(int argc, char *argv[]) {
    return DummyApp().Run(std::vector<std::string>(argv + 1, argv + argc));
}

// TODO:
// make full screen quad passes differently
// refactor repetitive things in shaders
// use one big array for instance indices