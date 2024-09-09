#pragma once

#include "Address.h"

namespace Net {
    class IConnection {
    public:
        virtual void Connect(const Address &address) = 0;

        virtual void Update(float dt_s) = 0;

        virtual void Start(int port) = 0;

        virtual bool SendPacket(const unsigned char data[], int size) = 0;

        virtual int ReceivePacket(unsigned char data[], int size) = 0;

        [[nodiscard]] virtual Address address() const = 0;

        [[nodiscard]] virtual Address local_addr() const = 0;

    protected:
        virtual void OnStart() {}

        virtual void OnStop() {}

        virtual void OnConnect() {}

        virtual void OnDisconnect() {}
    };
}