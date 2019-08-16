#include "VideoPlayer.h"

#include <cassert>

#include <Ren/Log.h>

#include <vpx/vp8dx.h>
#include <vpx/vpx_decoder.h>
#include <vpx/vpx_image.h>

namespace VideoPlayerInternal {
struct IVFHeader {
    char signature[4];
    uint16_t version;
    uint16_t length;
    uint32_t codec;
    uint16_t width;
    uint16_t height;
    uint32_t framerate; // frame count in 1000 seconds
    uint32_t timescale;
    uint32_t num_frames;
    uint32_t unused;
};
static_assert(sizeof(IVFHeader) == 32, "!");

static_assert(int(eYUVComp::Y) == VPX_PLANE_Y, "!");
static_assert(int(eYUVComp::U) == VPX_PLANE_U, "!");
static_assert(int(eYUVComp::V) == VPX_PLANE_V, "!");

int vpx_plane_width(const vpx_image_t *img, int plane) {
    if (plane && img->x_chroma_shift) {
        return int((img->d_w + 1) >> img->x_chroma_shift);
    }
    return int(img->d_w);
}

int vpx_plane_height(const vpx_image_t *img, int plane) {
    if (plane && img->y_chroma_shift) {
        return int((img->d_h + 1) >> img->y_chroma_shift);
    }
    return int(img->d_h);
}
} // namespace VideoPlayerInternal

struct VpxCtx {
    bool initialized = false;
    vpx_codec_ctx_t codec = {};

    bool Init(const unsigned w, const unsigned h) {
        Destroy();

        vpx_codec_dec_cfg_t cfg;
        cfg.threads = 1;
        cfg.w = w;
        cfg.h = h;

        initialized =
            vpx_codec_dec_init_ver(&codec, vpx_codec_vp9_dx(), &cfg, 0, VPX_DECODER_ABI_VERSION) == VPX_CODEC_OK;
        return initialized;
    }

    void Destroy() {
        if (initialized) {
            vpx_codec_destroy(&codec);
            initialized = false;
        }
    }

    ~VpxCtx() { Destroy(); }
};

VideoPlayer::VideoPlayer() { ctx_.reset(new VpxCtx); }

VideoPlayer::~VideoPlayer() = default;

bool VideoPlayer::initialized() const { return ctx_->initialized; }

bool VideoPlayer::OpenAndPreload(const char *name, Ren::ILog *log) {
    using namespace VideoPlayerInternal;

    if (!in_file_.Open(name)) {
        log->Error("VideoPlayer: Failed to open file %s!", name);
        return false;
    }

    IVFHeader header = {};
    if (in_file_.Read((char *)&header, sizeof(IVFHeader)) != sizeof(IVFHeader)) {
        log->Error("VideoPlayer: Failed to read IVF header from file %s!", name);
        return false;
    }

    if (header.signature[0] != 'D' || header.signature[1] != 'K' || header.signature[2] != 'I' ||
        header.signature[3] != 'F') {
        log->Error("VideoPlayer: Invalid file signature in file %s!", name);
        return false;
    }

    frames_.resize(header.num_frames);

    vpx_codec_stream_info_t si;
    si.sz = sizeof(vpx_codec_stream_info_t);

    for (uint32_t i = 0; i < header.num_frames; i++) {
        const auto frame_pos = size_t(in_file_.pos());

        uint32_t frame_len;
        if (in_file_.Read((char *)&frame_len, sizeof(uint32_t)) != sizeof(uint32_t)) {
            log->Error("VideoPlayer: Failed to read frame len from file %s!", name);
            return false;
        }

        uint64_t timestamp;
        if (in_file_.Read((char *)&timestamp, sizeof(uint64_t)) != sizeof(uint64_t)) {
            log->Error("VideoPlayer: Failed to read timestamp from file %s!", name);
            return false;
        }

        const int BytesNeededToPeekInfo = 10;
        uint8_t temp_buf[BytesNeededToPeekInfo];

        if (frame_len < BytesNeededToPeekInfo) {
            log->Error("VideoPlayer: Frame size is too small (%u)!", frame_len);
            return false;
        }

        // Read frame just enough to parse its info
        if (in_file_.Read((char *)&temp_buf[0], BytesNeededToPeekInfo) != BytesNeededToPeekInfo) {
            log->Error("VideoPlayer: Failed to read frame data from file %s!", name);
            return false;
        }
        // Skip rest of the frame
        in_file_.SeekRelative(frame_len - BytesNeededToPeekInfo);

        if (vpx_codec_peek_stream_info(&vpx_codec_vp9_dx_algo, &temp_buf[0], BytesNeededToPeekInfo, &si) ==
            VPX_CODEC_OK) {
            Frame &fr = frames_[i];

            fr.offset = uint64_t(frame_pos);
            fr.size = frame_len;
            fr.flags = si.is_kf ? KeyFrameFlag : 0;
        }
    }

    assert(frames_.size() == header.num_frames);
    assert(frames_[0].flags & KeyFrameFlag);

    timescale_ = header.timescale;
    framerate_ = header.framerate;
    video_dur_ms_ = 1000 * timescale_ * header.num_frames / framerate_;

    if (!ctx_->Init(unsigned(header.width), unsigned(header.height))) {
        log->Error("VideoPlayer: Failed to initialize codec!");
        return false;
    }

    w_ = header.width;
    h_ = header.height;
    last_keyframe_ = -1;

    return true;
}

