#include "VarContainer.h"

#include <stdexcept>

template<>
void Net::VarContainer::SaveVar<Net::VarContainer>(const Var<VarContainer> &v) {
    assert(CheckHashes(v.hash_.hash));
    Packet pack = v.Pack();

    header_.push_back((int_type) v.hash_.hash);
    header_.push_back((int_type) data_bytes_.size());

    data_bytes_.insert(data_bytes_.end(), pack.begin(), pack.end());
}

template<>
bool Net::VarContainer::LoadVar<Net::VarContainer>(Var<VarContainer> &v) const {
    for (unsigned int i = 0; i < header_.size(); i += 2) {
        if (header_[i] == (int_type) v.hash_.hash) {
            const size_t off = uint16_t(header_[i + 1]);
            const size_t end = i + 2 < header_.size() ? size_t(uint16_t(header_[(i + 2) + 1])) : data_bytes_.size();
            if (off > data_bytes_.size() || end < off || end > data_bytes_.size()) {
                return false;
            }
            v.UnPack(&data_bytes_[off], end - off);
            return true;
        }
    }
    return false;
}

template<>
void Net::VarContainer::SaveVar<std::string>(const Var<std::string> &v) {
    assert(CheckHashes(v.hash_.hash));

    header_.push_back((int_type) v.hash_.hash);
    header_.push_back((int_type) data_bytes_.size());

    data_bytes_.insert(data_bytes_.end(), v.begin(), v.end());
}

template<>
bool Net::VarContainer::LoadVar<std::string>(Var<std::string> &v) const {
    for (unsigned int i = 0; i < header_.size(); i += 2) {
        if (header_[i] == (int_type) v.hash_.hash) {
            const size_t off = uint16_t(header_[i + 1]);
            // length = next entry's offset minus this one's, never the raw next offset
            const size_t end = i + 2 < header_.size() ? size_t(uint16_t(header_[i + 2 + 1])) : data_bytes_.size();
            if (off > data_bytes_.size() || end < off || end > data_bytes_.size()) {
                return false;
            }
            v = std::string((const char *) &data_bytes_[off], end - off);
            return true;
        }
    }
    return false;
}

Net::Packet Net::VarContainer::Pack() const {
    Packet dst;

    auto num_vars = (int_type) (header_.size() / 2);
    auto num_vars_beg = (int_type) dst.size();
    auto data_size = (int_type) data_bytes_.size();
    dst.resize(num_vars_beg + sizeof(int_type) * 2);
    memcpy(&dst[num_vars_beg], &num_vars, sizeof(int_type));
    memcpy(&dst[num_vars_beg + sizeof(int_type)], &data_size, sizeof(int_type));

    auto header_beg = (int_type) dst.size();
    auto header_size = (int_type) (header_.size() * sizeof(int_type));
    dst.resize(header_beg + header_size);
    memcpy(&dst[header_beg], &header_[0], header_size);

    auto data_beg = (int_type) dst.size();
    dst.resize(data_beg + data_bytes_.size());
    memcpy(&dst[data_beg], &data_bytes_[0], data_bytes_.size());

    return dst;
}

bool Net::VarContainer::UnPack(const Packet &pack) {
    return UnPack(&pack[0], pack.size());
}

bool Net::VarContainer::UnPack(const unsigned char *pack, size_t len) {
    if (pack == nullptr || len < 2 * sizeof(int_type)) {
        return false;
    }

    int_type num_vars, data_size;
    memcpy(&num_vars, &pack[0], sizeof(int_type));
    memcpy(&data_size, &pack[sizeof(int_type)], sizeof(int_type));

    // num_vars and data_size are attacker-controlled: validate every region
    // against len with wraparound-safe arithmetic before copying anything
    const size_t num_vars_v = uint16_t(num_vars);
    const size_t data_size_v = uint16_t(data_size);
    const size_t header_beg = 2 * sizeof(int_type);
    const size_t header_size = size_t(2) * num_vars_v * sizeof(int_type);
    if (header_beg + header_size > len) {
        return false;
    }
    const size_t data_beg = header_beg + header_size;
    if (data_beg + data_size_v > len) {
        return false;
    }

    header_.resize(size_t(2) * num_vars_v);
    if (header_size != 0) {
        memcpy(&header_[0], &pack[header_beg], header_size);
    }

    data_bytes_.resize(data_size_v);
    if (data_size_v != 0) {
        memcpy(&data_bytes_[0], &pack[data_beg], data_size_v);
    }

    return true;
}

size_t Net::VarContainer::size() const {
    return header_.size() / 2;
}

void Net::VarContainer::clear() {
    header_.clear();
    data_bytes_.clear();
}