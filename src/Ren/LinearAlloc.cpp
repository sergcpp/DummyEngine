#include "LinearAlloc.h"

#include "Log.h"

#ifdef __GNUC__
#define force_inline __attribute__((always_inline)) inline
#endif
#ifdef _MSC_VER
#define force_inline __forceinline

#include <intrin.h>

#pragma intrinsic(_BitScanForward64)
//#pragma intrinsic(_tzcnt_u64)
#endif

namespace Ren {
force_inline bool GetFirstBit(const uint64_t mask, unsigned long *bit_index) {
#ifdef _MSC_VER
    return _BitScanForward64(bit_index, mask);
#else
    const int ret = __builtin_ffsll(mask);
    (*bit_index) = ret - 1;
    return ret != 0;
#endif
}

force_inline int CountTrailingZeroes(const uint64_t mask) {
#ifdef _MSC_VER
    //return int(_tzcnt_u64(mask));
    if (mask == 0) {
        return 64;
    }
    unsigned long r = 0;
    _BitScanForward64(&r, mask);
    return r;
#else
    if (mask == 0) {
        return 64;
    }
    return __builtin_ctzll(mask);
#endif
}
}

uint32_t Ren::LinearAlloc::Alloc(const uint32_t req_size, const char *tag) {
    const uint32_t blocks_required = (req_size + block_size_ - 1) / block_size_;
    if (blocks_required > block_count_) {
        return 0xffffffff;
    }

    uint32_t best_blocks_available = block_count_ + 1;
    uint32_t best_loc = 0xffffffff;

    uint32_t loc_beg = 0;
    // Skip initial occupied blocks
    while (loc_beg < (block_count_ / BitmapGranularity) && !bitmap_[loc_beg]) {
        ++loc_beg;
    }

#if 1
    const uint32_t loc_lim = (block_count_ - blocks_required) / BitmapGranularity + 1;
    unsigned long bit_beg = 0;
    while (loc_beg < loc_lim) {
        if (GetFirstBit(bitmap_[loc_beg] & ~((1ull << bit_beg) - 1), &bit_beg)) {
            int bit_end = CountTrailingZeroes(~(bitmap_[loc_beg] | ((1ull << bit_beg) - 1)));
            uint32_t loc_end = loc_beg;
            if (bit_end == BitmapGranularity) {
                ++loc_end;
                bit_end = 0;
                while (loc_end < (block_count_ / BitmapGranularity)) {
                    bit_end = CountTrailingZeroes(~bitmap_[loc_end]);
                    if (bit_end != BitmapGranularity) {
                        break;
                    }
                    ++loc_end;
                    bit_end = 0;
                }
            }

            const uint32_t blocks_found = (loc_end - loc_beg) * BitmapGranularity - bit_beg + bit_end;
            if (blocks_found >= blocks_required && blocks_found < best_blocks_available) {
                best_blocks_available = blocks_found;
                best_loc = loc_beg * BitmapGranularity + bit_beg;
                if (blocks_found == blocks_required) {
                    // Perfect fit was found, can stop here
                    break;
                }
            }
            bit_beg = bit_end;
            loc_beg = loc_end;
        } else {
            ++loc_beg;
            bit_beg = 0;
        }
    }
#else
    loc_beg *= BitmapGranularity;
    for (; loc_beg <= block_count_ - blocks_required;) {
        if (!bitmap_[loc_beg / BitmapGranularity]) {
            const int count = (BitmapGranularity - (loc_beg % BitmapGranularity));
            loc_beg += count;
            continue;
        }

        // Count the number of available blocks
        uint32_t loc_end = loc_beg;
        while (loc_end < block_count_) {
            const int xword_index = loc_end / BitmapGranularity;
            const int bit_index = loc_end % BitmapGranularity;
            if ((bitmap_[xword_index] & (1ull << bit_index)) == 0) {
                break;
            }
            ++loc_end;
        }

        if ((loc_end - loc_beg) >= blocks_required && (loc_end - loc_beg) < best_blocks_available) {
            best_blocks_available = (loc_end - loc_beg);
            best_loc = loc_beg;
            if ((loc_end - loc_beg) == blocks_required) {
                // Perfect fit was found, can stop here
                break;
            }
        }
        loc_beg = loc_end + 1;
    }
#endif

    if (best_loc != 0xffffffff) {
        // Mark blocks as occupied
        for (uint32_t i = best_loc; i < best_loc + blocks_required; ++i) {
            const int xword_index = i / BitmapGranularity;
            const int bit_index = i % BitmapGranularity;
            bitmap_[xword_index] &= ~(1ull << bit_index);
        }
        return block_size_ * best_loc;
    }

    return 0xffffffff;
}

void Ren::LinearAlloc::Free(const uint32_t offset, const uint32_t size) {
    assert(offset % block_size_ == 0);
    const uint32_t block_index = int(offset / block_size_);
    assert(block_index < block_count_);
    const uint32_t blocks_required = (size + block_size_ - 1) / block_size_;

    // Mark blocks as free
    for (uint32_t i = block_index; i < block_index + blocks_required; ++i) {
        const int xword_index = i / BitmapGranularity;
        const int bit_index = i % BitmapGranularity;
        assert((bitmap_[xword_index] & (1ull << bit_index)) == 0);
        bitmap_[xword_index] |= (1ull << bit_index);
    }
}

void Ren::LinearAlloc::PrintNode(int i, std::string prefix, bool is_tail, ILog *log) const {
#if 0
    const auto &node = nodes_[i];
    if (is_tail) {
        if (!node.has_children() && node.is_free) {
            log->Info("%s+- [0x%08x..0x%08x) <free>", prefix.c_str(), node.offset, node.offset + node.size);
        } else {
#ifndef NDEBUG
            log->Info("%s+- [0x%08x..0x%08x) <%s>", prefix.c_str(), node.offset, node.offset + node.size, node.tag);
#else
            log->Info("%s+- [0x%08x..0x%08x) <occupied>", prefix.c_str(), node.offset, node.offset + node.size);
#endif
        }
        prefix += "   ";
    } else {
        if (!node.has_children() && node.is_free) {
            log->Info("%s|- [0x%08x..0x%08x) <free>", prefix.c_str(), node.offset, node.offset + node.size);
        } else {
#ifndef NDEBUG
            log->Info("%s|- [0x%08x..0x%08x) <%s>", prefix.c_str(), node.offset, node.offset + node.size, node.tag);
#else
            log->Info("%s|- [0x%08x..0x%08x) <occupied>", prefix.c_str(), node.offset, node.offset + node.size);
#endif
        }
        prefix += "|  ";
    }

    if (node.child[0] != -1) {
        PrintNode(node.child[0], prefix, false, log);
    }

    if (node.child[1] != -1) {
        PrintNode(node.child[1], prefix, true, log);
    }
#endif
}

void Ren::LinearAlloc::Clear() {
    // Mark all blocks as free
    for (uint32_t i = 0; i < block_count_; ++i) {
        const int xword_index = i / BitmapGranularity;
        const int bit_index = i % BitmapGranularity;
        bitmap_[xword_index] |= (1ull << bit_index);
    }
}