void VideoPlayer::Close() {
    in_file_.Close();
    frames_.clear();
    ctx_->Destroy();
}

VideoPlayer::eFrUpdateResult VideoPlayer::UpdateFrame(const uint64_t time_ms) {
    using namespace VideoPlayerInternal;

    if (!ctx_->initialized || frames_.size() < 2) {
        return eFrUpdateResult::Failed;
    }

    const uint64_t time_looped_ms = time_ms % video_dur_ms_;
    const int frame_index = int(time_looped_ms * framerate_ / (1000ull * timescale_));

    if (time_looped_ms < last_time_ms_) {
        // reset keyframe
        last_keyframe_ = -1;
    }
    last_time_ms_ = time_looped_ms;

    if (next_frame_ == frame_index + 1) {
        // we can safely reuse last frame
        return eFrUpdateResult::Reused;
    }

    int nearest_keyframe = -1;
    for (int i = frame_index; i >= 0; --i) {
        if (frames_[i].flags & KeyFrameFlag) {
            nearest_keyframe = i;
            break;
        }
    }

    if (nearest_keyframe == -1) {
        // failed to find keyframe
        return eFrUpdateResult::Failed;
    }

    const int decode_beg = (nearest_keyframe == last_keyframe_) ? next_frame_ : nearest_keyframe;
    const int decode_end = frame_index + 1;

    for (int i = decode_beg; i < decode_end; i++) {
        const Frame &fr = frames_[i];

        temp_buf_.resize(fr.size);
        in_file_.SeekAbsolute(fr.offset + sizeof(uint32_t) + sizeof(uint64_t));
        in_file_.Read((char *)&temp_buf_[0], fr.size);

        const int res = vpx_codec_decode(&ctx_->codec, &temp_buf_[0], fr.size, nullptr, 0);
        if (res != VPX_CODEC_OK) {
            return eFrUpdateResult::Failed;
        }
    }

    // store last decoded keyframe
    last_keyframe_ = nearest_keyframe;
    // start from this frame next time
    next_frame_ = decode_end;

    vpx_codec_iter_t iter = nullptr;
    vpx_image_t *img = vpx_codec_get_frame(&ctx_->codec, &iter);
    if (img && img->fmt == VPX_IMG_FMT_I420) {
        cur_image_ = img;
    } else {
        cur_image_ = nullptr;
    }

    return eFrUpdateResult::Updated;
}

const uint8_t *VideoPlayer::GetImagePtr(const eYUVComp plane, int &w, int &h, int &stride) {
    using namespace VideoPlayerInternal;

    if (cur_image_) {
        w = vpx_plane_width(cur_image_, int(plane));
        h = vpx_plane_height(cur_image_, int(plane));
        stride = cur_image_->stride[int(plane)];
        return cur_image_->planes[int(plane)];
    }
    return nullptr;
}