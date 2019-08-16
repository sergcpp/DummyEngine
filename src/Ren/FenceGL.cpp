#include "Fence.h"

#include <cassert>

#include "GL.h"

namespace Ren {
#ifndef REN_EXCHANGE_DEFINED
template <class T, class U = T> T exchange(T &obj, U &&new_value) {
    T old_value = std::move(obj);
    obj = std::forward<U>(new_value);
    return old_value;
}
#define REN_EXCHANGE_DEFINED
#endif
}

Ren::SyncFence::~SyncFence() {
    if (sync_) {
        auto sync = reinterpret_cast<GLsync>(exchange(sync_, nullptr));
        glDeleteSync(sync);
    }
}

Ren::SyncFence::SyncFence(SyncFence &&rhs) : sync_(exchange(rhs.sync_, nullptr)) {}

Ren::SyncFence &Ren::SyncFence::operator=(SyncFence &&rhs) {
    if (sync_) {
        auto sync = reinterpret_cast<GLsync>(exchange(sync_, nullptr));
        glDeleteSync(sync);
    }
    sync_ = exchange(rhs.sync_, nullptr);
    return (*this);
}

void Ren::SyncFence::WaitSync() {
    assert(sync_);
    glWaitSync(reinterpret_cast<GLsync>(sync_), 0, GL_TIMEOUT_IGNORED);
}

Ren::WaitResult Ren::SyncFence::ClientWaitSync(const uint64_t timeout_us) {
    assert(sync_);
    auto sync = reinterpret_cast<GLsync>(sync_);
    const GLenum res = glClientWaitSync(sync, 0, timeout_us);

    WaitResult ret = WaitResult::Fail;
    if (res == GL_ALREADY_SIGNALED || res == GL_CONDITION_SATISFIED) {
        ret = WaitResult::Success;
    } else if (res == GL_TIMEOUT_EXPIRED) {
        ret = WaitResult::Timeout;
    }

    return ret;
}

Ren::SyncFence Ren::MakeFence() { return SyncFence{glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0)}; }