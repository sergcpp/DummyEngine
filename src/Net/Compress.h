#pragma once

#include "VarContainer.h"

namespace Net {
    Packet CompressLZO(const Packet &pack);
    Packet DecompressLZO(const Packet &pack);
}
