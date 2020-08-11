#pragma once

#include "Log.h"
#include "Storage.h"

namespace Ren {
class AnimSequence;
class Buffer;
class Camera;
class Context;
class Material;
class Mesh;
class Program;
class Shader;
class Texture2D;
class TextureAtlas;
class TextureSplitter;

using AnimSeqRef = StorageRef<AnimSequence>;
using BufferRef = StorageRef<Buffer>;
using MaterialRef = StorageRef<Material>;
using MeshRef = StorageRef<Mesh>;
using ProgramRef = StorageRef<Program>;
using ShaderRef = StorageRef<Shader>;
using Texture2DRef = StorageRef<Texture2D>;

#if defined(USE_GL_RENDER)
void CheckError(const char *op = "undefined");
#endif
}