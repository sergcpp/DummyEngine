#include "test_common.h"

#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include "../HTTPRequest.h"
#include "../Socket.h"
#include "../WsConnection.h"

void test_ws_connection() {
    using namespace Net;

    printf("Test ws_connection      | ");

    TCPSocket server, client_a, client_b;
    require_nothrow(server.Open(Address(127, 0, 0, 1, 0), 0));
    require(server.Listen());
    require_nothrow(client_a.Open(Address(127, 0, 0, 1, 0), 0));
    require_nothrow(client_b.Open(Address(127, 0, 0, 1, 0), 0));

    const unsigned short port = server.local_addr().port();

    // the WsConnection constructor answers the (simulated) upgrade handshake with a 101
    // response right away; drop it before checking the raw frame bytes. read one byte at
    // a time so the first frame bytes cannot be swallowed by a big read
    auto drain_handshake = [](TCPSocket &s) {
        char ring[4] = {0, 0, 0, 0};
        bool done = false;
        for (int i = 0; i < 4096 && !done; ++i) {
            char c = 0;
            if (!s.RecvExactly(&c, 1, 2000)) {
                break;
            }
            ring[0] = ring[1];
            ring[1] = ring[2];
            ring[2] = ring[3];
            ring[3] = c;
            done = memcmp(ring, "\r\n\r\n", 4) == 0;
        }
        require(done);
    };

    { // unmasked frames: RFC6455 length must survive all three length forms
        std::thread thr([&]() {
            for (int i = 0; i < 100 && !server.Accept(); ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        });
        require(client_a.Connect(Address(127, 0, 0, 1, port), 2000));
        thr.join();
        require(server.connected());
        TCPSocket conn_a = TCPSocket::PassClientConnection(server);

        WsConnection ws(std::move(client_a), HTTPRequest(), false);
        drain_handshake(conn_a);

        struct FrameCase {
            size_t size;
            size_t head;
            unsigned char head_bytes[10];
        };
        const std::vector<FrameCase> cases = {
            {5, 2, {0x82, 0x05, 0, 0, 0, 0, 0, 0, 0, 0}},
            {300, 4, {0x82, 126, 0x01, 0x2C, 0, 0, 0, 0, 0, 0}},
            {65537, 10, {0x82, 127, 0, 0, 0, 0, 0, 0x01, 0, 0x01}},
        };
        for (const auto &f : cases) {
            std::vector<unsigned char> payload(f.size);
            for (size_t i = 0; i < f.size; ++i) {
                payload[i] = (unsigned char) (i * 3u + 1u);
            }
            require(ws.Send(payload.data(), int(f.size)));

            std::vector<unsigned char> frame(f.head + f.size);
            require(conn_a.RecvExactly(frame.data(), frame.size(), 5000));
            require(memcmp(frame.data(), f.head_bytes, f.head) == 0);
            require(memcmp(frame.data() + f.head, payload.data(), f.size) == 0);
        }
    }
    { // masked frame: mask bit set, 4-byte key present, payload XORed
        std::thread thr([&]() {
            for (int i = 0; i < 100 && !server.Accept(); ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        });
        require(client_b.Connect(Address(127, 0, 0, 1, port), 2000));
        thr.join();
        TCPSocket conn_b = TCPSocket::PassClientConnection(server);

        WsConnection ws(std::move(client_b), HTTPRequest(), true);
        drain_handshake(conn_b);

        const size_t size = 300;
        std::vector<unsigned char> payload(size);
        for (size_t i = 0; i < size; ++i) {
            payload[i] = (unsigned char) (i * 5u + 7u);
        }
        require(ws.Send(payload.data(), int(size)));

        std::vector<unsigned char> frame(4 + 4 + size);
        require(conn_b.RecvExactly(frame.data(), frame.size(), 5000));
        require(frame[0] == 0x82);
        require(frame[1] == (unsigned char) (0x80 | 126));
        require(frame[2] == 0x01 && frame[3] == (unsigned char) 0x2C);
        const unsigned char *mask = frame.data() + 4;
        for (size_t i = 0; i < size; ++i) {
            require(frame[8 + i] == (unsigned char) (payload[i] ^ mask[i & 3u]));
        }
    }

    printf("OK\n");
}
