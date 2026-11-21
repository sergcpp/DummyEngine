#pragma warning(disable : 4996)

#include "Socket.h"

#include <cassert>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <stdexcept>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2ipdef.h>
#include <Ws2tcpip.h>
#endif
#if defined(__linux__) || defined(__APPLE__)

#include <fcntl.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>

#endif

class Net::SocketContext {
public:
    SocketContext() {   // NOLINT
        //srand((unsigned int)time(nullptr));
#ifdef _WIN32
        WSADATA WsaData;
        const int res = WSAStartup(MAKEWORD(2, 2), &WsaData);
        if (res != 0) {
            throw std::runtime_error("WSAStartup failed!");
        }
#endif
    }

    ~SocketContext() {  // NOLINT
#ifdef _WIN32
        WSACleanup();
#endif
    }
};

#ifdef __ANDROID__
extern unsigned int g_android_local_ip_address;
#endif

namespace {
    // function-local static: thread-safe init (C++11) and the context outlives every
    // socket, so WSACleanup can never run while another thread still uses sockets
    std::shared_ptr<Net::SocketContext> shared_context() {
        static std::shared_ptr<Net::SocketContext> ctx = std::make_shared<Net::SocketContext>();
        return ctx;
    }

    // POSIX raises SIGPIPE when writing to a broken connection; suppress it so the
    // send calls just report EPIPE (macOS uses SO_NOSIGPIPE on the socket instead)
    constexpr int send_flags() {
#if defined(_WIN32) || defined(__APPLE__)
        return 0;
#else
        return MSG_NOSIGNAL;
#endif
    }

    unsigned int GetLocalAddr() {
        unsigned int local_addr = 0;
#ifdef __ANDROID__
        local_addr = ntohl(g_android_local_ip_address);
#else
        // a UDP connect() sends no traffic: it only makes the kernel pick the source
        // address for that route, which getsockname then reports - this replaces the
        // gethostbyname() guess (unreliable and not thread-safe)
        const Net::SocketHandle probe = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (Net::sock_is_open(probe)) {
            sockaddr_in dst; // NOLINT
            memset(&dst, 0, sizeof(dst));
            dst.sin_family = AF_INET;
            dst.sin_port = htons(53);
            dst.sin_addr.s_addr = inet_addr("8.8.8.8");
            sockaddr_in src; // NOLINT
            socklen_t src_len = sizeof(src);
            if (::connect(probe, (const sockaddr *) &dst, sizeof(dst)) == 0 &&
                getsockname(probe, (sockaddr *) &src, &src_len) == 0) {
                local_addr = ntohl(src.sin_addr.s_addr);
            }
    #ifdef _WIN32
            closesocket(probe);
    #else
            close(probe);
    #endif
        }
#endif
        return local_addr;
    }
}

Net::UDPSocket::UDPSocket() {
    context_ = shared_context();
}

Net::UDPSocket::~UDPSocket() {
    Close();
}

void Net::UDPSocket::Open(unsigned short port, bool reuse_addr) {
    Open(Address(), port, reuse_addr);
}

void Net::UDPSocket::Open(const Address &bind_addr, unsigned short port, bool reuse_addr) {
    handle_ = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (!sock_is_open(handle_)) {
        handle_ = INVALID_SOCK;
        throw std::runtime_error("Cannot create socket.");
    }

    if (reuse_addr) {
        int one = 1;
        setsockopt(handle_, SOL_SOCKET, SO_REUSEADDR, (char *) &one, sizeof(int));
    }

    sockaddr_in address;    // NOLINT
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(bind_addr.address());

    address.sin_port = htons(port);

    if (::bind(handle_, (const sockaddr *) &address, sizeof(sockaddr_in)) < 0) {
        Close();
        throw std::runtime_error("Cannot bind socket.");
    }

    struct sockaddr_in sin; // NOLINT
    socklen_t addrlen = sizeof(sin);
    if (getsockname(handle_, (struct sockaddr *) &sin, &addrlen) == 0 &&
        sin.sin_family == AF_INET &&
        addrlen == sizeof(sin)) {
        // for an INADDR_ANY bind the default-route probe fills in the real address
        const unsigned int bound_addr = ntohl(sin.sin_addr.s_addr);
        const unsigned int local_addr = bound_addr != 0 ? bound_addr : GetLocalAddr();
        local_addr_ = Address(local_addr, ntohs(sin.sin_port));
    }

    SetBlocking(false);
}

