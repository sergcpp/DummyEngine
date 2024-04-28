#pragma once

#include "Bitmask.h"
#include "Log.h"
#include "Resource.h"
#include "Storage.h"

#if defined(USE_VK_RENDER)
typedef uint64_t VkDeviceAddress;
typedef uint32_t VkFlags;
typedef VkFlags VkMemoryPropertyFlags;
struct VkVertexInputBindingDescription;
struct VkVertexInputAttributeDescription;
struct VkMemoryRequirements;
typedef struct VkAccelerationStructureKHR_T *VkAccelerationStructureKHR;
typedef struct VkRenderPass_T *VkRenderPass;
typedef struct VkFence_T *VkFence;
typedef struct VkDescriptorPool_T *VkDescriptorPool;
typedef struct VkDescriptorSet_T *VkDescriptorSet;
typedef struct VkDescriptorSetLayout_T *VkDescriptorSetLayout;
typedef struct VkBuffer_T *VkBuffer;
typedef struct VkDeviceMemory_T *VkDeviceMemory;
typedef struct VkCommandBuffer_T *VkCommandBuffer;
typedef struct VkDescriptorSet_T *VkDescriptorSet;
typedef struct VkDescriptorSetLayout_T *VkDescriptorSetLayout;
typedef struct VkImage_T *VkImage;
typedef struct VkImageView_T *VkImageView;
typedef struct VkSampler_T *VkSampler;
#elif defined(USE_GL_RENDER)
typedef struct VkTraceRaysIndirectCommandKHR {
    uint32_t width;
    uint32_t height;
    uint32_t depth;
} VkTraceRaysIndirectCommandKHR;
#endif

namespace Ren {
#if defined(USE_VK_RENDER)
class AccStructureVK;
#endif
class AnimSequence;
class Buffer;
class Camera;
class Context;
class DescrPool;
class IAccStructure;
class ILog;
class Material;
class Mesh;
class Pipeline;
class ProbeStorage;
class Program;
struct RastState;
class RenderPass;
class Sampler;
class Shader;
class Texture2D;
class Texture1D;
class TextureAtlas;
class Texture2DAtlas;
class TextureSplitter;
class VertexInput;

using AnimSeqRef = StrongRef<AnimSequence>;
using BufferRef = StrongRef<Buffer>;
using WeakBufferRef = WeakRef<Buffer>;
using MaterialRef = StrongRef<Material>;
using MeshRef = StrongRef<Mesh>;
using PipelineRef = StrongRef<Pipeline, SparseArray<Pipeline>>;
using ProgramRef = StrongRef<Program>;
using SamplerRef = StrongRef<Sampler, SparseArray<Sampler>>;
using WeakSamplerRef = WeakRef<Sampler, SparseArray<Sampler>>;
using ShaderRef = StrongRef<Shader>;
using Tex2DRef = StrongRef<Texture2D>;
using WeakTex2DRef = WeakRef<Texture2D>;
using Tex1DRef = StrongRef<Texture1D>;
using WeakTex1DRef = WeakRef<Texture1D>;

const char *Version();
} // namespace Ren
