#include "VarContainer.h"

#include <stdexcept>

template<>
void Net::VarContainer::SaveVar<Net::VarContainer>(const Var<VarContainer> &v) {
    assert(CheckHashes(v.hash_.hash));
    Packet pack = v.Pack();

    header_.push_back((int_type)v.hash_.hash);
    header_.push_back((int_type)data_bytes_.size());

    data_bytes_.insert(data_bytes_.end(), pack.begin(), pack.end());
}

template<>
bool Net::VarContainer::LoadVar<Net::VarContainer>(Var<VarContainer> &v) const {
    for (unsigned int i = 0; i < header_.size(); i += 2) {
        if (header_[i] == (int_type)v.hash_.hash) {
            size_t len = i < header_.size() - 2 ? (header_[(i + 2) + 1] - header_[i + 1]) : (data_bytes_.size() - header_[i + 1]);
            v.UnPack(&data_bytes_[header_[i + 1]], len);
            return true;
        }
    }
    return false;
}

template<>
void Net::VarContainer::SaveVar<std::string>(const Var<std::string> &v) {
    assert(CheckHashes(v.hash_.hash));

    header_.push_back((int_type)v.hash_.hash);
    header_.push_back((int_type)data_bytes_.size());

    data_bytes_.insert(data_bytes_.end(), v.begin(), v.end());
}

template<>
bool Net::VarContainer::LoadVar<std::string>(Var<std::string> &v) const {
    for (unsigned int i = 0; i < header_.size(); i += 2) {
        if (header_[i] == (int_type)v.hash_.hash) {
            const char *beg = (const char *)&data_bytes_[header_[i + 1]];
            size_t n = (i + 2 < header_.size()) ? header_[i + 2 + 1] : (data_bytes_.size() - header_[i + 1]);
            v = std::string(beg, n);
            return true;
        }
    }
    return false;
}

Net::Packet Net::VarContainer::Pack() const {
    Packet dst;

    auto num_vars     = (int_type)(header_.size() / 2);
    auto num_vars_beg = (int_type)dst.size();
    auto data_size    = (int_type)data_bytes_.size();
    dst.resize(num_vars_beg + sizeof(int_type) * 2);
    memcpy(&dst[num_vars_beg], &num_vars, sizeof(int_type));
    memcpy(&dst[num_vars_beg + sizeof(int_type)], &data_size, sizeof(int_type));

    auto header_beg  = (int_type)dst.size();
    auto header_size = (int_type)(header_.size() * sizeof(int_type));
    dst.resize(header_beg + header_size);
    memcpy(&dst[header_beg], &header_[0], header_size);

    auto data_beg = (int_type)dst.size();
    dst.resize(data_beg + data_bytes_.size());
    memcpy(&dst[data_beg], &data_bytes_[0], data_bytes_.size());

    return dst;
}
bool Net::VarContainer::UnPack(const Packet &pack) {
    return UnPack(&pack[0], pack.size());
}

bool Net::VarContainer::UnPack(const unsigned char *pack, size_t len) {
    if (len < 8) return false;

    int_type num_vars, data_size;
    int_type num_vars_beg = 0;
    memcpy(&num_vars, &pack[0], sizeof(int_type));
    memcpy(&data_size, &pack[sizeof(int_type)], sizeof(int_type));

    auto header_beg = (int_type)(num_vars_beg + sizeof(int_type) * 2);
    auto header_size = (int_type)(2 * num_vars * sizeof(int_type));
    if (header_size + 8 > len || header_beg > header_size) {
        return false;
    }
    header_.resize(2 * num_vars);
    memcpy(&header_[0], &pack[header_beg], header_size);

    auto data_beg = (int_type)(header_beg + header_size);
    data_bytes_.resize(data_beg + data_size);
    memcpy(&data_bytes_[0], &pack[data_beg], data_size);

    return true;
}

size_t Net::VarContainer::size() const {
    return header_.size() / 2;
}

void Net::VarContainer::clear() {
    header_.clear();
    data_bytes_.clear();
}