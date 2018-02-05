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
	assert(status == ProgCreatedFromData);
}

void GSOccTest::DrawBBox(const float min[3], const float max[3]) {

}