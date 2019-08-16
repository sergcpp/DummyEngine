#version 310 es

#if defined(GL_ES) || defined(VULKAN)
	precision highp int;
	precision mediump float;
#endif

#include "_fs_common.glsl"
#include "_texturing.glsl"

/*
PERM @TRANSPARENT_PERM
*/

#ifdef TRANSPARENT_PERM
#if !defined(BINDLESS_TEXTURES)
	layout(binding = REN_MAT_TEX0_SLOT) uniform sampler2D alpha_texture;
#endif // BINDLESS_TEXTURES
#endif // TRANSPARENT_PERM

#ifdef TRANSPARENT_PERM
	LAYOUT(location = 0) in highp vec2 aVertexUVs1_;
	#if defined(BINDLESS_TEXTURES)
		LAYOUT(location = 1) in flat TEX_HANDLE alpha_texture;
	#endif // BINDLESS_TEXTURES
#endif // TRANSPARENT_PERM

void main() {
#ifdef TRANSPARENT_PERM
    float alpha = texture(SAMPLER2D(alpha_texture), aVertexUVs1_).a;
    if (alpha < 0.5) discard;
#endif
}
