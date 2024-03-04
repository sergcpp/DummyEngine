#pragma once

namespace Ren {
struct ApiContext;
struct DebugMarker {
    explicit DebugMarker(ApiContext *api_ctx, void *_cmd_buf, const char *name);
    ~DebugMarker();

    ApiContext *api_ctx_ = nullptr;
    void *cmd_buf_ = nullptr;
};
} // namespace Ren
