#include "DebugMarker.h"

#include "VKCtx.h"

Ren::DebugMarker::DebugMarker(ApiContext *api_ctx, void *_cmd_buf, std::string_view name)
    : api_ctx_(api_ctx), cmd_buf_(_cmd_buf) {
    VkDebugUtilsLabelEXT label = {VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT};
    label.pLabelName = name.data();
    label.color[0] = label.color[1] = label.color[2] = label.color[3] = 1.0f;

    VkCommandBuffer cmd_buf = reinterpret_cast<VkCommandBuffer>(_cmd_buf);
    api_ctx_->vkCmdBeginDebugUtilsLabelEXT(cmd_buf, &label);
}

Ren::DebugMarker::~DebugMarker() {
    VkCommandBuffer cmd_buf = reinterpret_cast<VkCommandBuffer>(cmd_buf_);
    api_ctx_->vkCmdEndDebugUtilsLabelEXT(cmd_buf);
}