void Net::UDPSocket::Close() {
    if (sock_is_open(handle_)) {
#ifdef _WIN32
        closesocket(handle_);
#else
        close(handle_);
#endif
        handle_ = INVALID_SOCK;
    }
}

bool Net::UDPSocket::Send(const Address &destination, const void *data, int size) const {
    assert(data);
    assert(size > 0);

    if (!sock_is_open(handle_)) {
        return false;
    }

    sockaddr_in address;    // NOLINT
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(destination.address());
    address.sin_port = htons((unsigned short) destination.port());

    const int sent_bytes = int(sendto(handle_, (const char *) data, size, 0, (sockaddr *) &address, sizeof(sockaddr_in)));
    if (sent_bytes != size) {
        last_error_ = last_socket_error();
        return false;
    }

    return true;
}

int Net::UDPSocket::Receive(Address &sender, void *data, const int size) const {
    assert(data);
    assert(size > 0);

    if (!sock_is_open(handle_)) {
        last_error_ = 0;
        return -1;
    }

    sockaddr_in from;   // NOLINT
    socklen_t from_len = sizeof(sockaddr_in);

    const int received_bytes = int(recvfrom(handle_, (char *) data, size, 0, (sockaddr *) &from, &from_len));

    if (received_bytes < 0) {
        last_error_ = last_socket_error();
        return sock_would_block(last_error_) || last_error_ == intr_error() ? -1 : -2;
    }

    last_error_ = 0;

    unsigned int address = ntohl(from.sin_addr.s_addr);
    unsigned short port = ntohs(from.sin_port);

    sender = Address(address, port);

    return received_bytes;
}

bool Net::UDPSocket::JoinMulticast(const Address &addr) {
    return JoinMulticast(addr, local_addr_);
}

bool Net::UDPSocket::DropMulticast(const Address &addr) {
    return DropMulticast(addr, local_addr_);
}

bool Net::UDPSocket::JoinMulticast(const Address &group, const Address &interface_addr) {
    if (!sock_is_open(handle_)) {
        return false;
    }

    struct ip_mreq mreq;    // NOLINT
    mreq.imr_interface.s_addr = htonl(interface_addr.address());
    mreq.imr_multiaddr.s_addr = htonl(group.address());
    return setsockopt(handle_, IPPROTO_IP, IP_ADD_MEMBERSHIP, (const char *) &mreq, sizeof(mreq)) == 0;
}

bool Net::UDPSocket::DropMulticast(const Address &group, const Address &interface_addr) {
    if (!sock_is_open(handle_)) {
        return false;
    }

    struct ip_mreq mreq;    // NOLINT
    mreq.imr_interface.s_addr = htonl(interface_addr.address());
    mreq.imr_multiaddr.s_addr = htonl(group.address());
    return setsockopt(handle_, IPPROTO_IP, IP_DROP_MEMBERSHIP, (const char *) &mreq, sizeof(mreq)) == 0;
}

bool Net::UDPSocket::SetBlocking(bool is_blocking) const {
    return Net::SetBlocking(handle_, is_blocking);
}

Net::TCPSocket::TCPSocket() {
    context_ = shared_context();
}

Net::TCPSocket::TCPSocket(TCPSocket &&rhs) noexcept
        : context_(std::move(rhs.context_)), handle_(rhs.handle_), connection_(rhs.connection_),
          remote_addr_(rhs.remote_addr_), local_addr_(rhs.local_addr_) {
    rhs.handle_ = INVALID_SOCK;
    rhs.connection_ = INVALID_SOCK;
    rhs.remote_addr_ = Address();
    rhs.local_addr_ = Address();
}

Net::TCPSocket::~TCPSocket() {
    Close();
    CloseClient();
}

