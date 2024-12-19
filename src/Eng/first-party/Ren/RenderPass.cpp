#include "RenderPass.h"

namespace Ren {
static const std::string_view g_load_op_names[] = {
    "Load",     // Load
    "Clear",    // Clear
    "DontCare", // DontCare
    "None"      // None
};
static_assert(std::size(g_load_op_names) == int(eLoadOp::_Count));

static const std::string_view g_store_op_names[] = {
    "Store",    // Store
    "DontCare", // DontCare
    "None"      // None
};
static_assert(std::size(g_store_op_names) == int(eStoreOp::_Count));
}; // namespace Ren

std::string_view Ren::LoadOpName(const eLoadOp op) { return g_load_op_names[uint8_t(op)]; }

Ren::eLoadOp Ren::LoadOp(std::string_view name) {
    for (int i = 0; i < int(eLoadOp::_Count); ++i) {
        if (name == g_load_op_names[i]) {
            return eLoadOp(i);
        }
    }
    return eLoadOp::DontCare;
}

std::string_view Ren::StoreOpName(const eStoreOp op) { return g_store_op_names[uint8_t(op)]; }

Ren::eStoreOp Ren::StoreOp(std::string_view name) {
    for (int i = 0; i < int(eStoreOp::_Count); ++i) {
        if (name == g_store_op_names[i]) {
            return eStoreOp(i);
        }
    }
    return eStoreOp::DontCare;
}
