#include "WsConnection.h"

#ifdef _WIN32
#include <winsock2.h>
#endif
#if defined(__linux__)

#include <netinet/in.h>

#endif

#include "HTTPRequest.h"
#include "HTTPResponse.h"
#include "Socket.h"

#include "hash/base64.h"
#include "hash/sha1.h"

namespace {
const char WS_MAGIC[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

template <class T, class... Args> std::unique_ptr<T> _make_unique(Args &&...args) {
    return std::unique_ptr<T>(new T(args...));
}

union WsHeader {
    struct {
        uint8_t opcode : 4;
        uint8_t reserved : 3;
        uint8_t fin : 1;
        uint8_t payload_len : 7;
        uint8_t mask : 1;
    };
    uint16_t s;
};

static_assert(sizeof(WsHeader) == 2, "WsHeader should be 16 bits long, deal with your compiler");

enum class eOpCode {
    WS_CONTINUATION = 0x0,
    WS_TEXT_MESSAGE = 0x1,
    WS_BINARY_MESSAGE = 0x2,
    WS_CONNECTION_CLOSE = 0x8,
    WS_PING = 0x9,
    WS_PONG = 0xA
};
} // namespace

Net::WsConnection::WsConnection(TCPSocket &&conn, const HTTPRequest &upgrade_req, bool should_mask)
    : conn_(std::move(conn)), should_mask_(should_mask) {
    std::string key_hash;
    std::string key = upgrade_req.field("Sec-WebSocket-Key");
    if (!key.empty()) {
        key = key + WS_MAGIC;
        unsigned int sha1_digest[5];
        sha1(key, sha1_digest);

        for (unsigned int &i : sha1_digest) {
            i = htonl(i);
        }

        key_hash = base64_encode((const unsigned char *)sha1_digest, 5 * sizeof(unsigned int));
    }

    HTTPResponse resp(101, "Switching Protocols");
    resp.AddField(_make_unique<SimpleField>("Upgrade", "websocket"));
    resp.AddField(_make_unique<SimpleField>("Connection", "Upgrade"));
    resp.AddField(_make_unique<SimpleField>("Sec-WebSocket-Accept", key_hash));
    resp.AddField(_make_unique<SimpleField>("Sec-WebSocket-Version", std::to_string(13)));
    resp.AddField(_make_unique<SimpleField>("Sec-WebSocket-Protocol", "binary"));

    std::string answer = resp.str();
    conn_.Send(answer.c_str(), int(answer.length()));
}

Net::WsConnection::WsConnection(WsConnection &&rhs) noexcept
    : conn_(std::move(rhs.conn_)), should_mask_(rhs.should_mask_),
      on_connection_close(std::move(rhs.on_connection_close)) {}

int Net::WsConnection::Receive(void *data, int size) {
    int received = conn_.Receive(data, size);
    if (received >= sizeof(WsHeader)) {
        auto *header = (WsHeader *)data;
        if (header->fin) {
            if (header->opcode == uint8_t(eOpCode::WS_CONTINUATION)) {
                fprintf(stderr, "Continuation received\n");
                return 0;
            } else if (header->opcode == uint8_t(eOpCode::WS_TEXT_MESSAGE)) {
                fprintf(stderr, "Text message received\n");
                return 0;
            } else if (header->opcode == uint8_t(eOpCode::WS_BINARY_MESSAGE)) {
                void *payload = (void *)(uintptr_t(data) + sizeof(WsHeader));
                int payload_len = header->payload_len;
                if (payload_len == 126) {
                    uint16_t len = *(uint16_t *)(uintptr_t(data) + sizeof(WsHeader));
                    payload_len = ntohs(len);
                    payload = (void *)(uintptr_t(data) + 4);
                } else if (payload_len == 127) {
                    // should not happen
                    fprintf(stderr, "Implement 64-bit payload len\n");
                }
                if (header->mask) {
                    uint32_t mask = *(uint32_t *)payload;
                    payload = (void *)(uintptr_t(payload) + 4);
                    ApplyMask(mask, (uint8_t *)payload, payload_len);
                }
                memmove(data, payload, (size_t)payload_len);
                // LOGI("Binary message received");
                return payload_len;
            } else if (header->opcode == uint8_t(eOpCode::WS_CONNECTION_CLOSE)) {
                printf("Connection close received\n");
                if (on_connection_close) {
                    on_connection_close(this);
                }
                return 0;
            } else if (header->opcode == uint8_t(eOpCode::WS_PING)) {
                header->opcode = uint8_t(eOpCode::WS_PONG);
                Send(data, received);
                return 0;
            } else {
                fprintf(stderr, "Unhandled opcode\n");
                return 0;
            }
        } else {
            // TODO: !!!
            fprintf(stderr, "Fragmented packet received\n");
            return 0;
        }
    } else {
        return 0;
    }
}

bool Net::WsConnection::Send(const void *data, int size) {
    uint8_t buf[2048];
    uint8_t *payload = buf + sizeof(WsHeader);
    auto *header = (WsHeader *)buf;
    header->opcode = uint8_t(eOpCode::WS_BINARY_MESSAGE);
    header->reserved = 0;
    header->fin = 1;
    if (size < 126) {
        header->payload_len = (uint8_t)size;
    } else {
        header->payload_len = 126;
        *(uint16_t *)(uintptr_t(buf) + sizeof(WsHeader)) = htons((uint16_t)size);
        payload += 2;
    }
    header->mask = (uint8_t)should_mask_;
    uint32_t mask = 0;

    if (should_mask_) {
        mask = uint32_t(rand()); // NOLINT
        *(uint32_t *)payload = mask;
        payload += 4;
    }

    memcpy(payload, data, (size_t)size);
    ApplyMask(mask, payload, size);

    return conn_.Send(buf, int(uintptr_t(payload - buf) + size));
}

void Net::WsConnection::ApplyMask(uint32_t mask, uint8_t *data, int size) {
    if (!mask)
        return;
    auto *m = (uint8_t *)&mask;
    for (int i = 0; i < size; i++) {
        data[i] = data[i] ^ m[unsigned(i) & 3u];
    }
}