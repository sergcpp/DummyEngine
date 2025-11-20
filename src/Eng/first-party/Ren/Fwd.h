#pragma once

#include "Bitmask.h"
#include "Log.h"
#include "Storage.h"

#if defined(REN_VK_BACKEND)
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
typedef struct VkBufferView_T *VkBufferView;
typedef struct VkDeviceMemory_T *VkDeviceMemory;
typedef struct VkCommandBuffer_T *VkCommandBuffer;
typedef struct VkDescriptorSet_T *VkDescriptorSet;
typedef struct VkDescriptorSetLayout_T *VkDescriptorSetLayout;
typedef struct VkImage_T *VkImage;
typedef struct VkImageView_T *VkImageView;
typedef struct VkSampler_T *VkSampler;
#elif defined(REN_GL_BACKEND)
typedef struct VkTraceRaysIndirectCommandKHR {
    uint32_t width;
    uint32_t height;
    uint32_t depth;
} VkTraceRaysIndirectCommandKHR;
#endif

namespace Ren {
#if defined(REN_VK_BACKEND)
class AccStructureVK;
using CommandBuffer = VkCommandBuffer;
#else
using CommandBuffer = void *;
#endif
class AnimSequence;
class Buffer;
class Camera;
class Context;
class DescrPool;
class DescrMultiPoolAlloc;
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
class Image;
class ImageAtlas;
class Texture2DAtlas;
class ImageSplitter;
class VertexInput;

using AnimSeqRef = StrongRef<AnimSequence, NamedStorage<AnimSequence>>;
using BufRef = StrongRef<Buffer, NamedStorage<Buffer>>;
using WeakBufRef = WeakRef<Buffer, NamedStorage<Buffer>>;
using MaterialRef = StrongRef<Material, NamedStorage<Material>>;
using MeshRef = StrongRef<Mesh, NamedStorage<Mesh>>;
using VertexInputRef = StrongRef<VertexInput, SortedStorage<VertexInput>>;
using PipelineRef = StrongRef<Pipeline, SortedStorage<Pipeline>>;
using ProgramRef = StrongRef<Program, SortedStorage<Program>>;
using SamplerRef = StrongRef<Sampler, SparseArray<Sampler>>;
using WeakSamplerRef = WeakRef<Sampler, SparseArray<Sampler>>;
using ShaderRef = StrongRef<Shader, NamedStorage<Shader>>;
using ImgRef = StrongRef<Image, NamedStorage<Image>>;
using WeakImgRef = WeakRef<Image, NamedStorage<Image>>;

const char *Version();
} // namespace Ren
