#pragma once

#include <cstdint>
#include <vector>

#include "Socket.h"

namespace Net {
    // length-delimited message framing over TCPSocket for the cross-process link:
    // fixed internal buffers, no allocation per message, whole messages only
    class MsgChannel {
      public:
        struct Msg {
            uint16_t type = 0;
            uint16_t version = 0;
            const uint8_t *payload = nullptr;
            size_t payload_bytes = 0;
        };

        static constexpr uint32_t MAGIC = 0x48435241u; // 'A','R','C','H' little-endian
        static constexpr uint16_t NEGOTIATE_TYPE = 0;  // reserved for NegotiateVersion
        static constexpr size_t HEADER_BYTES = 12;     // magic, version, type, payload_bytes

        explicit MsgChannel(TCPSocket &sock, size_t max_payload_bytes = 64 * 1024,
                            size_t buffer_bytes = 256 * 1024);

        MsgChannel(const MsgChannel &) = delete;
        MsgChannel &operator=(const MsgChannel &) = delete;

        [[nodiscard]] bool healthy() const { return !broken_; }

        bool Send(uint16_t type, const uint8_t *payload, size_t payload_bytes);

        // never blocks; a partial header or payload stays buffered untouched
        bool Poll(Msg &out);

        bool Recv(Msg &out, int timeout_ms);

        // exchanges version candidates and locks in the common version;
        // must be the first traffic on the wire (before any Send)
        bool NegotiateVersion(uint16_t our_version, uint16_t &out_common, int timeout_ms = 5000);

      private:
        bool Append(const uint8_t *data, size_t n);
        bool PollSocket();
        void Break();

        TCPSocket &sock_;
        const size_t max_payload_bytes_;
        std::vector<uint8_t> buf_; // incoming stream, live range [read_pos_, read_pos_ + fill_)
        std::vector<uint8_t> msg_; // delivered payload, stays valid until the next Poll/Recv
        uint16_t version_ = 1;
        size_t read_pos_ = 0;
        size_t fill_ = 0;
        bool broken_ = false;
    };
}
