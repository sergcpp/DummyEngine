#pragma once

#include <string_view>

namespace Ren {
struct ApiContext;
struct DebugMarker {
    explicit DebugMarker(ApiContext *api_ctx, void *_cmd_buf, std::string_view name);
    ~DebugMarker();

    ApiContext *api_ctx_ = nullptr;
    void *cmd_buf_ = nullptr;
};
} // namespace Ren
