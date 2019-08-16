#if !defined(VULKAN) && !defined(GL_SPIRV)
#extension GL_ARB_bindless_texture : enable
#endif

#if defined(VULKAN) || defined(GL_ARB_bindless_texture)
#define BINDLESS_TEXTURES
#endif

#if defined(VULKAN)
#extension GL_EXT_nonuniform_qualifier : enable
	#define TEX_HANDLE uint
	#define SAMPLER2D(ndx) scene_textures[nonuniformEXT(ndx)]
	#define GET_HANDLE
	
	layout(set = REN_SET_SCENETEXTURES, binding = REN_BINDLESS_TEX_SLOT) uniform sampler2D scene_textures[];
#else // VULKAN
	#if defined(GL_ARB_bindless_texture)
		#define TEX_HANDLE uvec2
		#define SAMPLER2D(ndx) sampler2D(ndx)
		#define GET_HANDLE(ndx) texture_handles[ndx]
		
		layout(binding = REN_BINDLESS_TEX_SLOT) readonly buffer TextureHandles {
			uvec2 texture_handles[];
		};
	#else // GL_ARB_bindless_texture
		#define SAMPLER2D
	#endif // GL_ARB_bindless_texture
#endif // VULKAN
