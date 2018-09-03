#pragma once

#include <Ren/Mesh.h>
#include <Ren/MMat.h>
#include <Ren/Storage.h>

struct Transform : public Ren::RefCounter {
    Ren::Mat4f mat;
    Ren::Vec3f bbox_min_ws, bbox_max_ws;
};

struct Drawable : public Ren::RefCounter {
    Ren::MeshRef mesh;
};

enum eObjectFlags { HasTransform = 1, HasDrawable = 2 };

struct SceneObject {
    uint32_t flags;
    Ren::StorageRef<Transform> tr;
    Ren::StorageRef<Drawable> dr;
};