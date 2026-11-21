#include "test_common.h"

#include <cstring>
#include <thread>
#include <vector>

#include "../MsgChannel.h"

namespace {
    using namespace Net;

    void make_tcp_pair(TCPSocket &server, TCPSocket &client) {
        require_nothrow(server.Open(Address(127, 0, 0, 1, 0), 0));
        require(server.Listen());
        require_nothrow(client.Open(Address(127, 0, 0, 1, 0), 0));

        std::thread thr([&]() {
            for (int i = 0; i < 100 && !server.Accept(); ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        });
        require(client.Connect(Address(127, 0, 0, 1, server.local_addr().port()), 2000));
        thr.join();
        require(server.connected());
    }

    std::vector<unsigned char> make_frame(uint32_t magic, uint16_t version, uint16_t type,
                                          const std::vector<uint8_t> *payload) {
        const uint32_t payload_bytes = payload ? (uint32_t) payload->size() : 0u;
        std::vector<unsigned char> f(MsgChannel::HEADER_BYTES + payload_bytes);
        f[0] = (unsigned char) (magic & 0xFFu);
        f[1] = (unsigned char) ((magic >> 8u) & 0xFFu);
        f[2] = (unsigned char) ((magic >> 16u) & 0xFFu);
        f[3] = (unsigned char) (magic >> 24u);
        f[4] = (unsigned char) (version & 0xFFu);
        f[5] = (unsigned char) (version >> 8u);
        f[6] = (unsigned char) (type & 0xFFu);
        f[7] = (unsigned char) (type >> 8u);
        f[8] = (unsigned char) (payload_bytes & 0xFFu);
        f[9] = (unsigned char) ((payload_bytes >> 8u) & 0xFFu);
        f[10] = (unsigned char) ((payload_bytes >> 16u) & 0xFFu);
        f[11] = (unsigned char) (payload_bytes >> 24u);
        if (payload) {
            memcpy(f.data() + MsgChannel::HEADER_BYTES, payload->data(), payload->size());
        }
        return f;
    }
}

void test_msg_channel() {
    printf("Test msg_channel        | ");

    { // round trip of several message ids with empty and max-size payloads
        TCPSocket server, client;
        make_tcp_pair(server, client);
        MsgChannel ch_s(server), ch_c(client);

        const uint16_t types[] = {1, 0x1234, 0xFFFF};
        const size_t sizes[] = {0, 64 * 1024};
        for (uint16_t type : types) {
            for (size_t size : sizes) {
                std::vector<uint8_t> payload(size);
                for (size_t i = 0; i < size; ++i) {
                    payload[i] = (uint8_t) (i * 31u + type);
                }
                require(ch_c.Send(type, payload.empty() ? nullptr : payload.data(), size));

                MsgChannel::Msg m{};
                require(ch_s.Recv(m, 5000));
                require(m.type == type);
                require(m.payload_bytes == size);
                require(size == 0 || memcmp(m.payload, payload.data(), size) == 0);
            }
        }
    }
    { // fragmented delivery: Poll must emit nothing until the message is complete
        TCPSocket server, client;
        make_tcp_pair(server, client);
        MsgChannel ch(server);

        std::vector<uint8_t> payload(100);
        for (size_t i = 0; i < payload.size(); ++i) {
            payload[i] = (uint8_t) (i + 3u);
        }
        const std::vector<unsigned char> frame = make_frame(MsgChannel::MAGIC, 1, 7, &payload);

        MsgChannel::Msg m{};
        require(client.SendAll(frame.data(), 3, -1));
        require(!ch.Poll(m));
        require(client.SendAll(frame.data() + 3, MsgChannel::HEADER_BYTES - 3, -1));
        require(!ch.Poll(m)); // header complete, payload missing
        require(client.SendAll(frame.data() + MsgChannel::HEADER_BYTES, 1, -1));
        require(!ch.Poll(m));
        require(client.SendAll(frame.data() + MsgChannel::HEADER_BYTES + 1,
                               frame.size() - MsgChannel::HEADER_BYTES - 1, -1));
        require(ch.Recv(m, 2000));
        require(m.type == 7 && m.payload_bytes == payload.size());
        require(memcmp(m.payload, payload.data(), payload.size()) == 0);
    }
    { // two messages coalesced into one write come out separately
        TCPSocket server, client;
        make_tcp_pair(server, client);
        MsgChannel ch(server);

        std::vector<uint8_t> p1(10, 0xAA), p2(20, 0x55);
        std::vector<unsigned char> wire = make_frame(MsgChannel::MAGIC, 1, 1, &p1);
        const std::vector<unsigned char> f2 = make_frame(MsgChannel::MAGIC, 1, 2, &p2);
        wire.insert(wire.end(), f2.begin(), f2.end());
        require(client.SendAll(wire.data(), wire.size(), -1));

        MsgChannel::Msg m{};
        require(ch.Recv(m, 2000));
        require(m.type == 1 && m.payload_bytes == p1.size() && memcmp(m.payload, p1.data(), p1.size()) == 0);
        require(ch.Recv(m, 2000));
        require(m.type == 2 && m.payload_bytes == p2.size() && memcmp(m.payload, p2.data(), p2.size()) == 0);
    }
    { // bad magic must reject the message and break the channel
        TCPSocket server, client;
        make_tcp_pair(server, client);
        MsgChannel ch(server);

        const std::vector<unsigned char> frame = make_frame(0x58585858u, 1, 5, nullptr);
        require(client.SendAll(frame.data(), frame.size(), -1));
        MsgChannel::Msg m{};
        require(!ch.Recv(m, 500));
        require(!ch.healthy());
    }
    { // version mismatch must reject and break
        TCPSocket server, client;
        make_tcp_pair(server, client);
        MsgChannel ch(server);

        const std::vector<unsigned char> frame = make_frame(MsgChannel::MAGIC, 9, 5, nullptr);
        require(client.SendAll(frame.data(), frame.size(), -1));
        MsgChannel::Msg m{};
        require(!ch.Recv(m, 500));
        require(!ch.healthy());
    }
    { // oversized payload_bytes: rejected without ever allocating the advertised size
        TCPSocket server, client;
        make_tcp_pair(server, client);
        MsgChannel ch(server);

        const std::vector<unsigned char> frame = make_frame(MsgChannel::MAGIC, 1, 5, nullptr);
        std::vector<unsigned char> bad = frame;
        bad[8] = bad[9] = bad[10] = bad[11] = 0xFF; // payload_bytes = 0xFFFFFFFF
        require(client.SendAll(bad.data(), 12, -1));
        MsgChannel::Msg m{};
        require(!ch.Recv(m, 500));
        require(!ch.healthy());
    }
    { // NegotiateVersion matrix: common version is the min, data flows at it afterwards
        struct Case {
            uint16_t a, b, common;
        };
        const Case cases[] = {{1, 1, 1}, {1, 2, 1}, {2, 3, 2}, {7, 7, 7}};
        for (const auto &c : cases) {
            TCPSocket server, client;
            make_tcp_pair(server, client);
            MsgChannel ch_s(server), ch_c(client);

            uint16_t common_c = 0;
            bool client_ok = false;
            std::thread thr([&]() {
                client_ok = ch_c.NegotiateVersion(c.b, common_c, 4000);
            });
            uint16_t common_s = 0;
            require(ch_s.NegotiateVersion(c.a, common_s, 4000));
            thr.join();
            require(client_ok);
            require(common_s == c.common);
            require(common_c == c.common);

            require(ch_c.Send(3, nullptr, 0));
            MsgChannel::Msg m{};
            require(ch_s.Recv(m, 2000) && m.type == 3);
            require(ch_s.healthy() && ch_c.healthy());
        }
        { // version 0 is not a valid candidate
            TCPSocket server, client;
            make_tcp_pair(server, client);
            MsgChannel ch_s(server);
            uint16_t common = 1;
            require(!ch_s.NegotiateVersion(0, common, 200));
        }
    }
    { // random garbage on the wire: never crashes, channel breaks, buffered good frames survive
        TCPSocket server, client;
        make_tcp_pair(server, client);
        MsgChannel ch(server);

        std::vector<uint8_t> payload(40, 0x33);
        const std::vector<unsigned char> good = make_frame(MsgChannel::MAGIC, 1, 11, &payload);
        require(client.SendAll(good.data(), good.size(), -1));
        MsgChannel::Msg m{};
        require(ch.Recv(m, 2000));
        require(m.type == 11 && m.payload_bytes == payload.size());

        std::vector<unsigned char> junk(1024);
        unsigned int seed = 12345u;
        for (auto &b : junk) {
            seed = seed * 1103515245u + 12345u;
            b = (unsigned char) (seed >> 16u);
        }
        junk[0] = 0x00; // guarantee the first buffered header is not our magic
        for (int i = 0; i < 10; ++i) {
            // Break() shuts the socket down, so sends after the rejection are
            // expected to start failing - either way the loop must stay alive
            client.SendAll(junk.data(), junk.size(), 2000);
            while (ch.Poll(m)) {
                require(m.payload_bytes <= 64u * 1024u);
            }
        }
        require(!ch.healthy());
        require(!ch.Poll(m)); // a broken channel stays broken
    }

    printf("OK\n");
}
