#include "DummyApp.h"

#include <cassert>
#include <algorithm>
#include <fstream>

#include <Snd/OpenAL/include/al.h>
#include <Snd/OpenAL/include/alc.h>

#include <Snd/Context.h>
#include <Snd/Source.h>
#include <Snd/Utils.h>
#include <Eng/Log.h>

#undef main
int main(int argc, char *argv[]) {
#if 0
    LogStdout log;

    Snd::Context ctx;
    ctx.Init(&log);

    /////////////////////////////////////////////////////////////////////////

    const char *sound_name = "assets/sounds/lawyer/28_trial_by_combat.wav";
    std::ifstream in_file(sound_name, std::ios::binary);
    if (!in_file) {
        return -1;
    }

    int channels, samples_per_sec, bits_per_sample;
    std::unique_ptr<uint8_t[]> samples;
    const int size = Snd::LoadWAV(in_file, channels, samples_per_sec, bits_per_sample, samples);
    if (!size) {
        return -1;
    }

    /////////////////////////////////////////////////////////////////////////

    Snd::BufParams params;

    if (channels == 1 && bits_per_sample == 8) {
        params.format = Snd::eBufFormat::Mono8;
    } else if (channels == 1 && bits_per_sample == 16) {
        params.format = Snd::eBufFormat::Mono16;
    } else if (channels == 2 && bits_per_sample == 8) {
        params.format = Snd::eBufFormat::Stereo8;
    } else if (channels == 2 && bits_per_sample == 16) {
        params.format = Snd::eBufFormat::Stereo16;
    } else {
        return -1;
    }
    params.samples_per_sec = samples_per_sec;

    const int BufferSize = 32 * 1024;
    const int BuffersCount = 4;
    Snd::BufferRef sound_bufs[BuffersCount];

    int buf_pos = 0;
    
    for (int i = 0; i < BuffersCount; i++) {
        char buf[16];
        sprintf(buf, "BUF %i", i);
        
        Snd::eBufLoadStatus status;
        sound_bufs[i] = ctx.LoadBuffer(buf, &samples[buf_pos], BufferSize,
                                       params, &status);
        buf_pos += BufferSize;
        assert(status == Snd::eBufLoadStatus::CreatedFromData);
    }

    Snd::Source snd_source;

    const float source_gain = 1.0f;
    const float source_pos[] = { 0.0f, 0.0f, 0.0f };
    snd_source.Init(source_gain, source_pos);

    assert(snd_source.GetState() == Snd::eSrcState::Initial);

    //snd_source.SetBuffer(sound_buf);
    snd_source.EnqueueBuffers(&sound_bufs[0], BuffersCount);
    snd_source.Play();

    assert(snd_source.GetState() == Snd::eSrcState::Playing);
    while (snd_source.GetState() == Snd::eSrcState::Playing || buf_pos < size) {
        Snd::BufferRef buf = snd_source.UnqueueProcessedBuffer();
        if (buf) {
            printf("buf %u is done\n", buf->id());
            const uint32_t chunk_size = std::min(BufferSize, size - buf_pos);
            buf->SetData(&samples[buf_pos], chunk_size, params);
            buf_pos += chunk_size;
            snd_source.EnqueueBuffers(&buf, 1);
        }
    }
    assert(buf_pos == size);

    return 0;
#endif
    return DummyApp().Run(argc, argv);
}

// TODO:
// refactor probe cache loading
// fix exposure flicker
// use texture array for lightmaps
// texture streaming
// use stencil to distinguich ssr/nossr regions
// velocities for skinned meshes
// use GL_EXT_shader_group_vote
// refactor msaa (resolve once, remove permutations)
// refactor file read on android
// start with scene editing
// use direct state access extension
// add assetstream
// get rid of SDL in Modl app
// make full screen quad passes differently
// refactor repetitive things in shaders
// use frame graph approach in renderer
// check GL_QCOM_alpha_test extension (for depth prepass and shadow rendering)
// check GL_QCOM_tiled_rendering extension
// try to use texture views to share framebuffers texture memory (GL_OES_texture_view)
// use one big array for instance indices
// get rid of SOIL in Ren (??? png loading left)
