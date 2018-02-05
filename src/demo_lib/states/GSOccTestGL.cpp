#include "GSOccTest.h"

#include <ren/Program.h>

namespace GSOccTestInternal {
	const char *vs_source = ""
							"/*\n"
							"ATTRIBUTES\n"
							"    aVertexPosition : 0\n"
							"    aVertexNormal : 1\n"
							"UNIFORMS\n"
							"    uMVPMatrix : 0\n"
							"*/\n"
							"\n"
							"attribute vec3 aVertexPosition;\n"
							"attribute vec3 aVertexNormal;\n"
							"uniform mat4 uMVPMatrix;\n"
							"\n"
							"varying vec3 aVertexNormal_;\n"
							"\n"
							"void main(void) {\n"
							"    aVertexNormal_ = aVertexNormal;\n"
							"    gl_Position = uMVPMatrix * vec4(aVertexPosition, 1.0);\n"
							"}\n";

	const char *fs_source = ""
							"#ifdef GL_ES\n"
							"    precision mediump float;\n"
							"#else\n"
							"    #define lowp\n"
							"    #define mediump\n"
							"    #define highp\n"
							"#endif\n"
							"\n"
							"/*\n"
							"UNIFORMS\n"
							"    col : 1\n"
							"*/\n"
							"\n"
							"uniform vec3 col;\n"
							"varying vec3 aVertexNormal_;\n"
							"\n"
							"void main(void) {\n"
							"    gl_FragColor = vec4(col, 1.0) * 0.001 + vec4(aVertexNormal_*0.5 + vec3(0.5, 0.5, 0.5), 1.0);\n"
							"}\n";
}

void GSOccTest::InitShaders() {
	using namespace GSOccTestInternal;

	ren::eProgLoadStatus status;
	main_prog_ = ctx_->LoadProgramGLSL("main", vs_source, fs_source, &status);
	assert(status == ren::ProgCreatedFromData);
}

void GSOccTest::DrawBBox(const float min[3], const float max[3]) {

}

void GSOccTest::BlitDepthBuf() {
    int w = cull_ctx_.zbuf.w, h = cull_ctx_.zbuf.h;
    std::vector<uint8_t> pixels(w * h * 4);
    for (int x = 0; x < w; x++) {
        for (int y = 0; y < h; y++) {
            pixels[4 * (y * w + x) + 0] = (uint8_t)(cull_ctx_.zbuf.depth[y * w + x] * 255);
            pixels[4 * (y * w + x) + 1] = (uint8_t)(cull_ctx_.zbuf.depth[y * w + x] * 255);
            pixels[4 * (y * w + x) + 2] = (uint8_t)(cull_ctx_.zbuf.depth[y * w + x] * 255);
            pixels[4 * (y * w + x) + 3] = 255;
        }
    }

    glUseProgram(0);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);

    glDrawPixels(w, h, GL_RGBA, GL_UNSIGNED_BYTE, &pixels[0]);
}