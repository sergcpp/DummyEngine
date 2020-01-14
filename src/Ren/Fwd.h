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
class Texture2D;
class TextureAtlas;
class TextureSplitter;

typedef StorageRef<AnimSequence> AnimSeqRef;
typedef StorageRef<Buffer> BufferRef;
typedef StorageRef<Material> MaterialRef;
typedef StorageRef<Mesh> MeshRef;
typedef StorageRef<Program> ProgramRef;
typedef StorageRef<Texture2D> Texture2DRef;

#if defined(USE_GL_RENDER)
void CheckError(const char *op = "undefined");
#endif
}