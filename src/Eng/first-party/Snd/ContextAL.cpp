#include "Context.h"

#include <al.h>
#include <alc.h>

#include "Log.h"

Snd::Context::~Context() {
    ReleaseAll();

    alcDestroyContext(oal_context_);
    alcCloseDevice(oal_device_);
}

bool Snd::Context::Init(ILog *log) {
    oal_device_ = alcOpenDevice(nullptr);
    if (!oal_device_) {
        log->Error("Failed to open device!");
        return false;
    }

    oal_context_ = alcCreateContext(oal_device_, nullptr);
    if (!oal_context_) {
        log->Error("Failed to create context!");
        return false;
    }

    if (alcMakeContextCurrent(oal_context_) != ALC_TRUE) {
        log->Error("Failed to make context current!");
        return false;
    }

    return true;
}

void Snd::Context::SetupListener(const float pos[3], const float vel[3], const float fwd_up[6]) {
    alListener3f(AL_POSITION, pos[0], pos[1], pos[2]);
    alListener3f(AL_VELOCITY, vel[0], vel[1], vel[2]);
    alListener3f(AL_POSITION, pos[0], pos[1], pos[2]);
    alListenerfv(AL_ORIENTATION, &fwd_up[0]);
}