#include "DummyApp.h"

#undef main
int main(int argc, char *argv[]) {
    return DummyApp().Run(std::vector<std::string>(argv + 1, argv + argc));
}

// TODO:
// scene saving (+ refactor probe caching)
// fix crash when minimizing window
// use direct access extension
// add logstream
// add assetstream
// make full screen quad passes differently
// refactor repetitive things in shaders
// use one big array for instance indices
// get rid of SOIL in Ren (??? png loading left)