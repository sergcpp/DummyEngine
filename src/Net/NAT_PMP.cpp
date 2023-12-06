#include "NAT_PMP.h"

Net::PMPSession::PMPSession(PMPProto proto, const Net::Address &gateway_addr, unsigned short internal_port,
                            unsigned short external_port, unsigned int lifetime)
    : state_(RETRIEVE_EXTERNAL_IP), gateway_addr_(gateway_addr), time_(0), proto_(proto), internal_port_(internal_port),
      external_port_(external_port), lifetime_(lifetime), main_timer_(0), request_timer_(0), deadline_timer_(64000),
      period_(250), mapped_time_(0) {
    sock_.Open(0);
    multicast_listen_sock_.Open(5350);
}

void Net::PMPSession::Update(unsigned int dt_ms) {
    char recv_buf[64];

    main_timer_ += dt_ms;
    if (state_ == RETRIEVE_EXTERNAL_IP) {
        if (main_timer_ >= deadline_timer_) {
            state_ = IDLE_UNSUPPORTED;
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
            if (resp->vers() == 0 && resp->op() == 128 + OP_EXTERNAL_IP_REQUEST) {

                if (resp->res_code() == PMP_RES_SUCCESS) {
                    external_ip_ = Address(resp->ip(), 0);
                    time_ = resp->time();
                    state_ = CREATE_PORT_MAPPING;

                    request_timer_ = main_timer_;
                    deadline_timer_ = main_timer_ + 64000;
                    period_ = 250;
                } else {
                    state_ = IDLE_RETRIEVE_EXTERNAL_IP_ERROR;
                }
            }
        } else if (size == sizeof(PMPUnsupportedVersionResponse) && sender == gateway_addr_) {
            const auto *resp = reinterpret_cast<const PMPUnsupportedVersionResponse *>(recv_buf);
            if (resp->vers() == 0 && resp->op() == 128 + OP_EXTERNAL_IP_REQUEST) {
                state_ = IDLE_RETRIEVE_EXTERNAL_IP_ERROR;
            }
        }
    } else if (state_ == CREATE_PORT_MAPPING) {
        if (main_timer_ >= deadline_timer_) {
            state_ = IDLE_UNSUPPORTED;
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
                resp->op() == 128 + (proto_ == PMP_UDP ? OP_MAP_UDP_REQUEST : OP_MAP_TCP_REQUEST)) {
                if (resp->res_code() == PMP_RES_SUCCESS) {
                    time_ = resp->time();
                    state_ = IDLE_MAPPED;
                    mapped_time_ = main_timer_;
                } else {
                    state_ = IDLE_CREATE_PORT_MAPPING_ERROR;
                }
            }
        }
    } else if (state_ == IDLE_MAPPED) {
        Address sender;
        if (multicast_listen_sock_.Receive(sender, recv_buf, sizeof(recv_buf)) == sizeof(PMPExternalIPResponse) &&
            sender.port() == gateway_addr_.port()) {
            const auto *resp = reinterpret_cast<const PMPExternalIPResponse *>(recv_buf);
            if (resp->vers() == 0 && resp->op() == 128 + OP_EXTERNAL_IP_REQUEST) {

                Address new_external_ip = Address(resp->ip(), 0);

                if (resp->res_code() == PMP_RES_SUCCESS &&
                    (external_ip_ != new_external_ip ||
                     // should add 7/8 of elapsed time and check if difference more than 2 secs
                     (time_ + ((main_timer_ - mapped_time_) / 1142) - resp->time()) > 2)) {

                    external_ip_ = Address(resp->ip(), 0);
                    state_ = CREATE_PORT_MAPPING;

                    request_timer_ = main_timer_;
                    deadline_timer_ = main_timer_ + 64000;
                    period_ = 250;
                }
            }
        } else if (main_timer_ - mapped_time_ > lifetime_ * 1000) {
            state_ = CREATE_PORT_MAPPING;

            request_timer_ = main_timer_;
            deadline_timer_ = main_timer_ + 64000;
            period_ = 250;
        }
    } else if (state_ == IDLE_RETRIEVE_EXTERNAL_IP_ERROR || state_ == IDLE_CREATE_PORT_MAPPING_ERROR ||
               state_ == IDLE_UNSUPPORTED) {
    }
}