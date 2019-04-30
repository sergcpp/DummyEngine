#pragma once

#include <Ren/Fwd.h>

#include "Common.h"

struct Occluder {
    Ren::MeshRef mesh;

    static void Read(const JsObject &js_in, Occluder &occ) {}
    static void Write(const Occluder &occ, JsObject &js_out) {}

    static const char *name() { return "occluder"; }
};