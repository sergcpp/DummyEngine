#include "MsgChannel.h"

#include <algorithm>
#include <chrono>
#include <cstring>

namespace {
    void put_le16(unsigned char *p, uint16_t v) {
        p[0] = (unsigned char) (v & 0xFFu);
        p[1] = (unsigned char) (v >> 8u);
    }

    void put_le32(unsigned char *p, uint32_t v) {
        put_le16(p, (uint16_t) (v & 0xFFFFu));
        put_le16(p + 2, (uint16_t) (v >> 16u));
    }

    uint16_t get_le16(const unsigned char *p) {
        return (uint16_t) (p[0] | (p[1] << 8));
    }

    uint32_t get_le32(const unsigned char *p) {
        return (uint32_t) get_le16(p) | ((uint32_t) get_le16(p + 2) << 16u);
    }
}

Net::MsgChannel::MsgChannel(TCPSocket &sock, size_t max_payload_bytes, size_t buffer_bytes)
        : sock_(sock), max_payload_bytes_(max_payload_bytes),
          buf_((std::max)(buffer_bytes, HEADER_BYTES + max_payload_bytes)), msg_(max_payload_bytes) {}

bool Net::MsgChannel::Send(uint16_t type, const uint8_t *payload, size_t payload_bytes) {
    if (broken_ || payload_bytes > max_payload_bytes_ || (payload_bytes != 0 && payload == nullptr)) {
        return false;
    }

    unsigned char header[HEADER_BYTES];
    put_le32(header, MAGIC);
    put_le16(header + 4, version_);
    put_le16(header + 6, type);
    put_le32(header + 8, (uint32_t) payload_bytes);

    if (!sock_.SendAll(header, sizeof(header)) ||
        (payload_bytes != 0 && !sock_.SendAll(payload, payload_bytes))) {
        Break();
        return false;
    }
    return true;
}

bool Net::MsgChannel::Append(const uint8_t *data, size_t n) {
    size_t room = buf_.size() - read_pos_ - fill_;
    if (room < n && read_pos_ != 0) {
        memmove(buf_.data(), buf_.data() + read_pos_, fill_);
        read_pos_ = 0;
        room = buf_.size() - fill_;
    }
    if (room < n) {
        // buffer overflow is a channel error, never silent message loss
        Break();
        return false;
    }
    memcpy(buf_.data() + read_pos_ + fill_, data, n);
    fill_ += n;
    return true;
}

bool Net::MsgChannel::PollSocket() {
    if (broken_) {
        return false;
    }

    uint8_t chunk[4096];
    while (true) {
        const int got = sock_.Receive(chunk, sizeof(chunk));
        if (got > 0) {
            if (!Append(chunk, (size_t) got)) {
                return false;
            }
        } else if (got == 0) {
            Break(); // orderly peer close: the channel is done
            return false;
        } else {
            if (got == -2) {
                Break();
                return false;
            }
            return true; // -1: nothing more pending
        }
    }
}

bool Net::MsgChannel::Poll(Msg &out) {
    if (!PollSocket() || fill_ < HEADER_BYTES) {
        return false;
    }

    const unsigned char *h = buf_.data() + read_pos_;
    const uint32_t magic = get_le32(h);
    const uint16_t ver = get_le16(h + 4);
    const uint16_t type = get_le16(h + 6);
    const uint32_t payload_bytes = get_le32(h + 8);

    const bool negotiating = type == NEGOTIATE_TYPE;
    if (magic != MAGIC || (!negotiating && ver != version_) || payload_bytes > max_payload_bytes_) {
        // the wire is untrusted: reject instead of buffering attacker-sized messages
        Break();
        return false;
    }
    if (fill_ < HEADER_BYTES + payload_bytes) {
        return false; // partial message: the header stays buffered
    }

    if (payload_bytes != 0) {
        memcpy(msg_.data(), buf_.data() + read_pos_ + HEADER_BYTES, payload_bytes);
    }
    read_pos_ += HEADER_BYTES + payload_bytes;
    fill_ -= HEADER_BYTES + payload_bytes;
    if (fill_ == 0) {
        read_pos_ = 0;
    }

    out.type = type;
    out.version = ver;
    out.payload = payload_bytes != 0 ? msg_.data() : nullptr;
    out.payload_bytes = payload_bytes;
    return true;
}

bool Net::MsgChannel::Recv(Msg &out, int timeout_ms) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    while (true) {
        if (Poll(out)) {
            return true;
        }
        if (broken_) {
            return false;
        }
        const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now()).count();
        if (left <= 0) {
            return false;
        }
        if (!sock_.WaitReadable(int(left))) {
            return false;
        }
    }
}

bool Net::MsgChannel::NegotiateVersion(uint16_t our_version, uint16_t &out_common, int timeout_ms) {
    if (broken_ || our_version == 0) {
        return false;
    }

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    const auto remaining_ms = [&deadline]() -> int {
        const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now()).count();
        return left > 0 ? int(left) : 0;
    };

    version_ = our_version;
    if (!Send(NEGOTIATE_TYPE, nullptr, 0)) {
        return false;
    }

    Msg m{};
    if (!Recv(m, remaining_ms()) || m.type != NEGOTIATE_TYPE) {
        Break();
        return false;
    }
    const uint16_t common = our_version < m.version ? our_version : m.version;
    if (common == 0) {
        Break();
        return false;
    }
    version_ = common;
    if (!Send(NEGOTIATE_TYPE, nullptr, 0)) { // confirm with the chosen version
        return false;
    }

    // wait for the peer's confirmation too: neither side may send data at an
    // unconfirmed version
    while (true) {
        if (!Recv(m, remaining_ms())) {
            Break();
            return false;
        }
        if (m.type == NEGOTIATE_TYPE && m.version == common) {
            break;
        }
    }

    out_common = common;
    return true;
}

void Net::MsgChannel::Break() {
    broken_ = true;
    sock_.Shutdown(); // the peer sees an orderly close instead of a silent drop
}
