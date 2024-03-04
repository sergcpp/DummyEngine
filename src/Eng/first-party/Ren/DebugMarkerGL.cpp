#include "DebugMarker.h"

#include "GL.h"

Ren::DebugMarker::DebugMarker(ApiContext *api_ctx, void *_cmd_buf, const char *name) {
#ifndef DISABLE_MARKERS
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, name);
#endif
}

Ren::DebugMarker::~DebugMarker() {
#ifndef DISABLE_MARKERS
    glPopDebugGroup();
#endif
}