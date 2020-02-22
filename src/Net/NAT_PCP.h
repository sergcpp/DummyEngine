#pragma once

#include <cstdint>
#include <cstring>

#include "Address.h"
#include "Socket.h"

namespace Net {
    enum PCPOpCode {
        OP_NONE = -1,
        OP_ANNOUNCE = 0,
        OP_MAP,
        OP_PEER
    };

    enum PCPResCode {
        PCP_RES_SUCCESS = 0,
        PCP_RES_UNSUPP_VERSION,
        PCP_RES_NOT_AUTHORIZED,
        PCP_RES_MALFORMED_REQUEST,
        PCP_RES_UNSUPP_OPCODE,
        PCP_RES_UNSUPP_OPTION,
        PCP_RES_NETWORK_FAILURE,
        PCP_RES_NO_RESOURCES,
        PCP_RES_UNSUPP_PROTOCOL,
        PCP_RES_USER_EX_QUOTA,
        PCP_RES_CANNOT_PROVIDE_EXTERNAL,
        PCP_RES_ADDRESS_MISMATCH,
        PCP_RES_EXCESSIVE_REMOTE_PEERS
    };

    enum PCPProto {
        PCP_UDP = 17,
        PCP_TCP = 6,
    };

    struct PCPNonce {
        char _[12];

        PCPNonce() {
            memset(_, 0, sizeof(_));
        }

        bool operator==(const PCPNonce &rhs) const {
            return memcmp(this, &rhs, sizeof(PCPNonce)) == 0;
        }
    };

    bool GenPCPNonce(void *buf, int len);

    class PCPRequest {
        PCPOpCode opcode_;
        uint32_t lifetime_;
        Net::Address client_address_, external_address_, remote_address_;
        PCPNonce nonce_;
        PCPProto proto_;
        uint16_t internal_port_, external_port_;
        uint16_t remote_port_;

    public:
        PCPRequest() : opcode_(OP_NONE) {}

        PCPOpCode opcode() const { return opcode_; }
        uint32_t lifetime() const { return lifetime_; }
        Net::Address client_address() const { return client_address_; }
        Net::Address external_address() const { return external_address_; }
        PCPNonce nonce() const { return nonce_; }
        PCPProto proto() const { return proto_; }
        uint16_t internal_port() const { return internal_port_; }
        uint16_t external_port() const { return external_port_; }
        uint16_t remote_port() const { return remote_port_; }
        Net::Address remote_address() const { return remote_address_; }

        void set_external_address(const Address &addr) {
            external_address_ = addr;
        }

        void MakeAnnounceRequest(const Address &client_address) {
            opcode_         = OP_ANNOUNCE;
            lifetime_       = 0;
            client_address_ = client_address;
        }

        void MakeMapRequest(
                PCPProto proto, uint16_t internal_port, uint16_t external_port, uint32_t lifetime,
                const Address &client_address, const PCPNonce &nonce) {
            opcode_         = OP_MAP;
            proto_          = proto;
            internal_port_  = internal_port;
            external_port_  = external_port;
            lifetime_       = lifetime;
            client_address_ = client_address;
            nonce_          = nonce;
        }

        void MakePeerRequest(
                PCPProto proto, uint16_t internal_port, uint16_t external_port, uint32_t lifetime,
                const Address &external_address, uint16_t remote_port, const Address &remote_address,
                const PCPNonce &nonce) {
            opcode_             = OP_PEER;
            proto_              = proto;
            internal_port_      = internal_port;
            external_port_      = external_port;
            lifetime_           = lifetime;
            external_address_   = external_address;
            remote_port_        = remote_port;
            remote_address_     = remote_address;
            nonce_              = nonce;
        }

        bool Read(const void *buf, int size);
        int Write(void *buf, int size) const;
    };

    class PCPResponse {
        PCPOpCode opcode_;
        PCPResCode res_code_;
        uint32_t lifetime_;
        uint32_t time_;
        PCPNonce nonce_;
        PCPProto proto_;
        Address external_address_, remote_address_;
        uint16_t internal_port_, external_port_, remote_port_;
    public:
        PCPResponse() : opcode_(OP_NONE) {}

        PCPOpCode opcode() const { return opcode_; }
        PCPResCode res_code() const { return res_code_; }
        uint32_t lifetime() const { return lifetime_; }
        uint32_t time() const { return time_; }
        Address external_address() const { return external_address_; }
        PCPNonce nonce() const { return nonce_; }
        PCPProto proto() const { return proto_; }
        uint16_t internal_port() const { return internal_port_; }
        uint16_t external_port() const { return external_port_; }

        void MakeAnnounceResponse(PCPResCode res_code) {
            opcode_     = OP_ANNOUNCE;
            res_code_   = res_code;
            lifetime_   = 0;
        }

        void MakeMapResponse(
                PCPProto proto, PCPResCode res_code, uint16_t internal_port, uint16_t external_port, uint32_t lifetime,
                uint32_t time, const Address &external_address, const PCPNonce &nonce) {
            opcode_             = OP_MAP;
            res_code_           = res_code;
            internal_port_      = internal_port;
            external_port_      = external_port;
            lifetime_           = lifetime;
            time_               = time;
            external_address_   = external_address;
            nonce_              = nonce;
            proto_              = proto;
        }

        void MakePeerResponse(
                PCPProto proto, PCPResCode res_code, uint16_t internal_port, uint16_t external_port, uint32_t lifetime,
                uint32_t time, const Address &external_address, uint16_t remote_port, const Address &remote_address,
                const PCPNonce &nonce) {
            opcode_             = OP_PEER;
            res_code_           = res_code;
            internal_port_      = internal_port;
            external_port_      = external_port;
            lifetime_           = lifetime;
            time_               = time;
            external_address_   = external_address;
            remote_port_        = remote_port;
            remote_address_     = remote_address;
            nonce_              = nonce;
            proto_              = proto;
        }

        bool Read(const void *buf, int size);
        int Write(void *buf, int size) const;
    };

    class PCPSession {
    public:
        enum State {
            REQUEST_MAPPING,
            IDLE_MAPPED,
            IDLE_FAILED,
        };

        PCPSession(
                PCPProto proto, const Address &pcp_server, uint16_t internal_port, uint16_t external_port,
                unsigned int lifetime = 7200)
                : proto_(proto), pcp_server_(pcp_server), internal_port_(internal_port), external_port_(external_port),
                  lifetime_(lifetime), err_code_(PCP_RES_SUCCESS), state_(REQUEST_MAPPING), main_timer_(0),
                  request_timer_(0), request_counter_(0) {
            GenPCPNonce(&nonce_, sizeof(PCPNonce));
            rt_ = (1 + RAND()) * IRT;

            sock_.Open(0);
        }

        Address local_addr() const {
            return sock_.local_addr();
        }

        State state() const { return state_; }
        PCPResCode err_code() const { return err_code_; }

        void Update(unsigned int dt_ms);
    private:
        State state_;

        PCPProto proto_;
        PCPResCode err_code_;
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
        static const unsigned int MRC = 0; //should be 0
        static const unsigned int MRT = 1024;
        static const unsigned int MRD = 0; //should be 0

        static float RAND() {
            return -0.1f + rand() / (RAND_MAX / 0.2f);
        }

        static float RT(float rt);
    };
}
