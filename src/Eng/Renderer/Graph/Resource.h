#pragma once

#include <cstdint>

#include <Ren/Resource.h>

enum class eRpResType : uint8_t { Undefined, Buffer, Texture };

struct RpResource {
    eRpResType type = eRpResType::Undefined;
    union {
        struct {
            uint8_t read_count;
            uint8_t write_count;
        };
        uint16_t _generation = 0;
    };
    Ren::eResState desired_state = Ren::eResState::Undefined;
    Ren::eStageBits stages = Ren::eStageBits::None;
    uint32_t index = 0xffffffff;

    Ren::eStageBits src_stages = Ren::eStageBits::None;
    Ren::eStageBits dst_stages = Ren::eStageBits::None;

    RpResource *next_use = nullptr;

    RpResource() = default;
    RpResource(eRpResType _type, uint16_t __generation, Ren::eResState _desired_state, Ren::eStageBits _stages,
               uint32_t _index)
        : type(_type), _generation(__generation), desired_state(_desired_state), stages(_stages), index(_index) {}

    operator bool() const { return type != eRpResType::Undefined; }

    static bool LessThanTypeAndIndex(const RpResource &lhs, const RpResource &rhs) {
        if (lhs.type != rhs.type) {
            return lhs.type < rhs.type;
        }
        return lhs.index < rhs.index;
    }
};

struct RpBufDesc {
    Ren::eBufType type;
    uint32_t size;
};

inline bool operator==(const RpBufDesc &lhs, const RpBufDesc &rhs) {
    return lhs.size == rhs.size && lhs.type == rhs.type;
}