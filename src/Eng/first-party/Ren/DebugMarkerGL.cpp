#include "DebugMarker.h"

#include "GL.h"

Ren::DebugMarker::DebugMarker(ApiContext *api_ctx, CommandBuffer cmd_buf, std::string_view name) {
#ifndef DISABLE_MARKERS
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, GLsizei(name.length()), name.data());
#endif
}

Ren::DebugMarker::~DebugMarker() {
#ifndef DISABLE_MARKERS
    glPopDebugGroup();
#endif
}