Net::TCPSocket &Net::TCPSocket::operator=(TCPSocket &&rhs) noexcept {
    context_ = std::move(rhs.context_);
    handle_ = rhs.handle_;
    connection_ = rhs.connection_;
    remote_addr_ = rhs.remote_addr_;
    local_addr_ = rhs.local_addr_;

    rhs.handle_ = INVALID_SOCK;
    rhs.connection_ = INVALID_SOCK;
    rhs.remote_addr_ = Address();
    rhs.local_addr_ = Address();
    return *this;
}

void Net::TCPSocket::Open(unsigned short port, bool reuse_addr) {
    Open(Address(), port, reuse_addr);
}

void Net::TCPSocket::Open(const Address &bind_addr, unsigned short port, bool reuse_addr) {
    handle_ = socket(AF_INET, SOCK_STREAM, 0);
    if (!sock_is_open(handle_)) {
        handle_ = INVALID_SOCK;
        throw std::runtime_error("Cannot create socket.");
    }

    if (reuse_addr) {
        int one = 1;
        setsockopt(handle_, SOL_SOCKET, SO_REUSEADDR, (char *) &one, sizeof(int));
    }

    sockaddr_in address;    // NOLINT
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(bind_addr.address());
    address.sin_port = htons(port);

    if (::bind(handle_, (const sockaddr *) &address, sizeof(sockaddr_in)) < 0) {
        Close();
        throw std::runtime_error("Cannot bind socket.");
    }

    struct sockaddr_in sin; // NOLINT
    socklen_t addrlen = sizeof(sin);
    if (getsockname(handle_, (struct sockaddr *) &sin, &addrlen) == 0 &&
        sin.sin_family == AF_INET &&
        addrlen == sizeof(sin)) {
        // for an INADDR_ANY bind the default-route probe fills in the real address
        const unsigned int bound_addr = ntohl(sin.sin_addr.s_addr);
        const unsigned int local_addr = bound_addr != 0 ? bound_addr : GetLocalAddr();
        local_addr_ = Address(local_addr, ntohs(sin.sin_port));
    }

    SetBlocking(false);
}

void Net::TCPSocket::Close() {
    if (sock_is_open(handle_)) {
#ifdef _WIN32
        closesocket(handle_);
#else
        close(handle_);
#endif
        handle_ = INVALID_SOCK;
    }
}

void Net::TCPSocket::CloseClient() {
    if (sock_is_open(connection_)) {
#ifdef _WIN32
        closesocket(connection_);
#else
        close(connection_);
#endif
        connection_ = INVALID_SOCK;
    }
}

bool Net::TCPSocket::Listen() {
    if (!sock_is_open(handle_)) {
        return false;
    }
    connection_ = INVALID_SOCK;
    return listen(handle_, 1024) == 0;
}

bool Net::TCPSocket::Accept(bool is_blocking) {
    if (!sock_is_open(handle_)) {
        return false;
    }

    sockaddr_in from;   // NOLINT
    socklen_t from_len = sizeof(from);

    connection_ = accept(handle_, (struct sockaddr *) &from, &from_len);
    if (!sock_is_open(connection_)) {
        last_error_ = last_socket_error();
        connection_ = INVALID_SOCK;
        return false;
    }

    int one = 1;
    setsockopt(connection_, IPPROTO_TCP, TCP_NODELAY, (const char *) &one, sizeof(one));
#ifdef __APPLE__
    setsockopt(connection_, SOL_SOCKET, SO_NOSIGPIPE, (const char *) &one, sizeof(one));
#endif

    Net::SetBlocking(connection_, is_blocking);

    unsigned int address = ntohl(from.sin_addr.s_addr);
    unsigned int port = ntohs(from.sin_port);
    remote_addr_ = Address(address, port);

    return true;
}

