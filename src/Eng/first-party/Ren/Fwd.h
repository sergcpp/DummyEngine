#pragma once

#include "Log.h"
#include "Storage.h"
#include "Pipeline.h"
#include "Resource.h"

namespace Ren {
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
}
