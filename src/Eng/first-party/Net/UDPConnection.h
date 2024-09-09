#pragma once

#include "IConnection.h"
#include "Socket.h"

namespace Net {
const int MAX_PACKET_SIZE = 4096;

class UDPConnection : public IConnection {
  public:
    enum class eMode { None, Client, Server };

    UDPConnection(unsigned int protocol_id, float timeout_s);

    ~UDPConnection();

    [[nodiscard]] const UDPSocket &socket() const { return socket_; }

    void Start(int port) override;

    void Stop();

    void Listen();

    void Connect(const Address &address) override;

    void Update(float dt_s) override;

    bool SendPacket(const unsigned char data[], int size) override;

    int ReceivePacket(unsigned char data[], int size) override;

    [[nodiscard]] bool listening() const { return state_ == eState::Listening; }
    [[nodiscard]] bool connecting() const { return state_ == eState::Connecting; }
    [[nodiscard]] bool connect_failed() const { return state_ == eState::ConnectFailed; }
    [[nodiscard]] bool connected() const { return state_ == eState::Connected; }

    [[nodiscard]] eMode mode() const { return mode_; }

    [[nodiscard]] bool running() const { return running_; }

    [[nodiscard]] Address address() const override { return address_; }

    [[nodiscard]] Address local_addr() const override { return socket_.local_addr(); }

    [[nodiscard]] float timeout_acc() const { return timeout_acc_; }

  private:
    void ClearData() {
        state_ = eState::Disconnected;
        timeout_acc_ = 0;
        address_ = Address();
    }

    enum class eState { Disconnected, Listening, Connecting, ConnectFailed, Connected };

    unsigned int protocol_id_;
    float timeout_s_, timeout_acc_;

    bool running_;
    eMode mode_;
    eState state_;
    UDPSocket socket_;
    Address address_;
};
} // namespace Net