bool Net::TCPSocket::Connect(const Address &dest, int timeout_ms) {
    if (!sock_is_open(handle_)) {
        last_error_ = 0;
        return false;
    }

    sockaddr_in address;    // NOLINT
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(dest.address());
    address.sin_port = htons((unsigned short) dest.port());

    if (timeout_ms > 0) {
        SetBlocking(false);
        if (connect(handle_, (struct sockaddr *) &address, sizeof(sockaddr_in)) < 0) {
            const int err = last_socket_error();
            if (err != would_block_error() && err != in_progress_error()) {
                last_error_ = err;
                return false;
            }
        }
        if (!WaitWritable(timeout_ms)) {
            const int err = last_socket_error();
            last_error_ = err != 0 ? err : intr_error();
            return false;
        }
    } else {
        SetBlocking(true);
        if (connect(handle_, (struct sockaddr *) &address, sizeof(sockaddr_in)) < 0) {
            last_error_ = last_socket_error();
            return false;
        }
    }

    int err = 0;
    socklen_t len = sizeof(err);
    if (getsockopt(handle_, SOL_SOCKET, SO_ERROR, (char *) &err, &len) != 0 || err != 0) {
        last_error_ = err != 0 ? err : last_socket_error();
        return false;
    }

    int one = 1;
    setsockopt(handle_, IPPROTO_TCP, TCP_NODELAY, (const char *) &one, sizeof(one));
#ifdef __APPLE__
    setsockopt(handle_, SOL_SOCKET, SO_NOSIGPIPE, (const char *) &one, sizeof(one));
#endif

    SetBlocking(false);
    last_error_ = 0;
    return true;
}

bool Net::TCPSocket::Send(const void *data, int size) {
    assert(data);
    assert(size > 0);

    const SocketHandle dst = sock_is_open(connection_) ? connection_ : handle_;
    if (!sock_is_open(dst)) {
        return false;
    }

    const char *ptr = (const char *) data;
    int remaining = size;
    while (remaining > 0) {
        const int sent_bytes = int(send(dst, ptr, (size_t) remaining, send_flags()));
        if (sent_bytes <= 0) {
            last_error_ = last_socket_error();
            return false;
        }
        ptr += sent_bytes;
        remaining -= sent_bytes;
    }

    last_error_ = 0;
    return true;
}

bool Net::TCPSocket::SendAll(const void *data, size_t size, int timeout_ms) {
    const SocketHandle dst = sock_is_open(connection_) ? connection_ : handle_;
    if (!sock_is_open(dst)) {
        return false;
    }

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    const char *ptr = (const char *) data;
    size_t remaining = size;
    while (remaining > 0) {
        const int sent_bytes = int(send(dst, ptr, int(remaining), send_flags()));
        if (sent_bytes > 0) {
            ptr += sent_bytes;
            remaining -= (size_t) sent_bytes;
            continue;
        }
        // Windows reports a full non-blocking send buffer as 0 + WSAEWOULDBLOCK
        const int err = last_socket_error();
        if (err != 0 && !sock_would_block(err) && err != intr_error()) {
            last_error_ = err;
            return false;
        }
        int wait_ms = -1;
        if (timeout_ms >= 0) {
            const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(
                deadline - std::chrono::steady_clock::now()).count();
            if (left <= 0) {
                last_error_ = would_block_error();
                return false;
            }
            wait_ms = int(left);
        }
        if (!WaitWritable(wait_ms)) {
            last_error_ = would_block_error();
            return false;
        }
    }

    last_error_ = 0;
    return true;
}

int Net::TCPSocket::Receive(void *data, int size) {
    assert(data);
    assert(size > 0);

    const SocketHandle src = sock_is_open(connection_) ? connection_ : handle_;
    if (!sock_is_open(src)) {
        last_error_ = 0;
        return -2;
    }

    const int received_bytes = int(recv(src, (char *) data, (size_t) size, 0));

    if (received_bytes == 0) {
        return 0; // orderly peer shutdown
    }
    if (received_bytes < 0) {
        last_error_ = last_socket_error();
        return sock_would_block(last_error_) || last_error_ == intr_error() ? -1 : -2;
    }

    last_error_ = 0;
    return received_bytes;
}

