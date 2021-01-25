#pragma once

struct DebugMarker {
    explicit DebugMarker(const char* name);
    ~DebugMarker();
};

#if defined(USE_GL_RENDER)
#include <Ren/GL.h>

inline DebugMarker::DebugMarker(const char* name) {
#ifndef DISABLE_MARKERS
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, name);
#endif
}

inline DebugMarker::~DebugMarker() {
#ifndef DISABLE_MARKERS
    glPopDebugGroup();
#endif
}
#endif