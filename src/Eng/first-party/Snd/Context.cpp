#include "Context.h"

Snd::BufferRef Snd::Context::LoadBuffer(const char *name, const void *data,
                                        const uint32_t size, const BufParams &params,
                                        eBufLoadStatus *load_status) {
    BufferRef ref = buffers_.FindByName(name);
    if (!ref) {
        ref = buffers_.Add(name, data, size, params, load_status, log_);
    } else {
        if (load_status) {
            (*load_status) = eBufLoadStatus::Found;
        }
        if (!ref->ready() && data) {
            ref->Init(data, size, params, load_status, log_);
        }
    }

    return ref;
}

void Snd::Context::ReleaseAll() { buffers_.clear(); }
