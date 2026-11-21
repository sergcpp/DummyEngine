#include "test_common.h"

#include <atomic>
#include <chrono>
#include <cstring>
#include <thread>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <winsock2.h>
#include <ws2ipdef.h>
#include <ws2tcpip.h>
#else
#include <fcntl.h>
#include <unistd.h>

#include <netinet/tcp.h>
#endif

#include "../Socket.h"

void test_tcp_socket() {
    using namespace Net;

    printf("Test tcp_socket         | ");

    { // TCPSocket open/close
        TCPSocket socket;
        require(!socket.IsOpen());
        require_nothrow(socket.Open(0));
        require(socket.IsOpen());
        require(socket.local_addr().port() != 0);
        socket.Close();
        require(!socket.IsOpen());
        require_nothrow(socket.Open(30000));
        require(socket.local_addr().port() == 30000);
        require(socket.IsOpen());
    }
    { // TCPSocket bind address is reported back
        TCPSocket s;
        require_nothrow(s.Open(Address(127, 0, 0, 1, 0), 0));
        require(s.local_addr().a() == 127);
        require(s.local_addr().port() != 0);
    }
    { // TCPSocket same port fail
        TCPSocket a, b;
        require_nothrow(a.Open(30000, false));
        require_throws(b.Open(30000, false));
        require(a.IsOpen());
        require(!b.IsOpen());
    }
    { // connect to a port with no listener must fail, not report success
        TCPSocket probe;
        require_nothrow(probe.Open(Address(127, 0, 0, 1, 0), 0));
        const unsigned short dead_port = probe.local_addr().port();
        probe.Close();

        TCPSocket a;
        require_nothrow(a.Open(Address(127, 0, 0, 1, 0), 0));
        require(!a.Connect(Address(127, 0, 0, 1, dead_port)));
        require(a.last_error() != 0);
    }
    { // fresh connection with no data pending: -1 (would-block), not 0
        TCPSocket server, client;
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

        char buffer[64];
        require(client.Receive(buffer, sizeof(buffer)) == -1);
    }
    { // WaitReadable: false on an idle socket, true once data arrives
        TCPSocket server, client;
        require_nothrow(server.Open(Address(127, 0, 0, 1, 0), 0));
        require(server.Listen());
        require_nothrow(client.Open(Address(127, 0, 0, 1, 0), 0));

        std::thread thr([&]() {
            for (int i = 0; i < 100 && !server.Accept(); ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            // delay the send so the idle-socket check below cannot race with it
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            const char data[] = "x";
            for (int i = 0; i < 100 && !server.Send(data, sizeof(data)); ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        });
        require(client.Connect(Address(127, 0, 0, 1, server.local_addr().port()), 2000));

        require(!client.WaitReadable(50));
        require(client.WaitReadable(2000));
        thr.join();
    }
    { // TCP_NODELAY is enabled after Accept/Connect
        TCPSocket server, client;
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

        int nodelay = 0;
        socklen_t len = sizeof(nodelay);
        require(getsockopt(client.socket_handle(), IPPROTO_TCP, TCP_NODELAY, (char *) &nodelay, &len) == 0);
        require(nodelay != 0);
        nodelay = 0;
        require(getsockopt(server.connection_handle(), IPPROTO_TCP, TCP_NODELAY, (char *) &nodelay, &len) == 0);
        require(nodelay != 0);
    }
    { // TCPSocket send and receive packets
        TCPSocket server, client;
        require_nothrow(server.Open(Address(127, 0, 0, 1, 0), 0));
        require(server.Listen());
        require_nothrow(client.Open(Address(127, 0, 0, 1, 0), 0));

        const char packet[] = "packet data";
        bool echoed = false;
        std::thread thr([&]() {
            for (int i = 0; i < 100 && !server.Accept(); ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            for (int i = 0; i < 100 && !echoed; ++i) {
                char buffer[256];
                const int bytes_read = server.Receive(buffer, sizeof(buffer));
                if (bytes_read > 0) {
                    echoed = bytes_read == sizeof(packet) && memcmp(buffer, packet, sizeof(packet)) == 0 &&
                             server.Send(packet, sizeof(packet));
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });
        require(client.Connect(Address(127, 0, 0, 1, server.local_addr().port()), 2000));

        bool client_received = false;
        for (int i = 0; i < 100 && !(client_received && echoed); ++i) {
            require(client.Send(packet, sizeof(packet)));

            char buffer[256];
            const int bytes_read = client.Receive(buffer, sizeof(buffer));
            if (bytes_read > 0) {
                client_received = bytes_read == sizeof(packet) && memcmp(buffer, packet, sizeof(packet)) == 0;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        thr.join();
        require(echoed);
        require(client_received);
    }
    { // SendAll of 1 MiB through a small send buffer arrives complete and in order
        TCPSocket server, client;
        require_nothrow(server.Open(Address(127, 0, 0, 1, 0), 0));
        require(server.Listen());
        require_nothrow(client.Open(Address(127, 0, 0, 1, 0), 0));

        const int sndbuf = 8192;
        require(setsockopt(client.socket_handle(), SOL_SOCKET, SO_SNDBUF, (char *) &sndbuf, sizeof(sndbuf)) == 0);

        const size_t payload_size = 1u << 20;
        std::vector<unsigned char> payload(payload_size);
        unsigned int rnd = 0x12345u;
        for (auto &b : payload) {
            rnd = rnd * 1103515245u + 12345u;
            b = (unsigned char) (rnd >> 16u);
        }
        // order-sensitive FNV-1a over the whole stream
        unsigned int expected_hash = 2166136261u;
        for (unsigned char b : payload) {
            expected_hash = (expected_hash ^ b) * 16777619u;
        }

        std::atomic<size_t> total{0};
        unsigned int received_hash = 2166136261u;
        std::thread thr([&]() {
            for (int i = 0; i < 100 && !server.Accept(); ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            char buffer[8192];
            int reads = 0;
            while (total < payload_size && server.WaitReadable(2000)) {
                const int bytes_read = server.Receive(buffer, sizeof(buffer));
                if (bytes_read > 0) {
                    for (int i = 0; i < bytes_read; ++i) {
                        received_hash = (received_hash ^ (unsigned char) buffer[i]) * 16777619u;
                    }
                    total += (size_t) bytes_read;
                    // throttle the reader so the small send buffer actually fills up
                    if (++reads % 16 == 0) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    }
                } else if (bytes_read == 0 || bytes_read == -2) {
                    break;
                }
            }
        });
        require(client.Connect(Address(127, 0, 0, 1, server.local_addr().port()), 2000));

        require(client.SendAll(payload.data(), payload_size, 15000));
        thr.join();
        require(total == payload_size);
        require(received_hash == expected_hash);
    }
    { // RecvExactly reassembles a stream that arrives in single-byte pieces
        TCPSocket server, client;
        require_nothrow(server.Open(Address(127, 0, 0, 1, 0), 0));
        require(server.Listen());
        require_nothrow(client.Open(Address(127, 0, 0, 1, 0), 0));

        const size_t payload_size = 4096;
        std::vector<unsigned char> payload(payload_size);
        for (size_t i = 0; i < payload_size; ++i) {
            payload[i] = (unsigned char) (i * 7u + 3u);
        }

        std::thread thr([&]() {
            for (int i = 0; i < 100 && !server.Accept(); ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            for (size_t i = 0; i < payload_size; ++i) {
                for (int r = 0; r < 200 && !server.Send(payload.data() + i, 1); ++r) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(25));
                }
            }
        });
        require(client.Connect(Address(127, 0, 0, 1, server.local_addr().port()), 2000));

        std::vector<unsigned char> received(payload_size, 0);
        require(client.RecvExactly(received.data(), payload_size, 10000));
        thr.join();
        require(memcmp(received.data(), payload.data(), payload_size) == 0);
    }
    { // orderly close: peer Shutdown + Close reads as Receive() == 0
        TCPSocket server, client;
        require_nothrow(server.Open(Address(127, 0, 0, 1, 0), 0));
        require(server.Listen());
        require_nothrow(client.Open(Address(127, 0, 0, 1, 0), 0));

        std::thread thr([&]() {
            for (int i = 0; i < 100 && !server.Accept(); ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            server.Shutdown();
            server.Close();
        });
        require(client.Connect(Address(127, 0, 0, 1, server.local_addr().port()), 2000));
        thr.join();

        require(client.WaitReadable(2000));
        char buffer[16];
        require(client.Receive(buffer, sizeof(buffer)) == 0);
        require(client.Receive(buffer, sizeof(buffer)) == 0);
        require(!client.RecvExactly(buffer, sizeof(buffer), 500));
    }
    { // peer fully closed: writes eventually fail with an error, not silently succeed
        TCPSocket server, client;
        require_nothrow(server.Open(Address(127, 0, 0, 1, 0), 0));
        require(server.Listen());
        require_nothrow(client.Open(Address(127, 0, 0, 1, 0), 0));

        std::thread thr([&]() {
            for (int i = 0; i < 100 && !server.Accept(); ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            server.Shutdown();
            server.Close();
            server.CloseClient();
        });
        require(client.Connect(Address(127, 0, 0, 1, server.local_addr().port()), 2000));
        thr.join();

        const char chunk[] = "ping";
        bool send_failed = false;
        for (int i = 0; i < 100 && !send_failed; ++i) {
            send_failed = !client.Send(chunk, sizeof(chunk));
            if (!send_failed) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        }
        require(send_failed);
        require(client.last_error() != 0);
    }
    { // a connect to an address that never answers is bounded by the timeout
      // argument, not by the OS connect timeout
        TCPSocket s;
        require_nothrow(s.Open(Address(127, 0, 0, 1, 0), 0));
        const auto t0 = std::chrono::steady_clock::now();
        const bool connected = s.Connect(Address(10, 255, 255, 1, 9), 300);
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - t0).count();
        require(!connected);
        require(!s.connected());
        require(elapsed < 1500); // the OS default is far longer; the wait must be ours
    }
    { // WaitWritable on an established connection is granted without waiting
        TCPSocket server, client;
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
        require(client.IsOpen()); // connected() only reports an accepted connection

        const auto t0 = std::chrono::steady_clock::now();
        require(client.WaitWritable(2000));
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - t0).count();
        require(elapsed < 100); // send space is available: no reason to wait
    }
    { // SetBlocking applies to the accepted connection, not only to the listener
        TCPSocket server, client;
        require_nothrow(server.Open(Address(127, 0, 0, 1, 0), 0));
        require(server.Listen());
        require_nothrow(client.Open(Address(127, 0, 0, 1, 0), 0));

        std::atomic<bool> accepted{false};
        std::thread accept_thr([&]() {
            for (int i = 0; i < 100 && !server.Accept(false); ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
            accepted = server.connected();
        });
        require(client.Connect(Address(127, 0, 0, 1, server.local_addr().port()), 2000));
        for (int i = 0; i < 100 && !accepted; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        accept_thr.join();
        require(accepted); // accept happens first: it decides the connection's mode

        TCPSocket conn = TCPSocket::PassClientConnection(server);
        require(conn.connected());

        char buf[8];
        require(conn.Receive(buf, sizeof(buf)) == -1); // still non-blocking here

        require(conn.SetBlocking(true));
        std::atomic<bool> sent{false};
        std::thread thr([&]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            sent = client.Send("abc", 3);
        });
        const auto t0 = std::chrono::steady_clock::now();
        const int got = conn.Receive(buf, sizeof(buf));
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - t0).count();
        thr.join();
        require(sent);
        require(got == 3); // the call waited for the delayed send instead of failing
        require(elapsed >= 100);
        require(memcmp(buf, "abc", 3) == 0);
    }
    { // PassClientConnection hands over the connection exactly once: destroying the
      // moved-to socket leaves the listener intact and usable
        TCPSocket server, client;
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

        TCPSocket conn = TCPSocket::PassClientConnection(server);
        require(conn.connected());
        require(!server.connected()); // the listener no longer references that handle

        const char msg[] = "hello";
        require(conn.Send(msg, sizeof(msg)));
        char echo[sizeof(msg)];
        require(client.RecvExactly(echo, sizeof(echo), 2000));
        require(memcmp(echo, msg, sizeof(msg)) == 0);

        conn.CloseClient(); // exactly one closesocket for the connection handle

        TCPSocket client2;
        require_nothrow(client2.Open(Address(127, 0, 0, 1, 0), 0));
        std::thread thr2([&]() {
            for (int i = 0; i < 100 && !server.Accept(); ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        });
        require(client2.Connect(Address(127, 0, 0, 1, server.local_addr().port()), 2000));
        thr2.join();
        require(server.connected()); // the same listener accepted the next client
    }
    { // open/close cycles must not leak socket handles
#ifdef _WIN32
        DWORD before = 0, after = 0;
        require(GetProcessHandleCount(GetCurrentProcess(), &before) != 0);
#else
        // no /proc on macOS: probe the descriptor table instead, bounded by RLIMIT_NOFILE
        auto open_fds = []() {
            int count = 0;
            long limit = sysconf(_SC_OPEN_MAX); // -1 when the limit is unlimited
            if (limit <= 0 || limit > 8192) {
                limit = 8192;
            }
            for (int fd = 0; fd < int(limit); ++fd) {
                if (::fcntl(fd, F_GETFD) != -1) {
                    ++count;
                }
            }
            return count;
        };
        const int before = open_fds();
#endif

        for (int i = 0; i < 2000; ++i) {
            TCPSocket s;
            require_nothrow(s.Open(Address(127, 0, 0, 1, 0), 0));
            s.Close();
        }
        for (int i = 0; i < 200; ++i) {
            TCPSocket moved;
            require_nothrow(moved.Open(Address(127, 0, 0, 1, 0), 0));
            TCPSocket target;
            target = std::move(moved); // the moved-from socket must not close twice
        }

#ifdef _WIN32
        require(GetProcessHandleCount(GetCurrentProcess(), &after) != 0);
        require(after <= before + 8); // creation and destruction stay in balance
#else
        require(open_fds() <= before + 8);
#endif
    }

    printf("OK\n");
}
