#include "Context.h"

const char *Snd::Version() { return "v0.1.0-unknown-commit"; }

Snd::BufferRef Snd::Context::LoadBuffer(std::string_view name, Span<const uint8_t> data, const BufParams &params,
                                        eBufLoadStatus *load_status) {
    BufferRef ref = buffers_.FindByName(name);
    if (!ref) {
        ref = buffers_.Add(name, data, params, load_status, log_);
    } else {
        if (load_status) {
            (*load_status) = eBufLoadStatus::Found;
        }
        if (!ref->ready() && !data.empty()) {
            ref->Init(data, params, load_status, log_);
        }
    }
    return ref;
}

void Snd::Context::ReleaseAll() { buffers_.clear(); }
