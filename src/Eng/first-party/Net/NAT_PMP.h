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
enum class ePMPOpCode { ExternalIPRequest = 0, MapUDPRequest, MapTCPRequest };

enum class ePMPResCode { Success = 0, UnsupportedVersion, Refused, Failure, OutOfResources };

enum class ePMPProto { UDP = 0, TCP };

struct PMPRequest {
    uint8_t buf[2];

    PMPRequest(uint8_t vers, ePMPOpCode op) {
        buf[0] = vers;
        buf[1] = uint8_t(op);
    }

    [[nodiscard]] uint8_t vers() const { return buf[0]; }

    [[nodiscard]] uint8_t op() const { return buf[1]; }
};

struct PMPExternalIPRequest : public PMPRequest {
    PMPExternalIPRequest() : PMPRequest(0, ePMPOpCode::ExternalIPRequest) {}
};

struct PMPExternalIPResponse {
    uint8_t buf[12];

    PMPExternalIPResponse() {
        buf[0] = 0;
        buf[1] = 128 + uint8_t(ePMPOpCode::ExternalIPRequest);
    }

    [[nodiscard]] uint8_t vers() const { return buf[0]; }

    [[nodiscard]] uint8_t op() const { return buf[1]; }

    [[nodiscard]] ePMPResCode res_code() const { return ePMPResCode(ntohs(*(uint16_t *)(&buf[2]))); }

    [[nodiscard]] uint32_t time() const { return ntohl(*(uint32_t *)(&buf[4])); }

    [[nodiscard]] uint32_t ip() const { return ntohl(*(uint32_t *)(&buf[8])); }

    void set_res_code(uint16_t code) { *(uint16_t *)(&buf[2]) = htons(code); }

    void set_time(uint32_t t) { *(uint32_t *)(&buf[4]) = htonl(t); }

    void set_ip(uint32_t ip) { *(uint32_t *)(&buf[8]) = htonl(ip); }
};

struct PMPMappingRequest {
    uint8_t buf[12];

    PMPMappingRequest(ePMPProto proto, uint16_t internal_port, uint16_t external_port) {
        buf[0] = 0;
        buf[1] = (proto == ePMPProto::UDP) ? uint8_t(ePMPOpCode::MapUDPRequest) : uint8_t(ePMPOpCode::MapTCPRequest);

        buf[2] = 0;
        buf[3] = 0;

        set_internal_port(internal_port);
        set_external_port(external_port);
        set_lifetime(7200);
    }

    [[nodiscard]] uint8_t vers() const { return buf[0]; }

    [[nodiscard]] uint8_t op() const { return buf[1]; }

    [[nodiscard]] uint16_t reserved() const { return *(uint16_t *)(&buf[2]); }

    [[nodiscard]] uint16_t internal_port() const { return ntohs(*(uint16_t *)(&buf[4])); }

    [[nodiscard]] uint16_t external_port() const { return ntohs(*(uint16_t *)(&buf[6])); }

    [[nodiscard]] uint32_t lifetime() const { return ntohl(*(uint32_t *)(&buf[8])); }

    void set_internal_port(uint16_t port) { *(uint16_t *)(&buf[4]) = htons(port); }

    void set_external_port(uint16_t port) { *(uint16_t *)(&buf[6]) = htons(port); }

    void set_lifetime(uint32_t t) { *(uint32_t *)(&buf[8]) = htonl(t); }
};

struct PMPMappingResponse {
    uint8_t buf[16];

    PMPMappingResponse() {}

    PMPMappingResponse(const ePMPProto proto) {
        buf[0] = 0;
        buf[1] = (proto == ePMPProto::UDP) ? 128 + uint8_t(ePMPOpCode::MapUDPRequest)
                                           : 128 + uint8_t(ePMPOpCode::MapTCPRequest);
    }

    [[nodiscard]] uint8_t vers() const { return buf[0]; }

    [[nodiscard]] uint8_t op() const { return buf[1]; }

    [[nodiscard]] ePMPResCode res_code() const { return ePMPResCode(ntohs(*(uint16_t *)(&buf[2]))); }

    [[nodiscard]] uint32_t time() const { return ntohl(*(uint32_t *)(&buf[4])); }

    [[nodiscard]] uint16_t internal_port() const { return ntohs(*(uint16_t *)(&buf[8])); }
    [[nodiscard]] uint16_t external_port() const { return ntohs(*(uint16_t *)(&buf[10])); }

    [[nodiscard]] uint32_t lifetime() const { return ntohl(*(uint32_t *)(&buf[12])); }

    void set_res_code(ePMPResCode code) { *(uint16_t *)(&buf[2]) = htons(uint16_t(code)); }

    void set_time(uint32_t t) { *(uint32_t *)(&buf[4]) = htonl(t); }

    void set_internal_port(uint16_t port) { *(uint16_t *)(&buf[8]) = htons(port); }

    void set_external_port(uint16_t port) { *(uint16_t *)(&buf[10]) = htons(port); }

    void set_lifetime(uint32_t t) { *(uint32_t *)(&buf[12]) = htonl(t); }
};

struct PMPUnsupportedVersionResponse {
    uint8_t buf[8];

    PMPUnsupportedVersionResponse() {}

    PMPUnsupportedVersionResponse(ePMPOpCode op) {
        buf[0] = 0;
        buf[1] = 128 + uint8_t(op);
    }

    [[nodiscard]] uint8_t vers() const { return buf[0]; }

    [[nodiscard]] uint8_t op() const { return buf[1]; }

    [[nodiscard]] ePMPResCode res_code() const { return ePMPResCode(ntohs(*(uint16_t *)(&buf[2]))); }

    [[nodiscard]] uint32_t time() const { return ntohl(*(uint32_t *)(&buf[4])); }

    void set_res_code(ePMPResCode code) { *(uint16_t *)(&buf[2]) = htons(uint16_t(code)); }

    void set_time(uint32_t t) { *(uint32_t *)(&buf[4]) = htonl(t); }
};

static_assert(sizeof(PMPUnsupportedVersionResponse) == 8);

class PMPSession {
  public:
    enum class eState {
        RetrieveExternalIP,
        CreatePortMapping,
        IdleMapped,
        IdleRetrieveExternalIPError,
        IdleCreatePortMappingError,
        IdleUnsupported,
    };

    PMPSession(ePMPProto proto, const Address &gateway_addr, unsigned short internal_port, unsigned short external_port,
               unsigned int lifetime = 7200);

    [[nodiscard]] Address local_addr() const { return sock_.local_addr(); }

    [[nodiscard]] eState state() const { return state_; }

    [[nodiscard]] unsigned int time() const { return time_; }

    [[nodiscard]] Address external_ip() const { return external_ip_; }

    void Update(unsigned int dt_ms);

  private:
    eState state_;

    UDPSocket sock_, multicast_listen_sock_;
    Address gateway_addr_, external_ip_;

    unsigned int time_;

    ePMPProto proto_;
    unsigned short internal_port_, external_port_;
    unsigned int lifetime_;

    unsigned int main_timer_;
    unsigned int request_timer_, deadline_timer_;
    unsigned int period_;
    unsigned int mapped_time_;
};
} // namespace Net
