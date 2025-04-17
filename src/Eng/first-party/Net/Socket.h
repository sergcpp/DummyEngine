#pragma once

#include <memory>

#include "Address.h"

namespace Net {
    class SocketContext;

    class UDPSocket {
        std::shared_ptr<SocketContext> context_;
        uint64_t handle_;
        Address local_addr_;
    public:
        UDPSocket();

        ~UDPSocket();

        [[nodiscard]] bool IsOpen() const { return handle_ != 0; }

        [[nodiscard]] Address local_addr() const { return local_addr_; }

        void Open(unsigned short port, bool reuse_addr = true);

        void Close();

        bool Send(const Address &destination, const void *data, int size) const;

        int Receive(Address &sender, void *data, int size) const;

        bool JoinMulticast(const Address &addr);
        bool DropMulticast(const Address &addr);

        bool SetBlocking(bool is_blocking) const;
    };

    class TCPSocket {
        std::shared_ptr<SocketContext> context_;
        uint64_t handle_, connection_;
        Address remote_addr_;
    public:
        TCPSocket();

        TCPSocket(TCPSocket &&rhs) noexcept;

        ~TCPSocket();

        TCPSocket(const TCPSocket &rhs) = delete;

        TCPSocket &operator=(const TCPSocket &rhs) = delete;

        TCPSocket &operator=(TCPSocket &&rhs) noexcept;

        static TCPSocket PassClientConnection(TCPSocket &rhs) {
            TCPSocket ret;
            ret.handle_ = 0;
            ret.connection_ = rhs.connection_;
            ret.remote_addr_ = rhs.remote_addr_;
            rhs.Listen();
            return ret;
        }

        [[nodiscard]] bool IsOpen() const { return handle_ != 0; }

        [[nodiscard]] bool connected() const { return connection_ != 0; }

        [[nodiscard]] Address remote_addr() const { return remote_addr_; }

        void Open(unsigned short port, bool reuse_addr = true);

        void Close();

        void CloseClient();

        bool Listen();

        bool Accept(bool is_blocking = false);

        bool Connect(const Address &dest);

        bool Send(const void *data, int size);

        int Receive(void *data, int size);

        bool SetBlocking(bool is_blocking);

        void WaitClientComplete(int t_ms);
    };

    bool SetBlocking(uint64_t sock, bool is_blocking);
}

