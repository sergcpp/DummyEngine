#include "Fence.h"

#include <cassert>
#include <utility>

#include "GL.h"

Ren::SyncFence::~SyncFence() {
    if (sync_) {
        auto sync = reinterpret_cast<GLsync>(std::exchange(sync_, nullptr));
        glDeleteSync(sync);
    }
}

Ren::SyncFence::SyncFence(SyncFence &&rhs) : sync_(std::exchange(rhs.sync_, nullptr)) {}

Ren::SyncFence &Ren::SyncFence::operator=(SyncFence &&rhs) {
    if (sync_) {
        auto sync = reinterpret_cast<GLsync>(std::exchange(sync_, nullptr));
        glDeleteSync(sync);
    }
    sync_ = std::exchange(rhs.sync_, nullptr);
    return (*this);
}

void Ren::SyncFence::WaitSync() {
    assert(sync_);
    glWaitSync(reinterpret_cast<GLsync>(sync_), 0, GL_TIMEOUT_IGNORED);
}

Ren::WaitResult Ren::SyncFence::ClientWaitSync(const uint64_t timeout_us) {
    assert(sync_);
    auto sync = reinterpret_cast<GLsync>(sync_);
    const GLenum res = glClientWaitSync(sync, GL_SYNC_FLUSH_COMMANDS_BIT, timeout_us);

    WaitResult ret = WaitResult::Fail;
    if (res == GL_ALREADY_SIGNALED || res == GL_CONDITION_SATISFIED) {
        ret = WaitResult::Success;
    } else if (res == GL_TIMEOUT_EXPIRED) {
        ret = WaitResult::Timeout;
    }

    return ret;
}

Ren::SyncFence Ren::MakeFence() { return SyncFence{glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0)}; }