bool Net::TCPSocket::RecvExactly(void *data, size_t size, int timeout_ms) {
    assert(data);

    const SocketHandle src = sock_is_open(connection_) ? connection_ : handle_;
    if (!sock_is_open(src)) {
        last_error_ = 0;
        return false;
    }

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    char *ptr = (char *) data;
    size_t remaining = size;
    while (remaining > 0) {
        const int received_bytes = int(recv(src, ptr, int(remaining), 0));
        if (received_bytes > 0) {
            ptr += received_bytes;
            remaining -= (size_t) received_bytes;
            continue;
        }
        if (received_bytes == 0) {
            last_error_ = 0; // peer closed before size bytes arrived
            return false;
        }
        const int err = last_socket_error();
        if (!sock_would_block(err) && err != intr_error()) {
            last_error_ = err;
            return false;
        }
        int wait_ms = -1;
        if (timeout_ms >= 0) {
            const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(
                deadline - std::chrono::steady_clock::now()).count();
            if (left <= 0) {
                last_error_ = would_block_error();
                return false;
            }
            wait_ms = int(left);
        }
        if (!WaitReadable(wait_ms)) {
            last_error_ = would_block_error();
            return false;
        }
    }

    last_error_ = 0;
    return true;
}

bool Net::TCPSocket::SetBlocking(bool is_blocking) {
    const SocketHandle sock = sock_is_open(connection_) ? connection_ : handle_;
    return Net::SetBlocking(sock, is_blocking);
}

bool Net::TCPSocket::WaitReadable(int timeout_ms) {
    const SocketHandle sock = sock_is_open(connection_) ? connection_ : handle_;
    if (!sock_is_open(sock)) {
        last_error_ = 0;
        return false;
    }

    fd_set read_set;
    FD_ZERO(&read_set);
    FD_SET(sock, &read_set);

    timeval tv;   // NOLINT
    timeval *timeout = nullptr;
    if (timeout_ms >= 0) {
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        timeout = &tv;
    }

    const int ready = select(int(sock) + 1, &read_set, nullptr, nullptr, timeout);
    if (ready <= 0) {
        last_error_ = ready < 0 ? last_socket_error() : 0;
        return false;
    }
    return true;
}

bool Net::TCPSocket::WaitWritable(int timeout_ms) {
    const SocketHandle sock = sock_is_open(connection_) ? connection_ : handle_;
    if (!sock_is_open(sock)) {
        last_error_ = 0;
        return false;
    }

    fd_set write_set;
    FD_ZERO(&write_set);
    FD_SET(sock, &write_set);

    timeval tv;   // NOLINT
    timeval *timeout = nullptr;
    if (timeout_ms >= 0) {
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        timeout = &tv;
    }

    const int ready = select(int(sock) + 1, nullptr, &write_set, nullptr, timeout);
    if (ready <= 0) {
        last_error_ = ready < 0 ? last_socket_error() : 0;
        return false;
    }
    return true;
}

void Net::TCPSocket::Shutdown(bool how_read, bool how_write) {
    const SocketHandle sock = sock_is_open(connection_) ? connection_ : handle_;
    if (!sock_is_open(sock) || (!how_read && !how_write)) {
        return;
    }

    int how;
#ifdef _WIN32
    how = how_read && how_write ? SD_BOTH : (how_read ? SD_RECEIVE : SD_SEND);
#else
    how = how_read && how_write ? SHUT_RDWR : (how_read ? SHUT_RD : SHUT_WR);
#endif
    if (shutdown(sock, how) != 0) {
        last_error_ = last_socket_error();
    } else {
        last_error_ = 0;
    }
}

bool Net::SetBlocking(SocketHandle sock, bool is_blocking) {
    bool ret = true;
#ifdef _WIN32
    /// @note windows sockets are created in blocking mode by default
    // currently on windows, there is no easy way to obtain the socket's current blocking mode since WSAIsBlocking was deprecated
    u_long non_blocking = is_blocking ? 0 : 1;
    ret = NO_ERROR == ioctlsocket(sock, FIONBIO, &non_blocking);
#else
    const int flags = fcntl(sock, F_GETFL, 0);
    if ((flags & O_NONBLOCK) && !is_blocking) { return ret; }
    if (!(flags & O_NONBLOCK) && is_blocking) { return ret; }
    ret = (0 == fcntl(sock, F_SETFL, is_blocking ? flags ^ O_NONBLOCK : flags | O_NONBLOCK));
#endif
    return ret;
}
