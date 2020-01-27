#pragma once

#include <functional>

#include "Socket.h"

namespace Net {
    class HTTPRequest;

    class WsConnection {
        TCPSocket   conn_;
        bool        should_mask_;

        void ApplyMask(uint32_t mask, uint8_t *data, int size);
    public:
        WsConnection(TCPSocket &&conn, const HTTPRequest &upgrade_req, bool should_mask = false);
        WsConnection(WsConnection &&rhs);

        Address remote_addr() const {
            return conn_.remote_addr();
        }

        int Receive(void *data, int size);
        bool Send(const void *data, int size);

        std::function<void(WsConnection *)> on_connection_close;
    };
}

