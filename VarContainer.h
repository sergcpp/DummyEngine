#pragma once

#include <cassert>
#include <string>
#include <vector>

#include "Types.h"
#include "Var.h"

namespace Net {
typedef std::vector<unsigned char> Packet;
class VarContainer {
public:
    typedef le_uint16 int_type;

    template<class T>
    void SaveVar(const Var<T> &v) {
        assert(CheckHashes(v.hash_.hash));
        int_type var_beg = (int_type)data_bytes_.size();
        header_.push_back(v.hash_.hash);
        header_.push_back(var_beg);

        int_type data_beg = (int_type)data_bytes_.size();
        data_bytes_.resize(var_beg + sizeof(T));

        memcpy(&data_bytes_[data_beg], v.p_val(), sizeof(T));
    }
    template<class T>
    bool LoadVar(Var<T> &v) const {
        for (unsigned int i = 0; i < header_.size(); i += 2) {
            if (header_[i] == (int_type)v.hash_.hash) {
                memcpy(v.p_val(), &data_bytes_[header_[i + 1]], sizeof(T));
                return true;
            }
        }
        return false;
    }
    template<class T>
    void UpdateVar(const Var<T> &v) {
        static_assert(!std::is_same<T, VarContainer>::value, "Cannot update VarContainer");
        for (unsigned int i = 0; i < header_.size(); i += 2) {
            if (header_[i] == (int_type)v.hash_.hash) {
                memcpy(&data_bytes_[header_[i + 1]], v.p_val(), sizeof(T));
                return;
            }
        }
        SaveVar(v);
    }
    Packet Pack() const;
    bool UnPack(const Packet &pack);
    bool UnPack(const unsigned char *pack, size_t len);
    size_t size() const;
    void clear();

private:
    std::vector<int_type> header_;
    std::vector<unsigned char> data_bytes_;

    bool CheckHashes(int_type hash) const {
        for (unsigned int i = 0; i < header_.size(); i += 2) {
            if (header_[i] == hash) {
                return false;
            }
        }
        return true;
    }
};

template<>
void VarContainer::SaveVar<VarContainer>(const Var<VarContainer> &v);
template<>
bool VarContainer::LoadVar<VarContainer>(Var<VarContainer> &v) const;

template<>
void VarContainer::SaveVar<std::string>(const Var<std::string> &v);
template<>
bool VarContainer::LoadVar<std::string>(Var<std::string> &v) const;

}
