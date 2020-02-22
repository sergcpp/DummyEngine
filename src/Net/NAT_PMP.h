#pragma once

#include <cstdint>

#ifdef _WIN32
    #include <winsock2.h>
#endif
#if defined(__linux__)
    #include <arpa/inet.h>
#endif

#include <string>

#include "Socket.h"


namespace Net {
    enum PMPOpCode {
        OP_EXTERNAL_IP_REQUEST = 0,
        OP_MAP_UDP_REQUEST,
        OP_MAP_TCP_REQUEST
    };

    enum PMPResCode {
        PMP_RES_SUCCESS = 0,
        PMP_RES_UNSUPPORTED_VERSION,
        PMP_RES_REFUSED,
        PMP_RES_FAILLURE,
        PMP_RES_OUT_OF_RESOURCES
    };

    enum PMPProto {
        PMP_UDP = 0,
        PMP_TCP
    };

    struct PMPRequest {
        uint8_t buf[2];

        PMPRequest(uint8_t vers, uint8_t op) {
            buf[0] = vers;
            buf[1] = op;
        }

        uint8_t vers() const { return buf[0]; }
        uint8_t op() const { return buf[1]; }
    };

    struct PMPExternalIPRequest : public PMPRequest {
        PMPExternalIPRequest() : PMPRequest(0, OP_EXTERNAL_IP_REQUEST) {}
    };

    struct PMPExternalIPResponse {
        uint8_t buf[12];

        PMPExternalIPResponse() {
            buf[0] = 0;
            buf[1] = 128 + OP_EXTERNAL_IP_REQUEST;
        }

        uint8_t vers() const { return buf[0]; }
        uint8_t op() const { return buf[1]; }

        uint16_t res_code() const {
            return ntohs(*(uint16_t*)(&buf[2]));
        }

        uint32_t time() const {
            return ntohl(*(uint32_t*)(&buf[4]));
        }

        uint32_t ip() const {
            return ntohl(*(uint32_t*)(&buf[8]));
        }

        void set_res_code(uint16_t code) {
            *(uint16_t*)(&buf[2]) = htons(code);
        }

        void set_time(uint32_t t) {
            *(uint32_t*)(&buf[4]) = htonl(t);
        }

        void set_ip(uint32_t ip) {
            *(uint32_t*)(&buf[8]) = htonl(ip);
        }
    };

    struct PMPMappingRequest {
        uint8_t buf[12];

        PMPMappingRequest(PMPProto proto, uint16_t internal_port, uint16_t external_port) {
            buf[0] = 0;
            buf[1] = (proto == PMP_UDP) ? (uint8_t)OP_MAP_UDP_REQUEST : (uint8_t)OP_MAP_TCP_REQUEST;

            buf[2] = 0;
            buf[3] = 0;

            set_internal_port(internal_port);
            set_external_port(external_port);
            set_lifetime(7200);
        }

        uint8_t vers() const { return buf[0]; }
        uint8_t op() const { return buf[1]; }

        uint16_t reserved() const {
            return *(uint16_t*)(&buf[2]);
        }

        uint16_t internal_port() const {
            return ntohs(*(uint16_t*)(&buf[4]));
        }

        uint16_t external_port() const {
            return ntohs(*(uint16_t*)(&buf[6]));
        }

        uint32_t lifetime() const {
            return ntohl(*(uint32_t*)(&buf[8]));
        }

        void set_internal_port(uint16_t port) {
            *(uint16_t*)(&buf[4]) = htons(port);
        }

        void set_external_port(uint16_t port) {
            *(uint16_t*)(&buf[6]) = htons(port);
        }

        void set_lifetime(uint32_t t) {
            *(uint32_t*)(&buf[8]) = htonl(t);
        }
    };

    struct PMPMappingResponse {
        uint8_t buf[16];

        PMPMappingResponse() {}
        PMPMappingResponse(PMPProto proto) {
            buf[0] = 0;
            buf[1] = (proto == PMP_UDP) ? (uint8_t)128 + OP_MAP_UDP_REQUEST : (uint8_t)128 + OP_MAP_TCP_REQUEST;
        }

        uint8_t vers() const { return buf[0]; }
        uint8_t op() const { return buf[1]; }

        uint16_t res_code() const {
            return ntohs(*(uint16_t*)(&buf[2]));
        }

        uint32_t time() const {
            return ntohl(*(uint32_t*)(&buf[4]));
        }

        uint16_t internal_port() const {
            return ntohs(*(uint16_t*)(&buf[8]));
        }

        uint16_t external_port() const {
            return ntohs(*(uint16_t*)(&buf[10]));
        }

        uint32_t lifetime() const {
            return ntohl(*(uint32_t*)(&buf[12]));
        }

        void set_res_code(uint16_t code) {
            *(uint16_t*)(&buf[2]) = htons(code);
        }

        void set_time(uint32_t t) {
            *(uint32_t*)(&buf[4]) = htonl(t);
        }

        void set_internal_port(uint16_t port) {
            *(uint16_t*)(&buf[8]) = htons(port);
        }

        void set_external_port(uint16_t port) {
            *(uint16_t*)(&buf[10]) = htons(port);
        }

        void set_lifetime(uint32_t t) {
            *(uint32_t*)(&buf[12]) = htonl(t);
        }
    };

    struct PMPUnsupportedVersionResponse {
        uint8_t buf[8];

        PMPUnsupportedVersionResponse() { }
        PMPUnsupportedVersionResponse(PMPOpCode op) {
            buf[0] = 0;
            buf[1] = (uint8_t)128 + op;
        }

        uint8_t vers() const { return buf[0]; }
        uint8_t op() const { return buf[1]; }

        uint16_t res_code() const {
            return ntohs(*(uint16_t*)(&buf[2]));
        }

        uint32_t time() const {
            return ntohl(*(uint32_t*)(&buf[4]));
        }

        void set_res_code(uint16_t code) {
            *(uint16_t*)(&buf[2]) = htons(code);
        }

        void set_time(uint32_t t) {
            *(uint32_t*)(&buf[4]) = htonl(t);
        }
    };

    static_assert(sizeof(PMPUnsupportedVersionResponse) == 8, "!");

    class PMPSession {
    public:
        enum State {
            RETRIEVE_EXTERNAL_IP,
            CREATE_PORT_MAPPING,
            IDLE_MAPPED,
            IDLE_RETRIEVE_EXTERNAL_IP_ERROR,
            IDLE_CREATE_PORT_MAPPING_ERROR,
            IDLE_UNSUPPORTED,
        };

        PMPSession(PMPProto proto, const Address &gateway_addr,
                   unsigned short internal_port, unsigned short external_port, unsigned int lifetime = 7200);

        Address local_addr() const {
            return sock_.local_addr();
        }

        State state() const { return state_; }

        unsigned int time() const { return time_; }
        Address external_ip() const { return external_ip_; }

        void Update(unsigned int dt_ms);

    private:
        State state_;

        UDPSocket sock_, multicast_listen_sock_;
        Address gateway_addr_, external_ip_;

        unsigned int time_;

        PMPProto proto_;
        unsigned short internal_port_, external_port_;
        unsigned int lifetime_;

        unsigned int main_timer_;
        unsigned int request_timer_, deadline_timer_;
        unsigned int period_;
        unsigned int mapped_time_;
    };
}

