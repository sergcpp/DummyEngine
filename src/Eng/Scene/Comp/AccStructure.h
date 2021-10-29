#pragma once

#include <Ren/Fwd.h>

#include "Common.h"

struct AccStructure {
    Ren::MeshRef mesh;

    static void Read(const JsObjectP &js_in, AccStructure &acc);
    static void Write(const AccStructure &acc, JsObjectP &js_out);

    static const char *name() { return "acc_structure"; }
};
