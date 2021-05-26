#pragma once

#include "Log.h"
#include "Storage.h"

namespace Ren {
class AnimSequence;
class Buffer;
class Camera;
class Context;
class ILog;
class Material;
class Mesh;
class Program;
struct RastState;
class Shader;
class Texture2D;
class Texture1D;
class TextureAtlas;
class TextureSplitter;

using AnimSeqRef = StrongRef<AnimSequence>;
using BufferRef = StrongRef<Buffer>;
using WeakBufferRef = WeakRef<Buffer>;
using MaterialRef = StrongRef<Material>;
using MeshRef = StrongRef<Mesh>;
using ProgramRef = StrongRef<Program>;
using ShaderRef = StrongRef<Shader>;
using Tex2DRef = StrongRef<Texture2D>;
using WeakTex2DRef = WeakRef<Texture2D>;
using Tex1DRef = StrongRef<Texture1D>;
using WeakTex1DRef = WeakRef<Texture1D>;
}