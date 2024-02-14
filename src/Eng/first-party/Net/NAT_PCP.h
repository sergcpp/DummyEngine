#pragma once

#include <cstdint>
#include <cstring>

#include "Address.h"
#include "Socket.h"

namespace Net {
enum class ePCPOpCode { None = -1, Announce = 0, Map, Peer };

enum class ePCPResCode {
    Success = 0,
    UnsuppVersion,
    NotAuthorized,
    MalformedRequest,
    UnsuppOpCode,
    UncuppOption,
    NetworkFailure,
    NoResources,
    UnsuppProtocol,
    UserExQuota,
    CannotProvideExternal,
    AddressMismatch,
    ExcessiveRemotePeers
};

enum class ePCPProto {
    UDP = 17,
    TCP = 6,
};

struct PCPNonce {
    char _[12];

    PCPNonce() { memset(_, 0, sizeof(_)); }

    bool operator==(const PCPNonce &rhs) const { return memcmp(this, &rhs, sizeof(PCPNonce)) == 0; }
};

bool GenPCPNonce(void *buf, int len);

class PCPRequest {
    ePCPOpCode opcode_;
    uint32_t lifetime_;
    Net::Address client_address_, external_address_, remote_address_;
    PCPNonce nonce_;
    ePCPProto proto_;
    uint16_t internal_port_, external_port_;
    uint16_t remote_port_;

  public:
    PCPRequest() : opcode_(ePCPOpCode::None) {}

    ePCPOpCode opcode() const { return opcode_; }

    uint32_t lifetime() const { return lifetime_; }

    Net::Address client_address() const { return client_address_; }

    Net::Address external_address() const { return external_address_; }

    PCPNonce nonce() const { return nonce_; }

    ePCPProto proto() const { return proto_; }

    uint16_t internal_port() const { return internal_port_; }

    uint16_t external_port() const { return external_port_; }

    uint16_t remote_port() const { return remote_port_; }

    Net::Address remote_address() const { return remote_address_; }

    void set_external_address(const Address &addr) { external_address_ = addr; }

    void MakeAnnounceRequest(const Address &client_address) {
        opcode_ = ePCPOpCode::Announce;
        lifetime_ = 0;
        client_address_ = client_address;
    }

    void MakeMapRequest(ePCPProto proto, uint16_t internal_port, uint16_t external_port, uint32_t lifetime,
                        const Address &client_address, const PCPNonce &nonce) {
        opcode_ = ePCPOpCode::Map;
        proto_ = proto;
        internal_port_ = internal_port;
        external_port_ = external_port;
        lifetime_ = lifetime;
        client_address_ = client_address;
        nonce_ = nonce;
    }

    void MakePeerRequest(ePCPProto proto, uint16_t internal_port, uint16_t external_port, uint32_t lifetime,
                         const Address &external_address, uint16_t remote_port, const Address &remote_address,
                         const PCPNonce &nonce) {
        opcode_ = ePCPOpCode::Peer;
        proto_ = proto;
        internal_port_ = internal_port;
        external_port_ = external_port;
        lifetime_ = lifetime;
        external_address_ = external_address;
        remote_port_ = remote_port;
        remote_address_ = remote_address;
        nonce_ = nonce;
    }

    bool Read(const void *buf, int size);

    int Write(void *buf, int size) const;
};

class PCPResponse {
    ePCPOpCode opcode_;
    ePCPResCode res_code_;
    uint32_t lifetime_;
    uint32_t time_;
    PCPNonce nonce_;
    ePCPProto proto_;
    Address external_address_, remote_address_;
    uint16_t internal_port_, external_port_, remote_port_;

  public:
    PCPResponse() : opcode_(ePCPOpCode::None) {}

    ePCPOpCode opcode() const { return opcode_; }

    ePCPResCode res_code() const { return res_code_; }

    uint32_t lifetime() const { return lifetime_; }

    uint32_t time() const { return time_; }

    Address external_address() const { return external_address_; }

    PCPNonce nonce() const { return nonce_; }

    ePCPProto proto() const { return proto_; }

    uint16_t internal_port() const { return internal_port_; }

    uint16_t external_port() const { return external_port_; }

    void MakeAnnounceResponse(ePCPResCode res_code) {
        opcode_ = ePCPOpCode::Announce;
        res_code_ = res_code;
        lifetime_ = 0;
    }

    void MakeMapResponse(ePCPProto proto, ePCPResCode res_code, uint16_t internal_port, uint16_t external_port,
                         uint32_t lifetime, uint32_t time, const Address &external_address, const PCPNonce &nonce) {
        opcode_ = ePCPOpCode::Map;
        res_code_ = res_code;
        internal_port_ = internal_port;
        external_port_ = external_port;
        lifetime_ = lifetime;
        time_ = time;
        external_address_ = external_address;
        nonce_ = nonce;
        proto_ = proto;
    }

    void MakePeerResponse(ePCPProto proto, ePCPResCode res_code, uint16_t internal_port, uint16_t external_port,
                          uint32_t lifetime, uint32_t time, const Address &external_address, uint16_t remote_port,
                          const Address &remote_address, const PCPNonce &nonce) {
        opcode_ = ePCPOpCode::Peer;
        res_code_ = res_code;
        internal_port_ = internal_port;
        external_port_ = external_port;
        lifetime_ = lifetime;
        time_ = time;
        external_address_ = external_address;
        remote_port_ = remote_port;
        remote_address_ = remote_address;
        nonce_ = nonce;
        proto_ = proto;
    }

    bool Read(const void *buf, int size);

    int Write(void *buf, int size) const;
};

class PCPSession {
  public:
    enum class eState {
        RequestMapping,
        IdleMapped,
        IdleFailed,
    };

    PCPSession(ePCPProto proto, const Address &pcp_server, uint16_t internal_port, uint16_t external_port,
               unsigned int lifetime = 7200)
        : state_(eState::RequestMapping), proto_(proto), err_code_(ePCPResCode::Success), internal_port_(internal_port),
          external_port_(external_port), lifetime_(lifetime), main_timer_(0), request_timer_(0), request_counter_(0),
          pcp_server_(pcp_server) {
        GenPCPNonce(&nonce_, sizeof(PCPNonce));
        rt_ = (1 + RAND()) * IRT;

        sock_.Open(0);
    }

    Address local_addr() const { return sock_.local_addr(); }

    eState state() const { return state_; }

    ePCPResCode err_code() const { return err_code_; }

    void Update(unsigned int dt_ms);

  private:
    eState state_;

    ePCPProto proto_;
    ePCPResCode err_code_;
    uint16_t internal_port_, external_port_;
    PCPNonce nonce_;
    unsigned int lifetime_;
    unsigned int main_timer_;
    unsigned int request_timer_;
    float rt_;

    unsigned int request_counter_;

    Address external_addr_;
    unsigned int mapped_time_;

    Address pcp_server_;
    UDPSocket sock_;

    static const unsigned int IRT = 3;
    static const unsigned int MRC = 0; // should be 0
    static const unsigned int MRT = 1024;
    static const unsigned int MRD = 0; // should be 0

    static float RAND() { return -0.1f + rand() / (RAND_MAX / 0.2f); }

    static float RT(float rt);
};
} // namespace Net
