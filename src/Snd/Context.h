#pragma once

#include "Buffer.h"

typedef struct ALCdevice_struct ALCdevice;
typedef struct ALCcontext_struct ALCcontext;

namespace Snd {
class ILog;

class Context {
    ILog *log_ = nullptr;
#if defined(USE_AL_SOUND)
    ALCdevice *oal_device_ = nullptr;
    ALCcontext *oal_context_ = nullptr;
#endif

    BufferStorage buffers_;

  public:
    Context() = default;
    ~Context();

    Context(const Context &rhs) = delete;

    bool Init(ILog *log);

    void SetupListener(const float pos[3], const float vel[3], const float fwd_up[6]);

    /*** Buffer ***/
    BufferRef LoadBuffer(const char *name, const void *data, uint32_t size,
                         const BufParams &params, eBufLoadStatus *load_status);

    void ReleaseAll();
};
} // namespace Snd
