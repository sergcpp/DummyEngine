#include "DemoApp.h"

#undef main
int main(int argc, char *argv[]) {
    return DemoApp().Run(std::vector<std::string>(argv + 1, argv + argc));
}

// improve Modl diag view with normal maps
// load scene from json file
// build bvh
// make 2-threaded renderer
// make sun with shadow cascades
// light sources with shadow maps
// forward+
// add ray renderer
// android support