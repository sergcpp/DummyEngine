#pragma once

#ifdef _WIN32
#include <winsock2.h>
#else
#include <cerrno>
#endif

namespace Net {
#ifdef _WIN32
    using SocketHandle = SOCKET;
    inline constexpr SocketHandle INVALID_SOCK = INVALID_SOCKET;
#else
    using SocketHandle = int;
    inline constexpr SocketHandle INVALID_SOCK = -1;
#endif

    inline constexpr bool sock_is_open(SocketHandle h) {
        return h != INVALID_SOCK;
    }

    inline int last_socket_error() {
#ifdef _WIN32
        return WSAGetLastError();
#else
        return errno;
#endif
    }

    inline constexpr int would_block_error() {
#ifdef _WIN32
        return WSAEWOULDBLOCK;
#else
        return EWOULDBLOCK;
#endif
    }

    inline constexpr int intr_error() {
#ifdef _WIN32
        return WSAEINTR;
#else
        return EINTR;
#endif
    }

    // non-blocking connect reports "pending" as WSAEWOULDBLOCK on Windows, EINPROGRESS on POSIX
    inline constexpr int in_progress_error() {
#ifdef _WIN32
        return WSAEINPROGRESS;
#else
        return EINPROGRESS;
#endif
    }

    // EWOULDBLOCK and EAGAIN are the same value on all supported POSIX platforms
    inline bool sock_would_block(int err) {
        return err == would_block_error();
    }
}
