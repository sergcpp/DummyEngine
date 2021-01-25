#pragma once

#include <cstdint>
#include <iosfwd>
#include <memory>
#include <vector>

#include <Sys/AssetFile.h>

namespace Ren {
class ILog;
}

struct VpxCtx;

extern "C" {
typedef struct vpx_image vpx_image_t;
}

enum class eYUVComp { Y, U, V };

class VideoPlayer {
    struct Frame {
        uint64_t offset;
        uint32_t size;
        uint32_t flags;
    };
    static const uint32_t KeyFrameFlag = (1u << 0u);

    std::unique_ptr<VpxCtx> ctx_;
    Sys::AssetFile in_file_;
    std::vector<Frame> frames_;

    uint32_t timescale_ = 1;
    uint32_t framerate_ = 0; // per timescale seconds
    uint64_t video_dur_ms_ = 0;

    int w_ = 0, h_ = 0;

    int next_frame_ = -1, last_keyframe_ = -1;
    uint64_t last_time_ms_ = 0;

    const vpx_image_t *cur_image_ = nullptr;
    std::vector<uint8_t> temp_buf_;

  public:
    VideoPlayer();
    ~VideoPlayer();

    bool initialized() const;
    int w() const { return w_; }
    int h() const { return h_; }

    bool OpenAndPreload(const char *name, Ren::ILog *log);
    void Close();

    enum class eFrUpdateResult { Failed, Reused, Updated };

    eFrUpdateResult UpdateFrame(uint64_t time_ms);

    // return pointer to image that will be valid until Update will return 'FrameUpdated'
    const uint8_t *GetImagePtr(eYUVComp plane, int &w, int &h, int &stride);
};
