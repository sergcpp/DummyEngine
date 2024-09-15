#pragma once

#include <string_view>

#include "Fwd.h"

namespace Ren {
struct ApiContext;
struct DebugMarker {
    explicit DebugMarker(ApiContext *api_ctx, CommandBuffer cmd_buf, std::string_view name);
    ~DebugMarker();

  private:
    ApiContext *api_ctx_ = nullptr;
    CommandBuffer cmd_buf_ = {};
};
} // namespace Ren
