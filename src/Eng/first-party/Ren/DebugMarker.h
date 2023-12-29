#pragma once

namespace Ren {
struct DebugMarker {
    explicit DebugMarker(ApiContext *api_ctx, void *_cmd_buf, const char *name);
    ~DebugMarker();

    ApiContext *api_ctx_ = nullptr;
    void *cmd_buf_ = nullptr;
};
} // namespace Ren

#if defined(USE_VK_RENDER)
#include "VK.h"

inline Ren::DebugMarker::DebugMarker(ApiContext *api_ctx, void *_cmd_buf, const char *name)
    : api_ctx_(api_ctx), cmd_buf_(_cmd_buf) {
    VkDebugUtilsLabelEXT label = {VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT};
    label.pLabelName = name;
    label.color[0] = label.color[1] = label.color[2] = label.color[3] = 1.0f;

    VkCommandBuffer cmd_buf = reinterpret_cast<VkCommandBuffer>(_cmd_buf);
    api_ctx_->vkCmdBeginDebugUtilsLabelEXT(cmd_buf, &label);
}

inline Ren::DebugMarker::~DebugMarker() {
    VkCommandBuffer cmd_buf = reinterpret_cast<VkCommandBuffer>(cmd_buf_);
    api_ctx_->vkCmdEndDebugUtilsLabelEXT(cmd_buf);
}
#elif defined(USE_GL_RENDER)
#include <Ren/GL.h>

inline Ren::DebugMarker::DebugMarker(ApiContext *api_ctx, void *_cmd_buf, const char *name) {
#ifndef DISABLE_MARKERS
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, name);
#endif
}

inline Ren::DebugMarker::~DebugMarker() {
#ifndef DISABLE_MARKERS
    glPopDebugGroup();
#endif
}
#endif
