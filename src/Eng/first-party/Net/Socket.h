#pragma once

#include <memory>

#include "Address.h"
#include "SocketError.h"

namespace Net {
    class SocketContext;

    class UDPSocket {
        std::shared_ptr<SocketContext> context_;
        SocketHandle handle_ = INVALID_SOCK;
        Address local_addr_;
        mutable int last_error_ = 0;
    public:
        UDPSocket();

        ~UDPSocket();

        [[nodiscard]] bool IsOpen() const { return sock_is_open(handle_); }

        [[nodiscard]] Address local_addr() const { return local_addr_; }

        [[nodiscard]] int last_error() const { return last_error_; }

        void Open(unsigned short port, bool reuse_addr = true);

        // bind an explicit interface (Address' own port field is ignored); IPC use must pass 127.0.0.1
        void Open(const Address &bind_addr, unsigned short port, bool reuse_addr = true);

        void Close();

        bool Send(const Address &destination, const void *data, int size) const;

        // >0 bytes read; 0 = zero-length datagram; -1 = no data pending (would-block); -2 = error, see last_error()
        int Receive(Address &sender, void *data, int size) const;

        bool JoinMulticast(const Address &addr);
        bool DropMulticast(const Address &addr);
        bool JoinMulticast(const Address &group, const Address &interface_addr);
        bool DropMulticast(const Address &group, const Address &interface_addr);

        bool SetBlocking(bool is_blocking) const;
    };

    class TCPSocket {
        std::shared_ptr<SocketContext> context_;
        SocketHandle handle_ = INVALID_SOCK, connection_ = INVALID_SOCK;
        Address remote_addr_, local_addr_;
        int last_error_ = 0;
    public:
        TCPSocket();

        TCPSocket(TCPSocket &&rhs) noexcept;

        ~TCPSocket();

        TCPSocket(const TCPSocket &rhs) = delete;

        TCPSocket &operator=(const TCPSocket &rhs) = delete;

        TCPSocket &operator=(TCPSocket &&rhs) noexcept;

        // moves the connection accepted by Accept() into the returned socket;
        // rhs is left as a fresh listener (its handle_ stays open for Accept again)
        static TCPSocket PassClientConnection(TCPSocket &rhs) {
            TCPSocket ret;
            ret.handle_ = INVALID_SOCK;
            ret.connection_ = rhs.connection_;
            ret.remote_addr_ = rhs.remote_addr_;
            ret.local_addr_ = rhs.local_addr_;
            rhs.connection_ = INVALID_SOCK;
            rhs.remote_addr_ = Address();
            return ret;
        }

        [[nodiscard]] bool IsOpen() const { return sock_is_open(handle_); }

        [[nodiscard]] bool connected() const { return sock_is_open(connection_); }

        [[nodiscard]] SocketHandle socket_handle() const { return handle_; }

        [[nodiscard]] SocketHandle connection_handle() const { return connection_; }

        [[nodiscard]] Address local_addr() const { return local_addr_; }

        [[nodiscard]] Address remote_addr() const { return remote_addr_; }

        [[nodiscard]] int last_error() const { return last_error_; }

        void Open(unsigned short port, bool reuse_addr = true);

        // bind an explicit interface (Address' own port field is ignored); IPC use must pass 127.0.0.1
        void Open(const Address &bind_addr, unsigned short port, bool reuse_addr = true);

        void Close();

        void CloseClient();

        bool Listen();

        bool Accept(bool is_blocking = false);

        // with a timeout_ms > 0 a non-blocking connect is verified within the timeout;
        // 0 waits for the OS connect timeout; returns false when the connection failed
        bool Connect(const Address &dest, int timeout_ms = 0);

        // loops over partial writes; on a non-blocking socket it can still fail with part
        // of the data already sent - use SendAll when the whole block must arrive
        bool Send(const void *data, int size);

        // blocks until every byte is queued, waiting for send-buffer space up to
        // timeout_ms (-1 = forever); false on error or timeout
        bool SendAll(const void *data, size_t size, int timeout_ms = -1);

        // >0 bytes read; 0 = peer closed the connection; -1 = no data pending (would-block); -2 = error, see last_error()
        int Receive(void *data, int size);

        // reads exactly size bytes; false if the peer closed first or timeout_ms elapsed
        bool RecvExactly(void *data, size_t size, int timeout_ms);

        bool SetBlocking(bool is_blocking);

        // negative timeout waits forever
        bool WaitReadable(int timeout_ms);
        bool WaitWritable(int timeout_ms);

        // half-close: makes the peer see an orderly close (Receive() == 0) before the
        // socket is destroyed, so a crash stays distinguishable from a deliberate finish
        void Shutdown(bool how_read = true, bool how_write = true);
    };

    bool SetBlocking(SocketHandle sock, bool is_blocking);
}
