#include "DummyApp.h"

#undef main
int main(int argc, char *argv[]) {
    return DummyApp().Run(argc, argv);
}

// TODO:
// fix freeze on early exit
// refactor file read on android
// fix decals (use compressed 16x16 blocks)
// start with scene editing
// use direct state access extension
// add assetstream
// get rid of SDL in Modl app
// make full screen quad passes differently
// refactor repetitive things in shaders
// use frame graph approach in renderer
// check GL_QCOM_alpha_test extension (for depth prepass and shadow rendering)
// check GL_QCOM_tiled_rendering extension
// try to use texture views to share framebuffers texture memory (GL_OES_texture_view)
// use one big array for instance indices
// get rid of SOIL in Ren (??? png loading left)