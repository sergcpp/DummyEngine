#include "NAT_PMP.h"

Net::PMPSession::PMPSession(ePMPProto proto, const Net::Address &gateway_addr, unsigned short internal_port,
                            unsigned short external_port, unsigned int lifetime)
    : state_(eState::RetrieveExternalIP), gateway_addr_(gateway_addr), time_(0), proto_(proto),
      internal_port_(internal_port), external_port_(external_port), lifetime_(lifetime), main_timer_(0),
      request_timer_(0), deadline_timer_(64000), period_(250), mapped_time_(0) {
    sock_.Open(0);
    multicast_listen_sock_.Open(5350);
}

void Net::PMPSession::Update(unsigned int dt_ms) {
    char recv_buf[64];

    main_timer_ += dt_ms;
    if (state_ == eState::RetrieveExternalIP) {
        if (main_timer_ >= deadline_timer_) {
            state_ = eState::IdleUnsupported;
            return;
        } else if (main_timer_ >= request_timer_) {
            PMPExternalIPRequest req;
            sock_.Send(gateway_addr_, &req, sizeof(PMPExternalIPRequest));

            request_timer_ += period_;
            period_ *= 2;
        }

        Address sender;
        int size = sock_.Receive(sender, recv_buf, sizeof(recv_buf));
        if (size == sizeof(PMPExternalIPResponse) && sender == gateway_addr_) {
            const auto *resp = reinterpret_cast<const PMPExternalIPResponse *>(recv_buf);
            if (resp->vers() == 0 && resp->op() == 128 + uint8_t(ePMPOpCode::ExternalIPRequest)) {

                if (resp->res_code() == ePMPResCode::Success) {
                    external_ip_ = Address(resp->ip(), 0);
                    time_ = resp->time();
                    state_ = eState::CreatePortMapping;

                    request_timer_ = main_timer_;
                    deadline_timer_ = main_timer_ + 64000;
                    period_ = 250;
                } else {
                    state_ = eState::IdleRetrieveExternalIPError;
                }
            }
        } else if (size == sizeof(PMPUnsupportedVersionResponse) && sender == gateway_addr_) {
            const auto *resp = reinterpret_cast<const PMPUnsupportedVersionResponse *>(recv_buf);
            if (resp->vers() == 0 && resp->op() == 128 + uint8_t(ePMPOpCode::ExternalIPRequest)) {
                state_ = eState::IdleRetrieveExternalIPError;
            }
        }
    } else if (state_ == eState::CreatePortMapping) {
        if (main_timer_ >= deadline_timer_) {
            state_ = eState::IdleUnsupported;
            return;
        } else if (main_timer_ >= request_timer_) {
            PMPMappingRequest req(proto_, internal_port_, external_port_);
            req.set_lifetime(lifetime_);
            sock_.Send(gateway_addr_, &req, sizeof(PMPMappingRequest));

            request_timer_ += period_;
            period_ *= 2;
        }

        Address sender;
        if (sock_.Receive(sender, recv_buf, sizeof(recv_buf)) == sizeof(PMPMappingResponse) &&
            sender == gateway_addr_) {
            const auto *resp = reinterpret_cast<const PMPMappingResponse *>(recv_buf);
            if (resp->vers() == 0 &&
                resp->op() ==
                    128 + uint8_t(proto_ == ePMPProto::UDP ? ePMPOpCode::MapUDPRequest : ePMPOpCode::MapTCPRequest)) {
                if (resp->res_code() == ePMPResCode::Success) {
                    time_ = resp->time();
                    state_ = eState::IdleMapped;
                    mapped_time_ = main_timer_;
                } else {
                    state_ = eState::IdleCreatePortMappingError;
                }
            }
        }
    } else if (state_ == eState::IdleMapped) {
        Address sender;
        if (multicast_listen_sock_.Receive(sender, recv_buf, sizeof(recv_buf)) == sizeof(PMPExternalIPResponse) &&
            sender.port() == gateway_addr_.port()) {
            const auto *resp = reinterpret_cast<const PMPExternalIPResponse *>(recv_buf);
            if (resp->vers() == 0 && resp->op() == 128 + uint8_t(ePMPOpCode::ExternalIPRequest)) {

                Address new_external_ip = Address(resp->ip(), 0);

                if (resp->res_code() == ePMPResCode::Success &&
                    (external_ip_ != new_external_ip ||
                     // should add 7/8 of elapsed time and check if difference more than 2 secs
                     (time_ + ((main_timer_ - mapped_time_) / 1142) - resp->time()) > 2)) {

                    external_ip_ = Address(resp->ip(), 0);
                    state_ = eState::CreatePortMapping;

                    request_timer_ = main_timer_;
                    deadline_timer_ = main_timer_ + 64000;
                    period_ = 250;
                }
            }
        } else if (main_timer_ - mapped_time_ > lifetime_ * 1000) {
            state_ = eState::CreatePortMapping;

            request_timer_ = main_timer_;
            deadline_timer_ = main_timer_ + 64000;
            period_ = 250;
        }
    } else if (state_ == eState::IdleRetrieveExternalIPError || state_ == eState::IdleCreatePortMappingError ||
               state_ == eState::IdleUnsupported) {
    }
}